#include "azuretranslate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrlQuery>
#include <algorithm>

namespace AzureTranslate {

namespace {
QUrl endpoint(const QString &path, const QString &source, const QString &target)
{
    QUrl url(QStringLiteral("https://api.cognitive.microsofttranslator.com/") + path);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("api-version"), QStringLiteral("3.0"));
    q.addQueryItem(QStringLiteral("from"), source);
    q.addQueryItem(QStringLiteral("to"), target);
    url.setQuery(q);
    return url;
}
} // namespace

QUrl translateUrl(const QString &source, const QString &target)
{
    return endpoint(QStringLiteral("translate"), source, target);
}

QUrl lookupUrl(const QString &source, const QString &target)
{
    return endpoint(QStringLiteral("dictionary/lookup"), source, target);
}

QByteArray requestBody(const QString &text)
{
    QJsonObject o;
    o.insert(QStringLiteral("Text"), text);
    return QJsonDocument(QJsonArray{o}).toJson(QJsonDocument::Compact);
}

bool isSingleWord(const QString &text)
{
    const QString t = text.trimmed();
    return !t.isEmpty() && !t.contains(QLatin1Char(' ')) && !t.contains(QLatin1Char('\n'));
}

QString errorMessage(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject e = doc.object().value(QStringLiteral("error")).toObject();
    if (e.isEmpty())
        return {};
    const QString m = e.value(QStringLiteral("message")).toString().trimmed();
    return m.isEmpty() ? QStringLiteral("error %1").arg(e.value(QStringLiteral("code")).toInt()) : m;
}

Result parseTranslation(const QByteArray &json)
{
    Result r;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        r.error = QStringLiteral("unexpected response");
        return r;
    }
    if (doc.isObject()) { // errors come as an object, answers as an array
        const QString m = errorMessage(json);
        r.error = m.isEmpty() ? QStringLiteral("unexpected response") : m;
        return r;
    }
    const QJsonArray items = doc.array().first().toObject().value(QStringLiteral("translations")).toArray();
    r.text = items.first().toObject().value(QStringLiteral("text")).toString().trimmed();
    if (r.text.isEmpty())
        r.error = QStringLiteral("empty translation");
    return r;
}

QStringList parseLookup(const QByteArray &json, const QString &main, int maxAlternatives)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray())
        return {};
    struct Entry { QString text; double confidence; };
    QList<Entry> entries;
    const QJsonArray items = doc.array().first().toObject().value(QStringLiteral("translations")).toArray();
    for (const QJsonValue &v : items) {
        const QJsonObject o = v.toObject();
        QString t = o.value(QStringLiteral("displayTarget")).toString().trimmed();
        if (t.isEmpty())
            t = o.value(QStringLiteral("normalizedTarget")).toString().trimmed();
        if (!t.isEmpty())
            entries.append({t, o.value(QStringLiteral("confidence")).toDouble()});
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry &a, const Entry &b) { return a.confidence > b.confidence; });

    QStringList out;
    const QString mainFolded = main.simplified().toCaseFolded();
    QStringList seen{mainFolded};
    for (const Entry &e : std::as_const(entries)) {
        const QString folded = e.text.toCaseFolded();
        if (seen.contains(folded))
            continue;
        seen.append(folded);
        out.append(e.text);
        if (out.size() >= maxAlternatives)
            break;
    }
    return out;
}

} // namespace AzureTranslate
