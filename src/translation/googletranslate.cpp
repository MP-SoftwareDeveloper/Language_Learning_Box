#include "googletranslate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrlQuery>

namespace GoogleTranslate {

QUrl requestUrl(const QString &source, const QString &target)
{
    QUrl url(QStringLiteral("https://translate.googleapis.com/translate_a/single"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client"), QStringLiteral("gtx"));
    q.addQueryItem(QStringLiteral("sl"), source);
    q.addQueryItem(QStringLiteral("tl"), target);
    q.addQueryItem(QStringLiteral("dt"), QStringLiteral("t"));   // translation
    q.addQueryItem(QStringLiteral("dt"), QStringLiteral("bd"));  // dictionary (alternatives)
    q.addQueryItem(QStringLiteral("ie"), QStringLiteral("UTF-8"));
    q.addQueryItem(QStringLiteral("oe"), QStringLiteral("UTF-8"));
    url.setQuery(q);
    return url;
}

QByteArray requestBody(const QString &text)
{
    return "q=" + QUrl::toPercentEncoding(text);
}

Result parse(const QByteArray &json, int maxAlternatives)
{
    Result r;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        r.error = QStringLiteral("unexpected response");
        return r;
    }
    const QJsonArray root = doc.array();

    // [0]: sentence segments, each [translated, original, ...]
    for (const QJsonValue &seg : root.at(0).toArray())
        r.text += seg.toArray().at(0).toString();
    r.text = r.text.trimmed();
    if (r.text.isEmpty()) {
        r.error = QStringLiteral("empty translation");
        return r;
    }

    // [1]: dictionary entries per part of speech, each [pos, [terms...], ...]
    const QString mainFolded = r.text.toCaseFolded();
    for (const QJsonValue &entry : root.at(1).toArray()) {
        for (const QJsonValue &term : entry.toArray().at(1).toArray()) {
            if (r.alternatives.size() >= maxAlternatives)
                return r;
            const QString t = term.toString().trimmed();
            if (t.isEmpty() || t.toCaseFolded() == mainFolded)
                continue;
            bool dup = false;
            for (const QString &a : std::as_const(r.alternatives))
                dup = dup || a.toCaseFolded() == t.toCaseFolded();
            if (!dup)
                r.alternatives.append(t);
        }
    }
    return r;
}

} // namespace GoogleTranslate
