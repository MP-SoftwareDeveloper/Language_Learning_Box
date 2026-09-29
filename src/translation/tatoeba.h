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

// Tatoeba language code for our target language: "fa" -> "pes" (Iranian Persian), "en" -> "eng".
QString languageCode(const QString &target);

// Search German sentences containing `word` (an article "der/die/das" is dropped), shortest
// first, with translations in `target` ("fa"/"en") included when they exist.
QUrl searchUrl(const QString &word, const QString &target, int limit = 10);

// The word that is searched for "das Brot" -> "Brot".
QString searchWord(const QString &front);

// Parses the answer; keeps sentences that really contain the word (case-insensitive), up to
// `max`, sentences with a translation first, no duplicates, max 12 words each.
QList<Example> parse(const QByteArray &json, const QString &word, const QString &target, int max = 3,
                     QString *error = nullptr);

} // namespace tatoeba
