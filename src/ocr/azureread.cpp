#include "azureread.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

namespace azureread {

QUrl analyzeUrl(const QString &endpoint)
{
    QString base = endpoint.trimmed();
    if (base.isEmpty())
        return {};
    if (!base.startsWith(QLatin1String("https://")) && !base.startsWith(QLatin1String("http://")))
        base.prepend(QLatin1String("https://"));
    while (base.endsWith(u'/'))
        base.chop(1);
    QUrl url(base + QStringLiteral("/computervision/imageanalysis:analyze"));
    if (!url.isValid() || url.host().isEmpty())
        return {};
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api-version"), QStringLiteral("2024-02-01"));
    q.addQueryItem(QStringLiteral("features"), QStringLiteral("read"));
    url.setQuery(q);
    return url;
}

namespace {

QRect boxOf(const QJsonArray &polygon, const QSize &imageSize)
{
    int x1 = INT_MAX, y1 = INT_MAX, x2 = INT_MIN, y2 = INT_MIN;
    for (const QJsonValue &v : polygon) {
        const QJsonObject p = v.toObject();
        const int x = p.value(QStringLiteral("x")).toInt(), y = p.value(QStringLiteral("y")).toInt();
        x1 = qMin(x1, x); y1 = qMin(y1, y); x2 = qMax(x2, x); y2 = qMax(y2, y);
    }
    if (x1 > x2 || y1 > y2)
        return {};
    const QRect r(QPoint(x1, y1), QPoint(x2, y2));
    return imageSize.isValid() ? r.intersected(QRect(QPoint(0, 0), imageSize)) : r;
}

bool hasLatinOrDigit(const QString &s)
{
    for (QChar c : s)
        if (c.isDigit() || (c.isLetter() && c.script() == QChar::Script_Latin))
            return true;
    return false;
}

} // namespace

OcrResult parse(const QByteArray &json, const QSize &imageSize)
{
    OcrResult r;
    r.imageSize = imageSize;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.isEmpty()) {
        r.error = QStringLiteral("unexpected answer from Azure");
        return r;
    }
    if (root.contains(QStringLiteral("error"))) {
        const QJsonObject e = root.value(QStringLiteral("error")).toObject();
        r.error = e.value(QStringLiteral("message")).toString();
        if (r.error.isEmpty())
            r.error = e.value(QStringLiteral("code")).toString();
        return r;
    }
    int line = 0, block = 0;
    const QJsonArray blocks = root.value(QStringLiteral("readResult")).toObject().value(QStringLiteral("blocks")).toArray();
    for (const QJsonValue &b : blocks) {
        bool blockHasWords = false;
        for (const QJsonValue &lv : b.toObject().value(QStringLiteral("lines")).toArray()) {
            bool lineHasWords = false;
            for (const QJsonValue &wv : lv.toObject().value(QStringLiteral("words")).toArray()) {
                const QJsonObject w = wv.toObject();
                const QString text = w.value(QStringLiteral("text")).toString().trimmed();
                const QRect box = boxOf(w.value(QStringLiteral("boundingPolygon")).toArray(), imageSize);
                if (text.isEmpty() || !box.isValid() || !hasLatinOrDigit(text))
                    continue;
                const float conf = float(w.value(QStringLiteral("confidence")).toDouble(0.9) * 100.0);
                r.words.append(OcrWord{text, box, line, block, conf});
                lineHasWords = blockHasWords = true;
            }
            if (lineHasWords)
                ++line;
        }
        if (blockHasWords)
            ++block;
    }
    return r;
}

} // namespace azureread
