#pragma once

#include <QList>
#include <QString>

// The 100 starter cards shipped with the app (data/starter/starter_100.tsv): common A1 words
// with English and Persian meanings and an example sentence in all three languages.
// Pure parser, unit-tested (tst_starterdeck).
struct StarterCard
{
    QString category;
    QString german;    // with article for nouns, e.g. "der Tisch"
    QString plural;    // e.g. "die Tische"; empty for words without plural
    QString english;
    QString persian;
    QString exampleDe;
    QString exampleEn;
    QString exampleFa;
};

// Lines: '#' comments, empty lines ignored; otherwise 8 tab-separated columns
// category, German, plural, English, Persian, example (de), example (en), example (fa).
// Returns false (and *error) on a malformed line.
bool parseStarterDeck(const QString &text, QList<StarterCard> *out, QString *error = nullptr);

// Back side of a card in `language` ("fa" or "en"): meaning, plus "Pl. <plural>" on a second line.
QString starterBack(const StarterCard &c, const QString &language);
