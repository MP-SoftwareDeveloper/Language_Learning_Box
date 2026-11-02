#include <QtTest>
#include <QTextBlock>
#include <QTextDocument>
#include "paragraphalign.h"

// Meaning field of a card: Persian lines right, English / German lines left.
class TstParagraphAlign : public QObject
{
    Q_OBJECT
private slots:
    void direction()
    {
        using paragraphalign::startsRightToLeft;
        QVERIFY(startsRightToLeft(QStringLiteral("سوسیس؛ کالباس")));
        QVERIFY(startsRightToLeft(QStringLiteral("  1. سوسیس")));   // digits and punctuation are skipped
        QVERIFY(!startsRightToLeft(QStringLiteral("Pl. Würste")));
        QVERIFY(!startsRightToLeft(QStringLiteral("sausage س")));   // the first letter decides
        QVERIFY(!startsRightToLeft(QStringLiteral("123")));
        QVERIFY(!startsRightToLeft(QString()));
    }

    void eachParagraphOnItsOwnSide()
    {
        QTextDocument doc;
        doc.setPlainText(QStringLiteral("سوسیس؛ کالباس\nPl. Würste\nsausage"));
        QVERIFY(paragraphalign::alignParagraphs(&doc));
        QVector<Qt::Alignment> a;
        for (QTextBlock b = doc.begin(); b.isValid(); b = b.next())
            a << b.blockFormat().alignment();
        QCOMPARE(a.size(), 3);
        QCOMPARE(a[0], Qt::AlignRight | Qt::AlignAbsolute);
        QCOMPARE(a[1], Qt::AlignLeft | Qt::AlignAbsolute);
        QCOMPARE(a[2], Qt::AlignLeft | Qt::AlignAbsolute);
        QVERIFY(!paragraphalign::alignParagraphs(&doc)); // nothing left to change: no endless change signals
    }

    void followsTheText()
    {
        QTextDocument doc;
        doc.setPlainText(QStringLiteral("hello"));
        paragraphalign::alignParagraphs(&doc);
        QCOMPARE(doc.begin().blockFormat().alignment(), Qt::AlignLeft | Qt::AlignAbsolute);
        doc.setPlainText(QStringLiteral("سلام"));
        paragraphalign::alignParagraphs(&doc);
        QCOMPARE(doc.begin().blockFormat().alignment(), Qt::AlignRight | Qt::AlignAbsolute);
    }
};

QTEST_MAIN(TstParagraphAlign)
#include "tst_paragraphalign.moc"
