#include <QtTest>

#include "devicetilt.h"

class TstDeviceTilt : public QObject
{
    Q_OBJECT
private slots:
    void heldUpright() { QCOMPARE(tilt::rotationFromGravity(0.3, 9.6, 90), 0); }
    void topEdgeLeft() { QCOMPARE(tilt::rotationFromGravity(9.7, 0.5, 0), 270); }
    void topEdgeRight() { QCOMPARE(tilt::rotationFromGravity(-9.7, 0.8, 0), 90); }
    void upsideDown() { QCOMPARE(tilt::rotationFromGravity(0.2, -9.5, 0), 180); }
    void tiltedTowardsTable()
    {
        // Phone leaning over a book (about 40 degrees), still clearly in landscape.
        QCOMPARE(tilt::rotationFromGravity(6.2, 0.9, 0), 270);
    }
    void flatKeepsPrevious()
    {
        QCOMPARE(tilt::rotationFromGravity(1.0, 1.5, 270), 270);
        QCOMPARE(tilt::rotationFromGravity(0.0, 0.0, 90), 90);
    }
    void diagonalKeepsPrevious() { QCOMPARE(tilt::rotationFromGravity(6.0, 6.5, 0), 0); }
    void relativeToUi()
    {
        QCOMPARE(tilt::relativeRotation(270, 0), 270);   // rotation lock: UI stays portrait
        QCOMPARE(tilt::relativeRotation(270, 270), 0);   // UI turned with the phone
        QCOMPARE(tilt::relativeRotation(90, 270), 180);
        QCOMPARE(tilt::relativeRotation(0, 90), 270);
    }
};

QTEST_APPLESS_MAIN(TstDeviceTilt)
#include "tst_devicetilt.moc"
