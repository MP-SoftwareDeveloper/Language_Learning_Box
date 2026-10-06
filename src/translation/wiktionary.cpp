#include "wiktionary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

namespace wiktionary {

namespace {

bool isArticle(const QString &w)
{
    const QString l = w.toLower();
    return l == QLatin1String("der") || l == QLatin1String("die") || l == QLatin1String("das");
}

QString capitalised(const QString &w)
{
    return w.isEmpty() ? w : w.at(0).toUpper() + w.mid(1);
}

// "[[Hunde]]" / "Hunde<ref>..</ref>" / "'''x'''" -> "Hunde"; "—" and "-" mean "none".
QString cleaned(QString v)
{
    v.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    v.remove(QStringLiteral("[["));
    v.remove(QStringLiteral("]]"));
    v.remove(QStringLiteral("'''"));
    v.remove(QStringLiteral("''"));
    v = v.trimmed();
    // QLatin1String would read the UTF-8 dashes as Latin-1: compare as real strings
    if (v == QStringLiteral("\u2014") || v == QStringLiteral("-") || v == QStringLiteral("\u2013")
            || v == QStringLiteral("?"))
        return QString();
    return v;
}

// The text of the first "{{Deutsch ... Übersicht" template (braces balanced), "" if none.
QString nounTable(const QString &wikitext)
{
    static const QRegularExpression start(QStringLiteral(R"(\{\{Deutsch (?:Substantiv|Toponym|Eigenname|Nachname|Vorname) Übersicht)"));
    const QRegularExpressionMatch m = start.match(wikitext);
    if (!m.hasMatch())
        return QString();
    int depth = 0;
    const int from = int(m.capturedStart());
    for (int i = from; i + 1 < wikitext.size(); ++i) {
        if (wikitext.at(i) == u'{' && wikitext.at(i + 1) == u'{') {
            ++depth;
            ++i;
        } else if (wikitext.at(i) == u'}' && wikitext.at(i + 1) == u'}') {
            --depth;
            ++i;
            if (depth == 0)
                return wikitext.mid(from, i + 1 - from);
        }
    }
    return wikitext.mid(from); // unbalanced: use the rest
}

} // namespace

QString lemmaOf(const QString &front)
{
    QStringList words = front.simplified().split(u' ', Qt::SkipEmptyParts);
    if (!words.isEmpty() && isArticle(words.first()))
        words.removeFirst();
    if (words.size() != 1)
        return QString();
    QString w = words.first();
    while (!w.isEmpty() && !w.back().isLetterOrNumber())
        w.chop(1);
    while (!w.isEmpty() && !w.front().isLetterOrNumber())
        w.remove(0, 1);
    return w;
}

QUrl requestUrl(const QString &word)
{
    QUrl url(QStringLiteral("https://de.wiktionary.org/w/api.php"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    q.addQueryItem(QStringLiteral("prop"), QStringLiteral("revisions"));
    q.addQueryItem(QStringLiteral("rvprop"), QStringLiteral("content"));
    q.addQueryItem(QStringLiteral("rvslots"), QStringLiteral("main"));
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("redirects"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("titles"), capitalised(word.trimmed()));
    url.setQuery(q);
    return url;
}

Grammar parseWikitext(const QString &wikitext)
{
    Grammar g;
    const QString table = nounTable(wikitext);
    if (table.isEmpty())
        return g;

    static const QRegularExpression genus(QStringLiteral(R"(^\s*\|\s*Genus(?:\s*\d)?\s*=\s*([^\n|]*))"),
                                          QRegularExpression::MultilineOption);
    static const QRegularExpression plural(QStringLiteral(R"(^\s*\|\s*Nominativ Plural(?:\s*\d)?\*{0,2}\s*=\s*([^\n|]*))"),
                                           QRegularExpression::MultilineOption);
    static const QRegularExpression singular(QStringLiteral(R"(^\s*\|\s*Nominativ Singular(?:\s*\d)?\*{0,2}\s*=\s*([^\n|]*))"),
                                             QRegularExpression::MultilineOption);

    for (auto it = genus.globalMatch(table); it.hasNext();) {
        const QString v = cleaned(it.next().captured(1)).toLower();
        if ((v == QLatin1String("m") || v == QLatin1String("f") || v == QLatin1String("n"))
                && !g.genders.contains(v))
            g.genders << v;
    }
    for (auto it = plural.globalMatch(table); it.hasNext();) {
        const QString v = cleaned(it.next().captured(1));
        if (!v.isEmpty() && !g.plurals.contains(v))
            g.plurals << v;
    }
    const QRegularExpressionMatch s = singular.match(table);
    if (s.hasMatch())
        g.lemma = cleaned(s.captured(1));
    return g;
}

Grammar parse(const QByteArray &json, QString *error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Wiktionary: unexpected answer");
        return {};
    }
    const QJsonArray pages = doc.object().value(QStringLiteral("query")).toObject().value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty())
        return {};
    const QJsonObject page = pages.first().toObject();
    if (page.value(QStringLiteral("missing")).toBool())
        return {};
    const QString content = page.value(QStringLiteral("revisions")).toArray().first().toObject()
                                .value(QStringLiteral("slots")).toObject().value(QStringLiteral("main")).toObject()
                                .value(QStringLiteral("content")).toString();
    Grammar g = parseWikitext(content);
    if (g.valid() && g.lemma.isEmpty())
        g.lemma = page.value(QStringLiteral("title")).toString();
    return g;
}

QString article(const QString &gender)
{
    if (gender == QLatin1String("m")) return QStringLiteral("der");
    if (gender == QLatin1String("f")) return QStringLiteral("die");
    if (gender == QLatin1String("n")) return QStringLiteral("das");
    return QString();
}

QString front(const Grammar &g)
{
    if (g.genders.isEmpty() || g.lemma.isEmpty())
        return QString();
    return article(g.genders.first()) + QLatin1Char(' ') + g.lemma;
}

QString pluralText(const Grammar &g)
{
    if (g.plurals.isEmpty())
        return QString();
    return QStringLiteral("die ") + g.plurals.join(QStringLiteral(" / "));
}

QString pluralLine(const Grammar &g)
{
    return g.plurals.isEmpty() ? QString() : QStringLiteral("Pl. ") + g.plurals.join(QStringLiteral(" / "));
}

QString encode(const Grammar &g)
{
    if (!g.valid())
        return QString();
    return g.genders.join(u'/') + QLatin1Char('|') + g.plurals.join(u'|');
}

Grammar decode(const QString &text, const QString &lemma)
{
    Grammar g;
    if (text.isEmpty())
        return g;
    const QStringList parts = text.split(u'|');
    if (!parts.first().isEmpty())
        g.genders = parts.first().split(u'/');
    for (int i = 1; i < parts.size(); ++i)
        if (!parts.at(i).isEmpty())
            g.plurals << parts.at(i);
    g.lemma = lemma;
    return g;
}

} // namespace wiktionary
