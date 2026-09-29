#include <QtTest>
#include "ocr/ocr.h"
static OcrWord W(const char *t, int x, int y, int line, int block, float c = 90) { return OcrWord{QString::fromUtf8(t), QRect(x, y, 80, 30), line, block, c}; }
class TstOcrMerge : public QObject { Q_OBJECT
private slots:
  void addsMissingSubtitleBetweenLines() {
    OcrResult base; base.imageSize = QSize(1000, 1000);
    base.words = { W("Browser", 10, 10, 0, 0), W("Leiste", 100, 10, 0, 0), W("Kinder", 10, 800, 1, 1), W("Cartoons", 100, 800, 1, 1) };
    OcrResult sparse;
    sparse.words = { W("Browser", 12, 11, 0, 0),                     // overlaps base -> dropped
                     W("gehen", 200, 500, 1, 1), W("wir", 290, 500, 1, 1), W("nach", 380, 500, 1, 1), W("Hause.", 470, 500, 1, 1),
                     W("Okay,", 200, 540, 2, 2), W("that's", 290, 540, 2, 2),
                     W("Il", 900, 100, 3, 3, 30) };                 // noise line -> dropped
    const OcrResult m = ocr::mergeResults(base, sparse);
    QStringList t; for (auto &w : m.words) t << w.text;
    QCOMPARE(t.join(' '), QString("Browser Leiste gehen wir nach Hause. Okay, that's Kinder Cartoons"));
    QCOMPARE(m.words.at(2).line, 1); QCOMPARE(m.words.at(6).line, 2); QCOMPARE(m.words.last().line, 3);
    QCOMPARE(m.words.at(2).block, m.words.at(6).block);            // stacked subtitle lines share a block
    QVERIFY(m.words.at(2).block != m.words.first().block);
    QCOMPARE(m.words.last().block, 2);
  }
  void joinsSparseFragmentsIntoRows() {
    OcrResult r;
    r.words = { W("heute.", 500, 502, 0, 0), W("Ja,", 590, 500, 0, 0),
                W("So,", 100, 500, 1, 1), W("das", 190, 501, 1, 1), W("ist", 280, 499, 1, 1),
                W("genug", 400, 503, 2, 2),
                W("Okay,", 150, 545, 3, 3), W("that's", 240, 545, 3, 3),
                W("Kinder", 10, 900, 4, 4) };
    const OcrResult j = ocr::joinLineFragments(r);
    QStringList t; for (auto &w : j.words) t << w.text;
    QCOMPARE(t.join(' '), QString("So, das ist genug heute. Ja, Okay, that's Kinder"));
    QCOMPARE(j.words.at(5).line, 0); QCOMPARE(j.words.at(6).line, 1); QCOMPARE(j.words.last().line, 2);
    QCOMPARE(j.words.at(0).block, j.words.at(6).block);  // subtitle + its second line
    QVERIFY(j.words.last().block != j.words.at(0).block);
  }
  void nothingToAdd() {
    OcrResult base; base.words = { W("Hallo", 10, 10, 0, 0) };
    OcrResult same; same.words = { W("Hallo", 11, 10, 0, 0) };
    QCOMPARE(ocr::mergeResults(base, same).words.size(), 1);
  }
};
QTEST_APPLESS_MAIN(TstOcrMerge)
#include "tst_ocrmerge.moc"
