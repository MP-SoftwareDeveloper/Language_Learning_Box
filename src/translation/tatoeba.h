#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

// Example sentences from Tatoeba (tatoeba.org, CC BY 2.0 FR): real German sentences that
// contain a word, with a translation when one exists. Free, no key. Pure request/response
// helpers (no networking), unit-tested; Translator does the HTTPS call.
namespace tatoeba {

struct Example
{
    QString text;        // German sentence
    QString translation; // in the requested language, "" if Tatoeba has none
};

// Tatoeba language code: "fa" -> "pes" (Iranian Persian), "en" -> "eng", "de" -> "deu".
QString languageCode(const QString &target);

// Search sentences in `source` ("de" German, "en" English) containing `word` (German
// "der/die/das", English "to/the/a/an" are dropped), shortest first, with translations in
// `target` ("fa"/"en") included when they exist.
QUrl searchUrl(const QString &word, const QString &target, int limit = 10, const QString &source = QStringLiteral("de"));

// The word that is searched for: "das Brot" -> "Brot", English "to go" -> "go".
QString searchWord(const QString &front, const QString &source = QStringLiteral("de"));

// Parses the answer; keeps sentences that really contain the word (case-insensitive), up to
// `max`, sentences with a translation first, no duplicates, max 12 words each.
QList<Example> parse(const QByteArray &json, const QString &word, const QString &target, int max = 3,
                     QString *error = nullptr, const QString &source = QStringLiteral("de"));

} // namespace tatoeba
