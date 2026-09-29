#include <QtTest>

#include "ocr/textselect.h"

namespace {
QList<OcrWord> words(const QList<std::tuple<QString, int, int>> &spec)
{
    QList<OcrWord> out;
    for (const auto &[t, line, block] : spec) {
        OcrWord w;
        w.text = t;
        w.line = line;
        w.block = block;
        out.append(w);
    }
    return out;
}
} // namespace

class TstTextSelect : public QObject
{
    Q_OBJECT
private slots:
    void cleanWord_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::addColumn<QString>("clean");
        QTest::newRow("comma") << "Apfel," << "Apfel";
        QTest::newRow("german quotes") << "„Hallo“" << "Hallo";
        QTest::newRow("parens hyphen") << "(Kinder-)" << "Kinder";
        QTest::newRow("apostrophe kept") << "geht's?" << "geht's";
        QTest::newRow("umlaut sz") << "Straße." << "Straße";
        QTest::newRow("only punct") << "—" << "";
    }
    void cleanWord()
    {
        QFETCH(QString, raw);
        QFETCH(QString, clean);
        QCOMPARE(textselect::cleanWord(raw), clean);
    }

    void joinInReadingOrderAndHyphenation()
    {
        // line 0: "Ich suche eine Woh-"   line 1: "nung in Berlin."
        const auto w = words({{"Ich", 0, 0}, {"suche", 0, 0}, {"eine", 0, 0}, {"Woh-", 0, 0},
                              {"nung", 1, 0}, {"in", 1, 0}, {"Berlin.", 1, 0}});
        QCOMPARE(textselect::joinWords(w, {6, 0, 1, 2, 3, 4, 5}), QStringLiteral("Ich suche eine Wohnung in Berlin."));
        QCOMPARE(textselect::joinWords(w, {3, 4}), QStringLiteral("Wohnung"));
        QCOMPARE(textselect::joinWords(w, {3}), QStringLiteral("Woh-"));            // no partner selected
        QCOMPARE(textselect::joinWords(w, {1, 1, 2}), QStringLiteral("suche eine")); // duplicates ignored
    }

    void trailingCommaDropped()
    {
        const auto w = words({{"Brot,", 0, 0}, {"Käse", 0, 0}, {"und", 0, 0}, {"Obst.", 0, 0}});
        QCOMPARE(textselect::joinWords(w, {0}), QStringLiteral("Brot"));
        QCOMPARE(textselect::joinWords(w, {0, 1}), QStringLiteral("Brot, Käse"));
        QCOMPARE(textselect::joinWords(w, {2, 3}), QStringLiteral("und Obst.")); // period kept
    }

    void inlineHyphenIsKept()
    {
        const auto w = words({{"E-Mail", 0, 0}, {"schreiben", 0, 0}});
        QCOMPARE(textselect::joinWords(w, {0, 1}), QStringLiteral("E-Mail schreiben"));
    }

    void sentenceRange()
    {
        // block 0: "Das ist gut. Wir gehen heute ins Kino!"  block 1: "Neu hier"
        const auto w = words({{"Das", 0, 0}, {"ist", 0, 0}, {"gut.", 0, 0}, {"Wir", 0, 0}, {"gehen", 1, 0},
                              {"heute", 1, 0}, {"ins", 1, 0}, {"Kino!", 1, 0}, {"Neu", 2, 1}, {"hier", 2, 1}});
        QCOMPARE(textselect::sentenceRange(w, 1), (QList<int>{0, 1, 2}));
        QCOMPARE(textselect::sentenceRange(w, 5), (QList<int>{3, 4, 5, 6, 7})); // spans a line break
        QCOMPARE(textselect::sentenceRange(w, 9), (QList<int>{8, 9}));          // stops at block end
        QVERIFY(textselect::sentenceRange(w, 42).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstTextSelect)
#include "tst_textselect.moc"
