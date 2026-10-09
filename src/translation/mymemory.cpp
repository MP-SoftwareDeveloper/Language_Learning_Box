#include "mymemory.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>

namespace MyMemory {

namespace {
// MyMemory sometimes returns HTML entities in the translated text.
QString decodeEntities(QString s)
{
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s.trimmed();
}
} // namespace

bool fits(const QString &text)
{
    return text.toUtf8().size() <= 480;
}

QUrl requestUrl(const QString &text, const QString &source, const QString &target)
{
    QUrl url(QStringLiteral("https://api.mymemory.translated.net/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), text);
    q.addQueryItem(QStringLiteral("langpair"), source + QLatin1Char('|') + target);
    url.setQuery(q);
    return url;
}

Result parse(const QByteArray &json, const QString &sourceText, int maxAlternatives)
{
    Result r;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("unexpected response");
        return r;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("quotaFinished")).toBool()) {
        r.error = QStringLiteral("MyMemory daily limit reached");
        return r;
    }
    // responseStatus is a number or a string ("200")
    const int status = root.value(QStringLiteral("responseStatus")).toVariant().toInt();
    if (status != 200) {
        const QString d = root.value(QStringLiteral("responseDetails")).toString().trimmed();
        r.error = d.isEmpty() ? QStringLiteral("MyMemory status %1").arg(status) : d;
        return r;
    }

    const QJsonObject data = root.value(QStringLiteral("responseData")).toObject();
    r.text = decodeEntities(data.value(QStringLiteral("translatedText")).toString());
    if (r.text.isEmpty() || r.text.startsWith(QLatin1String("MYMEMORY WARNING"), Qt::CaseInsensitive)
        || r.text.startsWith(QLatin1String("PLEASE SELECT"), Qt::CaseInsensitive)) {
        r.error = r.text.isEmpty() ? QStringLiteral("empty translation") : QStringLiteral("MyMemory daily limit reached");
        r.text.clear();
        return r;
    }
    const QString asked = sourceText.simplified().toCaseFolded();
    const double match = data.value(QStringLiteral("match")).toVariant().toDouble();
    if (r.text.toCaseFolded() == asked && match < 0.9) {
        r.text.clear();
        r.error = QStringLiteral("no translation");
        return r;
    }

    // Other translations of exactly the same text (translation memory entries), best first.
    const QString mainFolded = r.text.toCaseFolded();
    for (const QJsonValue &v : root.value(QStringLiteral("matches")).toArray()) {
        if (r.alternatives.size() >= maxAlternatives)
            break;
        const QJsonObject m = v.toObject();
        if (m.value(QStringLiteral("segment")).toString().simplified().toCaseFolded() != asked)
            continue;
        if (m.value(QStringLiteral("match")).toVariant().toDouble() < 0.5)
            continue;
        const QString t = decodeEntities(m.value(QStringLiteral("translation")).toString());
        if (t.isEmpty() || t.toCaseFolded() == mainFolded || t.toCaseFolded() == asked)
            continue;
        bool dup = false;
        for (const QString &a : std::as_const(r.alternatives))
            dup = dup || a.toCaseFolded() == t.toCaseFolded();
        if (!dup)
            r.alternatives.append(t);
    }
    return r;
}

} // namespace MyMemory
