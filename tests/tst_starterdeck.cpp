#include "starterdeck.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QtTest>

class TestStarterDeck : public QObject
{
    Q_OBJECT
private slots:
    void parsesLines()
    {
        QList<StarterCard> cards;
        QString err;
        QVERIFY2(parseStarterDeck(QStringLiteral("# comment\n\nHome\tder Tisch\tdie Tische\ttable\tمیز\tDer Tisch ist neu.\tThe table is new.\tمیز نو است.\r\n"
                                                "Verbs\tgehen\t-\tto go\tرفتن\tIch gehe.\tI go.\tمی‌روم.\n"),
                                 &cards, &err), qPrintable(err));
        QCOMPARE(cards.size(), 2);
        QCOMPARE(cards[0].german, QStringLiteral("der Tisch"));
        QCOMPARE(cards[0].exampleFa, QStringLiteral("میز نو است."));
        QVERIFY(cards[1].plural.isEmpty());
        QCOMPARE(starterBack(cards[0], QStringLiteral("en")), QStringLiteral("table\nPl. die Tische"));
        QCOMPARE(starterBack(cards[0], QStringLiteral("fa")), QStringLiteral("میز\nPl. die Tische"));
        QCOMPARE(starterBack(cards[1], QStringLiteral("en")), QStringLiteral("to go"));
    }

    void rejectsBrokenLine()
    {
        QList<StarterCard> cards;
        QString err;
        QVERIFY(!parseStarterDeck(QStringLiteral("Home\tder Tisch\ttable\n"), &cards, &err));
        QVERIFY(err.contains(QLatin1String("line 1")));
    }

    // The file shipped with the app: exactly 100 complete cards, no duplicates, Persian in Persian script.
    void shippedDeckIsComplete()
    {
        QFile f(QStringLiteral(LB_STARTER_FILE));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        QList<StarterCard> cards;
        QString err;
        QVERIFY2(parseStarterDeck(QString::fromUtf8(f.readAll()), &cards, &err), qPrintable(err));
        QCOMPARE(cards.size(), 100);
        const QRegularExpression persian(QStringLiteral("[\\x{0600}-\\x{06FF}]"));
        const QRegularExpression noun(QStringLiteral("^(der|die|das) "));
        QSet<QString> seen;
        for (const StarterCard &c : std::as_const(cards)) {
            QVERIFY2(!seen.contains(c.german.toCaseFolded()), qPrintable(c.german));
            seen.insert(c.german.toCaseFolded());
            QVERIFY2(!c.exampleDe.isEmpty() && !c.exampleEn.isEmpty() && !c.exampleFa.isEmpty(), qPrintable(c.german));
            QVERIFY2(c.persian.contains(persian) && c.exampleFa.contains(persian), qPrintable(c.german));
            QVERIFY2(!c.english.contains(persian), qPrintable(c.german));
            if (!c.plural.isEmpty())
                QVERIFY2(c.german.contains(noun), qPrintable(c.german));
        }
    }
};

QTEST_APPLESS_MAIN(TestStarterDeck)
#include "tst_starterdeck.moc"
