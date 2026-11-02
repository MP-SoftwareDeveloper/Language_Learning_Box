#include "levelpacks.h"
#include "cardstore.h"
#include "translator.h"

#include <QCoreApplication>
#include <algorithm>
#include <QDebug>
#include <QFile>
#include <QJSEngine>
#include <QHash>
#include <QQmlEngine>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>

namespace {
constexpr auto kResource = ":/data/levelpacks/starten_wir_levels.tsv";
constexpr auto kResourceSv = ":/data/levelpacks/svenska_levels.tsv";
constexpr auto kResourceAdv = ":/data/levelpacks/deutsch_b1_c1.tsv"; // German B1, B2, C1
constexpr auto kResourceA1Complete = ":/data/levelpacks/deutsch_a1_complete.tsv"; // "A1_complete": whole A1 word list
constexpr auto kResourceCore = ":/data/levelpacks/deutsch_core.tsv"; // "Alltag": original core words for everyday conversation

// Extra boxes next to the A1-C1 levels ("-complete" word lists, the "core-" everyday-conversation box):
// they repeat or go beyond the level words, so they are not counted in the totals.
bool isComplete(const LevelPack &p)
{
    return p.id.endsWith(QLatin1String("-complete")) || p.id.startsWith(QLatin1String("core-"));
}

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
    for (const char *resource : {kResource, kResourceA1Complete, kResourceAdv, kResourceCore, kResourceSv}) {
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
    // Order for the list: German before Swedish, by CEFR level; an extra box follows the levels of its CEFR step.
    std::stable_sort(m_packs.begin(), m_packs.end(), [](const LevelPack &a, const LevelPack &b) {
        const bool aSv = a.language != QLatin1String("de"), bSv = b.language != QLatin1String("de");
        if (aSv != bSv)
            return bSv;
        if (a.cefr != b.cefr)
            return a.cefr < b.cefr;
        return !isComplete(a) && isComplete(b);
    });

    connect(CardStore::instance(), &CardStore::changed, this, &LevelPacks::refresh);
    refresh();
}

int LevelPacks::totalWords() const
{
    int n = 0;
    const QString lang = learningLanguage();
    for (const LevelPack &p : m_packs)
        if (p.language == lang && !isComplete(p)) // the extra boxes repeat words of the levels
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
    if (tr)
        tr->beginBatch(); // one transaction for all saved translations of this chapter
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
    if (tr)
        tr->endBatch();
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

// Adds the words of the pack that the box does not have yet (cards are unique per box), e.g. when
// the first fill of a big box was interrupted.
void LevelPacks::fillBox(const LevelPack &p, int boxId)
{
    CardStore *store = CardStore::instance();
    store->selectCollection(boxId);
    for (const LevelChapter &c : p.chapters)
        addWords(p, c);
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
    if (language == QLatin1String("sv"))
        return QStringLiteral("levelpacks/swedishBoxes");
    if (language == QLatin1String("en"))
        return QStringLiteral("levelpacks/englishBoxes");
    return QStringLiteral("levelpacks/germanBoxes");
}
// Shows or hides every learning box of a language in the box list (the level boxes and the user's own ones).
void setLanguageVisible(const QString &language, bool visible)
{
    CardStore *store = CardStore::instance();
    for (const QVariant &v : store->collections()) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("language")).toString() == language)
            store->setCollectionHidden(m.value(QStringLiteral("id")).toInt(), !visible);
    }
}
// false only when the switch was turned off on purpose (a language never touched keeps its boxes, e.g. own Swedish boxes)
bool languageOn(const QString &language)
{
    return QSettings().value(boxesKey(language), true).toBool();
}
// The selected learning box always stays in the list, even when hidden: if its language is switched off,
// select the most recently used box of a language that is on instead.
void leaveSwitchedOffLanguage()
{
    CardStore *store = CardStore::instance();
    const int current = store->currentCollection();
    const QVariantList all = store->collections(); // most recently used first
    bool move = false;
    for (const QVariant &v : all) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toInt() == current)
            move = !languageOn(m.value(QStringLiteral("language")).toString());
    }
    if (!move)
        return;
    for (const QVariant &v : all) {
        const QVariantMap m = v.toMap();
        const int id = m.value(QStringLiteral("id")).toInt();
        if (id != current && languageOn(m.value(QStringLiteral("language")).toString()) && !store->isCollectionHidden(id)) {
            store->selectCollection(id);
            return;
        }
    }
}
// First-start flag per group of packs (German A1/A2 keeps its old key), so a deleted box does not come back.
// German B1-C1 has its own flag, so existing users get those boxes once after an update.
// Groups of German packs: 0 = A1/A2 levels, 1 = B1-C1 levels, 2 = "complete" boxes (A1_complete, ...), 3 = "Alltag" (original everyday-conversation words).
int packGroup(const LevelPack &p)
{
    if (p.id.startsWith(QLatin1String("core-")))
        return 3;
    if (isComplete(p))
        return 2;
    return p.language == QLatin1String("de") && p.cefr != QLatin1String("A1") && p.cefr != QLatin1String("A2") ? 1 : 0;
}
QString installedKey(const QString &language, int group)
{
    if (language == QLatin1String("sv"))
        return QStringLiteral("levelpacks/installedSv");
    switch (group) {
    case 1: return QStringLiteral("levelpacks/installedDeAdv");
    case 2: return QStringLiteral("levelpacks/installedDeComplete2"); // "2": completes boxes that were cut off
    case 3: return QStringLiteral("levelpacks/installedDeCore"); // installs "Alltag" once for existing users
    default: return QStringLiteral("levelpacks/installed");
    }
}
} // namespace

bool LevelPacks::boxesOn(const QString &language) const
{
    // German and English boxes are on by default, Swedish ones off (switched on in Settings)
    return QSettings().value(boxesKey(language), language != QLatin1String("sv")).toBool();
}

void LevelPacks::setBoxesOn(const QString &language, bool on)
{
    if (on == boxesOn(language))
        return;
    QSettings().setValue(boxesKey(language), on);
    CardStore *store = CardStore::instance();
    const int original = store->currentCollection();
    // One change notification at the end instead of one per card and box (each one rebuilds the lists in the UI),
    // and the boxes are looked up once (collections() counts the cards of every box).
    QSignalBlocker block(store);
    QHash<QString, int> idOf;
    for (const QVariant &v : store->collections())
        idOf.insert(v.toMap().value(QStringLiteral("name")).toString(), v.toMap().value(QStringLiteral("id")).toInt());
    if (on) {
        for (const LevelPack &p : std::as_const(m_packs))
            if (p.language == language && !idOf.contains(p.title))
                createBox(p); // creates, selects and fills it
    }
    setLanguageVisible(language, on); // the level boxes and the user's own boxes of this language
    if (original >= 0)
        store->selectCollection(original);
    leaveSwitchedOffLanguage();
    block.unblock();
    emit store->changed(); // refreshes the levels too
}

int LevelPacks::installBoxes(bool force)
{
    QSettings settings;
    CardStore *store = CardStore::instance();
    const int original = store->currentCollection();
    int created = 0;
    QSignalBlocker block(store); // one change notification at the end (see setBoxesOn)
    QHash<QString, int> idOf;
    for (const QVariant &v : store->collections())
        idOf.insert(v.toMap().value(QStringLiteral("name")).toString(), v.toMap().value(QStringLiteral("id")).toInt());
    // Once, for installations from before the switches covered all boxes of a language: a language that is
    // switched off hides all its boxes, not only the level boxes.
    if (!settings.value(QStringLiteral("levelpacks/hideByLanguage"), false).toBool()) {
        for (const char *lang : {"de", "sv", "en"})
            if (settings.contains(boxesKey(QString::fromLatin1(lang))) && !boxesOn(QString::fromLatin1(lang))) // switched off on purpose
                setLanguageVisible(QString::fromLatin1(lang), false);
        settings.setValue(QStringLiteral("levelpacks/hideByLanguage"), true);
    }
    const struct { const char *language; int group; } groups[] = {{"de", 0}, {"de", 1}, {"de", 2}, {"de", 3}, {"sv", 0}};
    for (const auto &g : groups) {
        const QString language = QString::fromLatin1(g.language);
        const QString key = installedKey(language, g.group);
        if (!force && settings.value(key, false).toBool())
            continue;
        // Switched off in Settings: no boxes now; switching on creates them.
        if (boxesOn(language))
            for (const LevelPack &p : std::as_const(m_packs))
                if (p.language == language && packGroup(p) == g.group) {
                    const int existingBox = idOf.value(p.title, -1);
                    if (existingBox < 0) {
                        if (createBox(p) >= 0)
                            ++created;
                    } else {
                        fillBox(p, existingBox); // complete a box whose first fill was interrupted
                    }
                }
        settings.setValue(key, true);
    }
    if (original >= 0)
        store->selectCollection(original);
    leaveSwitchedOffLanguage();
    block.unblock();
    emit store->changed();
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
