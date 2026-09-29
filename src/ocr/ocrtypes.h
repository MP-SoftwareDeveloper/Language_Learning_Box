#pragma once

#include <QList>
#include <QRect>
#include <QSize>
#include <QString>

// One recognized word, in image pixel coordinates of the processed image.
struct OcrWord
{
    QString text;   // as recognized, including punctuation ("Apfel," / „Hallo")
    QRect box;
    int line = 0;   // running text-line number (reading order)
    int block = 0;  // running block/paragraph number
    float confidence = 0; // 0..100
};

struct OcrResult
{
    QList<OcrWord> words; // reading order
    QSize imageSize;      // of the image the boxes refer to (after `rotation`)
    int rotation = 0;     // clockwise degrees (0/90/270) the input was turned to read it upright
    QString error;        // empty = success
};
