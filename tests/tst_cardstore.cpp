#include <QtTest>
#include <QSignalSpy>
#include <QDir>
#include <QImage>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

#include "cardstore.h"

// Runs against a throw-away database: QStandardPaths test mode redirects AppDataLocation.
class TstCardStore : public QObject
{
    Q_OBJECT

    QString imageDir() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/images");
    }
    int imageFiles() const { return int(QDir(imageDir()).entryList(QDir::Files).size()); }
    QString makePhoto(const QString &name)
    {
        const QString path = QDir::temp().filePath(name);
        QImage img(1600, 1200, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        img.save(path);
        return path;
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
        QSettings().remove(QStringLiteral("review")); // settings outlive the throw-away database
        QSettings().remove(QStringLiteral("translation"));
        QVERIFY2(CardStore::instance()->ready(), qPrintable(CardStore::instance()->lastError()));
    }

    // Regression: a null deck/image QString was bound as SQL NULL -> NOT NULL constraint failure.
    void addCardWithoutDeckOrImage()
    {
        auto *s = CardStore::instance();
        const int id = s->addCard(QStringLiteral("der Hund"), QStringLiteral("سگ"), QString());
        QVERIFY2(id > 0, qPrintable(s->lastError()));
        const auto c = s->cardById(id);
        QVERIFY(c);
        QCOMPARE(c->deck, QString());
        QCOMPARE(c->image, QString());
        QCOMPARE(c->box, 1);
    }

    void bulkAddSkipsDuplicates()
    {
        auto *s = CardStore::instance();
        QList<Card> cards(2);
        cards[0].front = QStringLiteral("DER HUND"); // already there, case-insensitive
        cards[1].front = QStringLiteral("die Katze");
        QCOMPARE(s->addCards(cards, QStringLiteral("Test · K1")), 1);
        QCOMPARE(s->addCards(cards, QStringLiteral("Test · K1")), 0);
    }

    void manualMoveBetweenBoxes()
    {
        auto *s = CardStore::instance();
        const int id = s->addCard(QStringLiteral("das Fenster"), QStringLiteral("پنجره"), QString());
        QVERIFY(id > 0);
        s->recordAnswer(id, false);                     // some history that a move must keep
        QVERIFY(s->moveCard(id, 3));
        auto c = s->cardById(id);
        QCOMPARE(c->box, 3);
        QCOMPARE(c->reviews, 1);
        QCOMPARE(c->dueAt.date(), QDate::currentDate().addDays(4)); // box 3 interval
        bool listed = false;
        for (const QVariant &v : s->cardsInBox(3))
            listed = listed || v.toMap().value(QStringLiteral("id")).toInt() == id;
        QVERIFY(listed);
        for (const QVariant &v : s->cardsInBox(2))
            QVERIFY(v.toMap().value(QStringLiteral("id")).toInt() != id);

        QVERIFY(s->moveCard(id, 6));                    // Learned: not scheduled
        c = s->cardById(id);
        QCOMPARE(c->box, 6);
        QVERIFY(!c->dueAt.isValid());
        QVERIFY(s->moveCard(id, 5));                    // back from Learned
        QCOMPARE(s->cardById(id)->box, 5);
        QVERIFY(!s->moveCard(99999, 2));                // unknown card
    }

    void learningBoxesAreSeparate()
    {
        auto *s = CardStore::instance();
        const int first = s->currentCollection();
        QVERIFY(first > 0);
        QVERIFY(!s->currentCollectionName().isEmpty());
        const int total1 = s->totalCount();

        const int a2 = s->createCollection(QStringLiteral("Netzwerk neu A2"));
        QVERIFY(a2 > 0);
        QCOMPARE(s->currentCollection(), first);            // created, not selected
        s->selectCollection(a2);
        QCOMPARE(s->currentCollection(), a2);
        QCOMPARE(s->currentCollectionName(), QStringLiteral("Netzwerk neu A2"));
        QCOMPARE(s->totalCount(), 0);                        // a new box is empty
        QVERIFY(s->addCard(QStringLiteral("der Hund"), QStringLiteral("dog"), QString()) > 0); // same word as box 1: allowed
        QCOMPARE(s->totalCount(), 1);
        QCOMPARE(s->dueCount(), 1);

        // Selected one is first in the list, remembered in the settings
        QVariantList list = s->collections();
        QCOMPARE(list.first().toMap().value(QStringLiteral("id")).toInt(), a2);
        QVERIFY(list.first().toMap().value(QStringLiteral("current")).toBool());
        QCOMPARE(list.first().toMap().value(QStringLiteral("total")).toInt(), 1);
        QCOMPARE(QSettings().value(QStringLiteral("learningbox/current")).toInt(), a2);

        s->selectCollection(first);
        QCOMPARE(s->totalCount(), total1);                   // back to the first one's cards
        list = s->collections();
        QCOMPARE(list.first().toMap().value(QStringLiteral("id")).toInt(), first);
        QCOMPARE(list.at(1).toMap().value(QStringLiteral("boxCounts")).toList().at(0).toInt(), 1);

        QVERIFY(s->renameCollection(a2, QStringLiteral("A2")));
        QVERIFY(s->deleteCollection(a2));                    // with its card
        QCOMPARE(s->collections().size(), 1);
        QVERIFY(!s->deleteCollection(first));                // never the last one
        QCOMPARE(s->currentCollection(), first);
    }

    // Start over: all cards of the selected learning box back to Box 1, other learning boxes
    // untouched, optional spread and statistics, exact undo.
    void startOverAndUndo()
    {
        auto *s = CardStore::instance();
        const int other = s->currentCollection();
        const int otherCard = s->addCard(QStringLiteral("das Nachbarwort"), QString(), QString());
        QVERIFY(s->moveCard(otherCard, 4));

        const int lb = s->createCollection(QStringLiteral("Reset test"));
        s->selectCollection(lb);
        QList<int> ids;
        for (int i = 0; i < 10; ++i)
            ids << s->addCard(QStringLiteral("Wort %1").arg(i), QString(), QString());
        QVERIFY(s->moveCard(ids[0], 3));
        QVERIFY(s->moveCard(ids[1], 5));
        QVERIFY(s->moveCard(ids[2], 6)); // learned
        QVERIFY(s->recordAnswer(ids[3], false));
        const auto before1 = *s->cardById(ids[1]);
        const auto before3 = *s->cardById(ids[3]);

        // Learned cards excluded
        QCOMPARE(s->resetCount(false), 9);
        QCOMPARE(s->resetCollection(false, 1, false), 9);
        QVERIFY(s->canUndoReset());
        QCOMPARE(s->cardById(ids[2])->box, 6);
        QCOMPARE(s->cardById(ids[1])->box, 1);
        QCOMPARE(s->cardById(ids[3])->lapses, before3.lapses); // statistics kept
        QCOMPARE(s->dueCount(), 9);

        // Undo restores box, due date and statistics
        QVERIFY(s->undoReset());
        QVERIFY(!s->canUndoReset());
        QCOMPARE(s->cardById(ids[1])->box, before1.box);
        QCOMPARE(s->cardById(ids[1])->dueAt, before1.dueAt);
        QCOMPARE(s->cardById(ids[0])->box, 3);

        // Everything, spread over 5 days, statistics cleared
        QCOMPARE(s->resetCollection(true, 5, true), 10);
        QCOMPARE(s->cardById(ids[2])->box, 1);
        QCOMPARE(s->cardById(ids[3])->lapses, 0);
        QCOMPARE(s->dueCount(), 2); // 10 cards over 5 days: 2 today
        QSet<QDate> days;
        for (int id : std::as_const(ids))
            days.insert(s->cardById(id)->dueAt.date());
        QCOMPARE(days.size(), 5);

        // One box only
        QVERIFY(s->moveCard(ids[4], 2));
        QVERIFY(s->moveCard(ids[5], 2));
        QCOMPARE(s->resetCount(true, 2), 2);
        QCOMPARE(s->resetBox(2), 2);
        QCOMPARE(s->cardById(ids[4])->box, 1);
        QCOMPARE(s->resetBox(1), 0); // Box 1 itself: nothing to do

        // The other learning box was not touched
        s->selectCollection(other);
        QCOMPARE(s->cardById(otherCard)->box, 4);
        s->deleteCollection(lb);
    }

    // Each learning box has its language; German by default.
    void learningLanguagePerBox()
    {
        auto *s = CardStore::instance();
        const int first = s->currentCollection();
        QCOMPARE(s->learningLanguage(), QStringLiteral("de"));
        const int en = s->createCollection(QStringLiteral("English A2"), QStringLiteral("en"));
        const int de = s->createCollection(QStringLiteral("Deutsch B1"));
        s->selectCollection(en);
        QCOMPARE(s->learningLanguage(), QStringLiteral("en"));
        bool found = false;
        for (const QVariant &v : s->collections())
            if (v.toMap().value(QStringLiteral("id")).toInt() == en) {
                QCOMPARE(v.toMap().value(QStringLiteral("language")).toString(), QStringLiteral("en"));
                found = true;
            }
        QVERIFY(found);
        s->selectCollection(de);
        QCOMPARE(s->learningLanguage(), QStringLiteral("de"));
        s->selectCollection(first);
        s->deleteCollection(en);
        s->deleteCollection(de);
    }

    // Meaning language per learning box; never the learning language itself.
    void meaningLanguagePerBox()
    {
        auto *s = CardStore::instance();
        const int first = s->currentCollection();
        QSettings().setValue(QStringLiteral("translation/target"), QStringLiteral("en"));
        QCOMPARE(s->meaningLanguage(), QStringLiteral("en")); // German box: the Settings default
        const int en = s->createCollection(QStringLiteral("English B1"), QStringLiteral("en"));
        const int enDe = s->createCollection(QStringLiteral("English for Germans"), QStringLiteral("en"), QStringLiteral("de"));
        s->selectCollection(en);
        QCOMPARE(s->meaningLanguage(), QStringLiteral("fa"));  // English box: Persian by default
        s->setMeaningLanguage(QStringLiteral("en"));             // not the learning language
        QCOMPARE(s->meaningLanguage(), QStringLiteral("fa"));
        s->setMeaningLanguage(QStringLiteral("de"));
        QCOMPARE(s->meaningLanguage(), QStringLiteral("de"));
        s->selectCollection(enDe);
        QCOMPARE(s->meaningLanguage(), QStringLiteral("de"));
        s->selectCollection(first);
        s->setMeaningLanguage(QStringLiteral("fa"));
        QCOMPARE(s->meaningLanguage(), QStringLiteral("fa"));    // stored, the default no longer matters
        // Regression: a box without a stored choice, whose default already equals the new choice,
        // must still store it and notify (Settings writes the default first, then the box).
        const int fresh = s->createCollection(QStringLiteral("Fresh"));
        s->selectCollection(fresh);
        QSettings().setValue(QStringLiteral("translation/target"), QStringLiteral("en"));
        QCOMPARE(s->meaningLanguage(), QStringLiteral("en")); // from the default
        QSignalSpy spy(s, &CardStore::changed);
        s->setMeaningLanguage(QStringLiteral("en"));
        QCOMPARE(spy.count(), 1);
        QSettings().setValue(QStringLiteral("translation/target"), QStringLiteral("fa"));
        QCOMPARE(s->meaningLanguage(), QStringLiteral("en")); // stored now, not the default
        s->selectCollection(first);
        s->deleteCollection(fresh);
        QSettings().remove(QStringLiteral("translation/target"));
        s->deleteCollection(en);
        s->deleteCollection(enDe);
    }

    // The review direction is remembered per learning box.
    void reviewDirectionPerLearningBox()
    {
        auto *s = CardStore::instance();
        const int first = s->currentCollection();
        QCOMPARE(s->reviewDirection(), QStringLiteral("german")); // default
        s->setReviewDirection(QStringLiteral("meaning"));
        QCOMPARE(s->reviewDirection(), QStringLiteral("meaning"));
        const int other = s->createCollection(QStringLiteral("Direction test"));
        s->selectCollection(other);
        QCOMPARE(s->reviewDirection(), QStringLiteral("german"));
        s->setReviewDirection(QStringLiteral("mixed"));
        s->setReviewDirection(QStringLiteral("nonsense")); // unknown -> German first
        QCOMPARE(s->reviewDirection(), QStringLiteral("german"));
        s->setReviewDirection(QStringLiteral("mixed"));
        s->selectCollection(first);
        QCOMPARE(s->reviewDirection(), QStringLiteral("meaning"));
        s->selectCollection(other);
        QCOMPARE(s->reviewDirection(), QStringLiteral("mixed"));
        s->selectCollection(first);
        s->setReviewDirection(QStringLiteral("german"));
        s->deleteCollection(other);
    }

    void pictureLifecycle()
    {
        auto *s = CardStore::instance();
        const QString first = s->importImage(QUrl::fromLocalFile(makePhoto(QStringLiteral("lb_a.png"))));
        QVERIFY2(!first.isEmpty(), qPrintable(s->lastError()));
        QVERIFY(s->imageUrl(first).isLocalFile());
        QVERIFY(s->imageUrl(QStringLiteral("../evil.jpg")).isEmpty()); // no path traversal

        const int id = s->addCard(QStringLiteral("der Baum"), QStringLiteral("درخت"), QString(), first);
        QVERIFY(id > 0);
        QCOMPARE(s->cardById(id)->image, first);
        QCOMPARE(imageFiles(), 1);

        // discardImage must not delete a picture a card still uses
        s->discardImage(first);
        QCOMPARE(imageFiles(), 1);

        // replacing the picture deletes the old file
        const QString second = s->importImage(QUrl::fromLocalFile(makePhoto(QStringLiteral("lb_b.png"))));
        QVERIFY(s->updateCard(id, QStringLiteral("der Baum"), QStringLiteral("درخت"), QString(), second));
        QVERIFY(!QFile::exists(imageDir() + u'/' + first));
        QCOMPARE(imageFiles(), 1);

        // deleting the card deletes its picture
        QVERIFY(s->removeCard(id));
        QCOMPARE(imageFiles(), 0);
    }

    void rejectsBrokenImage()
    {
        const QString path = QDir::temp().filePath(QStringLiteral("lb_not_an_image.jpg"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a jpeg");
        f.close();
        QVERIFY(CardStore::instance()->importImage(QUrl::fromLocalFile(path)).isEmpty());
        QVERIFY(!CardStore::instance()->lastError().isEmpty());
    }
};

QTEST_MAIN(TstCardStore)
#include "tst_cardstore.moc"
