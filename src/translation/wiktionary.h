#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// Gender (der / die / das) and plural of German nouns, from the German Wiktionary
// (de.wiktionary.org, CC BY-SA). Free, no key. Pure request/response helpers (no networking),
// unit-tested; Translator does the HTTPS call and keeps the answers for offline use.
namespace wiktionary {

struct Grammar
{
    QStringList genders; // "m", "f", "n" - usually one; "der/das Joghurt" has two
    QStringList plurals; // nominative plural forms without article: "Hunde"; empty = no plural
    QString lemma;       // the word as written in the dictionary: "Hund"

    // False when the word is not a noun (no entry, or an entry without a noun table).
    bool valid() const { return !genders.isEmpty() || !plurals.isEmpty(); }
};

// The word to look up for a card front: "der Hund" -> "Hund", "Hund" -> "Hund". Empty for text that
// is not a single word ("guten Morgen", "").
QString lemmaOf(const QString &front);

// Wiktionary API request for the page of `word` (first letter capitalised: nouns are).
QUrl requestUrl(const QString &word);

// Reads the answer of requestUrl(). A page without a noun table gives an invalid Grammar and no
// error; a body that is not the expected JSON sets *error.
Grammar parse(const QByteArray &json, QString *error = nullptr);

// The noun table ("{{Deutsch Substantiv Übersicht ...}}") of a page's wikitext.
Grammar parseWikitext(const QString &wikitext);

QString article(const QString &gender);    // "m" -> "der", "f" -> "die", "n" -> "das"

// "der Hund" (first gender's article + lemma); "" when there is no gender (plural-only nouns).
QString front(const Grammar &g);
// "die Hunde" / "die Hunde / Hündinnen"; "" when there is no plural.
QString pluralText(const Grammar &g);
// "Pl. die Hunde" - the plural line that goes on its own line on the card's back; "" without a plural.
QString pluralLine(const Grammar &g);

// For the offline cache: "m|Hunde" / "m/n|Joghurts|Joghurte" / "" (nothing) <-> Grammar.
QString encode(const Grammar &g);
Grammar decode(const QString &text, const QString &lemma);

} // namespace wiktionary
