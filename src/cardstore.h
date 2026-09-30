#pragma once

#include "card.h"

#include <QList>
#include <QSet>
#include <QUrl>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QQmlEngine;
class QJSEngine;

// Owns the SQLite database. Single process-wide instance, exposed to QML as a singleton.
class CardStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList boxCounts READ boxCounts NOTIFY changed)
    Q_PROPERTY(int learnedCount READ learnedCount NOTIFY changed)
    Q_PROPERTY(int dueCount READ dueCount NOTIFY changed)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY changed)
    // Starred cards (★) in the selected learning box. A star alone emits only favoritesChanged
    // (not changed), so card lists are not rebuilt (and do not flicker) when a star is tapped.
    Q_PROPERTY(int favoriteCount READ favoriteCount NOTIFY favoritesChanged)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    // Learning boxes: each has its own boxes 1-5 + Learned; all card functions work on the
    // selected one. `collections` = [{id, name, boxCounts[5], learned, total, due, current}],
    // most recently used first (so the selected one is on top).
    Q_PROPERTY(int currentCollection READ currentCollection WRITE selectCollection NOTIFY changed)
    Q_PROPERTY(QString currentCollectionName READ currentCollectionName NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    // Language being learned in the selected learning box: "de" (German) or "en" (English, US).
    // Speech, text recognition, translation and example sentences follow it.
    Q_PROPERTY(QString learningLanguage READ learningLanguage NOTIFY changed)
    // Language of the meanings in the selected learning box: "fa", "en" or "de" (never the learning
    // language). German boxes: Persian or English; English boxes: Persian or German.
    // Not chosen yet: English boxes Persian, German boxes the Settings default (translation/target).
    Q_PROPERTY(QString meaningLanguage READ meaningLanguage WRITE setMeaningLanguage NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    // True after resetCollection / resetBox until undoReset() or the next app start.
    Q_PROPERTY(bool canUndoReset READ canUndoReset NOTIFY changed)
    // How the selected learning box is reviewed: "german" (German first, the meaning is the answer),
    // "meaning" (meaning first, say the German word) or "mixed". Remembered per learning box.
    Q_PROPERTY(QString reviewDirection READ reviewDirection WRITE setReviewDirection NOTIFY changed)

public:
    static CardStore *instance();
    static CardStore *create(QQmlEngine *, QJSEngine *);

    bool ready() const { return m_ready; }
    QString lastError() const { return m_lastError; }

    QVariantList boxCounts() const; // index 0..4 -> boxes 1..5
    int learnedCount() const;
    int dueCount() const;
    int totalCount() const;
    int favoriteCount() const;

    Q_INVOKABLE int addCard(const QString &front, const QString &back, const QString &example,
                            const QString &image = QString());
    int addCardToDeck(const QString &front, const QString &back, const QString &example, const QString &deck,
                      const QString &image = QString());
    // `image` is required: passing "" removes the card's picture.
    Q_INVOKABLE bool updateCard(int id, const QString &front, const QString &back, const QString &example,
                                const QString &image);

    // Pictures: importImage copies+scales a picked image into app storage and returns its
    // file name ("" on error); imageUrl turns a stored name into a URL for QML Image;
    // discardImage deletes a stored picture unless a card still uses it.
    Q_INVOKABLE QString importImage(const QUrl &source);
    Q_INVOKABLE QUrl imageUrl(const QString &name) const;
    Q_INVOKABLE void discardImage(const QString &name);
    // Camera shots for a card: capture to cameraFilePath(), then importCameraShot() stores it
    // like importImage (turned by `rotation` for landscape shots) and deletes the temporary file.
    Q_INVOKABLE QString cameraFilePath() const;
    Q_INVOKABLE QString importCameraShot(const QString &path, int rotation = 0);
    Q_INVOKABLE bool removeCard(int id);
    // Several cards at once (box page selection); returns how many were deleted
    Q_INVOKABLE int removeCards(const QVariantList &ids);
    // Favorites (★): any card can be starred; the list is per learning box
    Q_INVOKABLE bool isFavorite(int id) const;
    Q_INVOKABLE bool setFavorite(int id, bool favorite);
    // Starred cards of the selected learning box, newest star first: [{id, front, back, example, box, imageUrl}]
    Q_INVOKABLE QVariantList favorites() const;
    Q_INVOKABLE bool resetCard(int id); // back to box 1, due now
    // Manual move to another box (1..5, 6 = Learned), scheduled with that box's interval.
    // Review counts are kept; returns false if the card does not exist.
    Q_INVOKABLE bool moveCard(int id, int box);
    // Cards in one box (1..5, 6 = Learned), due first: [{id, front, back, example, box, dueAt, image}]
    Q_INVOKABLE QVariantList cardsInBox(int box) const;
    Q_INVOKABLE QVariantMap card(int id) const;
    Q_INVOKABLE int findByFront(const QString &front) const; // -1 if none
    Q_INVOKABLE void refresh() { emit changed(); }

    int currentCollection() const { return m_collection; }
    QString currentCollectionName() const;
    QVariantList collections() const; // [{id, name, language, meaning, boxCounts, learned, total, due, current}]
    QString learningLanguage() const;
    QString meaningLanguage() const;
    void setMeaningLanguage(const QString &language);
    // New, empty learning box (not selected yet) for `language` ("de" or "en") with meanings in
    // `meaning` ("" = default). Returns its id, -1 on error.
    Q_INVOKABLE int createCollection(const QString &name, const QString &language = QStringLiteral("de"),
                                     const QString &meaning = QString());
    Q_INVOKABLE bool renameCollection(int id, const QString &name);
    // Deletes a learning box with all its cards; the last remaining one cannot be deleted.
    Q_INVOKABLE bool deleteCollection(int id);
    // Makes it the current learning box, first in the list, remembered for the next start.
    Q_INVOKABLE void selectCollection(int id);

    // ---- Start over ----
    // Puts cards of the current learning box back into Box 1: all of them (resetCollection;
    // Learned cards only with includeLearned) or those of one box (resetBox, 2..5 or 6 = Learned).
    // spreadDays > 1 spreads the due dates over that many days (today first, card order kept);
    // clearStats also sets reviews and mistakes to 0. The previous state is kept for undoReset().
    // Returns the number of cards reset, -1 on error.
    Q_INVOKABLE int resetCollection(bool includeLearned = true, int spreadDays = 1, bool clearStats = false);
    Q_INVOKABLE int resetBox(int box, int spreadDays = 1, bool clearStats = false);
    // How many cards a reset would change (box 0 = whole learning box).
    Q_INVOKABLE int resetCount(bool includeLearned = true, int box = 0) const;
    Q_INVOKABLE bool undoReset();
    bool canUndoReset() const { return m_canUndoReset; }

    QString reviewDirection() const;
    void setReviewDirection(const QString &direction);

    // C++ API
    std::optional<Card> cardById(int id) const;
    QList<Card> allCards() const;
    QList<int> dueCardIds(int limit = 0) const;
    bool recordAnswer(int id, bool correct);
    // Bulk insert in one transaction; skips fronts already in the box (case-insensitive).
    // Returns the number of cards inserted, or -1 on error.
    int addCards(const QList<Card> &cards, const QString &deck);
    QSet<QString> frontKeys() const; // case-folded fronts of all cards
    // Import: inserts complete cards (box, dueAt, reviews, lapses, image, deck) in one
    // transaction; fronts already in the box are skipped. Returns the number inserted, -1 on error.
    int insertCards(const QList<Card> &cards);
    // Stores picture bytes (JPEG/PNG) like importImage; returns the file name or "".
    QString storeImageData(const QByteArray &bytes);
    // Folder with the card pictures (export reads them from here).
    QString imageDir() const { return m_imageDir; }

signals:
    void changed();
    void favoritesChanged(); // a star was set / removed; also emitted with every changed()
    void errorOccurred(const QString &message);

private:
    explicit CardStore(QObject *parent = nullptr);
    bool open();
    bool migrate();
    void fail(const QString &where, const QString &what);
    int scalar(const QString &sql) const;
    void removeOrphanImages();
    QString scope() const; // " AND collection_id = <current>" for card queries
    bool collectionExists(int id) const;
    int resetWhere(const QString &where, int spreadDays, bool clearStats);

    bool m_ready = false;
    QString m_lastError;
    QString m_imageDir;
    int m_collection = 1;
    bool m_canUndoReset = false;
};
