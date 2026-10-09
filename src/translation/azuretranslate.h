#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// Optional translator: Azure AI Translator (v3). Used first when the user has entered their own key in
// Settings; without a key the app keeps using Google and MyMemory. Free tier F0: 2 million characters a
// month, after which Azure refuses (HTTP 403 / 429) instead of charging. Pure helpers (no networking),
// unit-tested; Translator does the HTTPS calls.
//
//   /translate          the translation of a word or sentence
//   /dictionary/lookup  other translations of a single word, best first (not every language pair has one;
//                       the caller just goes without alternatives then)
namespace AzureTranslate {

inline constexpr const char *kKeyHeader = "Ocp-Apim-Subscription-Key";
inline constexpr const char *kRegionHeader = "Ocp-Apim-Subscription-Region";

struct Result
{
    QString text;              // translation
    QStringList alternatives;  // dictionary lookup only: other translations, without `text`
    QString error;             // non-empty when there is no usable answer
};

// POST url (JSON body from requestBody). `source` / `target`: "de", "en", "fa".
QUrl translateUrl(const QString &source, const QString &target);
QUrl lookupUrl(const QString &source, const QString &target);

// [{"Text": "..."}]
QByteArray requestBody(const QString &text);

// Is it a single word (no blanks)? Only those are looked up in the dictionary.
bool isSingleWord(const QString &text);

// Reads the /translate answer.
Result parseTranslation(const QByteArray &json);

// Reads the /dictionary/lookup answer: up to `maxAlternatives` translations other than `main`, by Azure's
// confidence. Empty list when there is nothing (no error: this is only a bonus).
QStringList parseLookup(const QByteArray &json, const QString &main, int maxAlternatives = 3);

// Message of an Azure error body ({"error":{"code":401000,"message":"..."}}), or an empty string.
QString errorMessage(const QByteArray &json);

} // namespace AzureTranslate
