#include "appmode.h"
#include "cardstore.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QSettings>

namespace {
constexpr auto kModeKey = "app/mode";
constexpr auto kSetupKey = "setup/done";
constexpr auto kHelpLangKey = "help/language";
}

AppMode *AppMode::instance()
{
    static AppMode *s = new AppMode(QCoreApplication::instance());
    return s;
}

AppMode *AppMode::create(QQmlEngine *, QJSEngine *engine)
{
    AppMode *s = instance();
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

AppMode::AppMode(QObject *parent)
    : QObject(parent)
{
    QSettings st;
    if (!st.contains(QLatin1String(kSetupKey))) {
        // Installed before the setup existed and already has cards: keep everything as it was.
        int cards = 0;
        const QVariantList boxes = CardStore::instance()->collections();
        for (const QVariant &b : boxes)
            cards += b.toMap().value(QStringLiteral("total")).toInt();
        if (cards > 0) {
            st.setValue(QLatin1String(kSetupKey), true);
            st.setValue(QLatin1String(kModeKey), QStringLiteral("full"));
        }
    }
    m_setupDone = st.value(QLatin1String(kSetupKey), false).toBool();
    m_full = st.value(QLatin1String(kModeKey), QStringLiteral("full")).toString() != QLatin1String("simple");
    m_helpLanguage = st.value(QLatin1String(kHelpLangKey)).toString();
}

void AppMode::setMode(const QString &mode)
{
    const bool full = mode != QLatin1String("simple");
    if (full == m_full)
        return;
    m_full = full;
    QSettings().setValue(QLatin1String(kModeKey), this->mode());
    emit changed();
}

void AppMode::setSetupDone(bool done)
{
    if (done == m_setupDone)
        return;
    m_setupDone = done;
    QSettings().setValue(QLatin1String(kSetupKey), done);
    emit changed();
}

void AppMode::setHelpLanguage(const QString &lang)
{
    if (lang != QLatin1String("en") && lang != QLatin1String("fa") && lang != QLatin1String("de"))
        return;
    if (lang == m_helpLanguage)
        return;
    m_helpLanguage = lang;
    QSettings().setValue(QLatin1String(kHelpLangKey), lang);
    emit changed();
}
