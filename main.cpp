#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "cardstore.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("LearningBox"));
    QGuiApplication::setApplicationName(QStringLiteral("LearningBox"));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // Open the database before QML so an error is reported, not a blank window.
    if (!CardStore::instance()->ready())
        qCritical() << "Database unavailable:" << CardStore::instance()->lastError();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("LearningBox", "Main");

    return QGuiApplication::exec();
}
