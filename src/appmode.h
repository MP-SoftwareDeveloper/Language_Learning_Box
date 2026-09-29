#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Simple (plain Leitner flashcards, offline) or Full (Lens, online help, word packs, pictures ...).
// Both modes share the same data; switching only shows or hides features. Chosen in the first-run
// setup and in Settings; stored in QSettings (app/mode, setup/done).
class AppMode : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)          // "simple" | "full"
    Q_PROPERTY(bool full READ full NOTIFY changed)
    Q_PROPERTY(bool setupDone READ setupDone WRITE setSetupDone NOTIFY changed)

public:
    static AppMode *instance();
    static AppMode *create(QQmlEngine *, QJSEngine *);

    QString mode() const { return m_full ? QStringLiteral("full") : QStringLiteral("simple"); }
    void setMode(const QString &mode);
    bool full() const { return m_full; }
    bool setupDone() const { return m_setupDone; }
    void setSetupDone(bool done);

signals:
    void changed();

private:
    explicit AppMode(QObject *parent = nullptr);
    bool m_full = true;
    bool m_setupDone = false;
};
