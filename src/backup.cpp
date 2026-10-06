#include "backup.h"

#include "cardstore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace {
constexpr auto kDbName = "learningbox.sqlite";

QString appDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }

// Copies the files of `from` that `to` does not have yet (card pictures never change once saved).
void copyMissingFiles(const QString &from, const QString &to)
{
    const QDir src(from);
    if (!src.exists() || !QDir().mkpath(to))
        return;
    for (const QFileInfo &f : src.entryInfoList(QDir::Files))
        if (!QFile::exists(to + u'/' + f.fileName()))
            QFile::copy(f.absoluteFilePath(), to + u'/' + f.fileName());
}

// Number of cards in a copy of the database (-1 = cannot be read)
int cardsIn(const QString &file)
{
    int n = -1;
    {
        auto d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("backup-check"));
        d.setDatabaseName(file);
        d.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (d.open()) {
            QSqlQuery q(d);
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM cards")) && q.next())
                n = q.value(0).toInt();
        }
    }
    QSqlDatabase::removeDatabase(QStringLiteral("backup-check"));
    return n;
}

bool replaceFile(const QString &from, const QString &to)
{
    QFile::remove(to);
    return QFile::copy(from, to);
}
} // namespace

QString Backup::folder()
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return (docs.isEmpty() ? QStringLiteral("/storage/emulated/0/Documents") : docs) + QStringLiteral("/LearningBox");
}

bool Backup::restoreIfFresh()
{
    const QString local = appDir() + u'/' + QLatin1String(kDbName);
    if (QFile::exists(local))
        return false; // not a fresh install: never overwrite the cards on this phone
    // A new installation. Until the old cards were restored or the user said to start without them, the
    // copy in Documents is never overwritten (Android 11+ lets the app read it only after the permission).
    QSettings().setValue(QStringLiteral("backup/pending"), true);
    const QString copy = folder() + u'/' + QLatin1String(kDbName);
    if (!QFile::exists(copy) || !QDir().mkpath(appDir()))
        return false;
    if (!QFile::copy(copy, local)) {
        qWarning() << "Backup: cannot restore" << copy;
        return false;
    }
    copyMissingFiles(folder() + QStringLiteral("/images"), appDir() + QStringLiteral("/images"));
    qInfo() << "Backup: cards restored from" << copy;
    QSettings().setValue(QStringLiteral("backup/pending"), false);
    s_restored = true;
    return true;
}

Backup *Backup::instance()
{
    static Backup *self = new Backup(qApp);
    return self;
}

Backup *Backup::create(QQmlEngine *, QJSEngine *)
{
    return instance();
}

Backup::Backup(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(20 * 1000); // a review session changes cards all the time: copy when it calms down
    connect(m_timer, &QTimer::timeout, this, [this] { backupNow(); });
    connect(CardStore::instance(), &CardStore::changed, this, &Backup::scheduleBackup);

    m_access = checkAccess();
    checkRestore();
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
        if (s == Qt::ApplicationActive) {
            // back from the system screen where access is given
            const bool now = checkAccess();
            if (now != m_access) {
                m_access = now;
                emit accessChanged();
                checkRestore();
                if (now)
                    backupNow();
            }
        } else if (s == Qt::ApplicationSuspended || s == Qt::ApplicationInactive) {
            backupNow(); // leaving the app: copy right away
        }
    });
    if (m_access)
        QTimer::singleShot(3000, this, [this] { backupNow(); });
}

// A new installation (backup/pending): the copy from before is offered, and left untouched until the user
// decided. No copy to be found: nothing to decide, the automatic backup starts.
void Backup::checkRestore()
{
    const bool pending = QSettings().value(QStringLiteral("backup/pending"), false).toBool();
    const QString copy = folder() + u'/' + QLatin1String(kDbName);
    int cards = 0;
    bool available = false;
    if (pending && m_access) {
        cards = QFile::exists(copy) ? cardsIn(copy) : 0;
        available = cards > 0;
        if (!available)
            QSettings().setValue(QStringLiteral("backup/pending"), false);
    }
    if (available != m_restoreAvailable || cards != m_copyCards) {
        m_restoreAvailable = available;
        m_copyCards = cards;
        emit restoreAvailableChanged();
    }
}

bool Backup::restoreNow()
{
    const QString copy = folder() + u'/' + QLatin1String(kDbName);
    if (!m_access || !QFile::exists(copy))
        return false;
    const QString local = appDir() + u'/' + QLatin1String(kDbName);
    const QString safe = appDir() + QStringLiteral("/before-restore.sqlite");
    CardStore::instance()->closeDatabase();
    QFile::remove(safe);
    QFile::rename(local, safe); // the cards of this phone stay available until the restored file is in place
    QFile::remove(local + QStringLiteral("-wal"));
    QFile::remove(local + QStringLiteral("-shm"));
    if (!QFile::copy(copy, local)) {
        QFile::rename(safe, local);
        return false;
    }
    QFile::remove(safe);
    copyMissingFiles(folder() + QStringLiteral("/images"), appDir() + QStringLiteral("/images"));
    QSettings().setValue(QStringLiteral("backup/pending"), false);
    m_restoreAvailable = false;
    emit restoreAvailableChanged();
    return true;
}

void Backup::keepCurrent()
{
    QSettings().setValue(QStringLiteral("backup/pending"), false);
    m_restoreAvailable = false;
    emit restoreAvailableChanged();
    backupNow();
}

// The folder can be written: the permission is there. (A real write is the only test that is right on
// every Android version.)
bool Backup::checkAccess()
{
    const QString dir = folder();
    if (!QDir().mkpath(dir))
        return false;
    QFile probe(dir + QStringLiteral("/.write-test"));
    if (!probe.open(QIODevice::WriteOnly))
        return false;
    probe.close();
    probe.remove();
    return true;
}

void Backup::scheduleBackup()
{
    if (m_access)
        m_timer->start();
}

bool Backup::backupNow()
{
    m_timer->stop();
    if (!CardStore::instance()->ready() || !m_access || QSettings().value(QStringLiteral("backup/pending"), false).toBool())
        return false; // until the user decided about restoring, the copy is not touched
    // Nothing worth keeping yet: do not overwrite a good copy with an empty database
    if (CardStore::instance()->totalAllCards() <= 0)
        return false;
    const QString dir = folder();
    const QString tmp = appDir() + QStringLiteral("/backup-tmp.sqlite");
    if (!CardStore::instance()->backupTo(tmp)) {
        qWarning() << "Backup: could not write a consistent copy";
        return false;
    }
    const bool ok = replaceFile(tmp, dir + u'/' + QLatin1String(kDbName));
    QFile::remove(tmp);
    if (!ok) {
        qWarning() << "Backup: cannot write to" << dir;
        return false;
    }
    copyMissingFiles(appDir() + QStringLiteral("/images"), dir + QStringLiteral("/images"));
    m_last = QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat);
    emit backedUp();
    return true;
}

void Backup::requestAccess()
{
#ifdef Q_OS_ANDROID
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
        return;
    QJniEnvironment env;
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 30) {
        // "All files access" for this app (a single switch)
        const QString package = activity.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
        const QJniObject uri = QJniObject::callStaticObjectMethod(
            "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
            QJniObject::fromString(QStringLiteral("package:") + package).object<jstring>());
        const QJniObject action = QJniObject::fromString(
            QStringLiteral("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION"));
        QJniObject intent("android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
                          action.object<jstring>(), uri.object());
        activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
        if (env.checkAndClearExceptions()) {
            // some phones lack the per-app screen: the general one
            const QJniObject general = QJniObject::fromString(
                QStringLiteral("android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION"));
            QJniObject intent2("android/content/Intent", "(Ljava/lang/String;)V", general.object<jstring>());
            activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent2.object());
            env.checkAndClearExceptions();
        }
    } else {
        // Android 10 and older: the normal storage permission
        const jclass stringClass = env.findClass("java/lang/String");
        jobjectArray perms = env->NewObjectArray(1, stringClass, nullptr);
        env->SetObjectArrayElement(perms, 0,
            QJniObject::fromString(QStringLiteral("android.permission.WRITE_EXTERNAL_STORAGE")).object<jstring>());
        activity.callMethod<void>("requestPermissions", "([Ljava/lang/String;I)V", perms, jint(4711));
        env.checkAndClearExceptions();
    }
#endif
}
