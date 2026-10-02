#pragma once

#include "exchange/deckformats.h"

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

// Import & export of cards (QML singleton): LearningBox files (.lbox), CSV/text and Anki
// packages, from a file or a link. Import is two steps: open (file/link) -> `preview` shows
// what is inside -> applyImport() with the chosen options.
class DeckExchange : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // {format, title, count, newCount, existingCount, hasProgress, sample: [{front, back}], error}
    Q_PROPERTY(QVariantMap preview READ preview NOTIFY previewChanged)

public:
    static DeckExchange *create(QQmlEngine *, QJSEngine *);

    bool busy() const { return m_busy; }
    QVariantMap preview() const { return m_preview; }

    // Export. format: "lbox" or "csv"; scope: 0 = every card of ALL learning boxes, > 0 = all cards of the
    // learning box with that id, -1 = favorite cards (★) of all learning boxes.
    // Returns {ok, count, error}. `target` comes from a save dialog (file:// or content://).
    Q_INVOKABLE QVariantMap exportCards(const QUrl &target, const QString &format, bool withProgress, int scope);
    Q_INVOKABLE QString suggestedFileName(const QString &format, int scope = 0) const;
    // Export into the app's own folder and open the system share sheet (WhatsApp, Telegram,
    // e-mail, Drive ...). On desktop the folder with the file is opened instead.
    // Returns {ok, count, file, error}.
    Q_INVOKABLE QVariantMap shareCards(const QString &format, bool withProgress, int scope);

    // Import step 1: read a file (file:// or content://) or download a link; fills `preview`.
    Q_INVOKABLE void openFile(const QUrl &source);
    Q_INVOKABLE void openLink(const QString &link);
    // Import step 2. duplicates: "skip" or "update"; keepProgress: use boxes/dates from the file
    // (LearningBox files with progress); box: box for new cards otherwise; swap: file has the
    // meaning first; newLearningBox: if not empty, a new learning box with this name is created
    // and selected first. Returns {added, updated, skipped, error}.
    Q_INVOKABLE QVariantMap applyImport(const QString &duplicates, bool keepProgress, int box, bool swap,
                                        const QString &newLearningBox = QString());
    Q_INVOKABLE void clearPreview();

signals:
    void busyChanged();
    void previewChanged();

private:
    QVariantMap boxInfo(int id) const; // collections() entry of a learning box, {} if none
    explicit DeckExchange(QObject *parent = nullptr);
    void setPackage(deckformats::Package package, const QString &fallbackTitle);
    void setBusy(bool b);

    QNetworkAccessManager *m_nam = nullptr;
    deckformats::Package m_package;
    QVariantMap m_preview;
    bool m_busy = false;
};
