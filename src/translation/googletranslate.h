#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// Pure helpers for Google's public translate endpoint (translate.googleapis.com,
// client=gtx, no API key). Kept free of networking so they can be unit-tested.
namespace GoogleTranslate {

struct Result
{
    QString text;              // full translation
    QStringList alternatives;  // other dictionary meanings (single words only), without `text`
    QString error;             // non-empty on parse failure
};

// Endpoint URL; the text goes in the POST body (see requestBody) so long OCR pages fit.
QUrl requestUrl(const QString &source, const QString &target);
QByteArray requestBody(const QString &text);

// Parses the JSON array the endpoint returns:
// [[["Brot","bread",...],...], [["noun",["bread","loaf"],...],...], "de", ...]
Result parse(const QByteArray &json, int maxAlternatives = 3);

} // namespace GoogleTranslate
