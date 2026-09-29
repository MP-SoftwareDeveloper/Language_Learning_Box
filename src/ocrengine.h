#pragma once

#include "ocr/ocrtypes.h"

#include <QFutureWatcher>
#include <QNetworkReply>
#include <QPointer>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>



// "Lens": recognizes German text in a photo and exposes the words (with boxes) to QML.
// Usage from QML: recognize(fileUrl[, rotationHint]) -> wait for !busy -> words / imageUrl.
class OcrEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QUrl imageUrl READ imageUrl NOTIFY resultChanged)       // processed copy the boxes refer to
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY resultChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY resultChanged)
    Q_PROPERTY(QVariantList words READ words NOTIFY resultChanged)     // [{index,text,x,y,w,h,line,block}]
    Q_PROPERTY(QString error READ error NOTIFY resultChanged)
    // "cloud" (Azure AI Vision) or "offline" (Tesseract) for the current result
    Q_PROPERTY(QString source READ source NOTIFY resultChanged)
    // Why online recognition was not used for this result although it is switched on ("" if it was)
    Q_PROPERTY(QString cloudProblem READ cloudProblem NOTIFY resultChanged)

public:
    explicit OcrEngine(QObject *parent = nullptr);
    ~OcrEngine() override;

    bool available() const { return m_unavailableReason.isEmpty(); }
    QString unavailableReason() const { return m_unavailableReason; }
    bool busy() const { return m_watcher.isRunning() || m_prepWatcher.isRunning() || m_cloudReply; }
    QString source() const { return m_source; }
    QString cloudProblem() const { return m_cloudProblem; }
    QUrl imageUrl() const { return m_imageUrl; }
    int imageWidth() const { return m_result.imageSize.width(); }
    int imageHeight() const { return m_result.imageSize.height(); }
    QVariantList words() const { return m_words; }
    QString error() const { return m_result.error; }

    // source: file:// URL (camera capture) or content:// URL (Android picker)
    // rotationHint: clockwise turn that makes the photo upright (DeviceTilt.rotation at the shutter);
    // -1 = unknown (gallery), then the orientation is detected from the picture.
    Q_INVOKABLE void recognize(const QUrl &source, int rotationHint = -1);
    Q_INVOKABLE void clear();

    // Selection helpers (indices into `words`)
    Q_INVOKABLE QString joinSelected(const QVariantList &indices) const;
    Q_INVOKABLE QVariantList sentenceAt(int index) const;
    Q_INVOKABLE QString cleanWord(const QString &raw) const;
    // Path for the camera to save the next capture to (app cache).
    Q_INVOKABLE QString captureFilePath() const;

    // Where deu/eng.traineddata are installed (copied from the app resources on first use).
    static QString tessdataDir();
    static QString ensureTessdata(QString *error);

signals:
    void busyChanged();
    void resultChanged();

public:
    // Picture prepared for online recognition (loaded, turned upright, scaled, saved for display).
    struct Prepared
    {
        QByteArray jpeg;
        QSize size;
        int rotation = 0;
        QString error;
    };

private:
    void onFinished();
    void onPrepared();
    void onCloudReply();
    void startOffline();
    void applyResult(const OcrResult &result);

    QFutureWatcher<OcrResult> m_watcher;
    OcrResult m_result;
    QVariantList m_words;
    QUrl m_imageUrl;
    QString m_unavailableReason;
    QString m_pendingDisplayPath;
    QString m_pendingSource;
    int m_pendingHint = -1;
    QFutureWatcher<Prepared> m_prepWatcher;
    Prepared m_prepared;
    QPointer<QNetworkReply> m_cloudReply;
    QString m_source;
    QString m_cloudProblem;
};
