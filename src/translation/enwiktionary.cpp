#include "enwiktionary.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrlQuery>

namespace EnWiktionary {

namespace {

constexpr int kMaxItemChars = 40; // longer "glosses" are explanations ("A board with casters used to ..."), not meanings

// The lines of the "==German==" section (the page has one section per language).
QStringList germanLines(const QString &wikitext)
{
    static const QRegularExpression languageHeading(QStringLiteral("^==[^=].*[^=]==$")); // level 2 only
    QStringList out;
    bool inGerman = false;
    const QStringList lines = wikitext.split(u'\n');
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (languageHeading.match(t).hasMatch()) {
            inGerman = t == QLatin1String("==German==");
            continue;
        }
        if (inGerman)
            out.append(line);
    }
    return out;
}

// Index just after the "}}" that closes the "{{" at `start` (braces balanced), or -1.
qsizetype templateEnd(const QString &s, qsizetype start)
{
    int depth = 0;
    for (qsizetype i = start; i + 1 < s.size(); ++i) {
        if (s.at(i) == u'{' && s.at(i + 1) == u'{') {
            ++depth;
            ++i;
        } else if (s.at(i) == u'}' && s.at(i + 1) == u'}') {
            --depth;
            ++i;
            if (depth == 0)
                return i + 1;
        }
    }
    return -1;
}

// Templates are labels, notes and grammar ({{lb|de|reflexive}}, {{+aux|de|haben}}, {{gl|...}}): dropped. Link
// templates ({{l|en|scoundrel}}) are text: kept.
QString withoutTemplates(const QString &s)
{
    QString out;
    qsizetype i = 0;
    while (i < s.size()) {
        if (s.at(i) == u'{' && i + 1 < s.size() && s.at(i + 1) == u'{') {
            const qsizetype end = templateEnd(s, i);
            if (end < 0)
                break; // unbalanced: the rest is not a gloss
            const QStringList parts = s.mid(i + 2, end - i - 4).split(u'|');
            const QString name = parts.value(0).trimmed();
            if (name == QLatin1String("l") || name == QLatin1String("link"))
                out += parts.size() >= 4 && !parts.at(3).isEmpty() ? parts.at(3) : parts.value(2);
            i = end;
        } else {
            out += s.at(i++);
        }
    }
    return out;
}

// "[[get dressed]]" -> "get dressed", "[[touch|touched]]" -> "touched", "''x''" -> "x", comments and tags gone.
QString plainGloss(const QString &line)
{
    static const QRegularExpression comment(QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    static const QRegularExpression link(QStringLiteral(R"(\[\[(?:[^\]|]*\|)?([^\]]*)\]\])"));
    static const QRegularExpression note(QStringLiteral(R"(\s*\([^)]*\))"));
    QString s = withoutTemplates(line);
    s.remove(comment);
    s.remove(tag);
    s.replace(link, QStringLiteral("\\1"));
    s.remove(QStringLiteral("'''"));
    s.remove(QStringLiteral("''"));
    s.remove(note); // "to dress (someone)" -> "to dress"
    s = s.simplified();
    while (s.endsWith(u'.') || s.endsWith(u':'))
        s.chop(1);
    return s.simplified();
}

QStringList commaParts(const QString &item)
{
    QStringList parts = item.split(u',');
    for (QString &p : parts)
        p = p.trimmed().toCaseFolded();
    return parts;
}

} // namespace

bool isSingleWord(const QString &text)
{
    static const QRegularExpression word(QStringLiteral(R"(^\p{L}[\p{L}\-]+$)"));
    return word.match(text.trimmed()).hasMatch();
}

QUrl requestUrl(const QString &word)
{
    const QString w = word.trimmed();
    QString other = w;
    if (!w.isEmpty())
        other = (w.at(0).isUpper() ? w.at(0).toLower() : w.at(0).toUpper()) + w.mid(1);
    QUrl url(QStringLiteral("https://en.wiktionary.org/w/api.php"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("query"));
    q.addQueryItem(QStringLiteral("prop"), QStringLiteral("revisions"));
    q.addQueryItem(QStringLiteral("rvprop"), QStringLiteral("content"));
    q.addQueryItem(QStringLiteral("rvslots"), QStringLiteral("main"));
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("formatversion"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("redirects"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("titles"), other == w ? w : w + QLatin1Char('|') + other);
    url.setQuery(q);
    return url;
}

QStringList glossesOf(const QString &wikitext)
{
    // A sense line is "# gloss" or "## gloss"; "#:" examples and "#*" quotes have no blank after the "#".
    static const QRegularExpression senseLine(QStringLiteral(R"(^#{1,2}\s+(.*)$)"));
    QStringList items;
    QStringList knownParts;
    for (const QString &line : germanLines(wikitext)) {
        const QRegularExpressionMatch m = senseLine.match(line.trimmed());
        if (!m.hasMatch())
            continue;
        const QString gloss = plainGloss(m.captured(1));
        for (QString item : gloss.split(u';')) {
            item = item.trimmed();
            if (item.isEmpty() || item.size() > kMaxItemChars)
                continue;
            if (item.contains(u'{') || item.contains(u'}') || item.contains(u'|') || item.contains(u'='))
                continue;
            // each meaning once: "dog" after "dog, hound" and "to dress oneself in" after "to dress" add nothing
            const QStringList parts = commaParts(item);
            const bool known = std::all_of(parts.cbegin(), parts.cend(),
                                           [&](const QString &p) { return knownParts.contains(p); });
            const QString folded = item.toCaseFolded();
            const bool extension = std::any_of(items.cbegin(), items.cend(), [&](const QString &c) {
                return folded.startsWith(c.toCaseFolded() + QLatin1Char(' '));
            });
            if (known || extension)
                continue;
            items.append(item);
            knownParts += parts;
        }
    }
    return items;
}

Result parse(const QByteArray &json, const QString &word, int maxAlternatives)
{
    Result r;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("unexpected response");
        return r;
    }
    const QJsonArray pages = doc.object().value(QStringLiteral("query")).toObject().value(QStringLiteral("pages")).toArray();
    QList<QJsonObject> ordered; // the typed spelling first
    for (const QJsonValue &v : pages) {
        const QJsonObject page = v.toObject();
        if (page.value(QStringLiteral("title")).toString() == word.trimmed())
            ordered.prepend(page);
        else
            ordered.append(page);
    }
    for (const QJsonObject &page : std::as_const(ordered)) {
        const QJsonArray revisions = page.value(QStringLiteral("revisions")).toArray();
        if (page.contains(QStringLiteral("missing")) || revisions.isEmpty())
            continue;
        const QString content = revisions.at(0).toObject().value(QStringLiteral("slots")).toObject()
                                    .value(QStringLiteral("main")).toObject().value(QStringLiteral("content")).toString();
        const QStringList items = glossesOf(content);
        if (items.isEmpty())
            continue;
        r.text = items.first();
        r.alternatives = items.mid(1, maxAlternatives);
        return r;
    }
    r.error = QStringLiteral("no meaning");
    return r;
}

} // namespace EnWiktionary
