#include "speaker.h"
#include "cardstore.h"

#include <QSettings>

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
    m_tts->setRate(qBound(-1.0, QSettings().value(QStringLiteral("speech/rate"), 0.0).toDouble(), 1.0));
    connect(m_tts, &QTextToSpeech::errorOccurred, this, [this](auto, const QString &msg) {
        qWarning() << "TTS error:" << msg;
        emit stateChanged();
    });
    if (m_tts->state() == QTextToSpeech::Ready)
        onEngineState(QTextToSpeech::Ready);
    // Another learning box may learn another language: voiceAvailable / voiceName change.
    connect(CardStore::instance(), &CardStore::changed, this, &Speaker::stateChanged);
}

QString Speaker::learningTag()
{
    return CardStore::instance()->learningLanguage() == QLatin1String("en") ? QStringLiteral("en-US")
                                                                          : QStringLiteral("de-DE");
}

bool Speaker::hasLocale(const QLocale &wanted, bool anyVariant) const
{
    const auto locales = m_tts->availableLocales();
    for (const QLocale &l : locales)
        if (l == wanted || (anyVariant && l.language() == wanted.language()))
            return true;
    return false;
}

bool Speaker::voiceAvailable() const
{
    return hasVoice(learningTag());
}

bool Speaker::hasVoice(const QString &languageTag) const
{
    if (!m_initialised)
        return false;
    return hasLocale(QLocale(languageTag), !languageTag.startsWith(QLatin1String("en")));
}

QString Speaker::voiceName() const
{
    return learningTag() == QLatin1String("en-US") ? tr("American English") : tr("German");
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

bool Speaker::selectLocale(const QLocale &wanted, bool anyVariant)
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
        if (anyVariant && l.language() == wanted.language()) {
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
void Speaker::setRate(double r)
{
    const double v = qBound(-1.0, r, 1.0);
    m_tts->setRate(v);
    QSettings().setValue(QStringLiteral("speech/rate"), v); // remembered for the next start
}
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
    const QString tag = languageTag.isEmpty() ? learningTag() : languageTag;
    // American English only: no British / Australian voice instead.
    if (!selectLocale(QLocale(tag), !tag.startsWith(QLatin1String("en"))))
        return false;
    m_tts->stop();
    m_tts->say(text);
    return true;
}

void Speaker::stop() { m_tts->stop(); }
