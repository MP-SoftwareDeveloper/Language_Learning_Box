#pragma once

#include <QDateTime>
#include <QString>

struct Card
{
    int id = -1;
    QString front;   // German word or sentence
    QString back;    // free text (Persian, English, notes ...)
    QString example; // example sentence
    QString exampleTranslation; // Persian translation of `example` (word packs only; not stored in the DB)
    int box = 1;
    QDateTime dueAt; // invalid = not scheduled (learned)
    int reviews = 0;
    int lapses = 0;
    QString image;   // picture file name in AppDataLocation/images; empty = none
    QString deck;    // origin, e.g. "Netzwerk neu A1 · K3"; empty for hand-made cards
};
