#include "starterdeck.h"

#include <QStringList>

bool parseStarterDeck(const QString &text, QList<StarterCard> *out, QString *error)
{
    out->clear();
    const QStringList lines = text.split(u'\n');
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (line.trimmed().isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList f = line.split(u'\t');
        if (f.size() != 8) {
            if (error)
                *error = QStringLiteral("line %1: %2 columns, expected 8").arg(i + 1).arg(f.size());
            return false;
        }
        StarterCard c;
        c.category = f.at(0).trimmed();
        c.german = f.at(1).trimmed();
        c.plural = f.at(2).trimmed();
        c.english = f.at(3).trimmed();
        c.persian = f.at(4).trimmed();
        c.exampleDe = f.at(5).trimmed();
        c.exampleEn = f.at(6).trimmed();
        c.exampleFa = f.at(7).trimmed();
        if (c.plural == QLatin1String("-"))
            c.plural.clear();
        if (c.german.isEmpty() || c.english.isEmpty() || c.persian.isEmpty()) {
            if (error)
                *error = QStringLiteral("line %1: German, English and Persian are required").arg(i + 1);
            return false;
        }
        out->append(c);
    }
    return true;
}

QString starterBack(const StarterCard &c, const QString &language)
{
    QString back = language == QLatin1String("fa") ? c.persian : c.english;
    if (!c.plural.isEmpty())
        back += QStringLiteral("\nPl. ") + c.plural;
    return back;
}
