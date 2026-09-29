#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;
class QJSEngine;

// Online text recognition with Azure AI Vision "Read" (reads handwriting, like Google Lens).
// Free tier F0: 5000 pictures/month; beyond that Azure refuses instead of charging.
// Settings (QSettings "ocr/*"): switch + the user's own Azure endpoint and key. Lens falls back to
// the offline Tesseract reader when this is off, not configured, offline, or the call fails.
class CloudOcr : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY settingsChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY settingsChanged) // enabled, endpoint and key set
    Q_PROPERTY(bool testing READ testing NOTIFY testingChanged)

public:
    static CloudOcr *instance();
    static CloudOcr *create(QQmlEngine *, QJSEngine *);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QString endpoint() const { return m_endpoint; }
    void setEndpoint(const QString &endpoint);
    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &key);
    bool configured() const;
    bool testing() const { return m_testing; }

    // Sends one JPEG to Azure Read; the caller owns (deleteLater) the reply.
    QNetworkReply *annotate(const QByteArray &jpeg);

    // Checks endpoint + key with a tiny picture; answers with keyTested(ok, message).
    Q_INVOKABLE void testKey();

signals:
    void settingsChanged();
    void testingChanged();
    void keyTested(bool ok, const QString &message);

private:
    explicit CloudOcr(QObject *parent = nullptr);

    QNetworkAccessManager *m_nam = nullptr;
    bool m_enabled = false;
    QString m_endpoint;
    QString m_apiKey;
    bool m_testing = false;
};
