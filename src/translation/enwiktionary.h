#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// English meanings of a single German word, from the English Wiktionary (en.wiktionary.org, CC BY-SA): the
// "German" section lists the senses with short English glosses ("überzeugt": convinced; staunch, committed).
// Free, no key, and a real dictionary: unlike a machine translation of a lone word it knows the senses of the
// word. Pure request/response helpers (no networking), unit-tested; Translator does the HTTPS call and keeps the
// answer for offline use.
namespace EnWiktionary {

struct Result
{
    QString text;              // the first meaning ("to get dressed")
    QStringList alternatives;  // the following ones ("to dress", "to put on", "to attract")
    QString error;             // non-empty when the page has no meaning of the word
};

// One word: letters (umlauts, ß) and hyphens, no blanks, no digits.
bool isSingleWord(const QString &text);

// API request for the page of `word`, as typed and with the other spelling of the first letter ("essen" the verb,
// "Essen" the noun), redirects followed.
QUrl requestUrl(const QString &word);

// Meanings of the "==German==" section of a page's wikitext, in page order, each once: the short glosses of the
// "#" / "##" sense lines, without labels ({{lb|de|...}}), notes in parentheses and examples. Empty when the page
// has no German section or only inflection entries ("past participle of ...").
QStringList glossesOf(const QString &wikitext);

// Reads the answer of requestUrl(): the page of the typed spelling first, then the other one.
Result parse(const QByteArray &json, const QString &word, int maxAlternatives = 3);

} // namespace EnWiktionary
