#include "translator.h"
#include "appmode.h"
#include "cardstore.h"
#include "translation/googletranslate.h"
#include "translation/tatoeba.h"

#include <QCoreApplication>
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

QSqlDatabase db() { return QSqlDatabase::database(QLatin1String(kConnection)); }
QString key(const QString &source, const QString &text)
{
    const QString k = text.simplified().toCaseFolded();
    return source == QLatin1String("en") ? QStringLiteral("en|") + k : k;
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
    const int id = m_nextId++;
    const QString t = text.simplified();
    if (t.isEmpty()) {
        QTimer::singleShot(0, this, [=, this] { emit translated(id, {}, {}, {}, {}); });
        return id;
    }
    if (!useOnline()) {
        QTimer::singleShot(0, this, [=, this] { answerOffline(id, t, {}); });
        return id;
    }

    const QString source = sourceLanguage();
    const QString target = meaningLanguage();
    QNetworkRequest req(GoogleTranslate::requestUrl(source, target));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded;charset=UTF-8"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (LearningBox)"));
    QNetworkReply *reply = m_nam->post(req, GoogleTranslate::requestBody(t));
    connect(reply, &QNetworkReply::finished, this, [=, this] { onReply(reply, id, t, source, target); });
    return id;
}

void Translator::onReply(QNetworkReply *reply, int id, const QString &text, const QString &source,
                         const QString &target)
{
    reply->deleteLater();
    QString error;
    if (reply->error() != QNetworkReply::NoError) {
        error = reply->errorString();
    } else {
        const auto r = GoogleTranslate::parse(reply->readAll());
        if (r.error.isEmpty()) {
            store(source, target, text, r.text, r.alternatives);
            if (target == meaningLanguage() && source == sourceLanguage()) {
                emit translated(id, r.text, r.alternatives, QStringLiteral("online"), {});
                return;
            }
        }
        error = r.error;
    }
    qWarning().noquote() << "Translation online failed:" << error;
    answerOffline(id, text, error);
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

void Translator::answerOffline(int id, const QString &text, const QString &error)
{
    QString translation;
    QStringList alternatives;
    if (lookup(sourceLanguage(), meaningLanguage(), text, &translation, &alternatives))
        emit translated(id, translation, alternatives, QStringLiteral("saved"), error);
    else
        emit translated(id, {}, {}, {}, error);
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
