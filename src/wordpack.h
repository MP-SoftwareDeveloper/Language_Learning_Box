#pragma once

#include "card.h"

#include <QList>
#include <QString>
#include <QStringView>

// A built-in vocabulary pack, parsed from a UTF-8 TSV resource:
//   # comment
//   @pack     <id>      <title>
//   @chapter  <number>  <title>
//   <chapter> <front>   <back>   <example>  [<example in Persian>]
struct WordPackChapter
{
    int number = 0;
    QString title;
    QList<Card> cards;
};

struct WordPack
{
    QString id;
    QString title;
    QList<WordPackChapter> chapters; // ordered by number

    const WordPackChapter *chapter(int number) const
    {
        for (const auto &c : chapters)
            if (c.number == number)
                return &c;
        return nullptr;
    }
};

// Returns false and sets *error (with a line number) on malformed input.
bool parseWordPack(QStringView text, WordPack *pack, QString *error = nullptr);
