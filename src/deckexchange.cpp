#include "deckexchange.h"
#include "cardstore.h"
#include "leitner.h"

#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace {
constexpr qint64 kMaxDownload = 200 * 1024 * 1024; // Anki decks with audio can be large

QString localPath(const QUrl &url) { return url.isLocalFile() ? url.toLocalFile() : url.toString(); }

QString tempDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/import");
}
} // namespace

DeckExchange *DeckExchange::create(QQmlEngine *, QJSEngine *engine)
{
    static DeckExchange *s = new DeckExchange(QCoreApplication::instance());
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

DeckExchange::DeckExchange(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setTransferTimeout(60000);
}

void DeckExchange::setBusy(bool b)
{
    if (b != m_busy) {
        m_busy = b;
        emit busyChanged();
    }
}

namespace {
#ifdef Q_OS_ANDROID
// ACTION_SEND for a file in the app's files folder, through the FileProvider declared in
// AndroidManifest.xml ("<package>.qtprovider", paths in res/xml/qtprovider_paths.xml). C++ only.
bool androidShare(const QString &path, const QString &mime, const QString &title)
{
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;
    QJniEnvironment env;
    const QJniObject file("java/io/File", "(Ljava/lang/String;)V", QJniObject::fromString(path).object<jstring>());
    const QString package = context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "androidx/core/content/FileProvider", "getUriForFile",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)Landroid/net/Uri;",
        context.object(), QJniObject::fromString(package + QStringLiteral(".qtprovider")).object<jstring>(), file.object());
    if (env.checkAndClearExceptions() || !uri.isValid())
        return false;
    const QJniObject action = QJniObject::getStaticObjectField("android/content/Intent", "ACTION_SEND", "Ljava/lang/String;");
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V", action.object<jstring>());
    intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                            QJniObject::fromString(mime).object<jstring>());
    const QJniObject extraStream = QJniObject::getStaticObjectField("android/content/Intent", "EXTRA_STREAM", "Ljava/lang/String;");
    intent.callObjectMethod("putExtra", "(Ljava/lang/String;Landroid/os/Parcelable;)Landroid/content/Intent;",
                            extraStream.object<jstring>(), uri.object());
    const jint grantRead = QJniObject::getStaticField<jint>("android/content/Intent", "FLAG_GRANT_READ_URI_PERMISSION");
    intent.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", grantRead);
    QJniObject chooser = QJniObject::callStaticObjectMethod(
        "android/content/Intent", "createChooser",
        "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
        intent.object(), QJniObject::fromString(title).object<jstring>());
    const jint newTask = QJniObject::getStaticField<jint>("android/content/Intent", "FLAG_ACTIVITY_NEW_TASK");
    chooser.callObjectMethod("addFlags", "(I)Landroid/content/Intent;", newTask);
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", chooser.object());
    return !env.checkAndClearExceptions();
}
#endif
} // namespace

QVariantMap DeckExchange::shareCards(const QString &format, bool withProgress, int box)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/shared");
    QDir(dir).removeRecursively(); // only the file being shared now
    if (!QDir().mkpath(dir))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), tr("Cannot create %1").arg(dir)}};
    const QString path = dir + u'/' + suggestedFileName(format);
    QVariantMap r = exportCards(QUrl::fromLocalFile(path), format, withProgress, box);
    if (!r.value(QStringLiteral("ok")).toBool())
        return r;
    r.insert(QStringLiteral("file"), QFileInfo(path).fileName());
#ifdef Q_OS_ANDROID
    const QString mime = format == QLatin1String("csv") ? QStringLiteral("text/csv") : QStringLiteral("application/octet-stream");
    if (!androidShare(path, mime, tr("Send cards with")))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), tr("Sharing is not available. Use \"Save file\" instead.")}};
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
    return r;
}

QString DeckExchange::suggestedFileName(const QString &format) const
{
    // learningbox-<learning box>-<date>.lbox, e.g. learningbox-netzwerk-neu-a2-2026-09-29.lbox
    QString name = CardStore::instance()->currentCollectionName().toLower();
    name.replace(QStringLiteral("ä"), QStringLiteral("ae")).replace(QStringLiteral("ö"), QStringLiteral("oe"))
        .replace(QStringLiteral("ü"), QStringLiteral("ue")).replace(QStringLiteral("ß"), QStringLiteral("ss"));
    QString slug;
    for (const QChar ch : std::as_const(name))
        slug += (ch.isLetterOrNumber() && ch.unicode() < 128) ? ch : QChar(u'-');
    while (slug.contains(QStringLiteral("--")))
        slug.replace(QStringLiteral("--"), QStringLiteral("-"));
    while (slug.startsWith(u'-'))
        slug.remove(0, 1);
    while (slug.endsWith(u'-'))
        slug.chop(1);
    return QStringLiteral("learningbox-%1%2.%3")
        .arg(slug.isEmpty() ? QString() : slug + u'-', QDate::currentDate().toString(Qt::ISODate),
             format == QLatin1String("csv") ? QStringLiteral("csv") : QStringLiteral("lbox"));
}

QVariantMap DeckExchange::exportCards(const QUrl &target, const QString &format, bool withProgress, int box)
{
    CardStore *store = CardStore::instance();
    QList<deckformats::Item> items;
    QHash<QString, QByteArray> images;
    for (const Card &c : store->allCards()) {
        if (box > 0 && c.box != box)
            continue;
        deckformats::Item it;
        it.front = c.front;
        it.back = c.back;
        it.example = c.example;
        it.deck = c.deck;
        it.box = c.box;
        it.dueAt = c.dueAt;
        it.reviews = c.reviews;
        it.lapses = c.lapses;
        if (!c.image.isEmpty() && format != QLatin1String("csv")) {
            QFile f(store->imageDir() + u'/' + c.image);
            if (f.open(QIODevice::ReadOnly)) {
                images.insert(c.image, f.readAll());
                it.image = c.image;
            }
        }
        items << it;
    }
    if (items.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), tr("There are no cards to export.")}};

    const QByteArray bytes = format == QLatin1String("csv")
        ? deckformats::writeCsv(items)
        : deckformats::writeLbox(items, images, store->currentCollectionName(), withProgress);
    QFile out(localPath(target));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(bytes) != bytes.size())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), out.errorString()}};
    out.close();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("count"), int(items.size())}};
}

void DeckExchange::openFile(const QUrl &source)
{
    const QString path = localPath(source);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        deckformats::Package p;
        p.error = tr("Could not open the file: %1").arg(f.errorString());
        setPackage(p, {});
        return;
    }
    const QByteArray data = f.readAll();
    setPackage(deckformats::read(data, path, tempDir()), QFileInfo(source.fileName()).completeBaseName());
}

void DeckExchange::openLink(const QString &link)
{
    const QUrl url = deckformats::directDownloadUrl(link);
    if (!url.isValid() || url.host().isEmpty()) {
        deckformats::Package p;
        p.error = tr("That doesn't look like a link.");
        setPackage(p, {});
        return;
    }
    setBusy(true);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LearningBox (Qt)"));
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply](qint64 received, qint64) {
        if (received > kMaxDownload)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            deckformats::Package p;
            p.error = tr("Download failed: %1").arg(reply->errorString());
            setPackage(p, {});
            return;
        }
        const QByteArray data = reply->readAll();
        QString name = QFileInfo(url.path()).completeBaseName();
        setPackage(deckformats::read(data, url.fileName(), tempDir()), name);
    });
}

void DeckExchange::setPackage(deckformats::Package package, const QString &fallbackTitle)
{
    m_package = std::move(package);
    if (m_package.title.isEmpty())
        m_package.title = fallbackTitle;
    const QSet<QString> existing = CardStore::instance()->frontKeys();
    int already = 0;
    QVariantList sample;
    for (const deckformats::Item &it : std::as_const(m_package.items)) {
        if (existing.contains(it.front.trimmed().toCaseFolded()))
            ++already;
        if (sample.size() < 3)
            sample << QVariantMap{{QStringLiteral("front"), it.front}, {QStringLiteral("back"), it.back}};
    }
    const int count = int(m_package.items.size());
    m_preview = {
        {QStringLiteral("format"), m_package.format},
        {QStringLiteral("title"), m_package.title},
        {QStringLiteral("count"), count},
        {QStringLiteral("newCount"), count - already},
        {QStringLiteral("existingCount"), already},
        {QStringLiteral("hasProgress"), m_package.hasProgress},
        {QStringLiteral("pictures"), int(m_package.images.size())},
        {QStringLiteral("sample"), sample},
        {QStringLiteral("error"), m_package.error},
    };
    emit previewChanged();
}

void DeckExchange::clearPreview()
{
    m_package = {};
    m_preview.clear();
    emit previewChanged();
}

QVariantMap DeckExchange::applyImport(const QString &duplicates, bool keepProgress, int box, bool swap,
                                      const QString &newLearningBox)
{
    CardStore *store = CardStore::instance();
    if (m_package.items.isEmpty())
        return {{QStringLiteral("error"), tr("Nothing to import.")}};
    if (!newLearningBox.trimmed().isEmpty()) {
        const int id = store->createCollection(newLearningBox);
        if (id < 0)
            return {{QStringLiteral("error"), store->lastError()}};
        store->selectCollection(id);
    }
    const bool update = duplicates == QLatin1String("update");
    const int newBox = qBound(leitner::kFirstBox, box, leitner::kLearnedBox);
    const QString deck = m_package.title.isEmpty() ? tr("Import") : m_package.title;

    QHash<QString, QString> storedImage; // package key -> stored file name (each picture once)
    auto pictureFor = [&](const QString &key) -> QString {
        if (key.isEmpty() || !m_package.images.contains(key))
            return {};
        if (!storedImage.contains(key))
            storedImage.insert(key, store->storeImageData(m_package.images.value(key)));
        return storedImage.value(key);
    };

    QList<Card> fresh;
    QSet<QString> seen;
    int updated = 0, skipped = 0;
    for (deckformats::Item it : std::as_const(m_package.items)) {
        if (swap)
            std::swap(it.front, it.back);
        it.front = it.front.trimmed();
        const QString key = it.front.toCaseFolded();
        if (it.front.isEmpty() || seen.contains(key)) {
            ++skipped;
            continue;
        }
        seen.insert(key);
        const int existingId = store->findByFront(it.front);
        if (existingId >= 0) {
            if (!update) {
                ++skipped;
                continue;
            }
            const auto old = store->cardById(existingId);
            const QString image = old->image.isEmpty() ? pictureFor(it.image) : old->image;
            if (store->updateCard(existingId, old->front,
                                  it.back.trimmed().isEmpty() ? old->back : it.back,
                                  it.example.trimmed().isEmpty() ? old->example : it.example, image))
                ++updated;
            continue;
        }
        Card c;
        c.front = it.front;
        c.back = it.back;
        c.example = it.example;
        c.deck = it.deck.isEmpty() ? deck : it.deck;
        c.image = pictureFor(it.image);
        if (keepProgress && it.hasProgress) {
            c.box = it.box;
            c.dueAt = it.dueAt;
            c.reviews = it.reviews;
            c.lapses = it.lapses;
        } else {
            c.box = newBox;
            // Box 1: due now, like a new card. Higher boxes: after that box's interval.
            const int days = newBox == leitner::kFirstBox ? 0 : leitner::moveTo(newBox).intervalDays;
            c.dueAt = days < 0 ? QDateTime() : QDate::currentDate().addDays(days).startOfDay();
        }
        fresh << c;
    }
    const int added = store->insertCards(fresh);
    clearPreview();
    return {{QStringLiteral("added"), qMax(0, added)},
            {QStringLiteral("updated"), updated},
            {QStringLiteral("skipped"), skipped},
            {QStringLiteral("error"), added < 0 ? store->lastError() : QString()}};
}
