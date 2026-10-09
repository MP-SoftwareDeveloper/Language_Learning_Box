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

// Is `text` written in a script that fits the target language? MyMemory's translation memory sometimes answers
// in a wrong language ("Bildern" -> "图像"): English / German must not contain Chinese, Cyrillic, Arabic ...,
// Persian must contain Arabic-script letters and no Chinese or Cyrillic.
bool plausibleFor(const QString &text, const QString &target);

// Reads the JSON answer. `sourceText` is what was asked: an answer that only repeats it (MyMemory's way of
// saying "don't know"), quota warnings and error statuses all become `error`. Answers in a wrong script
// (plausibleFor) are skipped: the next fitting entry of the translation memory is used instead.
Result parse(const QByteArray &json, const QString &sourceText, const QString &target, int maxAlternatives = 3);

} // namespace MyMemory
