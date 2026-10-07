#include "wiktionary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
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

// Words linked in the section that follows a heading template such as "{{Weibliche Wortformen}}"
// (up to the next template or heading line): ":[1] [[Lehrerin]]" -> "Lehrerin".
QStringList linkedWords(const QString &wikitext, const QString &heading)
{
    QStringList out;
    const int at = wikitext.indexOf(QStringLiteral("{{") + heading + QStringLiteral("}}"));
    if (at < 0)
        return out;
    int from = wikitext.indexOf(u'\n', at);
    if (from < 0)
        return out;
    int end = from + 1;
    for (;;) { // the section ends at the next line that starts a template or a heading
        const int nl = wikitext.indexOf(u'\n', end);
        const QString line = wikitext.mid(end, nl < 0 ? -1 : nl - end);
        if (line.startsWith(QStringLiteral("{{")) || line.startsWith(QStringLiteral("==")))
            break;
        if (nl < 0) {
            end = wikitext.size();
            break;
        }
        end = nl + 1;
    }
    static const QRegularExpression link(QStringLiteral(R"(\[\[([^\]|#:]+)(?:\|[^\]]*)?\]\])"));
    for (auto it = link.globalMatch(wikitext.mid(from, end - from)); it.hasNext() && out.size() < 3;) {
        const QString w = it.next().captured(1).trimmed();
        if (!w.isEmpty() && !out.contains(w))
            out << w;
    }
    return out;
}

// "Hunde" is a plural form: its page points at the noun ("{{Grundformverweis Dekl|Hund}}" or
// "... Plural-Form des Substantivs [[Hund]]"). Only capitalised targets: nouns.
QString singularTarget(const QString &wikitext)
{
    static const QRegularExpression ref(QStringLiteral(R"(\{\{Grundformverweis Dekl\|([^}|]+))"));
    static const QRegularExpression prose(QStringLiteral(R"(Plural[^\n]*Substantivs\s*\[\[([^\]|#]+))"));
    for (const QRegularExpression *re : {&ref, &prose}) {
        const QRegularExpressionMatch m = re->match(wikitext);
        if (m.hasMatch()) {
            const QString t = m.captured(1).trimmed();
            if (!t.isEmpty() && t.at(0).isUpper())
                return t;
        }
    }
    return QString();
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

QUrl suggestUrl(const QString &language, const QString &prefix, int limit)
{
    QUrl url(QStringLiteral("https://%1.wiktionary.org/w/api.php").arg(language));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("opensearch"));
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("namespace"), QStringLiteral("0"));
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    q.addQueryItem(QStringLiteral("search"), prefix.trimmed());
    url.setQuery(q);
    return url;
}

QStringList suggestVariants(const QString &prefix)
{
    const QString p = prefix.trimmed();
    QStringList out;
    if (p.isEmpty())
        return out;
    out << p;
    const QString upper = p.at(0).toUpper() + p.mid(1);
    const QString lower = p.at(0).toLower() + p.mid(1);
    for (const QString &v : {upper, lower})
        if (!out.contains(v))
            out << v;
    return out;
}

QStringList parseSuggestions(const QByteArray &json)
{
    QStringList out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray() || doc.array().size() < 2)
        return out;
    for (const QJsonValue &v : doc.array().at(1).toArray()) {
        const QString t = v.toString().trimmed();
        if (!t.isEmpty() && !t.contains(u':') && !t.contains(u'/') && !out.contains(t))
            out << t;
    }
    return out;
}

QStringList mergeSuggestions(const QList<QStringList> &lists, const QString &prefix, int max)
{
    // Words that really start with what was typed (ignoring case) first, each spelling once
    const QString p = prefix.trimmed().toCaseFolded();
    QStringList out;
    QSet<QString> seen;
    for (int pass = 0; pass < 2; ++pass)
        for (const QStringList &l : lists)
            for (const QString &w : l) {
                const bool starts = w.toCaseFolded().startsWith(p);
                if ((pass == 0) != starts || seen.contains(w))
                    continue;
                seen.insert(w);
                out << w;
                if (out.size() >= max)
                    return out;
            }
    return out;
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
    // A plural form of a noun wins over a noun of the same spelling on the page ("Mauern": plural of "Mauer",
    // but also "das Mauern", the activity)
    const QString target = singularTarget(wikitext);
    if (table.isEmpty() || !target.isEmpty()) {
        g.singularOf = target;
        return g;
    }
    g.masculine = linkedWords(wikitext, QStringLiteral("M\u00e4nnliche Wortformen"));
    g.feminine = linkedWords(wikitext, QStringLiteral("Weibliche Wortformen"));

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

QUrl existsUrl(const QString &title)
{
    QUrl url(QStringLiteral("https://de.wiktionary.org/w/api.php"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    q.addQueryItem(QStringLiteral("prop"), QStringLiteral("info"));
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("titles"), title.trimmed());
    url.setQuery(q);
    return url;
}

bool pageExists(const QByteArray &json, QString *error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Wiktionary: unexpected answer");
        return false;
    }
    const QJsonArray pages = doc.object().value(QStringLiteral("query")).toObject().value(QStringLiteral("pages")).toArray();
    if (pages.isEmpty())
        return false;
    return !pages.first().toObject().value(QStringLiteral("missing")).toBool();
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
    if (g.valid() && g.singularOf.isEmpty() && g.lemma.isEmpty())
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
    if (g.genders.isEmpty() || g.lemma.isEmpty() || !g.singularOf.isEmpty())
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
    return g.plurals.isEmpty() ? QString() : QStringLiteral("Plural ") + g.plurals.join(QStringLiteral(" / "));
}

Grammar withSingular(const Grammar &pluralPage, const Grammar &noun)
{
    Grammar g = noun;
    g.plurals.clear();
    g.singularOf = pluralPage.singularOf.isEmpty() ? noun.lemma : pluralPage.singularOf;
    if (g.lemma.isEmpty())
        g.lemma = g.singularOf;
    return g;
}

QStringList formLines(const Grammar &g)
{
    QStringList lines;
    if (!g.singularOf.isEmpty()) {
        const QString art = g.genders.isEmpty() ? QString() : article(g.genders.first()) + QLatin1Char(' ');
        lines << QStringLiteral("Singular ") + art + g.lemma;
    } else if (!g.plurals.isEmpty()) {
        lines << pluralLine(g);
    }
    if (!g.masculine.isEmpty()) {
        lines << QStringLiteral("Mask. der ") + g.masculine.first();
        if (!g.masculinePlural.isEmpty())
            lines << QStringLiteral("Mask. Plural ") + g.masculinePlural;
    }
    if (!g.feminine.isEmpty()) {
        lines << QStringLiteral("Fem. die ") + g.feminine.first();
        if (!g.femininePlural.isEmpty())
            lines << QStringLiteral("Fem. Plural ") + g.femininePlural;
    }
    return lines;
}

QString encode(const Grammar &g)
{
    if (!g.valid())
        return QString();
    return QStringList{g.genders.join(u'/'), g.plurals.join(u'|'), g.singularOf, g.masculine.join(u'|'),
                       g.feminine.join(u'|'), g.lemma, g.masculinePlural, g.femininePlural}.join(u'\n');
}

Grammar decode(const QString &text, const QString &lemma)
{
    Grammar g;
    if (text.isEmpty())
        return g;
    const QStringList f = text.split(u'\n');
    const auto list = [&](int i, QChar sep) {
        return f.value(i).isEmpty() ? QStringList() : f.value(i).split(sep);
    };
    g.genders = list(0, u'/');
    g.plurals = list(1, u'|');
    g.singularOf = f.value(2);
    g.masculine = list(3, u'|');
    g.feminine = list(4, u'|');
    g.lemma = f.value(5).isEmpty() ? lemma : f.value(5);
    g.masculinePlural = f.value(6);
    g.femininePlural = f.value(7);
    return g;
}

} // namespace wiktionary
