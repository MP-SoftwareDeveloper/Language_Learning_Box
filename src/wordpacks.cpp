#include "wordpacks.h"
#include "cardstore.h"
#include "translator.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>
#include <QJSEngine>
#include <QQmlEngine>
#include <QDebug>

namespace {
constexpr auto kPackResource = ":/data/wordpacks/netzwerk_neu_a1.tsv";
constexpr auto kStarterResource = ":/data/starter/starter_100.tsv";
constexpr auto kStarterDeck = "Starter";
}

WordPacks *WordPacks::create(QQmlEngine *, QJSEngine *engine)
{
    static WordPacks *s = new WordPacks(QCoreApplication::instance());
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

WordPacks::WordPacks(QObject *parent)
    : QObject(parent)
{
    QFile f(QString::fromLatin1(kPackResource));
    if (!f.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("word pack resource missing");
    } else if (!parseWordPack(QString::fromUtf8(f.readAll()), &m_pack, &m_error)) {
        m_error.prepend(QStringLiteral("word pack: "));
    }
    if (!m_error.isEmpty())
        qWarning().noquote() << m_error;

    QFile sf(QString::fromLatin1(kStarterResource));
    QString starterError;
    if (!sf.open(QIODevice::ReadOnly) || !parseStarterDeck(QString::fromUtf8(sf.readAll()), &m_starter, &starterError))
        qWarning().noquote() << "starter cards:" << (starterError.isEmpty() ? sf.errorString() : starterError);

    // Lookup index: "der apfel" and "apfel" both map to the "der Apfel" entry.
    static const QStringList articles = {QStringLiteral("der "), QStringLiteral("die "), QStringLiteral("das ")};
    for (const auto &ch : std::as_const(m_pack.chapters)) {
        for (const Card &c : ch.cards) {
            QString key = c.front.toCaseFolded();
            key.remove(QStringLiteral(" (pl.)"));
            m_index.insert(key, &c);
            if (!c.exampleTranslation.isEmpty())
                m_exampleFa.insert(c.example.simplified().toCaseFolded(), c.exampleTranslation);
            for (const QString &a : articles)
                if (key.startsWith(a))
                    m_index.insert(key.mid(a.size()), &c);
        }
    }

    connect(CardStore::instance(), &CardStore::changed, this, &WordPacks::refresh);
    refresh();
}

QString WordPacks::deckName(const WordPack &pack, int chapter)
{
    return QStringLiteral("%1 · K%2").arg(pack.title).arg(chapter);
}

int WordPacks::totalWords() const
{
    int n = 0;
    for (const auto &c : m_pack.chapters)
        n += int(c.cards.size());
    return n;
}

void WordPacks::refresh()
{
    const QSet<QString> inBox = CardStore::instance()->frontKeys();
    m_chapters.clear();
    m_wordsInBox = 0;
    for (const auto &ch : std::as_const(m_pack.chapters)) {
        int added = 0;
        for (const Card &c : ch.cards)
            if (inBox.contains(c.front.toCaseFolded()))
                ++added;
        m_wordsInBox += added;
        m_chapters.append(QVariantMap{
            {QStringLiteral("number"), ch.number},
            {QStringLiteral("title"), ch.title},
            {QStringLiteral("total"), int(ch.cards.size())},
            {QStringLiteral("inBox"), added},
        });
    }
    m_starterInBox = 0;
    for (const StarterCard &c : std::as_const(m_starter))
        if (inBox.contains(c.german.toCaseFolded()))
            ++m_starterInBox;
    emit chaptersChanged();
}

int WordPacks::addStarterCards()
{
    if (CardStore::instance()->learningLanguage() != QLatin1String("de"))
        return 0; // German words: only for German learning boxes
    Translator *tr = Translator::instance();
    const QString lang = tr ? tr->targetLanguage() : QStringLiteral("fa");
    QList<Card> cards;
    cards.reserve(m_starter.size());
    for (const StarterCard &s : std::as_const(m_starter)) {
        Card c;
        c.front = s.german;
        c.back = starterBack(s, lang);
        c.example = s.exampleDe;
        cards.append(c);
        if (tr) {
            tr->rememberIn(QStringLiteral("en"), s.german, s.english);
            tr->rememberIn(QStringLiteral("fa"), s.german, s.persian);
            tr->rememberIn(QStringLiteral("en"), s.exampleDe, s.exampleEn);
            tr->rememberIn(QStringLiteral("fa"), s.exampleDe, s.exampleFa);
        }
    }
    return qMax(0, CardStore::instance()->addCards(cards, QString::fromLatin1(kStarterDeck)));
}

int WordPacks::addChapter(int number)
{
    const WordPackChapter *ch = m_pack.chapter(number);
    if (!ch)
        return 0;
    return qMax(0, CardStore::instance()->addCards(ch->cards, deckName(m_pack, number)));
}

int WordPacks::addAll()
{
    int n = 0;
    for (const auto &ch : std::as_const(m_pack.chapters))
        n += addChapter(ch.number);
    return n;
}

QVariantList WordPacks::chapterWords(int number) const
{
    QVariantList out;
    const WordPackChapter *ch = m_pack.chapter(number);
    if (!ch)
        return out;
    const QSet<QString> inBox = CardStore::instance()->frontKeys();
    for (const Card &c : ch->cards) {
        out.append(QVariantMap{
            {QStringLiteral("front"), c.front},
            {QStringLiteral("back"), c.back},
            {QStringLiteral("example"), c.example},
            {QStringLiteral("exampleFa"), c.exampleTranslation},
            {QStringLiteral("inBox"), inBox.contains(c.front.toCaseFolded())},
        });
    }
    return out;
}

const Card *WordPacks::find(const QString &word) const
{
    // The packs are German: an English learning box never uses them ("Bus" is not "der Bus").
    if (CardStore::instance()->learningLanguage() != QLatin1String("de"))
        return nullptr;
    const QString key = word.trimmed().toCaseFolded();
    if (key.isEmpty())
        return nullptr;
    return m_index.value(key, nullptr);
}

QVariantMap WordPacks::lookup(const QString &word) const
{
    const Card *c = find(word);
    if (!c)
        return {};
    return {
        {QStringLiteral("front"), c->front},
        {QStringLiteral("back"), c->back},
        {QStringLiteral("example"), c->example},
    };
}

int WordPacks::addWords(const QStringList &words, const QString &deck)
{
    QList<Card> cards;
    for (const QString &w : words) {
        const QString word = w.trimmed();
        if (word.isEmpty())
            continue;
        if (const Card *known = find(word)) {
            Card c = *known;
            cards.append(c);
        } else {
            Card c;
            c.front = word;
            cards.append(c);
        }
    }
    return qMax(0, CardStore::instance()->addCards(cards, deck));
}

int WordPacks::addTranslatedWords(const QVariantList &items, const QString &deck)
{
    QList<Card> cards;
    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        const QString word = m.value(QStringLiteral("word")).toString().trimmed();
        const QString back = m.value(QStringLiteral("back")).toString().trimmed();
        if (word.isEmpty())
            continue;
        Card c;
        if (const Card *known = find(word))
            c = *known;
        else
            c.front = word;
        if (!back.isEmpty())
            c.back = back;
        cards.append(c);
    }
    return qMax(0, CardStore::instance()->addCards(cards, deck));
}

QString WordPacks::exampleTranslation(const QString &example) const
{
    return m_exampleFa.value(example.simplified().toCaseFolded());
}

namespace {
QString withoutArticle(const QString &s)
{
    for (const QString &a : {QStringLiteral("der "), QStringLiteral("die "), QStringLiteral("das ")})
        if (s.startsWith(a))
            return s.mid(a.size());
    return s;
}
} // namespace

QVariantList WordPacks::suggest(const QString &prefix, int max) const
{
    QVariantList out;
    const QString p = withoutArticle(prefix.simplified().toCaseFolded());
    if (p.size() < 2)
        return out;
    QList<const Card *> starts, contains;
    for (const auto &ch : std::as_const(m_pack.chapters)) {
        for (const Card &c : ch.cards) {
            const QString f = withoutArticle(c.front.toCaseFolded());
            if (f == p && c.front.toCaseFolded() == prefix.simplified().toCaseFolded())
                continue; // exactly what is typed already
            if (f.startsWith(p))
                starts.append(&c);
            else if (p.size() >= 3 && f.contains(p))
                contains.append(&c);
        }
    }
    std::stable_sort(starts.begin(), starts.end(), [](const Card *a, const Card *b) {
        return withoutArticle(a->front).size() < withoutArticle(b->front).size();
    });
    for (const Card *c : starts + contains) {
        if (out.size() >= max)
            break;
        out.append(QVariantMap{{QStringLiteral("front"), c->front},
                               {QStringLiteral("back"), c->back},
                               {QStringLiteral("example"), c->example}});
    }
    return out;
}

QVariantList WordPacks::examplesContaining(const QString &word, int max) const
{
    QVariantList out;
    const QString w = withoutArticle(word.simplified().toCaseFolded());
    if (w.size() < 2)
        return out;
    const QRegularExpression re(QStringLiteral("(^|[^\\p{L}])%1($|[^\\p{L}])").arg(QRegularExpression::escape(w)),
                                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    for (const auto &ch : std::as_const(m_pack.chapters)) {
        for (const Card &c : ch.cards) {
            if (out.size() >= max)
                return out;
            if (re.match(c.example).hasMatch())
                out.append(QVariantMap{{QStringLiteral("text"), c.example},
                                       {QStringLiteral("translation"), c.exampleTranslation}});
        }
    }
    return out;
}
