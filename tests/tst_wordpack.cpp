#include <QtTest>
#include <QSet>

#include "wordpack.h"

class TstWordPack : public QObject
{
    Q_OBJECT
private slots:
    void parsesMinimalPack()
    {
        const QString text = QStringLiteral(
            "# comment\n"
            "@pack\tp\tMy Pack\n"
            "@chapter\t2\tZwei\n"
            "@chapter\t1\tEins\n"
            "1\tder Apfel\tسیب · Pl. die Äpfel\tIch esse einen Apfel.\r\n"
            "2\tgehen\traftaن\tIch gehe.\tمن می‌روم.\n"
            "\n");
        WordPack p;
        QString err;
        QVERIFY2(parseWordPack(text, &p, &err), qPrintable(err));
        QCOMPARE(p.id, QStringLiteral("p"));
        QCOMPARE(p.chapters.size(), 2);
        QCOMPARE(p.chapters[0].number, 1); // sorted
        QCOMPARE(p.chapters[0].title, QStringLiteral("Eins"));
        QCOMPARE(p.chapters[0].cards.size(), 1);
        QCOMPARE(p.chapters[0].cards[0].front, QStringLiteral("der Apfel"));
        QCOMPARE(p.chapters[0].cards[0].example, QStringLiteral("Ich esse einen Apfel.")); // \r stripped
        QVERIFY(p.chapters[0].cards[0].exampleTranslation.isEmpty());  // 4 fields: no translation
        QCOMPARE(p.chapters[1].cards[0].exampleTranslation,             // optional 5th field
                 QStringLiteral("\u0645\u0646 \u0645\u06CC\u200C\u0631\u0648\u0645."));
        // Grammar note goes to its own line (own bidi paragraph).
        QCOMPARE(p.chapters[0].cards[0].back, QStringLiteral("\u0633\u06CC\u0628\nPl. die \u00C4pfel"));
    }

    void rejectsMalformedLines()
    {
        WordPack p;
        QString err;
        QVERIFY(!parseWordPack(u"@pack\tp\tP\n1\tonly three\tfields\n", &p, &err));
        QVERIFY(err.startsWith(QStringLiteral("line 2")));
        QVERIFY(!parseWordPack(u"@pack\tp\tP\nx\ta\tb\tc\n", &p, &err));
        QVERIFY(!parseWordPack(u"1\ta\tb\tc\n", &p, &err)); // no @pack
    }

    // The shipped Netzwerk neu A1 pack must parse and be internally consistent.
    void shippedPackIsValid()
    {
        QFile f(QStringLiteral(LB_WORDPACK_FILE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        WordPack p;
        QString err;
        QVERIFY2(parseWordPack(QString::fromUtf8(f.readAll()), &p, &err), qPrintable(err));
        QCOMPARE(p.chapters.size(), 12);

        QSet<QString> fronts;
        int total = 0;
        for (const auto &ch : p.chapters) {
            QVERIFY2(!ch.title.isEmpty(), qPrintable(QString::number(ch.number)));
            QVERIFY2(ch.cards.size() >= 30, qPrintable(ch.title));
            for (const Card &c : ch.cards) {
                ++total;
                const QString key = c.front.toCaseFolded();
                QVERIFY2(!fronts.contains(key), qPrintable(QStringLiteral("duplicate: ") + c.front));
                fronts.insert(key);
                QVERIFY2(!c.back.isEmpty(), qPrintable(c.front));
                QVERIFY2(!c.example.isEmpty(), qPrintable(c.front));
                // Every shipped example has a Persian translation, written in Persian script.
                QVERIFY2(!c.exampleTranslation.isEmpty(), qPrintable(QStringLiteral("no Persian example: ") + c.front));
                bool persian = false;
                for (QChar ch : c.exampleTranslation)
                    persian = persian || (ch.unicode() >= 0x0600 && ch.unicode() <= 0x06FF);
                QVERIFY2(persian, qPrintable(QStringLiteral("example translation not Persian: ") + c.front));
                // Nouns carry an article; a plural note must follow them.
                const bool noun = c.front.startsWith(QStringLiteral("der ")) || c.front.startsWith(QStringLiteral("die "))
                                  || c.front.startsWith(QStringLiteral("das "));
                if (noun && !c.front.contains(u'?') && !c.front.contains(u'!'))
                    QVERIFY2(c.back.contains(QStringLiteral("\nPl. ")) || c.back.contains(QStringLiteral("nur Plural")),
                             qPrintable(QStringLiteral("no plural note: ") + c.front));
            }
        }
        QVERIFY(total > 500);
    }
};

QTEST_APPLESS_MAIN(TstWordPack)
#include "tst_wordpack.moc"
