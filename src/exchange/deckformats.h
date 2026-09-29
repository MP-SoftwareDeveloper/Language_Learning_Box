#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QUrl>

// Import/export formats for sharing cards. Pure (files in, bytes out), unit-tested.
//  * .lbox  - LearningBox package: a zip with cards.json (+ images/), optional progress
//  * .csv   - text: German, meaning, example (comma/semicolon/tab; Anki, Quizlet, Excel)
//  * .apkg  - Anki package (import only): notes' first fields + pictures
namespace deckformats {

struct Item
{
    QString front;
    QString back;
    QString example;
    QString deck;
    QString image;          // key into Package::images ("" = none)
    bool hasProgress = false;
    int box = 1;            // 1..5, 6 = Learned
    QDateTime dueAt;        // invalid = not scheduled
    int reviews = 0;
    int lapses = 0;
};

struct Package
{
    QString format;                   // "lbox", "csv", "apkg"
    QString title;
    QList<Item> items;
    QHash<QString, QByteArray> images; // key -> image file bytes (JPEG/PNG)
    bool hasProgress = false;
    QString language;                 // learning language of a .lbox ("de"/"en"); "" = unknown
    QString error;                    // non-empty: nothing could be read
};

// ---- writing ----
// `images`: key used in Item::image -> bytes. withProgress=false writes cards as new.
// `language`: the learning language ("de" / "en"), stored so an import can create the right box.
QByteArray writeLbox(const QList<Item> &items, const QHash<QString, QByteArray> &images,
                     const QString &title, bool withProgress, const QString &language = QStringLiteral("de"));
QByteArray writeCsv(const QList<Item> &items); // UTF-8 with BOM, header "German,Meaning,Example"

// ---- reading ----
// Detects the format from the content (zip with cards.json / collection.*, else text).
// tempDir: where an Anki database may be unpacked for reading (must be writable).
Package read(const QByteArray &data, const QString &fileName, const QString &tempDir);
Package readLbox(const QByteArray &zip);
Package readCsv(const QByteArray &text);
Package readApkg(const QByteArray &zip, const QString &tempDir);

// Share links -> direct download links (Google Drive, Dropbox, GitHub). Adds https:// if missing.
QUrl directDownloadUrl(const QString &link);

// Anki/HTML field -> plain text ("<b>Haus</b>&nbsp;[sound:x.mp3]" -> "Haus").
QString plainText(const QString &html);

} // namespace deckformats
