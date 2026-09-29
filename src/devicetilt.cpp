#include "devicetilt.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef LB_HAVE_SENSORS
#include <QAccelerometer>
#endif

namespace {
// Clockwise angle of the UI relative to portrait (0 when the app stays portrait, e.g. rotation lock).
int uiRotation()
{
    const QScreen *s = QGuiApplication::primaryScreen();
    if (!s)
        return 0;
    switch (s->orientation()) {
    case Qt::LandscapeOrientation: return 270;         // top edge left (Android ROTATION_90)
    case Qt::InvertedLandscapeOrientation: return 90;  // top edge right
    case Qt::InvertedPortraitOrientation: return 180;
    default: return 0;
    }
}
} // namespace

DeviceTilt::DeviceTilt(QObject *parent)
    : QObject(parent)
{
#ifdef LB_HAVE_SENSORS
    m_sensor = new QAccelerometer(this);
    m_sensor->setAccelerationMode(QAccelerometer::Combined); // includes gravity
    m_sensor->setAxesOrientationMode(QSensor::FixedOrientation); // raw device axes
    m_sensor->setDataRate(10);
    connect(m_sensor, &QSensor::readingChanged, this, &DeviceTilt::onReading);
#endif
}

bool DeviceTilt::available() const
{
#ifdef LB_HAVE_SENSORS
    return m_sensor && m_sensor->connectToBackend();
#else
    return false;
#endif
}

void DeviceTilt::setActive(bool on)
{
    if (on == m_active)
        return;
    m_active = on;
#ifdef LB_HAVE_SENSORS
    if (m_sensor) {
        if (on)
            m_sensor->start();
        else
            m_sensor->stop();
    }
#endif
    emit activeChanged();
}

int DeviceTilt::rotation() const
{
    return tilt::relativeRotation(m_device, uiRotation());
}

void DeviceTilt::onReading()
{
#ifdef LB_HAVE_SENSORS
    const QAccelerometerReading *r = m_sensor->reading();
    if (!r)
        return;
    const int next = tilt::rotationFromGravity(r->x(), r->y(), m_device);
    if (next != m_device) {
        m_device = next;
        emit rotationChanged();
    }
#endif
}
