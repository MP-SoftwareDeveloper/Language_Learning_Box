#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QAccelerometer;

namespace tilt {

// Clockwise turn (0/90/180/270) that makes a portrait-framed photo upright, from the gravity
// vector in device axes (Android: x to the right edge, y to the top edge, m/s^2).
//   upright portrait  y ~ +g  -> 0      top edge left  x ~ +g -> 270
//   upside down       y ~ -g  -> 180    top edge right x ~ -g -> 90
// Held flat (reading a page on the table) x and y are small: keep `previous`. Near 45 degrees,
// keep `previous` too, so the answer does not flicker.
constexpr int rotationFromGravity(double x, double y, int previous)
{
    constexpr double kMinTilt = 3.0;   // m/s^2, roughly 18 degrees from flat
    constexpr double kMargin = 1.25;   // dominant axis must be clearly larger
    const double ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ax < kMinTilt && ay < kMinTilt)
        return previous;
    if (ay >= ax * kMargin)
        return y > 0 ? 0 : 180;
    if (ax >= ay * kMargin)
        return x > 0 ? 270 : 90;
    return previous;
}

// Turn still needed when the app's UI is itself rotated (auto-rotate on): the camera frames
// the photo like the UI, so only the difference between phone and UI remains.
constexpr int relativeRotation(int device, int ui) { return ((device - ui) % 360 + 360) % 360; }

} // namespace tilt

// QML: how the phone is held right now, as the turn a photo taken now needs (like Google Lens,
// independent of the rotation lock). Uses the accelerometer (Qt Sensors, no Java); without
// sensors `available` is false and `rotation` stays 0.
class DeviceTilt : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(int rotation READ rotation NOTIFY rotationChanged)

public:
    explicit DeviceTilt(QObject *parent = nullptr);

    bool active() const { return m_active; }
    void setActive(bool on);
    bool available() const;
    int rotation() const;

signals:
    void activeChanged();
    void rotationChanged();

private:
    void onReading();

    QAccelerometer *m_sensor = nullptr;
    bool m_active = false;
    int m_device = 0; // turn from the phone's tilt alone
};
