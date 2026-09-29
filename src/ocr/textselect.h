#pragma once

#include "ocrtypes.h"

#include <QList>
#include <QString>

// Pure helpers for turning selected OCR words into card text. No Tesseract dependency.
namespace textselect {

// Strips surrounding punctuation/quotes, keeps inner hyphens and apostrophes:
// "„Apfel," -> "Apfel", "(Kinder-)" -> "Kinder", "geht's?" -> "geht's"
QString cleanWord(const QString &raw);

// Joins the words with the given indices (any order) in reading order.
// A word ending in "-" at the end of a line is merged with the next selected word
// ("Woh-" + "nung" -> "Wohnung"). Leading/trailing quotes and a trailing , ; : are removed.
QString joinWords(const QList<OcrWord> &words, QList<int> indices);

// Indices of the sentence containing `index`: from after the previous word that ends
// a sentence (. ! ? :) up to and including the next one, never crossing a block.
QList<int> sentenceRange(const QList<OcrWord> &words, int index);

} // namespace textselect
