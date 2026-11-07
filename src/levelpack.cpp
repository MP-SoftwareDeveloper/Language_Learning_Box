#include "levelpack.h"

#include <QStringList>

bool parseLevelPacks(QStringView text, QList<LevelPack> *out, QString *error)
{
    out->clear();
    const QStringList lines = text.toString().split(u'\n');
    auto fail = [&](int line, const QString &what) {
        if (error)
            *error = QStringLiteral("line %1: %2").arg(line).arg(what);
        return false;
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (line.trimmed().isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList f = line.split(u'\t');
        const int ln = i + 1;

        if (f.first() == QLatin1String("@level")) {
            if (f.size() < 3)
                return fail(ln, QStringLiteral("@level needs an id and a title"));
            LevelPack p;
            p.id = f.at(1).trimmed();
            p.title = f.at(2).trimmed();
            p.subtitle = f.value(3).trimmed();
            p.language = p.id.startsWith(QLatin1String("sv-")) ? QStringLiteral("sv")
                         : p.id.startsWith(QLatin1String("en-")) ? QStringLiteral("en") : QStringLiteral("de");
            p.cefr = p.id.mid(p.language == QLatin1String("de") ? 0 : 3, 2).toUpper();
            if (p.id.isEmpty() || p.title.isEmpty())
                return fail(ln, QStringLiteral("@level needs an id and a title"));
            out->append(p);
            continue;
        }

        auto findLevel = [&](const QString &id) -> LevelPack * {
            for (LevelPack &p : *out)
                if (p.id == id)
                    return &p;
            return nullptr;
        };

        if (f.first() == QLatin1String("@chapter")) {
            if (f.size() < 4)
                return fail(ln, QStringLiteral("@chapter needs a level, a number and a title"));
            LevelPack *p = findLevel(f.at(1).trimmed());
            bool ok = false;
            const int number = f.at(2).trimmed().toInt(&ok);
            if (!p || !ok || number <= 0 || f.at(3).trimmed().isEmpty())
                return fail(ln, QStringLiteral("bad @chapter line"));
            LevelChapter c;
            c.number = number;
            c.title = f.at(3).trimmed();
            p->chapters.append(c);
            continue;
        }

        if (f.size() != 9)
            return fail(ln, QStringLiteral("%1 columns, expected 9").arg(f.size()));
        LevelPack *p = findLevel(f.at(0).trimmed());
        if (!p)
            return fail(ln, QStringLiteral("unknown level \"%1\"").arg(f.at(0)));
        bool ok = false;
        const int number = f.at(1).trimmed().toInt(&ok);
        LevelChapter *chapter = nullptr;
        for (LevelChapter &c : p->chapters)
            if (ok && c.number == number)
                chapter = &c;
        if (!chapter)
            return fail(ln, QStringLiteral("unknown chapter \"%1\"").arg(f.at(1)));

        LevelWord w;
        w.german = f.at(2).trimmed();
        w.plural = f.at(3).trimmed();
        w.english = f.at(4).trimmed();
        w.persian = f.at(5).trimmed();
        w.exampleDe = f.at(6).trimmed();
        w.exampleEn = f.at(7).trimmed();
        w.exampleFa = f.at(8).trimmed();
        if (w.plural == QLatin1String("-"))
            w.plural.clear();
        if (w.german.isEmpty() || w.english.isEmpty() || w.persian.isEmpty())
            return fail(ln, QStringLiteral("German, English and Persian are required"));
        chapter->words.append(w);
    }
    if (out->isEmpty()) {
        if (error)
            *error = QStringLiteral("no levels found");
        return false;
    }
    return true;
}

QString levelWordBack(const LevelWord &w, const QString &language)
{
    QString back = language == QLatin1String("fa") ? w.persian : w.english;
    if (!w.plural.isEmpty())
        back += QStringLiteral("\nPl. ") + w.plural;
    return back;
}

namespace {
// "der Satz" -> article "der" (lower case), bare "Satz" (as written); no article -> article "".
void splitArticle(const QString &text, QString *article, QString *bare)
{
    const QString t = text.simplified();
    const int blank = t.indexOf(QLatin1Char(' '));
    const QString first = blank > 0 ? t.left(blank).toCaseFolded() : QString();
    if (first == QLatin1String("der") || first == QLatin1String("die") || first == QLatin1String("das")) {
        *article = first;
        *bare = t.mid(blank + 1).simplified();
    } else {
        article->clear();
        *bare = t;
    }
}
} // namespace

const LevelWord *findGermanWord(const QList<LevelPack> &packs, const QString &word)
{
    QString article, bare;
    splitArticle(word, &article, &bare);
    if (bare.isEmpty())
        return nullptr;
    // First the spelling as typed ("morgen" is tomorrow, "Morgen" the morning), then any case ("satz")
    for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (const LevelPack &pack : packs) {
            if (pack.language != QLatin1String("de"))
                continue;
            for (const LevelChapter &chapter : pack.chapters) {
                for (const LevelWord &w : chapter.words) {
                    QString a, b;
                    splitArticle(w.german, &a, &b);
                    // "die Satz" is not "der Satz": a typed article has to agree with the entry's
                    if (b.compare(bare, cs) == 0 && (article.isEmpty() || a.isEmpty() || a == article))
                        return &w;
                }
            }
        }
    }
    return nullptr;
}
