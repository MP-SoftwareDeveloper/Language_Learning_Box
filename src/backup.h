#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Automatic safety copy of all cards, so nothing is lost when the app is uninstalled and installed again
// (an uninstall deletes the app's own storage, a folder in Documents survives).
//
//   Documents/LearningBox/learningbox.sqlite   all learning boxes, cards and progress
//   Documents/LearningBox/images/              card pictures
//
// - The copy is refreshed a little after every change and whenever the app goes to the background.
// - A fresh install with no cards of its own restores the copy before the database is opened.
// - Android 11+ needs "All files access" for this (asked once, with an explanation); older Android uses the
//   normal storage permission. Without the permission nothing is copied and nothing breaks.
class Backup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // True when the backup folder can be written (permission given).
    Q_PROPERTY(bool accessGranted READ accessGranted NOTIFY accessChanged)
    // When the last copy was made, as text for the user ("" = never).
    Q_PROPERTY(QString lastBackup READ lastBackup NOTIFY backedUp)
    // True when the previous start restored the cards from the copy.
    Q_PROPERTY(bool restored READ restored CONSTANT)
    // A copy from before this installation exists: offer to bring the cards back.
    // Until the user decides, the copy is not touched.
    Q_PROPERTY(bool restoreAvailable READ restoreAvailable NOTIFY restoreAvailableChanged)
    Q_PROPERTY(int copyCards READ copyCards NOTIFY restoreAvailableChanged)

public:
    static Backup *instance();
    static Backup *create(QQmlEngine *, QJSEngine *);

    // Call before CardStore opens its database: restores the copy when this is a fresh install.
    static bool restoreIfFresh();
    static QString folder();

    bool accessGranted() const { return m_access; }
    QString lastBackup() const { return m_last; }
    bool restored() const { return s_restored; }
    bool restoreAvailable() const { return m_restoreAvailable; }
    int copyCards() const { return m_copyCards; }

    // Opens the system screen / dialog where the user allows the access.
    Q_INVOKABLE void requestAccess();
    // Replaces the cards on this phone with the copy; the app must be closed and opened again after it.
    Q_INVOKABLE bool restoreNow();
    // "Start without the old cards": the copy is overwritten from now on.
    Q_INVOKABLE void keepCurrent();
    // Copies now (also called automatically).
    Q_INVOKABLE bool backupNow();

signals:
    void accessChanged();
    void backedUp();
    void restoreAvailableChanged();

private:
    explicit Backup(QObject *parent = nullptr);
    bool checkAccess();
    void checkRestore();
    void scheduleBackup();

    bool m_access = false;
    bool m_restoreAvailable = false;
    int m_copyCards = 0;
    QString m_last;
    class QTimer *m_timer = nullptr;
    static inline bool s_restored = false;
};
