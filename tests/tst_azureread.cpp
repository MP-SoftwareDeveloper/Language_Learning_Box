#include <QtTest>

#include "ocr/azureread.h"

namespace {
QString poly(int x, int y, int w, int h)
{
    return QStringLiteral(R"([{"x":%1,"y":%2},{"x":%3,"y":%2},{"x":%3,"y":%4},{"x":%1,"y":%4}])")
        .arg(x).arg(y).arg(x + w).arg(y + h);
}
QString word(const QString &t, int x, int y, double conf = 0.98)
{
    return QStringLiteral(R"({"text":"%1","boundingPolygon":%2,"confidence":%3})").arg(t, poly(x, y, 20 * t.size(), 30)).arg(conf);
}
QString line(const QStringList &words, int y)
{
    return QStringLiteral(R"({"text":"...","boundingPolygon":%1,"words":[%2]})").arg(poly(0, y, 600, 30), words.join(u','));
}
} // namespace

class TstAzureRead : public QObject
{
    Q_OBJECT
private slots:
    void url()
    {
        QCOMPARE(azureread::analyzeUrl(QStringLiteral(" https://my-vision.cognitiveservices.azure.com/ ")).toString(),
                 QStringLiteral("https://my-vision.cognitiveservices.azure.com/computervision/imageanalysis:analyze"
                                "?api-version=2024-02-01&features=read"));
        // Missing scheme is added, extra slashes removed.
        QCOMPARE(azureread::analyzeUrl(QStringLiteral("westeurope.api.cognitive.microsoft.com//")).host(),
                 QStringLiteral("westeurope.api.cognitive.microsoft.com"));
        QVERIFY(azureread::analyzeUrl(QString()).isEmpty());
    }
    void blocksLinesWords()
    {
        const QByteArray json = QStringLiteral(
            R"({"modelVersion":"2023-10-01","metadata":{"width":1000,"height":1000},"readResult":{"blocks":[)"
            R"({"lines":[%1,%2]},{"lines":[%3]}]}})")
            .arg(line({word("noch", 0, 10), word("einmal", 100, 10), word("bitte", 250, 10)}, 10),
                 line({word("Woh-", 0, 60)}, 60),
                 line({word("buchstabieren", 0, 200, 0.7)}, 200)).toUtf8();
        const OcrResult r = azureread::parse(json, QSize(1000, 1000));
        QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
        QStringList t;
        for (const auto &w : r.words) t << QStringLiteral("%1/%2/%3").arg(w.text).arg(w.line).arg(w.block);
        QCOMPARE(t.join(' '), QStringLiteral("noch/0/0 einmal/0/0 bitte/0/0 Woh-/1/0 buchstabieren/2/1"));
        QCOMPARE(r.words.first().box, QRect(QPoint(0, 10), QPoint(80, 40)));
        QCOMPARE(qRound(r.words.last().confidence), 70);
    }
    void skipsPersianNotes()
    {
        const QByteArray json = QStringLiteral(R"({"readResult":{"blocks":[{"lines":[%1]}]}})")
            .arg(line({word(QStringLiteral("آدرس"), 0, 0), word("Bindestrich", 100, 0), word("11@eret.com", 400, 0)}, 0)).toUtf8();
        QStringList t;
        for (const auto &w : azureread::parse(json, QSize(800, 800)).words) t << w.text;
        QCOMPARE(t, QStringList({"Bindestrich", "11@eret.com"}));
    }
    void errors()
    {
        QCOMPARE(azureread::parse(R"({"error":{"code":"401","message":"Access denied due to invalid subscription key or wrong API endpoint."}})", {}).error,
                 QStringLiteral("Access denied due to invalid subscription key or wrong API endpoint."));
        QCOMPARE(azureread::parse(R"({"error":{"code":"429"}})", {}).error, QStringLiteral("429"));
        QVERIFY(!azureread::parse("<html>", {}).error.isEmpty());
        const OcrResult empty = azureread::parse(R"({"readResult":{"blocks":[]}})", {});
        QVERIFY(empty.error.isEmpty() && empty.words.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstAzureRead)
#include "tst_azureread.moc"
