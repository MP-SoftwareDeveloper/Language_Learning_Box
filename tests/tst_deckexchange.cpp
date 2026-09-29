#include <QtTest>
#include <QDir>
#include <QImage>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "cardstore.h"
#include "deckexchange.h"

// Export -> delete -> import round trips against a throw-away database (test mode paths).
class TstDeckExchange : public QObject
{
    Q_OBJECT
    QQmlEngine engine;
    DeckExchange *ex = nullptr;
    QTemporaryDir tmp;

    int idOf(const QString &front) { return CardStore::instance()->findByFront(front); }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QVERIFY(CardStore::instance()->ready());
        ex = DeckExchange::create(&engine, &engine);
    }

    void lboxRoundTripKeepsProgressAndPicture()
    {
        auto *s = CardStore::instance();
        const QString photo = tmp.filePath(QStringLiteral("p.jpg"));
        QImage img(300, 200, QImage::Format_RGB32);
        img.fill(Qt::red);
        img.save(photo);
        const QString pic = s->importImage(QUrl::fromLocalFile(photo));
        QVERIFY(!pic.isEmpty());
        const int a = s->addCard(QStringLiteral("das Haus"), QStringLiteral("خانه"), QStringLiteral("Das Haus ist alt."), pic);
        const int b = s->addCard(QStringLiteral("gehen"), QStringLiteral("رفتن"), QString());
        QVERIFY(s->moveCard(a, 3));
        s->recordAnswer(b, false);

        const QUrl file = QUrl::fromLocalFile(tmp.filePath(QStringLiteral("deck.lbox")));
        const QVariantMap r = ex->exportCards(file, QStringLiteral("lbox"), true, 0);
        QVERIFY2(r.value(QStringLiteral("ok")).toBool(), qPrintable(r.value(QStringLiteral("error")).toString()));
        QCOMPARE(r.value(QStringLiteral("count")).toInt(), 2);

        QVERIFY(s->removeCard(a));
        QVERIFY(s->removeCard(b));
        ex->openFile(file);
        QVariantMap p = ex->preview();
        QVERIFY2(p.value(QStringLiteral("error")).toString().isEmpty(), qPrintable(p.value(QStringLiteral("error")).toString()));
        QCOMPARE(p.value(QStringLiteral("format")).toString(), QStringLiteral("lbox"));
        QCOMPARE(p.value(QStringLiteral("newCount")).toInt(), 2);
        QVERIFY(p.value(QStringLiteral("hasProgress")).toBool());

        const QVariantMap res = ex->applyImport(QStringLiteral("skip"), true, 1, false);
        QCOMPARE(res.value(QStringLiteral("added")).toInt(), 2);
        const auto house = s->cardById(idOf(QStringLiteral("das Haus")));
        QCOMPARE(house->box, 3);                                   // progress restored
        QCOMPARE(house->dueAt.date(), QDate::currentDate().addDays(4));
        QCOMPARE(house->example, QStringLiteral("Das Haus ist alt."));
        QVERIFY(!house->image.isEmpty());                          // picture restored
        QVERIFY(QFile::exists(s->imageDir() + u'/' + house->image));
        QCOMPARE(s->cardById(idOf(QStringLiteral("gehen")))->lapses, 1);
    }

    void importAgainSkipsOrUpdates()
    {
        auto *s = CardStore::instance();
        const QUrl file = QUrl::fromLocalFile(tmp.filePath(QStringLiteral("deck.lbox")));
        ex->openFile(file);
        QCOMPARE(ex->preview().value(QStringLiteral("existingCount")).toInt(), 2);
        QCOMPARE(ex->applyImport(QStringLiteral("skip"), false, 1, false).value(QStringLiteral("skipped")).toInt(), 2);

        s->updateCard(idOf(QStringLiteral("gehen")), QStringLiteral("gehen"), QStringLiteral("old"), QString(), QString());
        ex->openFile(file);
        const QVariantMap r = ex->applyImport(QStringLiteral("update"), false, 1, false);
        QCOMPARE(r.value(QStringLiteral("updated")).toInt(), 2);
        QCOMPARE(s->cardById(idOf(QStringLiteral("gehen")))->back, QStringLiteral("رفتن"));
    }

    void csvIntoChosenBoxWithoutProgress()
    {
        auto *s = CardStore::instance();
        const QString path = tmp.filePath(QStringLiteral("quizlet.txt"));
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("dog\tder Hund\ncat\tdie Katze\n"); // meaning first: needs "swap"
        f.close();
        ex->openFile(QUrl::fromLocalFile(path));
        QCOMPARE(ex->preview().value(QStringLiteral("format")).toString(), QStringLiteral("csv"));
        QCOMPARE(ex->preview().value(QStringLiteral("title")).toString(), QStringLiteral("quizlet"));
        const QVariantMap r = ex->applyImport(QStringLiteral("skip"), true, 2, true);
        QCOMPARE(r.value(QStringLiteral("added")).toInt(), 2);
        const auto dog = s->cardById(idOf(QStringLiteral("der Hund")));
        QVERIFY(dog);
        QCOMPARE(dog->back, QStringLiteral("dog"));
        QCOMPARE(dog->box, 2);
        QCOMPARE(dog->dueAt.date(), QDate::currentDate().addDays(2));
        QCOMPARE(dog->deck, QStringLiteral("quizlet"));
    }

    void csvExportOfOneBox()
    {
        const QUrl file = QUrl::fromLocalFile(tmp.filePath(QStringLiteral("box2.csv")));
        const QVariantMap r = ex->exportCards(file, QStringLiteral("csv"), false, 2);
        QCOMPARE(r.value(QStringLiteral("count")).toInt(), 2); // der Hund, die Katze
        QFile f(file.toLocalFile());
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY(text.contains(QStringLiteral("der Hund,dog")));
        QVERIFY(!text.contains(QStringLiteral("das Haus")));
    }

    void importIntoNewLearningBox()
    {
        auto *s = CardStore::instance();
        const int before = s->currentCollection();
        const QUrl file = QUrl::fromLocalFile(tmp.filePath(QStringLiteral("deck.lbox")));
        ex->openFile(file);
        QCOMPARE(ex->preview().value(QStringLiteral("title")).toString(), s->currentCollectionName()); // export title
        const QVariantMap r = ex->applyImport(QStringLiteral("skip"), true, 1, false, QStringLiteral("From Sara"));
        QCOMPARE(r.value(QStringLiteral("added")).toInt(), 2);   // not skipped: the new box is empty
        QVERIFY(s->currentCollection() != before);
        QCOMPARE(s->currentCollectionName(), QStringLiteral("From Sara"));
        QCOMPARE(s->totalCount(), 2);
        s->selectCollection(before);
    }

    void badFileIsReported()
    {
        const QString path = tmp.filePath(QStringLiteral("empty.csv"));
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.close();
        ex->openFile(QUrl::fromLocalFile(path));
        QVERIFY(!ex->preview().value(QStringLiteral("error")).toString().isEmpty());
        QVERIFY(!ex->applyImport(QStringLiteral("skip"), false, 1, false).value(QStringLiteral("error")).toString().isEmpty());
    }
};

QTEST_MAIN(TstDeckExchange)
#include "tst_deckexchange.moc"
