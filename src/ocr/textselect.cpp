#include "textselect.h"

#include <algorithm>

namespace textselect {

namespace {

bool isEdgePunct(QChar c)
{
    // Everything that is not a letter/digit may be trimmed from the edges,
    // except an apostrophe inside a word (handled by only trimming edges).
    return !c.isLetterOrNumber();
}

bool endsSentence(const QString &w)
{
    QString t = w;
    while (!t.isEmpty() && (t.back() == u'"' || t.back() == u'“' || t.back() == u'”' || t.back() == u'«'
                            || t.back() == u'»' || t.back() == u')' || t.back() == u'\''))
        t.chop(1);
    return t.endsWith(u'.') || t.endsWith(u'!') || t.endsWith(u'?') || t.endsWith(u':');
}

} // namespace

QString cleanWord(const QString &raw)
{
    qsizetype b = 0, e = raw.size();
    while (b < e && isEdgePunct(raw.at(b)))
        ++b;
    while (e > b && isEdgePunct(raw.at(e - 1)))
        --e;
    return raw.mid(b, e - b);
}

QString joinWords(const QList<OcrWord> &words, QList<int> indices)
{
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    QString out;
    int prevLine = -1;
    bool pendingHyphen = false;
    for (int i : std::as_const(indices)) {
        if (i < 0 || i >= words.size())
            continue;
        const OcrWord &w = words.at(i);
        QString t = w.text.trimmed();
        if (t.isEmpty())
            continue;
        if (pendingHyphen && w.line != prevLine) {
            out += t; // "Woh-" + "nung" across a line break
        } else {
            if (!out.isEmpty())
                out += u' ';
            out += t;
        }
        pendingHyphen = false;
        // A trailing hyphen after a letter at the end of a line = word split by hyphenation.
        const bool lastOnLine = (i + 1 >= words.size()) || words.at(i + 1).line != w.line;
        if (lastOnLine && out.size() >= 2 && out.back() == u'-' && out.at(out.size() - 2).isLetter()) {
            out.chop(1);
            pendingHyphen = true;
        }
        prevLine = w.line;
    }
    if (pendingHyphen)
        out += u'-';

    // Drop unmatched quotes around the whole selection.
    static const QString quotes = QStringLiteral("\"„“”«»‚‘’");
    while (!out.isEmpty() && quotes.contains(out.front()))
        out.remove(0, 1);
    while (!out.isEmpty() && quotes.contains(out.back()))
        out.chop(1);
    // A trailing comma/semicolon/colon belongs to the page, not to the selection;
    // sentence-ending . ! ? are kept.
    while (!out.isEmpty() && (out.back() == u',' || out.back() == u';' || out.back() == u':'))
        out.chop(1);
    return out.trimmed();
}

QList<int> sentenceRange(const QList<OcrWord> &words, int index)
{
    if (index < 0 || index >= words.size())
        return {};
    const int block = words.at(index).block;
    int first = index;
    while (first > 0 && words.at(first - 1).block == block && !endsSentence(words.at(first - 1).text))
        --first;
    int last = index;
    while (!endsSentence(words.at(last).text) && last + 1 < words.size() && words.at(last + 1).block == block)
        ++last;
    QList<int> out;
    for (int i = first; i <= last; ++i)
        out.append(i);
    return out;
}

} // namespace textselect
