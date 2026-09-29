#pragma once

#include "ocrtypes.h"

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QUrl>

// Azure AI Vision - Image Analysis 4.0 "Read" (print and handwriting). Free tier F0: 5000
// pictures/month, 20/minute; beyond that Azure refuses (429/403) instead of charging.
// Pure request/response helpers (no networking), unit-tested; CloudOcr does the HTTPS call.
namespace azureread {

// endpoint: the resource's "Endpoint" from the Azure portal, e.g.
// "https://my-vision.cognitiveservices.azure.com/" (trailing slash optional). Empty URL if invalid.
QUrl analyzeUrl(const QString &endpoint);

inline constexpr const char *kKeyHeader = "Ocp-Apim-Subscription-Key";

// Turns the answer into OcrWords in reading order (block/line numbers like Tesseract's).
// Words without a Latin letter or digit (e.g. Persian notes next to German text) are skipped.
// Sets `error` on an API error (bad key, quota, bad picture ...).
OcrResult parse(const QByteArray &json, const QSize &imageSize);

} // namespace azureread
