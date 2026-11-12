// The automatic backup copy lives in Documents/LearningBox, where other apps can change it. A restore must only
// accept an intact database of this app (SECURITY.md F-1), and only this app's own picture files.
#include <QtTest>

#include <QDir>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

#include "backup.h"
#include "cardstore.h"

class TstBackup : public QObject
{
    Q_OBJECT

    static QString appDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }
    static QString sharedDb() { return Backup::folder() + QStringLiteral("/learningbox.sqlite"); }
    static QString localDb() { return appDir() + QStringLiteral("/learningbox.sqlite"); }

    static void makeDb(const QString &path, const QStringList &statements)
    {
        QFile::remove(path);
        {
            auto d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("tst-make"));
            d.setDatabaseName(path);
            QVERIFY(d.open());
            QSqlQuery q(d);
            for (const QString &s : statements)
                QVERIFY2(q.exec(s), qPrintable(s));
        }
        QSqlDatabase::removeDatabase(QStringLiteral("tst-make"));
    }
    static QStringList goodDb(int version)
    {
        return {QStringLiteral("CREATE TABLE cards (id INTEGER PRIMARY KEY, front TEXT, back TEXT)"),
                QStringLiteral("INSERT INTO cards (front, back) VALUES ('Haus', 'house')"),
                QStringLiteral("PRAGMA user_version = %1").arg(version)};
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("learningbox-test"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_backup"));
    }
    void init()
    {
        QDir(appDir()).removeRecursively();
        QDir(Backup::folder()).removeRecursively();
        QVERIFY(QDir().mkpath(Backup::folder()));
        QSettings().clear();
    }

    void intactDatabaseIsRestored()
    {
        makeDb(sharedDb(), goodDb(CardStore::schemaVersion()));
        QVERIFY(Backup::restoreIfFresh());
        QVERIFY(QFile::exists(localDb()));
        QVERIFY(!QFile::exists(localDb() + QStringLiteral(".restoring")));
    }
    void garbageIsNotRestored()
    {
        QFile f(sharedDb());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, 'x'));
        f.close();
        QVERIFY(!Backup::restoreIfFresh());
        QVERIFY(!QFile::exists(localDb()));
        QVERIFY(!QFile::exists(localDb() + QStringLiteral(".restoring")));
    }
    void foreignDatabaseIsNotRestored()
    {
        makeDb(sharedDb(), {QStringLiteral("CREATE TABLE other (x)"), QStringLiteral("PRAGMA user_version = 1")});
        QVERIFY(!Backup::restoreIfFresh());
        QVERIFY(!QFile::exists(localDb()));
    }
    void newerSchemaIsNotRestored()
    {
        makeDb(sharedDb(), goodDb(CardStore::schemaVersion() + 1));
        QVERIFY(!Backup::restoreIfFresh());
        QVERIFY(!QFile::exists(localDb()));
    }
    void unversionedDatabaseIsNotRestored()
    {
        makeDb(sharedDb(), goodDb(0));
        QVERIFY(!Backup::restoreIfFresh());
    }
    void existingCardsAreNeverOverwritten()
    {
        QVERIFY(QDir().mkpath(appDir()));
        makeDb(localDb(), goodDb(CardStore::schemaVersion()));
        const qint64 before = QFileInfo(localDb()).size();
        makeDb(sharedDb(), goodDb(CardStore::schemaVersion()));
        QVERIFY(!Backup::restoreIfFresh());
        QCOMPARE(QFileInfo(localDb()).size(), before);
    }
    void onlyOwnPicturesAreCopied()
    {
        makeDb(sharedDb(), goodDb(CardStore::schemaVersion()));
        const QString images = Backup::folder() + QStringLiteral("/images");
        QVERIFY(QDir().mkpath(images));
        const QString own = QStringLiteral("0b6e7f3a-1c2d-4e5f-8a9b-0c1d2e3f4a5b.jpg");
        for (const QString &name : {own, QStringLiteral("evil.php"), QStringLiteral("..jpg"), QStringLiteral("notes.jpg")}) {
            QFile f(images + u'/' + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("data");
        }
        { // right name, far too big
            QFile f(images + QStringLiteral("/1b6e7f3a-1c2d-4e5f-8a9b-0c1d2e3f4a5b.jpg"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.resize(9 * 1024 * 1024);
        }
        QVERIFY(Backup::restoreIfFresh());
        const QStringList got = QDir(appDir() + QStringLiteral("/images")).entryList(QDir::Files);
        QCOMPARE(got, QStringList{own});
    }
};

QTEST_MAIN(TstBackup)
#include "tst_backup.moc"
