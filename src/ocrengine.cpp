#include "ocrengine.h"
#include "ocr/ocr.h"
#include "ocr/textselect.h"
#include "ocr/azureread.h"
#include "cloudocr.h"
#include "cardstore.h"

#include <QBuffer>
#include <QDebug>
#include <QNetworkReply>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QTransform>
#include <QUuid>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtConcurrent/QtConcurrentRun>

namespace {
// Longest edge for OCR. Phone photos are ~4000 px; ~2400 keeps book text legible
// for Tesseract while bounding memory and time.
constexpr int kOcrMaxEdge = 2400;
// Tesseract models shipped with the app: German and English (tessdata_fast).
const char *const kLanguages[] = {"deu", "eng"};

// Model for the language of the selected learning box.
QString tesseractLanguage()
{
    return CardStore::instance()->learningLanguage() == QLatin1String("en") ? QStringLiteral("eng")
                                                                          : QStringLiteral("deu");
}

QString cacheDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/lens");
    QDir().mkpath(d);
    return d;
}

// Loads + orients + downscales the picture; writes the exact image that is OCR'd to
// `displayPath`, so QML shows the same pixels the word boxes refer to.
OcrResult loadAndRecognize(const QString &source, const QString &displayPath, const QString &tessdata,
                           const QString &lang, int rotationHint)
{
    OcrResult r;
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("cannot open picture: %1").arg(in.errorString());
        return r;
    }
    QImageReader reader(&in);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    if (full.isValid() && qMax(full.width(), full.height()) > kOcrMaxEdge)
        reader.setScaledSize(full.scaled(kOcrMaxEdge, kOcrMaxEdge, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (img.isNull()) {
        r.error = QStringLiteral("not a readable picture: %1").arg(reader.errorString());
        return r;
    }
    if (qMax(img.width(), img.height()) > kOcrMaxEdge)
        img = img.scaled(kOcrMaxEdge, kOcrMaxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img.convertTo(QImage::Format_RGB32);
    OcrResult r2 = ocr::recognize(img, tessdata, lang, rotationHint);
    // A sideways photo is read turned; show it turned the same way so the boxes line up.
    if (r2.rotation != 0)
        img = img.transformed(QTransform().rotate(r2.rotation));
    img.save(displayPath, "jpg", 85);
    return r2;
}

// Online path: load + EXIF + scale + turn by the tilt hint, save the display copy and encode the
// JPEG that is sent, so the returned boxes match what Lens shows.
OcrEngine::Prepared prepareForCloud(const QString &source, const QString &displayPath, int rotationHint)
{
    OcrEngine::Prepared p;
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        p.error = QStringLiteral("cannot open picture: %1").arg(in.errorString());
        return p;
    }
    QImageReader reader(&in);
    reader.setAutoTransform(true);
    const QSize full = reader.size();
    if (full.isValid() && qMax(full.width(), full.height()) > kOcrMaxEdge)
        reader.setScaledSize(full.scaled(kOcrMaxEdge, kOcrMaxEdge, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (img.isNull()) {
        p.error = QStringLiteral("not a readable picture: %1").arg(reader.errorString());
        return p;
    }
    if (qMax(img.width(), img.height()) > kOcrMaxEdge)
        img = img.scaled(kOcrMaxEdge, kOcrMaxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img.convertTo(QImage::Format_RGB32);
    if (rotationHint == 90 || rotationHint == 180 || rotationHint == 270) {
        img = img.transformed(QTransform().rotate(rotationHint));
        p.rotation = rotationHint;
    }
    img.save(displayPath, "jpg", 85);
    QBuffer buf(&p.jpeg);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 85);
    p.size = img.size();
    return p;
}

} // namespace

OcrEngine::OcrEngine(QObject *parent)
    : QObject(parent)
{
    if (!ocr::compiledIn())
        m_unavailableReason = QStringLiteral("The OCR engine (Tesseract) is not included in this build.");
    else
        ensureTessdata(&m_unavailableReason);

    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, &OcrEngine::onFinished);
    connect(&m_prepWatcher, &QFutureWatcher<Prepared>::finished, this, &OcrEngine::onPrepared);
}

OcrEngine::~OcrEngine()
{
    m_watcher.waitForFinished(); // the workers must not outlive the engine
    m_prepWatcher.waitForFinished();
    if (m_cloudReply)
        m_cloudReply->abort();
}

QString OcrEngine::tessdataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/tessdata");
}

QString OcrEngine::ensureTessdata(QString *error)
{
    // Tesseract reads models from a real directory, so the bundled model is copied
    // out of the Qt resources once (1.5 MB).
    const QString dir = tessdataDir();
    QDir().mkpath(dir);
    for (const char *lang : kLanguages) {
        const QString name = QLatin1String(lang) + QStringLiteral(".traineddata");
        const QString target = dir + u'/' + name;
        const QString source = QStringLiteral(":/ocr/tessdata/") + name;
        if (QFileInfo(target).size() == QFileInfo(source).size() && QFileInfo(target).size() > 0)
            continue;
        QFile::remove(target);
        if (!QFile::exists(source) || !QFile::copy(source, target)) {
            if (error)
                *error = QStringLiteral("The OCR language data (%1) is missing.").arg(name);
            return {};
        }
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
    return dir;
}

void OcrEngine::recognize(const QUrl &source, int rotationHint)
{
    if (!available() || busy())
        return;
    m_pendingSource = source.isLocalFile() ? source.toLocalFile() : source.toString();
    m_pendingHint = rotationHint;
    m_pendingDisplayPath = cacheDir() + QStringLiteral("/page-") + QUuid::createUuid().toString(QUuid::Id128)
                           + QStringLiteral(".jpg");
    m_cloudProblem.clear();
    if (CloudOcr::instance()->configured()) {
        // Online first (handwriting); prepare the picture off the UI thread.
        const QString path = m_pendingSource, display = m_pendingDisplayPath;
        m_prepWatcher.setFuture(QtConcurrent::run([path, display, rotationHint] {
            return prepareForCloud(path, display, rotationHint);
        }));
    } else {
        startOffline();
    }
    emit busyChanged();
}

void OcrEngine::startOffline()
{
    const QString path = m_pendingSource, display = m_pendingDisplayPath, tess = tessdataDir();
    const QString lang = tesseractLanguage(); // read on the UI thread
    const int hint = m_pendingHint;
    m_watcher.setFuture(QtConcurrent::run([path, display, tess, lang, hint] {
        return loadAndRecognize(path, display, tess, lang, hint);
    }));
}

void OcrEngine::onPrepared()
{
    m_prepared = m_prepWatcher.result();
    if (!m_prepared.error.isEmpty()) {
        OcrResult r;
        r.error = m_prepared.error;
        m_source = QStringLiteral("cloud");
        applyResult(r);
        return;
    }
    m_cloudReply = CloudOcr::instance()->annotate(m_prepared.jpeg);
    connect(m_cloudReply, &QNetworkReply::finished, this, &OcrEngine::onCloudReply);
}

void OcrEngine::onCloudReply()
{
    QNetworkReply *reply = m_cloudReply;
    m_cloudReply = nullptr;
    if (!reply)
        return;
    reply->deleteLater();
    const QByteArray body = reply->readAll();
    OcrResult r = azureread::parse(body, m_prepared.size);
    // No JSON at all (no connection, proxy, timeout): the network error says more than the parser.
    if (reply->error() != QNetworkReply::NoError && (r.error.isEmpty() || !body.trimmed().startsWith('{')))
        r.error = reply->errorString();
    if (r.error.isEmpty() && !r.words.isEmpty()) {
        r.rotation = m_prepared.rotation;
        m_source = QStringLiteral("cloud");
        applyResult(r);
        return;
    }
    // Offline, quota, bad key, or nothing found: read it with Tesseract instead.
    m_cloudProblem = r.error.isEmpty() ? tr("no text found online") : r.error;
    qWarning().noquote() << "Lens: online recognition not used:" << m_cloudProblem;
    startOffline();
}

void OcrEngine::onFinished()
{
    m_source = QStringLiteral("offline");
    applyResult(m_watcher.result());
}

void OcrEngine::applyResult(const OcrResult &result)
{
    const QString oldDisplay = m_imageUrl.toLocalFile();
    m_result = result;
    m_words.clear();
    for (int i = 0; i < m_result.words.size(); ++i) {
        const OcrWord &w = m_result.words.at(i);
        m_words.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("text"), w.text},
            {QStringLiteral("x"), w.box.x()},
            {QStringLiteral("y"), w.box.y()},
            {QStringLiteral("w"), w.box.width()},
            {QStringLiteral("h"), w.box.height()},
            {QStringLiteral("line"), w.line},
            {QStringLiteral("block"), w.block},
        });
    }
    m_imageUrl = QFile::exists(m_pendingDisplayPath) ? QUrl::fromLocalFile(m_pendingDisplayPath) : QUrl();
    if (!oldDisplay.isEmpty() && oldDisplay != m_pendingDisplayPath)
        QFile::remove(oldDisplay);
    emit resultChanged();
    emit busyChanged();
}

void OcrEngine::clear()
{
    if (busy())
        return;
    if (m_imageUrl.isLocalFile())
        QFile::remove(m_imageUrl.toLocalFile());
    m_result = {};
    m_words.clear();
    m_imageUrl.clear();
    emit resultChanged();
}

QString OcrEngine::joinSelected(const QVariantList &indices) const
{
    QList<int> idx;
    for (const QVariant &v : indices)
        idx.append(v.toInt());
    return textselect::joinWords(m_result.words, idx);
}

QVariantList OcrEngine::sentenceAt(int index) const
{
    QVariantList out;
    for (int i : textselect::sentenceRange(m_result.words, index))
        out.append(i);
    return out;
}

QString OcrEngine::cleanWord(const QString &raw) const
{
    return textselect::cleanWord(raw);
}

bool OcrEngine::grabFrame(QObject *videoSink)
{
    m_grabbedFrame = QImage();
    auto *sink = qobject_cast<QVideoSink *>(videoSink);
    if (!sink)
        return false;
    m_grabbedFrame = sink->videoFrame().toImage();
    return !m_grabbedFrame.isNull();
}

bool OcrEngine::recognizeGrabbed(int rotationHint)
{
    if (m_grabbedFrame.isNull() || busy())
        return false;
    const QString path = captureFilePath();
    const bool saved = m_grabbedFrame.save(path, "jpg", 92);
    m_grabbedFrame = QImage();
    if (!saved)
        return false;
    recognize(QUrl::fromLocalFile(path), rotationHint);
    return true;
}

QString OcrEngine::captureFilePath() const
{
    return cacheDir() + QStringLiteral("/capture-") + QUuid::createUuid().toString(QUuid::Id128)
           + QStringLiteral(".jpg");
}
