#include "speaker.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QQmlEngine>
#include <QDebug>

Speaker *Speaker::create(QQmlEngine *, QJSEngine *engine)
{
    static Speaker *s = new Speaker(QCoreApplication::instance());
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

Speaker::Speaker(QObject *parent)
    : QObject(parent)
    , m_tts(new QTextToSpeech(this))
{
    // Android initialises its engine asynchronously: locale lists are only
    // meaningful once the first Ready arrives.
    connect(m_tts, &QTextToSpeech::stateChanged, this, &Speaker::onEngineState);
    connect(m_tts, &QTextToSpeech::rateChanged, this, &Speaker::rateChanged);
    connect(m_tts, &QTextToSpeech::errorOccurred, this, [this](auto, const QString &msg) {
        qWarning() << "TTS error:" << msg;
        emit stateChanged();
    });
    if (m_tts->state() == QTextToSpeech::Ready)
        onEngineState(QTextToSpeech::Ready);
}

void Speaker::onEngineState(QTextToSpeech::State s)
{
    if (s == QTextToSpeech::Ready && !m_initialised) {
        m_initialised = true;
        m_germanAvailable = selectLocale(QLocale(QLocale::German, QLocale::Germany));
        if (!m_germanAvailable)
            qWarning() << "TTS: no German voice installed. Available:" << m_tts->availableLocales();
        if (!m_pendingText.isEmpty()) {
            const QString text = std::exchange(m_pendingText, {});
            const QString tag = std::exchange(m_pendingTag, {});
            sayNow(text, tag); // same utterance id as the original request
        }
    }
    emit stateChanged();
}

bool Speaker::selectLocale(const QLocale &wanted)
{
    if (m_tts->locale() == wanted)
        return true;
    const auto locales = m_tts->availableLocales();
    // Exact match first, then any locale of the same language (e.g. de_AT).
    for (const QLocale &l : locales) {
        if (l == wanted) {
            m_tts->setLocale(l);
            return true;
        }
    }
    for (const QLocale &l : locales) {
        if (l.language() == wanted.language()) {
            m_tts->setLocale(l);
            return true;
        }
    }
    return false;
}

bool Speaker::ready() const
{
    return m_tts->state() == QTextToSpeech::Ready || m_tts->state() == QTextToSpeech::Speaking;
}

bool Speaker::speaking() const { return m_tts->state() == QTextToSpeech::Speaking; }
double Speaker::rate() const { return m_tts->rate(); }
void Speaker::setRate(double r) { m_tts->setRate(qBound(-1.0, r, 1.0)); }
QString Speaker::errorString() const { return m_tts->errorString(); }

int Speaker::speak(const QString &text, const QString &languageTag)
{
    const QString t = text.trimmed();
    if (t.isEmpty())
        return 0;
    const int id = ++m_utterance;
    if (!m_initialised) {
        m_pendingText = t;
        m_pendingTag = languageTag;
    } else if (!sayNow(t, languageTag)) {
        emit stateChanged();
        return 0;
    }
    emit stateChanged();
    return id;
}

bool Speaker::sayNow(const QString &text, const QString &languageTag)
{
    if (!selectLocale(QLocale(languageTag)))
        return false;
    m_tts->stop();
    m_tts->say(text);
    return true;
}

void Speaker::stop() { m_tts->stop(); }
