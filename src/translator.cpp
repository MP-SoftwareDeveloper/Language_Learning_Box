#include "translator.h"

#include <QSharedPointer>
#include "appmode.h"
#include "cardstore.h"
#include "translation/azuretranslate.h"
#include "translation/googletranslate.h"
#include "translation/mymemory.h"
#include "translation/tatoeba.h"
#include "translation/wiktionary.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>

namespace {
constexpr auto kConnection = "translations";
constexpr int kTimeoutMs = 8000;
constexpr qint64 kGoogleCooldownMs = 10 * 60 * 1000; // Google said 429: use the fallback for 10 minutes
constexpr qint64 kAzureCooldownMs = 10 * 60 * 1000;  // Azure refused (bad key, monthly quota): skip it for a while

QSqlDatabase db() { return QSqlDatabase::database(QLatin1String(kConnection)); }
QString key(const QString &source, const QString &text)
{
    const QString k = text.simplified().toCaseFolded();
    // German keeps the original keys; other source languages get a prefix ("en|", "fa|").
    return source == QLatin1String("de") ? k : source + u'|' + k;
}
} // namespace

Translator *Translator::instance()
{
    static Translator *s = new Translator(QCoreApplication::instance());
    return s;
}

Translator *Translator::create(QQmlEngine *, QJSEngine *engine)
{
    Translator *s = instance();
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

Translator::Translator(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(AppMode::instance(), &AppMode::changed, this, &Translator::networkChanged); // useOnline depends on it
    // Switching to a learning box with another language changes source and meaning language.
    // The meaning language is chosen per learning box, too.
    m_lastSource = sourceLanguage() + u'>' + meaningLanguage();
    connect(CardStore::instance(), &CardStore::changed, this, [this] {
        const QString s = sourceLanguage() + u'>' + meaningLanguage();
        if (s != m_lastSource) {
            m_lastSource = s;
            emit settingsChanged();
            emit savedCountChanged();
        }
    });
    QSettings settings;
    m_onlineEnabled = settings.value(QStringLiteral("translation/online"), true).toBool();
    m_azureKey = settings.value(QStringLiteral("translation/azureKey")).toString().trimmed();
    m_azureRegion = settings.value(QStringLiteral("translation/azureRegion")).toString().trimmed();

    m_nam->setTransferTimeout(kTimeoutMs);

    if (QNetworkInformation::loadDefaultBackend() && QNetworkInformation::instance()) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged,
                this, [this](QNetworkInformation::Reachability r) {
                    qInfo() << "Translator: network reachability" << r;
                    emit networkChanged();
                });
        qInfo() << "Translator: network backend" << QNetworkInformation::instance()->backendName()
                << "reachability" << QNetworkInformation::instance()->reachability();
    }
    m_cacheOk = openCache();
}

bool Translator::networkAvailable() const
{
    const auto *info = QNetworkInformation::instance();
    if (!info)
        return true; // no backend on this platform: just try, and fall back on failure
    const auto r = info->reachability();
    return r == QNetworkInformation::Reachability::Online
        || r == QNetworkInformation::Reachability::Unknown;
}

QString Translator::targetLanguage() const
{
    return CardStore::instance()->meaningLanguage();
}

void Translator::setTargetLanguage(const QString &lang)
{
    CardStore *store = CardStore::instance();
    // German boxes: the last choice (Persian / English) is also the default for new German boxes.
    if (store->learningLanguage() == QLatin1String("de") && (lang == QLatin1String("fa") || lang == QLatin1String("en")))
        QSettings().setValue(QStringLiteral("translation/target"), lang);
    store->setMeaningLanguage(lang); // emits CardStore::changed -> settingsChanged above
}

void Translator::setOnlineEnabled(bool on)
{
    if (on == m_onlineEnabled)
        return;
    m_onlineEnabled = on;
    QSettings().setValue(QStringLiteral("translation/online"), m_onlineEnabled);
    emit settingsChanged();
    emit networkChanged(); // useOnline depends on it
}

QString Translator::sourceLanguage() const
{
    return CardStore::instance()->learningLanguage();
}

QString Translator::meaningLanguage() const
{
    return CardStore::instance()->meaningLanguage();
}

bool Translator::useOnline() const
{
    return m_onlineEnabled && AppMode::instance()->full();
}

int Translator::translate(const QString &text)
{
    return translateBetween(text, sourceLanguage(), meaningLanguage());
}

int Translator::translateBetween(const QString &text, const QString &source, const QString &target)
{
    const int id = m_nextId++;
    const QString t = text.simplified();
    if (t.isEmpty()) {
        QTimer::singleShot(0, this, [=, this] { emit translated(id, {}, {}, {}, {}); });
        return id;
    }
    if (!useOnline()) {
        QTimer::singleShot(0, this, [=, this] { answerOffline(id, t, source, target, {}); });
        return id;
    }

    if (azureConfigured() && QDateTime::currentMSecsSinceEpoch() >= m_azureBlockedUntil)
        requestAzure(id, t, source, target);
    else
        requestFree(id, t, source, target);
    return id;
}

void Translator::requestFree(int id, const QString &text, const QString &source, const QString &target)
{
    if (QDateTime::currentMSecsSinceEpoch() < m_googleBlockedUntil)
        requestMyMemory(id, text, source, target, QStringLiteral("Google limit reached (429)"));
    else
        requestGoogle(id, text, source, target);
}

void Translator::setAzureKey(const QString &key)
{
    const QString k = key.trimmed();
    if (k == m_azureKey)
        return;
    m_azureKey = k;
    m_azureBlockedUntil = 0; // a new key deserves a new try
    QSettings().setValue(QStringLiteral("translation/azureKey"), k);
    emit azureChanged();
}

void Translator::setAzureRegion(const QString &region)
{
    const QString r = region.trimmed().toLower();
    if (r == m_azureRegion)
        return;
    m_azureRegion = r;
    m_azureBlockedUntil = 0;
    QSettings().setValue(QStringLiteral("translation/azureRegion"), r);
    emit azureChanged();
}

QNetworkRequest Translator::azureRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=UTF-8"));
    req.setRawHeader(AzureTranslate::kKeyHeader, m_azureKey.toUtf8());
    if (!m_azureRegion.isEmpty()) // needed for regional resources; the global endpoint wants it with the key
        req.setRawHeader(AzureTranslate::kRegionHeader, m_azureRegion.toUtf8());
    return req;
}

void Translator::requestAzure(int id, const QString &text, const QString &source, const QString &target)
{
    QNetworkReply *reply = m_nam->post(azureRequest(AzureTranslate::translateUrl(source, target)),
                                       AzureTranslate::requestBody(text));
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (http == 401 || http == 403 || http == 429) // wrong key or this month's quota used up
                m_azureBlockedUntil = QDateTime::currentMSecsSinceEpoch() + kAzureCooldownMs;
            const QString detail = AzureTranslate::errorMessage(body);
            error = QStringLiteral("Azure ") + errorText(reply)
                    + (detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QLatin1Char(')'));
        } else {
            const auto r = AzureTranslate::parseTranslation(body);
            if (r.error.isEmpty()) {
                if (AzureTranslate::isSingleWord(text)
                    && !m_azureNoDictionary.contains(source + u'>' + target))
                    azureAlternatives(id, text, source, target, r.text);
                else {
                    store(source, target, text, r.text, {});
                    emit translated(id, r.text, {}, QStringLiteral("online"), {});
                }
                return;
            }
            error = QStringLiteral("Azure: ") + r.error;
        }
        qWarning().noquote() << "Azure translation failed:" << error;
        requestFree(id, text, source, target); // Google, then MyMemory, as without a key
    });
}

void Translator::azureAlternatives(int id, const QString &text, const QString &source, const QString &target,
                                   const QString &main)
{
    QNetworkReply *reply = m_nam->post(azureRequest(AzureTranslate::lookupUrl(source, target)),
                                       AzureTranslate::requestBody(text));
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        QStringList alternatives;
        if (reply->error() == QNetworkReply::NoError) {
            alternatives = AzureTranslate::parseLookup(reply->readAll(), main);
        } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400) {
            m_azureNoDictionary.insert(source + u'>' + target); // no dictionary for this pair: don't ask again
        }
        // The translation itself is already good; the dictionary is only a bonus.
        store(source, target, text, main, alternatives);
        emit translated(id, main, alternatives, QStringLiteral("online"), {});
    });
}

void Translator::testAzure()
{
    if (m_azureTesting)
        return;
    if (m_azureKey.isEmpty()) {
        emit azureTested(false, tr("Enter the key first (Azure portal \u2192 your Translator resource \u2192 Keys and Endpoint)."));
        return;
    }
    m_azureTesting = true;
    emit azureChanged();
    QNetworkReply *reply = m_nam->post(azureRequest(AzureTranslate::translateUrl(QStringLiteral("de"), QStringLiteral("en"))),
                                       AzureTranslate::requestBody(QStringLiteral("Hallo")));
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        bool ok = false;
        QString message;
        if (reply->error() == QNetworkReply::NoError) {
            const auto r = AzureTranslate::parseTranslation(body);
            ok = r.error.isEmpty();
            message = ok ? tr("The key works (Hallo \u2192 %1).").arg(r.text) : r.error;
        } else {
            const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString detail = AzureTranslate::errorMessage(body);
            message = detail.isEmpty() ? errorText(reply) : detail;
            if (http == 401 || http == 403)
                message += tr(" Check the key and the region (Location in Keys and Endpoint).");
        }
        if (ok)
            m_azureBlockedUntil = 0;
        m_azureTesting = false;
        emit azureChanged();
        emit azureTested(ok, message);
    });
}

QString Translator::errorText(QNetworkReply *reply)
{
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http == 429)
        return QStringLiteral("limit reached (429)");
    if (http >= 400)
        return QStringLiteral("HTTP %1").arg(http);
    return reply->errorString();
}

void Translator::requestGoogle(int id, const QString &text, const QString &source, const QString &target)
{
    QNetworkRequest req(GoogleTranslate::requestUrl(source, target));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded;charset=UTF-8"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (LearningBox)"));
    QNetworkReply *reply = m_nam->post(req, GoogleTranslate::requestBody(text));
    connect(reply, &QNetworkReply::finished, this, [=, this] { onReply(reply, id, text, source, target); });
}

void Translator::onReply(QNetworkReply *reply, int id, const QString &text, const QString &source,
                         const QString &target)
{
    reply->deleteLater();
    QString error;
    if (reply->error() != QNetworkReply::NoError) {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        error = QStringLiteral("Google ") + errorText(reply);
        if (http == 429 || http == 403) // blocked for now: don't hit it again for every word
            m_googleBlockedUntil = QDateTime::currentMSecsSinceEpoch() + kGoogleCooldownMs;
    } else {
        const auto r = GoogleTranslate::parse(reply->readAll());
        if (r.error.isEmpty()) {
            store(source, target, text, r.text, r.alternatives);
            // Always the pair that was asked for; callers drop answers they no longer wait for
            // (request ids are reset when the language changes).
            emit translated(id, r.text, r.alternatives, QStringLiteral("online"), {});
            return;
        }
        error = QStringLiteral("Google: ") + r.error;
    }
    qWarning().noquote() << "Translation online failed:" << error;
    requestMyMemory(id, text, source, target, error);
}

void Translator::requestMyMemory(int id, const QString &text, const QString &source, const QString &target,
                                 const QString &googleError)
{
    if (!MyMemory::fits(text)) { // long text: the free fallback only takes short lines
        answerOffline(id, text, source, target, googleError);
        return;
    }
    QNetworkRequest req(MyMemory::requestUrl(text, source, target));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox/1.0 (Qt; German vocabulary app)"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            error = QStringLiteral("MyMemory ") + errorText(reply);
        } else {
            const auto r = MyMemory::parse(reply->readAll(), text, target);
            if (r.error.isEmpty()) {
                store(source, target, text, r.text, r.alternatives); // offline later, like Google's answers
                emit translated(id, r.text, r.alternatives, QStringLiteral("online"), {});
                return;
            }
            error = QStringLiteral("MyMemory: ") + r.error;
        }
        qWarning().noquote() << "Translation fallback failed:" << error;
        answerOffline(id, text, source, target, googleError + QStringLiteral("; ") + error);
    });
}

void Translator::remember(const QString &text, const QString &translation)
{
    if (!text.trimmed().isEmpty() && !translation.trimmed().isEmpty())
        store(sourceLanguage(), meaningLanguage(), text.simplified(), translation.trimmed(), {});
}

void Translator::rememberIn(const QString &language, const QString &text, const QString &translation)
{
    if (!language.isEmpty() && !text.trimmed().isEmpty() && !translation.trimmed().isEmpty())
        store(sourceLanguage(), language, text.simplified(), translation.trimmed(), {});
}

int Translator::suggestExamples(const QString &word)
{
    const int id = m_nextId++;
    const QString source = sourceLanguage();
    const QString w = tatoeba::searchWord(word, source);
    if (w.isEmpty() || !useOnline()) {
        QTimer::singleShot(0, this, [=, this] {
            emit examplesSuggested(id, {}, useOnline() ? QString() : QStringLiteral("offline"));
        });
        return id;
    }
    const QString target = meaningLanguage();
    QNetworkRequest req(tatoeba::searchUrl(w, target, 10, source));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox (Qt)"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        QString error;
        QVariantList out;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && !body.trimmed().startsWith('{')) {
            error = reply->errorString();
        } else {
            for (const tatoeba::Example &e : tatoeba::parse(body, w, target, 3, &error, source)) {
                if (!e.translation.isEmpty())
                    store(source, target, e.text, e.translation, {}); // offline later, and ExampleText finds it
                out.append(QVariantMap{{QStringLiteral("text"), e.text},
                                       {QStringLiteral("translation"), e.translation}});
            }
        }
        if (!error.isEmpty())
            qWarning().noquote() << "Tatoeba:" << error;
        emit examplesSuggested(id, out, error);
    });
    return id;
}

namespace {
QVariantMap grammarMap(const wiktionary::Grammar &g)
{
    if (!g.valid())
        return {};
    return {{QStringLiteral("front"), wiktionary::front(g)},
            {QStringLiteral("lemma"), g.lemma},
            {QStringLiteral("plural"), wiktionary::pluralText(g)},
            {QStringLiteral("pluralLine"), wiktionary::pluralLine(g)},
            // "Plural Hunde" / "Singular der Hund", "Singular die Lehrerin" + "Plural Lehrerinnen": one line each
            {QStringLiteral("forms"), wiktionary::formLines(g).join(QLatin1Char('\n'))}};
}
} // namespace

// One Wiktionary page; finished(grammar, error). A word that is a plural form ("Hunde") is followed to its
// noun ("Hund"): the answer then describes the noun and has singularOf set.
void Translator::fetchGrammar(const QString &word, bool followSingular, const QString &article,
                              std::function<void(const wiktionary::Grammar &, const QString &)> finished)
{
    QNetworkRequest req(wiktionary::requestUrl(word));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox/1.0 (Qt; German vocabulary app)"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        reply->deleteLater();
        QString error;
        wiktionary::Grammar g;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && !body.trimmed().startsWith('{'))
            error = reply->errorString();
        else
            g = wiktionary::parse(body, &error, article);
        if (!error.isEmpty() || g.singularOf.isEmpty() || !followSingular) {
            finished(g, error);
            return;
        }
        fetchGrammar(g.singularOf, false, QString(), [=](const wiktionary::Grammar &noun, const QString &err) {
            finished(noun.valid() ? wiktionary::withSingular(g, noun) : wiktionary::Grammar{}, err);
        });
    });
}

int Translator::suggestWords(const QString &prefix, const QString &language)
{
    const int id = m_nextId++;
    const QString p = prefix.trimmed();
    const QStringList variants = wiktionary::suggestVariants(p);
    if (variants.isEmpty() || !useOnline() || !(language == QLatin1String("de") || language == QLatin1String("en")
                                                 || language == QLatin1String("fa"))) {
        QTimer::singleShot(0, this, [=, this] { emit wordsSuggested(id, {}, QString()); });
        return id;
    }
    struct State
    {
        int pending = 0;
        QList<QStringList> lists;
        QString error;
    };
    auto state = QSharedPointer<State>::create();
    state->pending = int(variants.size());
    state->lists.resize(variants.size());
    for (int i = 0; i < variants.size(); ++i) {
        QNetworkRequest req(wiktionary::suggestUrl(language, variants.at(i)));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox/1.0 (Qt; German vocabulary app)"));
        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [=, this] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                state->lists[i] = wiktionary::parseSuggestions(reply->readAll());
            else
                state->error = reply->errorString();
            if (--state->pending == 0)
                emit wordsSuggested(id, wiktionary::mergeSuggestions(state->lists, p), state->error);
        });
    }
    return id;
}

int Translator::lookupGrammar(const QString &word)
{
    const int id = m_nextId++;
    const QString w = wiktionary::lemmaOf(word);
    const auto answer = [=, this](const QVariantMap &map, const QString &error) {
        QTimer::singleShot(0, this, [=, this] { emit grammarFound(id, map, error); });
    };
    // Typed with "der" / "das" ("der Reis"): a singular noun. A page that says the word is the plural form of
    // another noun ("Reis" = plural of "Real") is then the wrong entry: no grammar, not the other noun's forms.
    const QString first = word.simplified().section(QLatin1Char(' '), 0, 0).toLower();
    const bool hasArticle = word.simplified().contains(QLatin1Char(' '))
                            && (first == QLatin1String("der") || first == QLatin1String("die") || first == QLatin1String("das"));
    // The article picks the right entry on a page with several ("der Reis" / "die Reise"), see wiktionary::parse
    const QString hint = hasArticle ? first : QString();
    const QString cacheKey = hasArticle ? first + QLatin1Char(' ') + w : w;
    const bool singularTyped = hasArticle && first != QLatin1String("die");
    const auto view = [=](const wiktionary::Grammar &g) {
        return singularTyped && !g.singularOf.isEmpty() ? QVariantMap() : grammarMap(g);
    };
    if (w.isEmpty()) {
        answer({}, QString());
        return id;
    }
    // Saved answer first (row in the translation cache, language "grammar7"; "-" = looked up, not a noun)
    QString saved;
    QStringList extra;
    if (lookup(QStringLiteral("de"), QStringLiteral("grammar7"), cacheKey, &saved, &extra)) {
        answer(saved == QLatin1String("-") ? QVariantMap()
                                           : view(wiktionary::decode(saved, extra.value(0, w))),
               QString());
        return id;
    }
    if (!useOnline()) {
        answer({}, QStringLiteral("offline"));
        return id;
    }
    const auto done = [=, this](const wiktionary::Grammar &g, const QString &error) {
        if (error.isEmpty())
            store(QStringLiteral("de"), QStringLiteral("grammar7"), cacheKey,
                  g.valid() ? wiktionary::encode(g) : QStringLiteral("-"),
                  g.valid() ? QStringList{g.lemma.isEmpty() ? w : g.lemma} : QStringList());
        else
            qWarning().noquote() << "Wiktionary:" << error;
        emit grammarFound(id, view(g), error);
    };
    const auto proceed = [=, this] {
    fetchGrammar(w, true, hint, [=, this](const wiktionary::Grammar &g, const QString &error) {
        // No entry, or only a plural-only entry whose "lemma" is the word itself: "Nudeln" may be the plural of
        // "Nudel" without Wiktionary's page saying so in a form we read. Guess the singular and check it.
        const bool pluralOnly = g.valid() && g.singularOf.isEmpty() && g.lemma.compare(w, Qt::CaseInsensitive) == 0
                                && g.plurals.contains(w, Qt::CaseInsensitive);
        if (error.isEmpty() && hint.isEmpty() && (!g.valid() || pluralOnly)) {
            const QStringList guesses = wiktionary::singularCandidates(w);
            if (!guesses.isEmpty()) {
                guessSingular(w, g, guesses, done);
                return;
            }
        }
        if (!error.isEmpty() || !g.valid()) {
            done(g, error);
            return;
        }
        // The male / female forms get their plural from their own pages
        const auto femalePlural = [=, this](const wiktionary::Grammar &g2) {
            if (g2.feminine.isEmpty()) {
                done(g2, QString());
                return;
            }
            fetchGrammar(g2.feminine.first(), false, QString(), [=](const wiktionary::Grammar &f, const QString &) {
                wiktionary::Grammar g3 = g2;
                g3.femininePlural = f.plurals.join(QStringLiteral(" / "));
                done(g3, QString());
            });
        };
        if (g.masculine.isEmpty()) {
            femalePlural(g);
            return;
        }
        fetchGrammar(g.masculine.first(), false, QString(), [=](const wiktionary::Grammar &m, const QString &) {
            wiktionary::Grammar g2 = g;
            g2.masculinePlural = m.plurals.join(QStringLiteral(" / "));
            femalePlural(g2);
        });
    });
    };
    // A word typed in lowercase (Lens often reads it that way) without an article: "kellner" is a noun without
    // its capital letter, but "gehen" or "gut" are not ("das Gehen", "das Gut" would be wrong). Wiktionary
    // titles are case-sensitive, so a page with the lowercase title means: not a noun.
    const bool hadArticle = word.simplified().contains(QLatin1Char(' '));
    if (!hadArticle && w.at(0).isLower()) {
        QNetworkRequest req(wiktionary::existsUrl(w));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox/1.0 (Qt; German vocabulary app)"));
        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [=, this] {
            reply->deleteLater();
            const QByteArray body = reply->readAll();
            QString error;
            if (reply->error() != QNetworkReply::NoError && !body.trimmed().startsWith('{')) {
                done({}, reply->errorString());
                return;
            }
            const bool exists = wiktionary::pageExists(body, &error);
            if (!error.isEmpty())
                done({}, error);
            else if (exists)
                done({}, QString()); // saved as "not a noun"
            else
                proceed();
        });
        return id;
    }
    proceed();
    return id;
}

void Translator::guessSingular(const QString &plural, const wiktionary::Grammar &original,
                               QStringList candidates,
                               std::function<void(const wiktionary::Grammar &, const QString &)> finished)
{
    if (candidates.isEmpty()) {
        finished(original, QString());
        return;
    }
    const QString candidate = candidates.takeFirst();
    fetchGrammar(candidate, false, QString(), [=, this](const wiktionary::Grammar &noun, const QString &error) {
        if (!error.isEmpty()) { // no connection: not "no singular", so nothing is saved
            finished(original, error);
            return;
        }
        if (noun.valid() && noun.singularOf.isEmpty() && !noun.genders.isEmpty()
                && noun.plurals.contains(plural, Qt::CaseInsensitive)) {
            wiktionary::Grammar page;
            page.singularOf = candidate;
            finished(wiktionary::withSingular(page, noun), QString());
            return;
        }
        guessSingular(plural, original, candidates, finished);
    });
}

void Translator::answerOffline(int id, const QString &text, const QString &source, const QString &target,
                               const QString &error)
{
    QString translation;
    QStringList alternatives;
    if (lookup(source, target, text, &translation, &alternatives))
        emit translated(id, translation, alternatives, QStringLiteral("saved"), error);
    else
        emit translated(id, {}, {}, {}, error);
}

QString Translator::savedBetween(const QString &text, const QString &source, const QString &target) const
{
    QString t;
    QStringList alt;
    return lookup(source, target, text, &t, &alt) ? t : QString();
}

QString Translator::saved(const QString &text) const
{
    QString t;
    QStringList alt;
    return lookup(sourceLanguage(), meaningLanguage(), text, &t, &alt) ? t : QString();
}

// ---------- cache (AppDataLocation/translations.sqlite) ----------

bool Translator::openCache()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QSqlDatabase d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kConnection));
    d.setDatabaseName(dir + QStringLiteral("/translations.sqlite"));
    if (!d.open()) {
        qWarning() << "Translation cache:" << d.lastError().text();
        return false;
    }
    QSqlQuery q(d);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS translations ("
            " lang TEXT NOT NULL, source TEXT NOT NULL, text TEXT NOT NULL,"
            " alternatives TEXT NOT NULL DEFAULT '', saved_at INTEGER NOT NULL,"
            " PRIMARY KEY (lang, source))"))) {
        qWarning() << "Translation cache:" << q.lastError().text();
        return false;
    }
    return true;
}

void Translator::store(const QString &source, const QString &target, const QString &text,
                       const QString &translation, const QStringList &alternatives)
{
    if (!m_cacheOk)
        return;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO translations (lang, source, text, alternatives, saved_at)"
                             " VALUES (?, ?, ?, ?, strftime('%s','now'))"));
    q.addBindValue(target);
    q.addBindValue(key(source, text));
    q.addBindValue(translation);
    q.addBindValue(alternatives.join(QLatin1Char('\n')));
    if (q.exec())
        emit savedCountChanged();
    else
        qWarning() << "Translation cache:" << q.lastError().text();
}

bool Translator::lookup(const QString &source, const QString &target, const QString &text,
                        QString *translation, QStringList *alternatives) const
{
    if (!m_cacheOk)
        return false;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT text, alternatives FROM translations WHERE lang = ? AND source = ?"));
    q.addBindValue(target);
    q.addBindValue(key(source, text));
    if (!q.exec() || !q.next())
        return false;
    *translation = q.value(0).toString();
    // An answer saved earlier in a wrong script (Chinese for an English meaning, from the fallback translator) is
    // not used; looking the word up again online replaces it.
    if ((target == QLatin1String("en") || target == QLatin1String("de"))
            && !MyMemory::plausibleFor(*translation, target))
        return false;
    const QString alts = q.value(1).toString();
    *alternatives = alts.isEmpty() ? QStringList() : alts.split(QLatin1Char('\n'));
    return true;
}

int Translator::savedCount() const
{
    if (!m_cacheOk)
        return 0;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM translations WHERE lang = ?"));
    q.addBindValue(meaningLanguage());
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

void Translator::clearSaved()
{
    if (!m_cacheOk)
        return;
    QSqlQuery q(db());
    q.exec(QStringLiteral("DELETE FROM translations"));
    emit savedCountChanged();
}
