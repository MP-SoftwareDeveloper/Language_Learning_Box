#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// Fallback translator: MyMemory (api.mymemory.translated.net), free, no key. Used when Google's unofficial
// endpoint refuses (HTTP 429 "too many requests"). Anonymous use allows about 5000 characters a day and
// 500 bytes per request, which is plenty for single words and short lines. Pure helpers (no networking),
// unit-tested; Translator does the HTTPS call.
namespace MyMemory {

struct Result
{
    QString text;              // translation
    QStringList alternatives;  // other translations of the same text (single words), without `text`
    QString error;             // non-empty when there is no usable translation
};

// The free API takes at most 500 bytes of text per request.
bool fits(const QString &text);

// GET url: .../get?q=<text>&langpair=de|en
QUrl requestUrl(const QString &text, const QString &source, const QString &target);

// Reads the JSON answer. `sourceText` is what was asked: an answer that only repeats it (MyMemory's way of
// saying "don't know"), quota warnings and error statuses all become `error`.
Result parse(const QByteArray &json, const QString &sourceText, int maxAlternatives = 3);

} // namespace MyMemory
