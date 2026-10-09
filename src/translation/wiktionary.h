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
    QStringList masculine; // male form of a female noun ("Lehrerin" -> "Lehrer"); usually empty
    QStringList feminine;  // female form of a male noun ("Lehrer" -> "Lehrerin"); usually empty
    QString masculinePlural; // plural of the first masculine form ("Ärzte"); filled in by a second lookup
    QString femininePlural;  // plural of the first feminine form ("Lehrerinnen")
    // Set when the looked-up word is the plural form of a noun ("Hunde" -> "Hund"): the gender, lemma,
    // masculine and feminine then describe that singular noun and `plurals` is empty.
    QString singularOf;

    // False when the word is not a noun (no entry, or an entry without a noun table).
    bool valid() const { return !genders.isEmpty() || !plurals.isEmpty() || !singularOf.isEmpty(); }
};

// The word to look up for a card front: "der Hund" -> "Hund", "Hund" -> "Hund". Empty for text that
// is not a single word ("guten Morgen", "").
QString lemmaOf(const QString &front);

// Wiktionary API request for the page of `word` (first letter capitalised: nouns are).
QUrl requestUrl(const QString &word);

// Does a page with exactly this title exist (case-sensitive, no redirects)? For a word typed in lowercase:
// "gehen" has a page (a verb), "kellner" has none (a noun typed without its capital letter).
QUrl existsUrl(const QString &title);
bool pageExists(const QByteArray &json, QString *error = nullptr);

// Possible singulars of a plural noun, most likely first ("Nudeln" -> "Nudel", "Frauen" -> "Frau", "Hände" ->
// "Hand", "Kinder" -> "Kind", "Lehrerinnen" -> "Lehrerin"). Only guesses: each one is checked against Wiktionary
// (its plural must be the word that was looked up). Empty for words that are not capitalised (not nouns).
QStringList singularCandidates(const QString &plural);

// Word suggestions while typing ("hau" -> Haus, Hausaufgabe, ...): Wiktionary's prefix search in the given
// language ("de", "en", "fa"). Titles are case-sensitive there, so the caller asks for each spelling of the
// first letter (suggestVariants) and merges the answers (mergeSuggestions).
QUrl suggestUrl(const QString &language, const QString &prefix, int limit = 8);
QStringList suggestVariants(const QString &prefix);
QStringList parseSuggestions(const QByteArray &json);
QStringList mergeSuggestions(const QList<QStringList> &lists, const QString &prefix, int max = 8);

// Reads the answer of requestUrl(). A page without a noun table gives an invalid Grammar and no
// error; a body that is not the expected JSON sets *error.
// `article` ("der" / "die" / "das", or "") is the article the word comes with: a page can hold several entries
// for one spelling ("Reis": der Reis = rice, das Reis = twig, and the plural of "Real"; "Reise": die Reise and
// the plural of "Reis"). The entry that fits the article is the one meant.
Grammar parse(const QByteArray &json, QString *error = nullptr, const QString &article = QString());

// The noun table ("{{Deutsch Substantiv Übersicht ...}}") of a page's wikitext (see parse() for `article`).
Grammar parseWikitext(const QString &wikitext, const QString &article = QString());

QString article(const QString &gender);    // "m" -> "der", "f" -> "die", "n" -> "das"

// "der Hund" (first gender's article + lemma); "" when there is no gender (plural-only nouns).
QString front(const Grammar &g);
// "die Hunde" / "die Hunde / Hündinnen"; "" when there is no plural.
QString pluralText(const Grammar &g);
// "Pl. Hunde" - the plural line (no article) that goes on its own line on the card's back; "" without a plural.
QString pluralLine(const Grammar &g);

// The plural-form page "Hunde" names its noun "Hund" (singularOf only): the noun's own Grammar plus that.
Grammar withSingular(const Grammar &pluralPage, const Grammar &noun);

// Lines for the card's back, each on its own line, label first: "Pl. Hunde" (or "Sg. der Hund" when the word
// is itself a plural form), then "Singular der Arzt", "Plural Ärzte", "Singular die Ärztin", "Plural Ärztinnen" (no gender word: the card shows the gender by colour)
// when the noun has such forms.
QStringList formLines(const Grammar &g);

// For the offline cache: one text with the fields on separate lines ("" = nothing) <-> Grammar.
QString encode(const Grammar &g);
Grammar decode(const QString &text, const QString &lemma);

} // namespace wiktionary
