#include "levelpacks.h"
#include "cardstore.h"
#include "translator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSet>
#include <QSettings>

namespace {
constexpr auto kResource = ":/data/levelpacks/starten_wir_levels.tsv";
constexpr auto kResourceSv = ":/data/levelpacks/svenska_levels.tsv";

QString learningLanguage()
{
    return CardStore::instance()->learningLanguage();
}

QString deckName(const LevelPack &pack, int chapter)
{
    return QStringLiteral("%1 · K%2").arg(pack.title).arg(chapter);
}

QString meaningLanguage()
{
    Translator *tr = Translator::instance();
    return tr ? tr->targetLanguage() : QStringLiteral("fa");
}
} // namespace

LevelPacks *LevelPacks::create(QQmlEngine *, QJSEngine *engine)
{
    static LevelPacks *s = new LevelPacks(QCoreApplication::instance());
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

LevelPacks::LevelPacks(QObject *parent)
    : QObject(parent)
{
    for (const char *resource : {kResource, kResourceSv}) {
        QFile f(QString::fromLatin1(resource));
        QList<LevelPack> packs;
        QString error;
        if (!f.open(QIODevice::ReadOnly))
            error = QStringLiteral("level pack resource missing: %1").arg(QString::fromLatin1(resource));
        else if (!parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &error))
            error.prepend(QStringLiteral("level packs: "));
        if (!error.isEmpty() && m_error.isEmpty())
            m_error = error;
        m_packs += packs;
    }
    if (!m_error.isEmpty())
        qWarning().noquote() << m_error;

    connect(CardStore::instance(), &CardStore::changed, this, &LevelPacks::refresh);
    refresh();
}

int LevelPacks::totalWords() const
{
    int n = 0;
    const QString lang = learningLanguage();
    for (const LevelPack &p : m_packs)
        if (p.language == lang)
            n += p.totalWords();
    return n;
}

bool LevelPacks::available() const
{
    const QString lang = learningLanguage();
    return lang == QLatin1String("de") || lang == QLatin1String("sv");
}

void LevelPacks::refresh()
{
    const QSet<QString> inBox = CardStore::instance()->frontKeys();
    m_levels.clear();
    m_wordsInBox = 0;
    const QVariantList boxes = CardStore::instance()->collections();
    const QString lang = learningLanguage();
    for (const LevelPack &p : std::as_const(m_packs)) {
        if (p.language != lang)
            continue; // only the levels of the current learning language
        QVariantMap own;
        for (const QVariant &v : boxes)
            if (v.toMap().value(QStringLiteral("name")).toString() == p.title)
                own = v.toMap();
        QVariantList chapters;
        int levelIn = 0;
        for (const LevelChapter &c : p.chapters) {
            int added = 0;
            for (const LevelWord &w : c.words)
                if (inBox.contains(w.german.toCaseFolded()))
                    ++added;
            levelIn += added;
            chapters.append(QVariantMap{
                {QStringLiteral("number"), c.number},
                {QStringLiteral("title"), c.title},
                {QStringLiteral("total"), int(c.words.size())},
                {QStringLiteral("inBox"), added},
            });
        }
        const int ownTotal = own.value(QStringLiteral("total")).toInt();
        m_wordsInBox += ownTotal;
        m_levels.append(QVariantMap{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("title"), p.title},
            {QStringLiteral("subtitle"), p.subtitle},
            {QStringLiteral("cefr"), p.cefr},
            {QStringLiteral("total"), p.totalWords()},
            {QStringLiteral("inBox"), ownTotal},
            {QStringLiteral("learned"), own.value(QStringLiteral("learned")).toInt()},
            {QStringLiteral("due"), own.value(QStringLiteral("due")).toInt()},
            {QStringLiteral("boxId"), own.isEmpty() ? -1 : own.value(QStringLiteral("id")).toInt()},
            {QStringLiteral("chapters"), chapters},
        });
    }
    emit changed();
}

int LevelPacks::addWords(const LevelPack &pack, const LevelChapter &chapter)
{
    if (learningLanguage() != pack.language)
        return 0; // German words go to German boxes, Swedish words to Swedish boxes
    Translator *tr = Translator::instance();
    const QString lang = meaningLanguage();
    QList<Card> cards;
    cards.reserve(chapter.words.size());
    for (const LevelWord &w : chapter.words) {
        Card c;
        c.front = w.german;
        c.back = levelWordBack(w, lang);
        c.example = w.exampleDe;
        cards.append(c);
        if (tr) {
            tr->rememberIn(QStringLiteral("en"), w.german, w.english);
            tr->rememberIn(QStringLiteral("fa"), w.german, w.persian);
            if (!w.exampleDe.isEmpty()) {
                tr->rememberIn(QStringLiteral("en"), w.exampleDe, w.exampleEn);
                tr->rememberIn(QStringLiteral("fa"), w.exampleDe, w.exampleFa);
            }
        }
    }
    return qMax(0, CardStore::instance()->addCards(cards, deckName(pack, chapter.number)));
}

int LevelPacks::boxId(const LevelPack &p) const
{
    for (const QVariant &v : CardStore::instance()->collections()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("name")).toString() == p.title)
            return m.value(QStringLiteral("id")).toInt();
    }
    return -1;
}

int LevelPacks::createBox(const LevelPack &p)
{
    CardStore *store = CardStore::instance();
    const int id = store->createCollection(p.title, p.language);
    if (id < 0)
        return -1;
    store->selectCollection(id); // cards are added to the current learning box
    for (const LevelChapter &c : p.chapters)
        addWords(p, c);
    return id;
}

namespace {
// Settings switches "German / Swedish A1 and A2 learning boxes in the box list" (German on, Swedish off by default).
QString boxesKey(const QString &language)
{
    return language == QLatin1String("sv") ? QStringLiteral("levelpacks/swedishBoxes")
                                           : QStringLiteral("levelpacks/germanBoxes");
}
// First-start flag per language (German keeps its old key), so a deleted box does not come back.
QString installedKey(const QString &language)
{
    return language == QLatin1String("sv") ? QStringLiteral("levelpacks/installedSv")
                                           : QStringLiteral("levelpacks/installed");
}
} // namespace

bool LevelPacks::boxesOn(const QString &language) const
{
    // German boxes are on by default, Swedish ones off (switched on in Settings)
    return QSettings().value(boxesKey(language), language == QLatin1String("de")).toBool();
}

void LevelPacks::setBoxesOn(const QString &language, bool on)
{
    if (on == boxesOn(language))
        return;
    QSettings().setValue(boxesKey(language), on);
    CardStore *store = CardStore::instance();
    const int original = store->currentCollection();
    for (const LevelPack &p : std::as_const(m_packs)) {
        if (p.language != language)
            continue;
        int id = boxId(p);
        if (id < 0 && on)
            id = createBox(p); // creates, selects and fills it
        if (id >= 0)
            store->setCollectionHidden(id, !on);
    }
    if (original >= 0)
        store->selectCollection(original);
    refresh();
}

int LevelPacks::installBoxes(bool force)
{
    QSettings settings;
    CardStore *store = CardStore::instance();
    const int original = store->currentCollection();
    int created = 0;
    for (const QString &language : {QStringLiteral("de"), QStringLiteral("sv")}) {
        if (!force && settings.value(installedKey(language), false).toBool())
            continue;
        // Switched off in Settings: no boxes now; switching on creates them.
        if (boxesOn(language))
            for (const LevelPack &p : std::as_const(m_packs))
                if (p.language == language && boxId(p) < 0 && createBox(p) >= 0)
                    ++created;
        settings.setValue(installedKey(language), true);
    }
    if (original >= 0)
        store->selectCollection(original);
    refresh();
    return created;
}

bool LevelPacks::openLevel(const QString &levelId)
{
    for (const LevelPack &p : std::as_const(m_packs)) {
        if (p.id != levelId)
            continue;
        int id = boxId(p);
        if (id < 0)
            id = createBox(p);
        if (id < 0)
            return false;
        CardStore::instance()->setCollectionHidden(id, false); // opened on purpose: shown in the list
        CardStore::instance()->selectCollection(id);
        return true;
    }
    return false;
}

int LevelPacks::addChapter(const QString &levelId, int number)
{
    for (const LevelPack &p : std::as_const(m_packs))
        if (p.id == levelId)
            if (const LevelChapter *c = p.chapter(number))
                return addWords(p, *c);
    return 0;
}

int LevelPacks::addLevel(const QString &levelId)
{
    int n = 0;
    for (const LevelPack &p : std::as_const(m_packs))
        if (p.id == levelId)
            for (const LevelChapter &c : p.chapters)
                n += addWords(p, c);
    return n;
}

QVariantList LevelPacks::chapterWords(const QString &levelId, int number) const
{
    QVariantList out;
    const QString lang = meaningLanguage();
    const QSet<QString> inBox = CardStore::instance()->frontKeys();
    for (const LevelPack &p : m_packs) {
        if (p.id != levelId)
            continue;
        const LevelChapter *c = p.chapter(number);
        if (!c)
            break;
        for (const LevelWord &w : c->words) {
            out.append(QVariantMap{
                {QStringLiteral("front"), w.german},
                {QStringLiteral("back"), levelWordBack(w, lang)},
                {QStringLiteral("example"), w.exampleDe},
                {QStringLiteral("exampleFa"), w.exampleFa},
                {QStringLiteral("exampleEn"), w.exampleEn},
                {QStringLiteral("inBox"), inBox.contains(w.german.toCaseFolded())},
            });
        }
    }
    return out;
}
