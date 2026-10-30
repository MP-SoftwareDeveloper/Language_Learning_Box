#pragma once

#include <QList>
#include <QString>
#include <QStringView>

// Level packs: German and Swedish A1 / A2 vocabulary in levels, each level split into themed chapters.
// Swedish level ids start with "sv-" (e.g. "sv-a1-1"); the "german" fields then hold the Swedish word.
// Parsed from a UTF-8 TSV resource:
//   # comment
//   @level    <id>      <title>   <subtitle>
//   @chapter  <levelId> <number>  <title>
//   <levelId> <chapter> <German> <plural or "-"> <English> <Persian> <example de> <example en> <example fa>
struct LevelWord
{
    QString german;
    QString plural;     // "" = none
    QString english;
    QString persian;
    QString exampleDe;
    QString exampleEn;
    QString exampleFa;
};

struct LevelChapter
{
    int number = 0;
    QString title;
    QList<LevelWord> words;
};

struct LevelPack
{
    QString id;         // "a1-1"
    QString title;      // "A1 · Level 1"
    QString subtitle;
    QString cefr;       // "A1" / "A2" (from the id)
    QString language;   // learning language of the pack: "de" or "sv" (from the id)
    QList<LevelChapter> chapters;

    const LevelChapter *chapter(int number) const
    {
        for (const auto &c : chapters)
            if (c.number == number)
                return &c;
        return nullptr;
    }
    int totalWords() const
    {
        int n = 0;
        for (const auto &c : chapters)
            n += int(c.words.size());
        return n;
    }
};

// Returns false and sets *error (with a line number) on malformed input.
bool parseLevelPacks(QStringView text, QList<LevelPack> *out, QString *error = nullptr);

// Back side of a card for the meaning language ("fa" = Persian, anything else = English),
// with the plural on a second line for nouns: "house\nPl. Häuser".
QString levelWordBack(const LevelWord &word, const QString &language);
