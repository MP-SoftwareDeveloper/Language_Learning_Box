#include <QtTest>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <cmath>

#include "ocr/ocr.h"

// End-to-end OCR on a synthetic "book page": German text with umlauts and ß.
class TstOcr : public QObject
{
    Q_OBJECT

    static QImage page(const QStringList &lines, int pixelSize = 42)
    {
        QImage img(1600, 120 + int(lines.size()) * pixelSize * 2, QImage::Format_RGB32);
        img.fill(QColor(250, 248, 240)); // paper
        QPainter p(&img);
        p.setRenderHint(QPainter::TextAntialiasing);
        QFont f(QStringLiteral("DejaVu Serif"));
        f.setPixelSize(pixelSize);
        p.setFont(f);
        p.setPen(QColor(30, 30, 30));
        int y = 80;
        for (const QString &l : lines) {
            p.drawText(60, y, l);
            y += pixelSize * 2;
        }
        return img;
    }

    // Photo of a monitor: strong vertical/diagonal moire stripes, low contrast, uneven light.
    static QImage moire(const QImage &src)
    {
        QImage img = src.convertToFormat(QImage::Format_RGB32);
        const double cx = img.width() / 2.0, cy = img.height() / 2.0;
        for (int y = 0; y < img.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const double stripes = 0.5 + 0.5 * std::sin(x * 1.9) * std::sin((x + y) * 0.23);
                const double light = 1.0 - 0.35 * (std::hypot(x - cx, y - cy) / std::hypot(cx, cy));
                const int g = qGray(line[x]);
                const int v = qBound(0, int((110 + g * 0.45 + 70 * stripes) * light), 255);
                line[x] = qRgb(v, v, v);
            }
        }
        return img;
    }

private slots:
    void recognizesGermanText()
    {
        QVERIFY(ocr::compiledIn());
        const QImage img = page({QStringLiteral("Ich esse gern einen Apfel."),
                                 QStringLiteral("Die Straße ist heute sehr schön."),
                                 QStringLiteral("Wir fahren übermorgen nach München.")});
        const OcrResult r = ocr::recognize(img, QStringLiteral(LB_TESSDATA_DIR));
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.imageSize, img.size());

        QStringList texts;
        for (const OcrWord &w : r.words)
            texts << w.text;
        qInfo().noquote() << "recognized:" << texts.join(u' ');
        for (const char *expected : {"Apfel.", "Straße", "schön.", "übermorgen", "München."})
            QVERIFY2(texts.contains(QString::fromUtf8(expected)), expected);

        // Reading order + line numbers + plausible boxes
        QCOMPARE(r.words.first().text, QStringLiteral("Ich"));
        QCOMPARE(r.words.first().line, 0);
        QCOMPARE(r.words.last().line, 2);
        for (const OcrWord &w : r.words)
            QVERIFY(QRect(QPoint(0, 0), img.size()).contains(w.box));
    }

    void recognizesScreenPhotoWithMoire()
    {
        const QImage img = moire(page({QStringLiteral("Ich wohne in Deutschland."),
                                       QStringLiteral("Kommen Sie aus Zürich, Frau Weber?"),
                                       QStringLiteral("Schreiben Sie Ihre Antworten.")}));
        if (qEnvironmentVariableIsSet("LB_SAVE_MOIRE"))
            img.save(QStringLiteral("moire.png"));
        const OcrResult r = ocr::recognize(img, QStringLiteral(LB_TESSDATA_DIR));
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QCOMPARE(r.imageSize, img.size());
        QStringList texts;
        for (const OcrWord &w : r.words)
            texts << w.text;
        qInfo().noquote() << "moire recognized:" << texts.join(u' ');
        int found = 0;
        for (const char *expected : {"wohne", "Deutschland.", "Kommen", "Zürich,", "Weber?", "Schreiben", "Antworten."})
            found += texts.contains(QString::fromUtf8(expected)) ? 1 : 0;
        QVERIFY2(found >= 5, qPrintable(QStringLiteral("only %1 of 7 words").arg(found)));
    }

    void recognizesCaptionInsidePicture()
    {
        // A video frame: busy coloured shapes, subtitle in white on a dark translucent bar.
        QImage img(1600, 1000, QImage::Format_RGB32);
        img.fill(QColor(120, 180, 240));
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 60; ++i) {
            p.setBrush(QColor::fromHsv((i * 37) % 360, 160, 200));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPoint((i * 263) % 1600, (i * 151) % 1000), 60 + (i * 17) % 120, 40 + (i * 29) % 90);
        }
        QFont f(QStringLiteral("DejaVu Sans"));
        f.setPixelSize(48);
        f.setBold(true);
        p.setFont(f);
        const QString caption = QStringLiteral("Papa Wutz faltet Schiffchen für alle.");
        const QRect bar = QFontMetrics(f).boundingRect(caption).adjusted(-20, -12, 20, 12).translated(260, 700);
        p.fillRect(bar, QColor(20, 30, 20, 215));
        p.setPen(Qt::white);
        p.drawText(260, 700, caption);
        p.end();
        if (qEnvironmentVariableIsSet("LB_SAVE_MOIRE"))
            img.save(QStringLiteral("caption.png"));

        const OcrResult r = ocr::recognize(img, QStringLiteral(LB_TESSDATA_DIR));
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QStringList texts;
        for (const OcrWord &w : r.words)
            texts << w.text;
        qInfo().noquote() << "caption recognized:" << texts.join(u' ');
        int found = 0;
        for (const char *expected : {"Papa", "Wutz", "faltet", "Schiffchen", "für", "alle."})
            found += texts.contains(QString::fromUtf8(expected)) ? 1 : 0;
        QVERIFY2(found >= 5, qPrintable(QStringLiteral("only %1 of 6 words").arg(found)));
    }

    void missingLanguageDataIsReported()
    {
        const OcrResult r = ocr::recognize(page({QStringLiteral("Hallo")}), QStringLiteral("/nonexistent"));
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.words.isEmpty());
    }
};

QTEST_MAIN(TstOcr)
#include "tst_ocr.moc"
