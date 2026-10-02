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
};

QTEST_GUILESS_MAIN(TstLevelPack)
#include "tst_levelpack.moc"
