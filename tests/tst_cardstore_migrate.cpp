#include <QtTest>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

#include "cardstore.h"

// An existing (schema v3) database must keep all cards after the upgrade to learning boxes.
class TstCardStoreMigrate : public QObject
{
    Q_OBJECT
private slots:
    void upgradesV3()
    {
        QStandardPaths::setTestModeEnabled(true);
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("old"));
            db.setDatabaseName(dir + QStringLiteral("/learningbox.sqlite"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE cards (id INTEGER PRIMARY KEY AUTOINCREMENT, front TEXT NOT NULL,"
                                          " back TEXT NOT NULL DEFAULT '', example TEXT NOT NULL DEFAULT '',"
                                          " box INTEGER NOT NULL DEFAULT 1, due_at INTEGER, reviews INTEGER NOT NULL DEFAULT 0,"
                                          " lapses INTEGER NOT NULL DEFAULT 0, created_at INTEGER NOT NULL,"
                                          " updated_at INTEGER NOT NULL, deck TEXT NOT NULL DEFAULT '',"
                                          " image TEXT NOT NULL DEFAULT '')")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO cards (front, back, box, due_at, created_at, updated_at)"
                                          " VALUES ('das Haus', 'house', 3, 0, 0, 0), ('gehen', 'go', 1, 0, 0, 0),"
                                          " ('der Reis', 'rice\nSingular der Real\nPlural Reais\nPlural -', 1, 0, 0, 0),"
                                          " ('der Lehrer', 'teacher\nPlural Lehrer\nSingular die Lehrerin\nPlural Lehrerinnen', 1, 0, 0, 0),"
                                          " ('die Mauern', 'walls\nSingular die Mauer', 1, 0, 0, 0),"
                                          " ('die Reise', 'trip\nSingular Reis', 1, 0, 0, 0)")));
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 3")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("old"));

        auto *s = CardStore::instance();
        QVERIFY2(s->ready(), qPrintable(s->lastError()));
        QCOMPARE(s->collections().size(), 1);
        QCOMPARE(s->totalCount(), 6);                 // nothing lost
        QCOMPARE(s->boxCounts().at(2).toInt(), 1);    // progress kept
        QCOMPARE(s->currentCollectionName(), QStringLiteral("My learning box"));
        // Another noun's forms on an old card are removed, real counterparts and plural fronts are kept
        QCOMPARE(s->card(s->findByFront(QStringLiteral("der Reis"))).value(QStringLiteral("back")).toString(),
                 QStringLiteral("rice"));
        QCOMPARE(s->card(s->findByFront(QStringLiteral("der Lehrer"))).value(QStringLiteral("back")).toString(),
                 QStringLiteral("teacher\nPlural Lehrer\nSingular die Lehrerin\nPlural Lehrerinnen"));
        QCOMPARE(s->card(s->findByFront(QStringLiteral("die Mauern"))).value(QStringLiteral("back")).toString(),
                 QStringLiteral("walls\nSingular die Mauer"));
        QCOMPARE(s->card(s->findByFront(QStringLiteral("die Reise"))).value(QStringLiteral("back")).toString(),
                 QStringLiteral("trip"));
        // ... and never saved again
        const int id = s->addCard(QStringLiteral("der Reis2"), QStringLiteral("rice\nSingular der Real"), QString());
        QCOMPARE(s->card(id).value(QStringLiteral("back")).toString(), QStringLiteral("rice"));
    }
};

QTEST_MAIN(TstCardStoreMigrate)
#include "tst_cardstore_migrate.moc"
