#include "wordpack.h"

#include <algorithm>

namespace {

bool setError(QString *error, int line, const QString &what)
{
    if (error)
        *error = QStringLiteral("line %1: %2").arg(line).arg(what);
    return false;
}

WordPackChapter *findOrAdd(WordPack *pack, int number)
{
    for (auto &c : pack->chapters)
        if (c.number == number)
            return &c;
    pack->chapters.append(WordPackChapter{number, {}, {}});
    return &pack->chapters.last();
}

} // namespace

bool parseWordPack(QStringView text, WordPack *pack, QString *error)
{
    *pack = WordPack{};
    int lineNo = 0;
    for (QStringView raw : text.split(u'\n')) {
        ++lineNo;
        QStringView line = raw;
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (line.trimmed().isEmpty() || line.startsWith(u'#'))
            continue;

        const QList<QStringView> f = line.split(u'\t');
        if (line.startsWith(u'@')) {
            if (f.size() != 3)
                return setError(error, lineNo, QStringLiteral("directive needs 3 fields"));
            if (f[0] == u"@pack") {
                pack->id = f[1].toString();
                pack->title = f[2].toString();
            } else if (f[0] == u"@chapter") {
                bool ok = false;
                const int n = f[1].toInt(&ok);
                if (!ok || n <= 0)
                    return setError(error, lineNo, QStringLiteral("bad chapter number"));
                findOrAdd(pack, n)->title = f[2].toString();
            } else {
                return setError(error, lineNo, QStringLiteral("unknown directive"));
            }
            continue;
        }

        if (f.size() != 4 && f.size() != 5)
            return setError(error, lineNo, QStringLiteral("expected 4 or 5 fields, got %1").arg(f.size()));
        bool ok = false;
        const int n = f[0].toInt(&ok);
        if (!ok || n <= 0)
            return setError(error, lineNo, QStringLiteral("bad chapter number"));
        if (f[1].trimmed().isEmpty())
            return setError(error, lineNo, QStringLiteral("empty German field"));

        Card c;
        c.front = f[1].trimmed().toString();
        // "Persian meaning · Pl. die Äpfel · ..." -> meaning on line 1, German grammar note on
        // line 2, so each paragraph gets its own text direction (RTL vs LTR) when displayed.
        const QStringList parts = f[2].trimmed().toString().split(QStringLiteral(" \u00B7 "));
        c.back = parts.first();
        if (parts.size() > 1)
            c.back += u'\n' + parts.mid(1).join(QStringLiteral(" \u00B7 "));
        c.example = f[3].trimmed().toString();
        if (f.size() == 5)
            c.exampleTranslation = f[4].trimmed().toString();
        findOrAdd(pack, n)->cards.append(c);
    }

    if (pack->id.isEmpty())
        return setError(error, lineNo, QStringLiteral("missing @pack"));
    std::sort(pack->chapters.begin(), pack->chapters.end(),
              [](const auto &a, const auto &b) { return a.number < b.number; });
    return true;
}
