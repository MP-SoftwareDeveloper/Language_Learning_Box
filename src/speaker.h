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
    // Voice for the language of the selected learning box (German, or American English only).
    Q_PROPERTY(bool voiceAvailable READ voiceAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString voiceName READ voiceName NOTIFY stateChanged) // "German" / "American English"
    Q_PROPERTY(double rate READ rate WRITE setRate NOTIFY rateChanged) // -1.0 .. 1.0
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

public:
    static Speaker *create(QQmlEngine *, QJSEngine *);

    bool ready() const;
    bool speaking() const;
    int utterance() const { return m_utterance; }
    bool germanAvailable() const { return m_germanAvailable; }
    bool voiceAvailable() const;
    QString voiceName() const;
    // BCP-47 tag of the learning language: "de-DE" or "en-US".
    static QString learningTag();
    double rate() const;
    void setRate(double r);
    QString errorString() const;

    // languageTag: BCP-47 such as "de-DE", "en-US" or "fa-IR"; empty = the language of the
    // selected learning box. Returns the utterance id, 0 if nothing is spoken (empty text or
    // no such voice).
    Q_INVOKABLE int speak(const QString &text, const QString &languageTag = QString());
    Q_INVOKABLE void stop();
    // Is there a voice for "de-DE" / "en-US" (English: American only)?
    Q_INVOKABLE bool hasVoice(const QString &languageTag) const;

signals:
    void stateChanged();
    void rateChanged();

private:
    explicit Speaker(QObject *parent = nullptr);
    void onEngineState(QTextToSpeech::State s);
    // `anyVariant`: accept another country of the same language (de_AT for de_DE); off for
    // English, which must be American.
    bool selectLocale(const QLocale &wanted, bool anyVariant = true);
    bool hasLocale(const QLocale &wanted, bool anyVariant) const;

    QTextToSpeech *m_tts = nullptr;
    bool m_germanAvailable = false;
    bool m_initialised = false;
    bool sayNow(const QString &text, const QString &languageTag);

    int m_utterance = 0;
    QString m_pendingText;
    QString m_pendingTag;
};
