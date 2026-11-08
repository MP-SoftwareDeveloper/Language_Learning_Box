#include <QtTest>
#include <QFile>
#include <QSet>

#include "levelpack.h"

class TstLevelPack : public QObject
{
    Q_OBJECT
private slots:
    void parsesSmallInput()
    {
        const QString text = QStringLiteral(
            "# comment\n"
            "@level\ta1-1\tA1 · Level 1\tSub\n"
            "@chapter\ta1-1\t1\tHallo\n"
            "a1-1\t1\tdas Haus\tdie Häuser\thouse\tخانه\tDas Haus ist groß.\tThe house is big.\tخانه بزرگ است.\n"
            "a1-1\t1\tgehen\t-\tto go\tرفتن\tIch gehe.\tI go.\tمن می‌روم.\n");
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(text, &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 1);
        QCOMPARE(packs[0].cefr, QStringLiteral("A1"));
        QCOMPARE(packs[0].totalWords(), 2);
        const LevelChapter *c = packs[0].chapter(1);
        QVERIFY(c);
        QCOMPARE(c->words[0].plural, QStringLiteral("die Häuser"));
        QVERIFY(c->words[1].plural.isEmpty());
        QCOMPARE(levelWordBack(c->words[0], QStringLiteral("en")), QStringLiteral("house\nPl. die Häuser"));
        QCOMPARE(levelWordBack(c->words[1], QStringLiteral("fa")), QStringLiteral("رفتن"));
    }
    void findsGermanWords()
    {
        const QString text = QStringLiteral(
            "@level\ta1-1\tA1 · Level 1\tSub\n"
            "@chapter\ta1-1\t1\tHallo\n"
            "a1-1\t1\tder Satz\tSätze\tsentence\tجمله\tIch lese den Satz.\tI read the sentence.\tمن جمله را می‌خوانم.\n"
            "a1-1\t1\tgehen\t-\tto go\tرفتن\tIch gehe.\tI go.\tمن می‌روم.\n"
            "a1-1\t1\tder Morgen\tMorgen\tmorning\tصبح\tGuten Morgen.\tGood morning.\tصبح بخیر.\n"
            "a1-1\t1\tmorgen\t-\ttomorrow\tفردا\tBis morgen.\tSee you tomorrow.\tفردا می‌بینمت.\n"
            "@level\tsv-a2-1\tSvenska A2 · Nivå 1\tSub\n"
            "@chapter\tsv-a2-1\t1\tHej\n"
            "sv-a2-1\t1\ten bil\tbilar\tcar\tماشین\tEn bil.\tA car.\tیک ماشین.\n");
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(text, &packs, &err), qPrintable(err));
        const LevelWord *w = findGermanWord(packs, QStringLiteral("Satz"));
        QVERIFY(w);
        QCOMPARE(w->german, QStringLiteral("der Satz"));
        QCOMPARE(levelWordBack(*w, QStringLiteral("en")), QStringLiteral("sentence\nPl. Sätze"));
        QVERIFY(findGermanWord(packs, QStringLiteral("der Satz")));
        QVERIFY(findGermanWord(packs, QStringLiteral("SATZ")));
        QVERIFY(!findGermanWord(packs, QStringLiteral("die Satz"))); // wrong article: not that word
        QVERIFY(findGermanWord(packs, QStringLiteral("gehen")));
        QCOMPARE(findGermanWord(packs, QStringLiteral("morgen"))->english, QStringLiteral("tomorrow")); // as typed
        QCOMPARE(findGermanWord(packs, QStringLiteral("Morgen"))->english, QStringLiteral("morning"));
        QVERIFY(!findGermanWord(packs, QStringLiteral("bil"))); // only German packs count
        QVERIFY(!findGermanWord(packs, QString()));
    }
    void swedishIdsGetSwedishLanguage()
    {
        const QString text = QStringLiteral(
            "@level\tsv-a2-1\tSvenska A2 · Nivå 1\tSub\n"
            "@level\ta2-1\tA2 · Level 1\tSub\n");
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(text, &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 2);
        QCOMPARE(packs[0].language, QStringLiteral("sv"));
        QCOMPARE(packs[0].cefr, QStringLiteral("A2"));
        QCOMPARE(packs[1].language, QStringLiteral("de"));
        QCOMPARE(packs[1].cefr, QStringLiteral("A2"));
    }
    void englishIdsGetEnglishLanguage()
    {
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QStringLiteral("@level\ten-a1-2\tEnglish A1 · Level 2\tSub\n"), &packs, &err), qPrintable(err));
        QCOMPARE(packs.first().language, QStringLiteral("en"));
        QCOMPARE(packs.first().cefr, QStringLiteral("A1"));
    }
    void shippedEnglishDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_EN_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 3);
        for (const LevelPack &p : packs) {
            QCOMPARE(p.language, QStringLiteral("en"));
            QCOMPARE(p.cefr, QStringLiteral("A1"));
            QCOMPARE(p.chapters.size(), 4);
            QSet<QString> seen;
            for (const LevelChapter &c : p.chapters) {
                QVERIFY2(c.words.size() >= 10, qPrintable(p.id));
                for (const LevelWord &w : c.words) {
                    QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                    QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                    QVERIFY2(!seen.contains(w.german.toCaseFolded()), qPrintable(w.german));
                    seen.insert(w.german.toCaseFolded());
                }
            }
        }
    }
    void rejectsUnknownLevel()
    {
        QList<LevelPack> packs;
        QString err;
        QVERIFY(!parseLevelPacks(QStringLiteral("zz\t1\ta\t-\tb\tc\td\te\tf\n"), &packs, &err));
        QVERIFY(!err.isEmpty());
    }
    void shippedDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 6);
        int a1 = 0, a2 = 0;
        QSet<QString> seen;
        for (const LevelPack &p : packs) {
            QCOMPARE(p.chapters.size(), 4);
            (p.cefr == QLatin1String("A1") ? a1 : a2)++;
            for (const LevelChapter &c : p.chapters) {
                QVERIFY2(!c.words.isEmpty(), qPrintable(p.id));
                for (const LevelWord &w : c.words) {
                    QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                    QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                    QVERIFY2(!seen.contains(w.german), qPrintable(w.german));
                    seen.insert(w.german);
                }
            }
        }
        QCOMPARE(a1, 3);
        QCOMPARE(a2, 3);
    }
    void shippedSwedishDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_SV_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 6);
        int a1 = 0, a2 = 0;
        QSet<QString> seen;
        for (const LevelPack &p : packs) {
            QCOMPARE(p.language, QStringLiteral("sv"));
            QCOMPARE(p.chapters.size(), 4);
            (p.cefr == QLatin1String("A1") ? a1 : a2)++;
            for (const LevelChapter &c : p.chapters) {
                QVERIFY2(!c.words.isEmpty(), qPrintable(p.id));
                for (const LevelWord &w : c.words) {
                    QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                    QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                    QVERIFY2(!seen.contains(w.german), qPrintable(w.german));
                    seen.insert(w.german);
                }
            }
        }
        QCOMPARE(a1, 3);
        QCOMPARE(a2, 3);
    }
    void shippedAdvancedGermanDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_ADV_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 6);
        QHash<QString, int> perLevel;
        QSet<QString> seen;
        // the words must not repeat the A1/A2 packs either
        QFile base(QStringLiteral(LB_LEVELPACK_FILE));
        QVERIFY(base.open(QIODevice::ReadOnly));
        QList<LevelPack> basic;
        QVERIFY(parseLevelPacks(QString::fromUtf8(base.readAll()), &basic, &err));
        for (const LevelPack &p : basic)
            for (const LevelChapter &c : p.chapters)
                for (const LevelWord &w : c.words)
                    seen.insert(w.german);
        for (const LevelPack &p : packs) {
            QCOMPARE(p.language, QStringLiteral("de"));
            QCOMPARE(p.chapters.size(), 4);
            perLevel[p.cefr]++;
            for (const LevelChapter &c : p.chapters) {
                QVERIFY2(c.words.size() >= 8, qPrintable(p.id));
                for (const LevelWord &w : c.words) {
                    QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                    QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                    QVERIFY2(!seen.contains(w.german), qPrintable(w.german));
                    seen.insert(w.german);
                }
            }
        }
        QCOMPARE(perLevel.value(QStringLiteral("B1")), 2);
        QCOMPARE(perLevel.value(QStringLiteral("B2")), 2);
        QCOMPARE(perLevel.value(QStringLiteral("C1")), 2);
    }
    void shippedA1CompleteDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_A1C_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 1);
        const LevelPack &p = packs.first();
        QCOMPARE(p.id, QStringLiteral("a1-complete"));
        QCOMPARE(p.title, QStringLiteral("A1_complete"));
        QCOMPARE(p.cefr, QStringLiteral("A1"));
        QCOMPARE(p.language, QStringLiteral("de"));
        QCOMPARE(p.chapters.size(), 12);
        QVERIFY(p.totalWords() > 600);
        QSet<QString> seen;
        for (const LevelChapter &c : p.chapters) {
            QVERIFY(!c.words.isEmpty());
            for (const LevelWord &w : c.words) {
                QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                // cards are unique per box by case-folded front ("sie" and "Sie" would collide)
                QVERIFY2(!seen.contains(w.german.toCaseFolded()), qPrintable(w.german));
                seen.insert(w.german.toCaseFolded());
            }
        }
    }
    void shippedCoreDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_CORE_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 1);
        const LevelPack &p = packs.first();
        QCOMPARE(p.id, QStringLiteral("core-1"));
        QCOMPARE(p.title, QStringLiteral("Alltag"));
        QCOMPARE(p.language, QStringLiteral("de"));
        QCOMPARE(p.chapters.size(), 12);
        QVERIFY(p.totalWords() >= 400);
        QSet<QString> seen;
        for (const LevelChapter &c : p.chapters) {
            QVERIFY2(c.words.size() >= 25, qPrintable(c.title));
            for (const LevelWord &w : c.words) {
                QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                QVERIFY(!w.exampleDe.isEmpty() && !w.exampleEn.isEmpty() && !w.exampleFa.isEmpty());
                // cards are unique per box by case-folded front
                QVERIFY2(!seen.contains(w.german.toCaseFolded()), qPrintable(w.german));
                seen.insert(w.german.toCaseFolded());
            }
        }
    }
    void shippedJobDataIsComplete()
    {
        QFile f(QStringLiteral(LB_LEVELPACK_JOB_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QList<LevelPack> packs;
        QString err;
        QVERIFY2(parseLevelPacks(QString::fromUtf8(f.readAll()), &packs, &err), qPrintable(err));
        QCOMPARE(packs.size(), 1);
        const LevelPack &p = packs.first();
        QCOMPARE(p.id, QStringLiteral("job-1"));
        QCOMPARE(p.title, QStringLiteral("Job-Wortschatz"));
        QCOMPARE(p.language, QStringLiteral("de"));
        QCOMPARE(p.chapters.size(), 11);
        QVERIFY(p.totalWords() >= 500);
        QSet<QString> seen;
        for (const LevelChapter &c : p.chapters) {
            QVERIFY2(!c.words.isEmpty(), qPrintable(c.title));
            for (const LevelWord &w : c.words) {
                QVERIFY(!w.german.isEmpty() && !w.english.isEmpty() && !w.persian.isEmpty());
                QVERIFY(!w.exampleDe.isEmpty());
                // cards are unique per box by case-folded front
                QVERIFY2(!seen.contains(w.german.toCaseFolded()), qPrintable(w.german));
                seen.insert(w.german.toCaseFolded());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TstLevelPack)
#include "tst_levelpack.moc"
