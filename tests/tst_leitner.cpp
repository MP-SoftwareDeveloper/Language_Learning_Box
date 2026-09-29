#include <QtTest>

#include "leitner.h"

class TstLeitner : public QObject
{
    Q_OBJECT
private slots:
    void intervals()
    {
        QCOMPARE(leitner::intervalDays(1), 1);
        QCOMPARE(leitner::intervalDays(2), 2);
        QCOMPARE(leitner::intervalDays(3), 4);
        QCOMPARE(leitner::intervalDays(4), 8);
        QCOMPARE(leitner::intervalDays(5), 16);
        QCOMPARE(leitner::intervalDays(leitner::kLearnedBox), -1);
    }
    void correctMovesUp()
    {
        for (int b = 1; b < 5; ++b) {
            const auto s = leitner::next(b, true);
            QCOMPARE(s.box, b + 1);
            QCOMPARE(s.intervalDays, leitner::intervalDays(b + 1));
        }
    }
    void lastBoxRetires()
    {
        const auto s = leitner::next(5, true);
        QCOMPARE(s.box, leitner::kLearnedBox);
        QCOMPARE(s.intervalDays, -1);
    }
    void manualMoveUsesBoxInterval()
    {
        for (int b = 1; b <= 5; ++b) {
            const auto s = leitner::moveTo(b);
            QCOMPARE(s.box, b);
            QCOMPARE(s.intervalDays, leitner::intervalDays(b));
        }
        QCOMPARE(leitner::moveTo(leitner::kLearnedBox).intervalDays, -1);
        QCOMPARE(leitner::moveTo(0).box, 1);   // clamped
        QCOMPARE(leitner::moveTo(9).box, leitner::kLearnedBox);
    }
    void wrongResets()
    {
        for (int b = 1; b <= 5; ++b) {
            const auto s = leitner::next(b, false);
            QCOMPARE(s.box, 1);
            QCOMPARE(s.intervalDays, 1);
        }
    }
    void outOfRangeIsSane()
    {
        QCOMPARE(leitner::next(0, true).box, 2);
        QCOMPARE(leitner::next(99, true).box, leitner::kLearnedBox);
    }
};

QTEST_APPLESS_MAIN(TstLeitner)
#include "tst_leitner.moc"
