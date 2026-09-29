#pragma once

#include <QObject>
#include <QPointer>
#include <QTextToSpeech>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

// Thin wrapper over QTextToSpeech. On Android this drives the system TTS engine
// (Google TTS etc.) through Qt's own plugin, so no project Java is required.
class Speaker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool speaking READ speaking NOTIFY stateChanged)
    // Id of the latest speak() request, so each 🔊 button can tell whether the current
    // speech is its own (only that one shows ⏹).
    Q_PROPERTY(int utterance READ utterance NOTIFY stateChanged)
    Q_PROPERTY(bool germanAvailable READ germanAvailable NOTIFY stateChanged)
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged) // -1.0 .. 1.0
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

public:
    static Speaker *create(QQmlEngine *, QJSEngine *);

    bool ready() const;
    bool speaking() const;
    int utterance() const { return m_utterance; }
    bool germanAvailable() const { return m_germanAvailable; }
    double rate() const;
    void setRate(double r);
    QString errorString() const;

    // languageTag: BCP-47 such as "de-DE" or "fa-IR". Returns the utterance id, 0 if nothing
    // is spoken (empty text or no such voice).
    Q_INVOKABLE int speak(const QString &text, const QString &languageTag = QStringLiteral("de-DE"));
    Q_INVOKABLE void stop();

signals:
    void stateChanged();
    void rateChanged();

private:
    explicit Speaker(QObject *parent = nullptr);
    void onEngineState(QTextToSpeech::State s);
    bool selectLocale(const QLocale &wanted);

    QTextToSpeech *m_tts = nullptr;
    bool m_germanAvailable = false;
    bool m_initialised = false;
    bool sayNow(const QString &text, const QString &languageTag);

    int m_utterance = 0;
    QString m_pendingText;
    QString m_pendingTag;
};
