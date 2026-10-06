#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "backup.h"
#include "cardstore.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("LearningBox"));
    QGuiApplication::setApplicationName(QStringLiteral("LearningBox"));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // A fresh install (no cards of its own) gets the cards back from the backup folder first.
    Backup::restoreIfFresh();

    // Open the database before QML so an error is reported, not a blank window.
    if (!CardStore::instance()->ready())
        qCritical() << "Database unavailable:" << CardStore::instance()->lastError();

    Backup::instance(); // keeps the backup up to date from now on

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
