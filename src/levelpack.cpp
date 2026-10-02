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
            p.cefr = p.id.left(2).toUpper();
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
