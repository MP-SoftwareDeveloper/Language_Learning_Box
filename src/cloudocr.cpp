#include "cloudocr.h"
#include "ocr/azureread.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QQmlEngine>
#include <QSettings>

namespace {
constexpr int kTimeoutMs = 25000; // a 1 MB photo over mobile data
}

CloudOcr *CloudOcr::instance()
{
    static CloudOcr *s = new CloudOcr(QCoreApplication::instance());
    return s;
}

CloudOcr *CloudOcr::create(QQmlEngine *, QJSEngine *engine)
{
    CloudOcr *s = instance();
    engine->setObjectOwnership(s, QJSEngine::CppOwnership);
    return s;
}

CloudOcr::CloudOcr(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("ocr/cloud"), false).toBool();
    m_endpoint = s.value(QStringLiteral("ocr/azureEndpoint")).toString();
    m_apiKey = s.value(QStringLiteral("ocr/azureKey")).toString();
    m_nam->setTransferTimeout(kTimeoutMs);
}

void CloudOcr::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("ocr/cloud"), on);
    emit settingsChanged();
}

void CloudOcr::setEndpoint(const QString &endpoint)
{
    const QString e = endpoint.trimmed();
    if (e == m_endpoint)
        return;
    m_endpoint = e;
    QSettings().setValue(QStringLiteral("ocr/azureEndpoint"), e);
    emit settingsChanged();
}

void CloudOcr::setApiKey(const QString &key)
{
    const QString k = key.trimmed();
    if (k == m_apiKey)
        return;
    m_apiKey = k;
    QSettings().setValue(QStringLiteral("ocr/azureKey"), k);
    emit settingsChanged();
}

bool CloudOcr::configured() const
{
    return m_enabled && !m_apiKey.isEmpty() && !azureread::analyzeUrl(m_endpoint).isEmpty();
}

QNetworkReply *CloudOcr::annotate(const QByteArray &jpeg)
{
    QNetworkRequest req(azureread::analyzeUrl(m_endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    req.setRawHeader(azureread::kKeyHeader, m_apiKey.toUtf8());
    return m_nam->post(req, jpeg);
}

void CloudOcr::testKey()
{
    if (m_testing)
        return;
    if (azureread::analyzeUrl(m_endpoint).isEmpty()) {
        emit keyTested(false, tr("Enter the endpoint first (Azure portal \u2192 your Vision resource \u2192 Keys and Endpoint)."));
        return;
    }
    if (m_apiKey.isEmpty()) {
        emit keyTested(false, tr("Enter a key first."));
        return;
    }
    // A small picture with a word on it: a valid key answers with text, a wrong one with an error.
    QImage img(240, 80, QImage::Format_RGB32);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        QFont f = p.font();
        f.setPixelSize(40);
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("Hallo"));
    }
    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 90);

    m_testing = true;
    emit testingChanged();
    QNetworkReply *reply = annotate(jpeg);
    connect(reply, &QNetworkReply::finished, this, [this, reply, size = img.size()] {
        reply->deleteLater();
        m_testing = false;
        emit testingChanged();
        const QByteArray body = reply->readAll();
        const OcrResult r = azureread::parse(body, size);
        if (reply->error() != QNetworkReply::NoError && !body.trimmed().startsWith('{'))
            emit keyTested(false, reply->errorString()); // no connection etc.
        else if (!r.error.isEmpty())
            emit keyTested(false, r.error);              // e.g. "API key not valid."
        else if (reply->error() != QNetworkReply::NoError)
            emit keyTested(false, reply->errorString());
        else
            emit keyTested(true, r.words.isEmpty() ? tr("Key works.") : tr("Key works (read: %1).").arg(r.words.first().text));
    });
}
