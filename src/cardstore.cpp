#include "cardstore.h"
#include "leitner.h"
#include "cardimages.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>
#include <QSqlDatabase>
#include <QTemporaryFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>
#include <QUrl>
#include <QUuid>

namespace {

constexpr auto kConnection = "learningbox";
constexpr int kSchemaVersion = 7;
constexpr auto kCurrentKey = "learningbox/current"; // QSettings: selected learning box

QSqlDatabase db() { return QSqlDatabase::database(QLatin1String(kConnection)); }

qint64 nowSecs() { return QDateTime::currentSecsSinceEpoch(); }

// QSqlQuery binds a null QString as SQL NULL, which violates the NOT NULL text columns.
// Always bind text through this so "" stays "".
QString text(const QString &s) { return s.isNull() ? QStringLiteral("") : s; }

// Due dates are day-aligned (local midnight) so a card reviewed late in the
// evening is due again at the start of the target day, not 24 h later.
QVariant dueFor(int intervalDays)
{
    if (intervalDays < 0)
        return QVariant(); // NULL: learned, not scheduled
    if (intervalDays == 0)
        return nowSecs();
    return QDate::currentDate().addDays(intervalDays).startOfDay().toSecsSinceEpoch();
}

Card readCard(const QSqlQuery &q)
{
    Card c;
    c.id = q.value(0).toInt();
    c.front = q.value(1).toString();
    c.back = q.value(2).toString();
    c.example = q.value(3).toString();
    c.box = q.value(4).toInt();
    if (!q.value(5).isNull())
        c.dueAt = QDateTime::fromSecsSinceEpoch(q.value(5).toLongLong());
    c.reviews = q.value(6).toInt();
    c.lapses = q.value(7).toInt();
    c.deck = q.value(8).toString();
    c.image = q.value(9).toString();
    return c;
}

constexpr auto kCardColumns = "id, front, back, example, box, due_at, reviews, lapses, deck, image";

} // namespace

CardStore *CardStore::instance()
{
    static CardStore *s = new CardStore(QCoreApplication::instance());
    return s;
}

CardStore *CardStore::create(QQmlEngine *, QJSEngine *engine)
{
    auto *s = instance();
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

CardStore::CardStore(QObject *parent)
    : QObject(parent)
{
    m_ready = open() && migrate();
    if (m_ready) {
        removeOrphanImages();
        QSqlQuery(db()).exec(QStringLiteral("DELETE FROM reset_undo")); // undo lasts one session
        // The learning box used last (it is also first in the list).
        const int saved = QSettings().value(QLatin1String(kCurrentKey), 0).toInt();
        m_collection = saved > 0 && collectionExists(saved)
            ? saved
            : scalar(QStringLiteral("SELECT id FROM collections ORDER BY last_used_at DESC, id ASC LIMIT 1"));
    }
}

void CardStore::fail(const QString &where, const QString &what)
{
    m_lastError = where + QStringLiteral(": ") + what;
    qWarning().noquote() << "CardStore" << m_lastError;
    emit errorOccurred(m_lastError);
}

bool CardStore::open()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_imageDir = dir + QStringLiteral("/images");
    if (!QDir().mkpath(dir)) {
        fail(QStringLiteral("open"), QStringLiteral("cannot create ") + dir);
        return false;
    }
    auto d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kConnection));
    d.setDatabaseName(dir + QStringLiteral("/learningbox.sqlite"));
    if (!d.open()) {
        fail(QStringLiteral("open"), d.lastError().text());
        return false;
    }
    QSqlQuery(d).exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    return true;
}

bool CardStore::migrate()
{
    auto d = db();
    QSqlQuery q(d);
    q.exec(QStringLiteral("PRAGMA user_version"));
    const int version = q.next() ? q.value(0).toInt() : 0;
    if (version >= kSchemaVersion)
        return true;

    // Each step upgrades from the previous version; a fresh DB runs all of them.
    QStringList steps;
    if (version < 1) {
        steps << QStringLiteral("CREATE TABLE IF NOT EXISTS cards ("
                                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                " front TEXT NOT NULL,"
                                " back TEXT NOT NULL DEFAULT '',"
                                " example TEXT NOT NULL DEFAULT '',"
                                " box INTEGER NOT NULL DEFAULT 1,"
                                " due_at INTEGER,"
                                " reviews INTEGER NOT NULL DEFAULT 0,"
                                " lapses INTEGER NOT NULL DEFAULT 0,"
                                " created_at INTEGER NOT NULL,"
                                " updated_at INTEGER NOT NULL)")
              << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cards_due ON cards(box, due_at)");
    }
    if (version < 2) {
        // v2: origin of a card, e.g. "Netzwerk neu A1 · K3" for word-pack imports.
        steps << QStringLiteral("ALTER TABLE cards ADD COLUMN deck TEXT NOT NULL DEFAULT ''");
    }
    if (version < 3) {
        // v3: optional picture, file name inside AppDataLocation/images
        steps << QStringLiteral("ALTER TABLE cards ADD COLUMN image TEXT NOT NULL DEFAULT ''");
    }
    if (version < 4) {
        // v4: several learning boxes ("collections"), each with its own boxes 1-5 + Learned.
        // Existing cards go into a first learning box.
        const qint64 now = nowSecs();
        steps << QStringLiteral("CREATE TABLE IF NOT EXISTS collections ("
                                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                " name TEXT NOT NULL,"
                                " created_at INTEGER NOT NULL,"
                                " last_used_at INTEGER NOT NULL)")
              << QStringLiteral("INSERT INTO collections (id, name, created_at, last_used_at) VALUES (1, '%1', %2, %2)")
                     .arg(QCoreApplication::translate("CardStore", "My learning box").replace(u'\'', QLatin1String("''")))
                     .arg(now)
              << QStringLiteral("ALTER TABLE cards ADD COLUMN collection_id INTEGER NOT NULL DEFAULT 1")
              << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cards_collection ON cards(collection_id, box, due_at)");
    }
    if (version < 5) {
        // v5: state before the last "start over", so it can be undone.
        steps << QStringLiteral("CREATE TABLE IF NOT EXISTS reset_undo ("
                                " card_id INTEGER PRIMARY KEY,"
                                " box INTEGER NOT NULL,"
                                " due_at INTEGER,"
                                " reviews INTEGER NOT NULL,"
                                " lapses INTEGER NOT NULL)");
    }
    if (version < 6) {
        // v6: the language learned in each learning box ("de" German, "en" English).
        steps << QStringLiteral("ALTER TABLE collections ADD COLUMN language TEXT NOT NULL DEFAULT 'de'");
    }
    if (version < 7) {
        // v7: meaning language of each learning box ("" = not chosen: default).
        steps << QStringLiteral("ALTER TABLE collections ADD COLUMN meaning TEXT NOT NULL DEFAULT ''");
    }
    steps << QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion);

    d.transaction();
    for (const auto &sql : std::as_const(steps)) {
        if (!q.exec(sql)) {
            fail(QStringLiteral("migrate"), q.lastError().text());
            d.rollback();
            return false;
        }
    }
    return d.commit();
}

int CardStore::scalar(const QString &sql) const
{
    QSqlQuery q(db());
    if (q.exec(sql) && q.next())
        return q.value(0).toInt();
    return 0;
}

QVariantList CardStore::boxCounts() const
{
    QVariantList counts;
    for (int i = leitner::kFirstBox; i <= leitner::kLastBox; ++i)
        counts << 0;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT box, COUNT(*) FROM cards WHERE box BETWEEN 1 AND 5%1 GROUP BY box").arg(scope()));
    while (q.next())
        counts[q.value(0).toInt() - 1] = q.value(1).toInt();
    return counts;
}

int CardStore::learnedCount() const
{
    return scalar(QStringLiteral("SELECT COUNT(*) FROM cards WHERE box > 5%1").arg(scope()));
}

int CardStore::totalCount() const
{
    return scalar(QStringLiteral("SELECT COUNT(*) FROM cards WHERE 1%1").arg(scope()));
}

int CardStore::dueCount() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM cards WHERE box BETWEEN 1 AND 5 AND due_at <= ?%1").arg(scope()));
    q.addBindValue(nowSecs());
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

int CardStore::addCard(const QString &front, const QString &back, const QString &example,
                       const QString &image)
{
    return addCardToDeck(front, back, example, QString(), image);
}

int CardStore::addCardToDeck(const QString &front, const QString &back, const QString &example,
                             const QString &deck, const QString &image)
{
    const QString f = front.trimmed();
    if (f.isEmpty())
        return -1;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO cards (front, back, example, deck, image, box, due_at, created_at, updated_at,"
                             " collection_id) VALUES (?, ?, ?, ?, ?, 1, ?, ?, ?, ?)"));
    const qint64 now = nowSecs();
    q.addBindValue(f);
    q.addBindValue(text(back.trimmed()));
    q.addBindValue(text(example.trimmed()));
    q.addBindValue(text(deck));
    q.addBindValue(text(image));
    q.addBindValue(now); // new cards are due immediately
    q.addBindValue(now);
    q.addBindValue(now);
    q.addBindValue(m_collection);
    if (!q.exec()) {
        fail(QStringLiteral("addCard"), q.lastError().text());
        return -1;
    }
    emit changed();
    return q.lastInsertId().toInt();
}

bool CardStore::updateCard(int id, const QString &front, const QString &back, const QString &example,
                           const QString &image)
{
    const QString f = front.trimmed();
    if (f.isEmpty())
        return false;
    const auto old = cardById(id);
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE cards SET front = ?, back = ?, example = ?, image = ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(f);
    q.addBindValue(text(back.trimmed()));
    q.addBindValue(text(example.trimmed()));
    q.addBindValue(text(image));
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) {
        fail(QStringLiteral("updateCard"), q.lastError().text());
        return false;
    }
    if (old && old->image != image)
        discardImage(old->image); // picture replaced or removed
    emit changed();
    return q.numRowsAffected() > 0;
}

bool CardStore::removeCard(int id)
{
    const auto old = cardById(id);
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM cards WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        fail(QStringLiteral("removeCard"), q.lastError().text());
        return false;
    }
    if (old)
        discardImage(old->image);
    emit changed();
    return q.numRowsAffected() > 0;
}

bool CardStore::resetCard(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE cards SET box = 1, due_at = ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(nowSecs());
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) {
        fail(QStringLiteral("resetCard"), q.lastError().text());
        return false;
    }
    emit changed();
    return true;
}

bool CardStore::moveCard(int id, int box)
{
    const auto step = leitner::moveTo(box);
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE cards SET box = ?, due_at = ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(step.box);
    q.addBindValue(dueFor(step.intervalDays));
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) {
        fail(QStringLiteral("moveCard"), q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() <= 0)
        return false;
    emit changed();
    return true;
}

QVariantList CardStore::cardsInBox(int box) const
{
    QVariantList out;
    QSqlQuery q(db());
    // Learned cards have no due date: newest first. Active boxes: soonest due first.
    q.prepare(QStringLiteral("SELECT %1 FROM cards WHERE box = ?%2 ORDER BY due_at IS NULL, due_at ASC, updated_at DESC, id ASC")
                  .arg(QLatin1String(kCardColumns), scope()));
    q.addBindValue(leitner::moveTo(box).box);
    if (!q.exec())
        return out;
    while (q.next()) {
        const Card c = readCard(q);
        out.append(QVariantMap{
            {QStringLiteral("id"), c.id},
            {QStringLiteral("front"), c.front},
            {QStringLiteral("back"), c.back},
            {QStringLiteral("example"), c.example},
            {QStringLiteral("box"), c.box},
            {QStringLiteral("dueAt"), c.dueAt},
            {QStringLiteral("reviews"), c.reviews},
            {QStringLiteral("imageUrl"), imageUrl(c.image)},
        });
    }
    return out;
}

std::optional<Card> CardStore::cardById(int id) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT %1 FROM cards WHERE id = ?").arg(QLatin1String(kCardColumns)));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return readCard(q);
    return std::nullopt;
}

QVariantMap CardStore::card(int id) const
{
    const auto c = cardById(id);
    if (!c)
        return {};
    return {
        {QStringLiteral("id"), c->id},
        {QStringLiteral("front"), c->front},
        {QStringLiteral("back"), c->back},
        {QStringLiteral("example"), c->example},
        {QStringLiteral("box"), c->box},
        {QStringLiteral("dueAt"), c->dueAt},
        {QStringLiteral("reviews"), c->reviews},
        {QStringLiteral("lapses"), c->lapses},
        {QStringLiteral("deck"), c->deck},
        {QStringLiteral("image"), c->image},
    };
}

int CardStore::findByFront(const QString &front) const
{
    // Case-insensitive, Unicode-aware compare in C++ (SQLite's NOCASE is ASCII-only).
    const QString needle = front.trimmed();
    if (needle.isEmpty())
        return -1;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT id, front FROM cards WHERE 1%1").arg(scope()));
    while (q.next()) {
        if (q.value(1).toString().trimmed().toCaseFolded() == needle.toCaseFolded())
            return q.value(0).toInt();
    }
    return -1;
}

QList<Card> CardStore::allCards() const
{
    QList<Card> out;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT %1 FROM cards WHERE 1%2 ORDER BY created_at DESC, id ASC").arg(QLatin1String(kCardColumns), scope()));
    while (q.next())
        out << readCard(q);
    return out;
}

QList<int> CardStore::dueCardIds(int limit) const
{
    QList<int> out;
    QSqlQuery q(db());
    QString sql = QStringLiteral("SELECT id FROM cards WHERE box BETWEEN 1 AND 5 AND due_at <= ?%1"
                                 " ORDER BY box ASC, due_at ASC, id ASC").arg(scope());
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    q.prepare(sql);
    q.addBindValue(nowSecs());
    if (q.exec())
        while (q.next())
            out << q.value(0).toInt();
    return out;
}

bool CardStore::recordAnswer(int id, bool correct)
{
    const auto c = cardById(id);
    if (!c)
        return false;
    const auto step = leitner::next(c->box, correct);

    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE cards SET box = ?, due_at = ?, reviews = reviews + 1,"
                             " lapses = lapses + ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(step.box);
    q.addBindValue(dueFor(step.intervalDays));
    q.addBindValue(correct ? 0 : 1);
    q.addBindValue(nowSecs());
    q.addBindValue(id);
    if (!q.exec()) {
        fail(QStringLiteral("recordAnswer"), q.lastError().text());
        return false;
    }
    emit changed();
    return true;
}

QSet<QString> CardStore::frontKeys() const
{
    QSet<QString> keys;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT front FROM cards WHERE 1%1").arg(scope()));
    while (q.next())
        keys.insert(q.value(0).toString().trimmed().toCaseFolded());
    return keys;
}

int CardStore::addCards(const QList<Card> &cards, const QString &deck)
{
    QSet<QString> existing = frontKeys();
    auto d = db();
    d.transaction();
    QSqlQuery q(d);
    q.prepare(QStringLiteral("INSERT INTO cards (front, back, example, deck, box, due_at, created_at, updated_at, collection_id)"
                             " VALUES (?, ?, ?, ?, 1, ?, ?, ?, ?)"));
    const qint64 now = nowSecs();
    int inserted = 0;
    for (const Card &c : cards) {
        const QString f = c.front.trimmed();
        const QString key = f.toCaseFolded();
        if (f.isEmpty() || existing.contains(key))
            continue; // never duplicate a card the learner already has
        q.addBindValue(f);
        q.addBindValue(text(c.back.trimmed()));
        q.addBindValue(text(c.example.trimmed()));
        q.addBindValue(text(deck));
        q.addBindValue(now);
        q.addBindValue(now);
        q.addBindValue(now);
        q.addBindValue(m_collection);
        if (!q.exec()) {
            fail(QStringLiteral("addCards"), q.lastError().text());
            d.rollback();
            return -1;
        }
        existing.insert(key);
        ++inserted;
    }
    d.commit();
    if (inserted > 0)
        emit changed();
    return inserted;
}

int CardStore::insertCards(const QList<Card> &cards)
{
    QSet<QString> existing = frontKeys();
    auto d = db();
    d.transaction();
    QSqlQuery q(d);
    q.prepare(QStringLiteral("INSERT INTO cards (front, back, example, deck, image, box, due_at, reviews, lapses,"
                             " created_at, updated_at, collection_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    const qint64 now = nowSecs();
    int inserted = 0;
    for (const Card &c : cards) {
        const QString f = c.front.trimmed();
        const QString key = f.toCaseFolded();
        if (f.isEmpty() || existing.contains(key))
            continue;
        const int box = qBound(leitner::kFirstBox, c.box, leitner::kLearnedBox);
        QVariant due;                                   // learned: not scheduled
        if (leitner::isActive(box))
            due = c.dueAt.isValid() ? QVariant(c.dueAt.toSecsSinceEpoch()) : QVariant(now);
        q.addBindValue(f);
        q.addBindValue(text(c.back.trimmed()));
        q.addBindValue(text(c.example.trimmed()));
        q.addBindValue(text(c.deck));
        q.addBindValue(text(c.image));
        q.addBindValue(box);
        q.addBindValue(due);
        q.addBindValue(qMax(0, c.reviews));
        q.addBindValue(qMax(0, c.lapses));
        q.addBindValue(now);
        q.addBindValue(now);
        q.addBindValue(m_collection);
        if (!q.exec()) {
            fail(QStringLiteral("insertCards"), q.lastError().text());
            d.rollback();
            return -1;
        }
        existing.insert(key);
        ++inserted;
    }
    d.commit();
    if (inserted > 0)
        emit changed();
    return inserted;
}

QString CardStore::storeImageData(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return {};
    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/lb-import-XXXXXX.img"));
    if (!tmp.open() || tmp.write(bytes) != bytes.size())
        return {};
    tmp.flush();
    QString error;
    const QString name = cardimages::store(tmp.fileName(), m_imageDir, &error);
    if (name.isEmpty())
        qWarning().noquote() << "import picture:" << error;
    return name;
}

// ---- Learning boxes (collections) ----

QString CardStore::reviewDirection() const
{
    const QString d = QSettings().value(QStringLiteral("review/direction/%1").arg(m_collection)).toString();
    return d == QLatin1String("meaning") || d == QLatin1String("mixed") ? d : QStringLiteral("german");
}

void CardStore::setReviewDirection(const QString &direction)
{
    const QString d = direction == QLatin1String("meaning") || direction == QLatin1String("mixed")
        ? direction : QStringLiteral("german");
    if (d == reviewDirection())
        return;
    QSettings().setValue(QStringLiteral("review/direction/%1").arg(m_collection), d);
    emit changed();
}

int CardStore::resetCount(bool includeLearned, int box) const
{
    const QString where = box > 0 ? QStringLiteral("box = %1").arg(leitner::moveTo(box).box)
                                  : (includeLearned ? QStringLiteral("1") : QStringLiteral("box BETWEEN 1 AND 5"));
    return scalar(QStringLiteral("SELECT COUNT(*) FROM cards WHERE %1%2").arg(where, scope()));
}

int CardStore::resetCollection(bool includeLearned, int spreadDays, bool clearStats)
{
    return resetWhere(includeLearned ? QStringLiteral("1") : QStringLiteral("box BETWEEN 1 AND 5"),
                      spreadDays, clearStats);
}

int CardStore::resetBox(int box, int spreadDays, bool clearStats)
{
    const int b = leitner::moveTo(box).box;
    if (b <= leitner::kFirstBox)
        return 0;
    return resetWhere(QStringLiteral("box = %1").arg(b), spreadDays, clearStats);
}

int CardStore::resetWhere(const QString &where, int spreadDays, bool clearStats)
{
    auto d = db();
    QSqlQuery q(d);
    // Card order is kept: lower boxes and sooner due dates are asked first.
    if (!q.exec(QStringLiteral("SELECT id FROM cards WHERE %1%2 ORDER BY box ASC, due_at IS NULL, due_at ASC, id ASC")
                    .arg(where, scope()))) {
        fail(QStringLiteral("reset"), q.lastError().text());
        return -1;
    }
    QList<int> ids;
    while (q.next())
        ids.append(q.value(0).toInt());
    if (ids.isEmpty())
        return 0;

    const int days = qMax(1, spreadDays);
    const qint64 now = nowSecs();
    const QDate today = QDate::currentDate();
    d.transaction();
    bool ok = q.exec(QStringLiteral("DELETE FROM reset_undo"))
              && q.exec(QStringLiteral("INSERT INTO reset_undo (card_id, box, due_at, reviews, lapses)"
                                       " SELECT id, box, due_at, reviews, lapses FROM cards WHERE %1%2")
                            .arg(where, scope()));
    QSqlQuery u(d);
    u.prepare(clearStats ? QStringLiteral("UPDATE cards SET box = 1, due_at = ?, reviews = 0, lapses = 0, updated_at = ? WHERE id = ?")
                         : QStringLiteral("UPDATE cards SET box = 1, due_at = ?, updated_at = ? WHERE id = ?"));
    for (int i = 0; ok && i < ids.size(); ++i) {
        // Day 0 = due now; the others at the start of their day, evenly spread.
        const int day = int(qint64(i) * days / ids.size());
        u.addBindValue(day == 0 ? now : today.addDays(day).startOfDay().toSecsSinceEpoch());
        u.addBindValue(now);
        u.addBindValue(ids.at(i));
        ok = u.exec();
    }
    if (!ok) {
        fail(QStringLiteral("reset"), (u.lastError().isValid() ? u.lastError() : q.lastError()).text());
        d.rollback();
        return -1;
    }
    d.commit();
    m_canUndoReset = true;
    emit changed();
    return int(ids.size());
}

bool CardStore::undoReset()
{
    auto d = db();
    QSqlQuery q(d);
    d.transaction();
    const bool ok = q.exec(QStringLiteral(
                        "UPDATE cards SET"
                        " box = (SELECT r.box FROM reset_undo r WHERE r.card_id = cards.id),"
                        " due_at = (SELECT r.due_at FROM reset_undo r WHERE r.card_id = cards.id),"
                        " reviews = (SELECT r.reviews FROM reset_undo r WHERE r.card_id = cards.id),"
                        " lapses = (SELECT r.lapses FROM reset_undo r WHERE r.card_id = cards.id)"
                        " WHERE id IN (SELECT card_id FROM reset_undo)"))
                    && q.exec(QStringLiteral("DELETE FROM reset_undo"));
    if (!ok) {
        fail(QStringLiteral("undoReset"), q.lastError().text());
        d.rollback();
        return false;
    }
    d.commit();
    m_canUndoReset = false;
    emit changed();
    return true;
}

QString CardStore::scope() const
{
    return QStringLiteral(" AND collection_id = %1").arg(m_collection);
}

bool CardStore::collectionExists(int id) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT 1 FROM collections WHERE id = ?"));
    q.addBindValue(id);
    return q.exec() && q.next();
}

QString CardStore::currentCollectionName() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT name FROM collections WHERE id = ?"));
    q.addBindValue(m_collection);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

QVariantList CardStore::collections() const
{
    // Counts per learning box and box in one pass.
    struct Counts { QVariantList boxes{0, 0, 0, 0, 0}; int learned = 0, total = 0, due = 0; };
    QHash<int, Counts> counts;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT collection_id, box, COUNT(*),"
                             " SUM(CASE WHEN box BETWEEN 1 AND 5 AND due_at <= ? THEN 1 ELSE 0 END)"
                             " FROM cards GROUP BY collection_id, box"));
    q.addBindValue(nowSecs());
    if (q.exec()) {
        while (q.next()) {
            Counts &c = counts[q.value(0).toInt()];
            const int box = q.value(1).toInt(), n = q.value(2).toInt();
            if (leitner::isActive(box))
                c.boxes[box - 1] = n;
            else if (box > leitner::kLastBox)
                c.learned += n;
            c.total += n;
            c.due += q.value(3).toInt();
        }
    }
    QVariantList out;
    // Most recently used first; the selected one is always the most recent.
    q.exec(QStringLiteral("SELECT id, name, language, meaning FROM collections ORDER BY last_used_at DESC, id ASC"));
    while (q.next()) {
        const int id = q.value(0).toInt();
        const Counts c = counts.value(id);
        out.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), q.value(1).toString()},
            {QStringLiteral("language"), q.value(2).toString()},
            {QStringLiteral("meaning"), q.value(3).toString()}, // "" = default
            {QStringLiteral("boxCounts"), c.boxes},
            {QStringLiteral("learned"), c.learned},
            {QStringLiteral("total"), c.total},
            {QStringLiteral("due"), c.due},
            {QStringLiteral("current"), id == m_collection},
        });
    }
    return out;
}

QString CardStore::learningLanguage() const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT language FROM collections WHERE id = ?"));
    q.addBindValue(m_collection);
    return q.exec() && q.next() && q.value(0).toString() == QLatin1String("en") ? QStringLiteral("en")
                                                                              : QStringLiteral("de");
}

namespace {
bool isMeaningLanguage(const QString &l)
{
    return l == QLatin1String("fa") || l == QLatin1String("en") || l == QLatin1String("de");
}
} // namespace

QString CardStore::meaningLanguage() const
{
    const QString learn = learningLanguage();
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT meaning FROM collections WHERE id = ?"));
    q.addBindValue(m_collection);
    const QString m = q.exec() && q.next() ? q.value(0).toString() : QString();
    if (isMeaningLanguage(m) && m != learn)
        return m;
    if (learn == QLatin1String("en"))
        return QStringLiteral("fa");
    return QSettings().value(QStringLiteral("translation/target")).toString() == QLatin1String("en")
        ? QStringLiteral("en") : QStringLiteral("fa");
}

void CardStore::setMeaningLanguage(const QString &language)
{
    if (!isMeaningLanguage(language) || language == learningLanguage())
        return;
    // Compare with what is stored, not with the resolved default: a box that has no choice yet
    // follows the Settings default, which the caller may just have changed to this language too.
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT meaning FROM collections WHERE id = ?"));
    q.addBindValue(m_collection);
    if (q.exec() && q.next() && q.value(0).toString() == language)
        return;
    q.prepare(QStringLiteral("UPDATE collections SET meaning = ? WHERE id = ?"));
    q.addBindValue(language);
    q.addBindValue(m_collection);
    if (!q.exec()) {
        fail(QStringLiteral("setMeaningLanguage"), q.lastError().text());
        return;
    }
    emit changed();
}

int CardStore::createCollection(const QString &name, const QString &language, const QString &meaning)
{
    const QString n = name.simplified();
    if (n.isEmpty())
        return -1;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO collections (name, created_at, last_used_at, language, meaning) VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(n);
    q.addBindValue(nowSecs());
    q.addBindValue(0); // not used yet: goes to the end until it is selected
    const QString learn = language == QLatin1String("en") ? QStringLiteral("en") : QStringLiteral("de");
    q.addBindValue(learn);
    q.addBindValue(isMeaningLanguage(meaning) && meaning != learn ? meaning : QStringLiteral("")); // "" = default
    if (!q.exec()) {
        fail(QStringLiteral("createCollection"), q.lastError().text());
        return -1;
    }
    emit changed();
    return q.lastInsertId().toInt();
}

bool CardStore::renameCollection(int id, const QString &name)
{
    const QString n = name.simplified();
    if (n.isEmpty())
        return false;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE collections SET name = ? WHERE id = ?"));
    q.addBindValue(n);
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() <= 0)
        return false;
    emit changed();
    return true;
}

bool CardStore::deleteCollection(int id)
{
    if (!collectionExists(id) || scalar(QStringLiteral("SELECT COUNT(*) FROM collections")) <= 1)
        return false; // there is always at least one learning box
    auto d = db();
    d.transaction();
    QSqlQuery q(d);
    q.prepare(QStringLiteral("DELETE FROM cards WHERE collection_id = ?"));
    q.addBindValue(id);
    const bool okCards = q.exec();
    q.prepare(QStringLiteral("DELETE FROM collections WHERE id = ?"));
    q.addBindValue(id);
    if (!okCards || !q.exec()) {
        fail(QStringLiteral("deleteCollection"), q.lastError().text());
        d.rollback();
        return false;
    }
    d.commit();
    removeOrphanImages(); // pictures no other card uses
    if (id == m_collection)
        selectCollection(scalar(QStringLiteral("SELECT id FROM collections ORDER BY last_used_at DESC, id ASC LIMIT 1")));
    else
        emit changed();
    return true;
}

void CardStore::selectCollection(int id)
{
    if (!collectionExists(id))
        return;
    m_collection = id;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE collections SET last_used_at = ? WHERE id = ?"));
    // Strictly increasing, so the latest selection wins even within the same second.
    q.addBindValue(qMax(nowSecs(), qint64(scalar(QStringLiteral("SELECT MAX(last_used_at) FROM collections"))) + 1));
    q.addBindValue(id);
    q.exec();
    QSettings().setValue(QLatin1String(kCurrentKey), id);
    emit changed();
}

// ---- Pictures ----

QString CardStore::importImage(const QUrl &source)
{
    // Local files come as file:// URLs; Android's picker returns content:// URIs,
    // which QFile opens directly on Android.
    const QString path = source.isLocalFile() ? source.toLocalFile() : source.toString();
    QString error;
    const QString name = cardimages::store(path, m_imageDir, &error);
    if (name.isEmpty())
        fail(QStringLiteral("importImage"), error);
    return name;
}

QString CardStore::cameraFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/card-shot-") + QUuid::createUuid().toString(QUuid::Id128)
           + QStringLiteral(".jpg");
}

QString CardStore::importCameraShot(const QString &path, int rotation)
{
    QString local = path;
    if (local.startsWith(QStringLiteral("file:")))
        local = QUrl(path).toLocalFile();
    QString error;
    const QString name = cardimages::store(local, m_imageDir, &error, rotation);
    QFile::remove(local);
    if (name.isEmpty())
        fail(QStringLiteral("importCameraShot"), error);
    return name;
}

QUrl CardStore::imageUrl(const QString &name) const
{
    if (name.isEmpty() || name.contains(u'/') || name.contains(u'\\'))
        return {};
    return QUrl::fromLocalFile(m_imageDir + u'/' + name);
}

void CardStore::discardImage(const QString &name)
{
    if (name.isEmpty() || name.contains(u'/') || name.contains(u'\\'))
        return;
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM cards WHERE image = ?"));
    q.addBindValue(name);
    if (q.exec() && q.next() && q.value(0).toInt() > 0)
        return; // still used by a card
    QFile::remove(m_imageDir + u'/' + name);
}

void CardStore::removeOrphanImages()
{
    // Pictures picked in the editor but never saved (editor left with Back) are removed here.
    QDir dir(m_imageDir);
    if (!dir.exists())
        return;
    QSet<QString> used;
    QSqlQuery q(db());
    q.exec(QStringLiteral("SELECT image FROM cards WHERE image <> ''"));
    while (q.next())
        used.insert(q.value(0).toString());
    const auto files = dir.entryList(QDir::Files);
    for (const QString &f : files)
        if (!used.contains(f))
            dir.remove(f);
}
