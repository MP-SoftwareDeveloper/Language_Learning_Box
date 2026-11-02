#include "paragraphalign.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace paragraphalign {

bool startsRightToLeft(const QString &text)
{
    for (const QChar c : text) {
        if (!c.isLetter())
            continue;
        const ushort u = c.unicode();
        return (u >= 0x0590 && u <= 0x08FF) || (u >= 0xFB1D && u <= 0xFDFF) || (u >= 0xFE70 && u <= 0xFEFF);
    }
    return false;
}

bool alignParagraphs(QTextDocument *doc)
{
    if (!doc)
        return false;
    bool changed = false;
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
        const Qt::Alignment want = (startsRightToLeft(b.text()) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignAbsolute;
        QTextBlockFormat f = b.blockFormat();
        if (f.hasProperty(QTextFormat::BlockAlignment) && f.alignment() == want)
            continue;
        f.setAlignment(want);
        QTextCursor c(b);
        c.joinPreviousEditBlock(); // one undo step together with the typing that caused it
        c.setBlockFormat(f);
        c.endEditBlock();
        changed = true;
    }
    return changed;
}

} // namespace paragraphalign

void ParagraphAlign::setDocument(QQuickTextDocument *document)
{
    if (document == m_document)
        return;
    m_document = document;
    emit documentChanged();
    apply();
}

void ParagraphAlign::apply()
{
    if (m_busy || !m_document)
        return;
    m_busy = true;
    paragraphalign::alignParagraphs(m_document->textDocument());
    m_busy = false;
}
