#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QTextDocument;

namespace paragraphalign {
// True when the first letter of the text is from a right-to-left script (Persian/Arabic, Hebrew).
// Digits, spaces and punctuation before it are skipped; text without a letter counts as left-to-right.
bool startsRightToLeft(const QString &text);
// Aligns every paragraph of the document by its own writing direction: Persian lines to the right,
// English / German lines to the left (a Qt text box otherwise aligns all lines like the first one).
// Returns true when a paragraph was changed.
bool alignParagraphs(QTextDocument *doc);
}

// QML: ParagraphAlign { document: textArea.textDocument } and call apply() whenever the text changes.
class ParagraphAlign : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
public:
    explicit ParagraphAlign(QObject *parent = nullptr) : QObject(parent) {}
    QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    Q_INVOKABLE void apply();
signals:
    void documentChanged();
private:
    QQuickTextDocument *m_document = nullptr;
    bool m_busy = false; // a change of the alignment makes the text box report a change again
};
