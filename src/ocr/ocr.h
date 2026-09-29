#pragma once

#include "ocrtypes.h"

#include <QImage>
#include <QString>

namespace ocr {

// True when the app was built with Tesseract (LB_HAVE_OCR).
bool compiledIn();

// Runs Tesseract on `image` (any format; converted to 8-bit gray) using
// `<tessdataDir>/<lang>.traineddata`. Blocking: call from a worker thread.
// rotationHint: clockwise turn (0/90/180/270) that makes the picture upright, e.g. from the phone's
// tilt at capture; -1 = unknown. The result's `rotation` says which turn was finally used.
OcrResult recognize(const QImage &image, const QString &tessdataDir, const QString &lang = QStringLiteral("deu"),
                    int rotationHint = -1);

// Rebuilds reading order for sparse-text results: fragments on the same visual line are joined
// left to right, closely stacked lines share a block. Pure, no Tesseract.
OcrResult joinLineFragments(const OcrResult &r);

// Adds the text lines of `extra` that hold real text and don't overlap `base`, placed in
// reading position (by height); line/block numbers are renumbered. Pure, no Tesseract.
OcrResult mergeResults(const OcrResult &base, const OcrResult &extra);

// How much real text a result holds (confident words of 3+ letters). Used to pick between passes.
double qualityScore(const OcrResult &result);

#ifdef LB_HAVE_OCR
// Moire/texture/shadow clean-up before OCR (box blur + background and contrast normalisation).
// Same size as the input; returns 8-bit gray.
QImage cleanForOcr(const QImage &gray, int blurRadius);
#endif

// Tesseract version string, or empty if not compiled in.
QString engineVersion();

} // namespace ocr
