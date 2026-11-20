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

// The text of every template that starts like `start` (braces balanced), in page order: one per entry.
QStringList tablesOf(const QString &wikitext, const QRegularExpression &start)
{
    QStringList out;
    int pos = 0;
    for (;;) {
        const QRegularExpressionMatch m = start.match(wikitext, pos);
        if (!m.hasMatch())
            return out;
        int depth = 0;
        const int from = int(m.capturedStart());
        int end = -1;
        for (int i = from; i + 1 < wikitext.size(); ++i) {
            if (wikitext.at(i) == u'{' && wikitext.at(i + 1) == u'{') {
                ++depth;
                ++i;
            } else if (wikitext.at(i) == u'}' && wikitext.at(i + 1) == u'}') {
                --depth;
                ++i;
                if (depth == 0) {
                    end = i + 1;
                    break;
                }
            }
        }
        if (end < 0) { // unbalanced: use the rest
            out << wikitext.mid(from);
            return out;
        }
        out << wikitext.mid(from, end - from);
        pos = end;
    }
}

// The text of every "{{Deutsch ... Übersicht" template of a noun, in page order: one per entry.
QStringList nounTables(const QString &wikitext)
{
    static const QRegularExpression start(QStringLiteral(R"(\{\{Deutsch (?:Substantiv|Toponym|Eigenname|Nachname|Vorname) Übersicht)"));
    return tablesOf(wikitext, start);
}

// "{{Deutsch Verb Übersicht" of a verb
QStringList verbTables(const QString &wikitext)
{
    static const QRegularExpression start(QStringLiteral(R"(\{\{Deutsch Verb Übersicht)"));
    return tablesOf(wikitext, start);
}

// Weather verbs and the like have no "ich": "Es hat geregnet."
bool isImpersonal(const QString &infinitive)
{
    static const QSet<QString> verbs{
        QStringLiteral("regnen"),  QStringLiteral("schneien"), QStringLiteral("hageln"),
        QStringLiteral("donnern"), QStringLiteral("blitzen"),  QStringLiteral("nieseln"),
        QStringLiteral("gewittern"), QStringLiteral("d\u00e4mmern"), QStringLiteral("geschehen"),
        QStringLiteral("gelingen"), QStringLiteral("misslingen")};
    return verbs.contains(infinitive.trimmed().toLower());
}

QString auxiliaryWord(const QString &auxiliary) // third person: "sein" -> "ist", "haben" -> "hat"
{
    return auxiliary == QLatin1String("sein") ? QStringLiteral("ist") : QStringLiteral("hat");
}

// "m" / "f" / "n" of a noun table ("Genus=m", "Genus 2=f", ...)
QStringList tableGenders(const QString &table)
{
    static const QRegularExpression genus(QStringLiteral(R"(^\s*\|\s*Genus(?:\s*\d)?\s*=\s*([^\n|]*))"),
                                          QRegularExpression::MultilineOption);
    QStringList out;
    for (auto it = genus.globalMatch(table); it.hasNext();) {
        const QString v = cleaned(it.next().captured(1)).toLower();
        if ((v == QLatin1String("m") || v == QLatin1String("f") || v == QLatin1String("n")) && !out.contains(v))
            out << v;
    }
    return out;
}

// Does the table have a plural of its own ("Nominativ Plural=Reisen"; "—" means none)?
bool tableHasPlural(const QString &table)
{
    static const QRegularExpression plural(QStringLiteral(R"(^\s*\|\s*Nominativ Plural(?:\s*\d)?\*{0,2}\s*=\s*([^\n|]*))"),
                                           QRegularExpression::MultilineOption);
    for (auto it = plural.globalMatch(table); it.hasNext();)
        if (!cleaned(it.next().captured(1)).isEmpty())
            return true;
    return false;
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

QStringList singularCandidates(const QString &plural)
{
    const QString p = plural.trimmed();
    QStringList out;
    if (p.size() < 4 || !p.at(0).isUpper())
        return out;
    const auto add = [&](const QString &s) {
        if (s.size() >= 3 && s != p && !out.contains(s))
            out << s;
    };
    // The stem as it is, and with the last umlaut undone ("Händ" -> "Hand", "Häus" -> "Haus")
    const auto stem = [&](int cut) {
        const QString s = p.chopped(cut);
        add(s);
        for (int i = int(s.size()) - 1; i > 0; --i) {
            const QChar c = s.at(i);
            const QChar r = c == u'\u00e4' ? u'a' : c == u'\u00f6' ? u'o' : c == u'\u00fc' ? u'u' : QChar();
            if (!r.isNull()) {
                QString t = s;
                t[i] = r;
                add(t);
                break;
            }
        }
    };
    if (p.endsWith(QStringLiteral("nen")))
        stem(3);                       // Lehrerinnen -> Lehrerin
    if (p.endsWith(QStringLiteral("en"))) {
        stem(2);                       // Frauen -> Frau, Studenten -> Student
        stem(1);                       // Namen -> Name
    }
    if (p.endsWith(u'n'))
        stem(1);                       // Nudeln -> Nudel
    if (p.endsWith(QStringLiteral("er")))
        stem(2);                       // Kinder -> Kind, Bücher -> Buch
    if (p.endsWith(u'e'))
        stem(1);                       // Hände -> Hand, Hunde -> Hund
    if (p.endsWith(u's'))
        stem(1);                       // Autos -> Auto
    return out.mid(0, 5);
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

QUrl requestUrl(const QString &word, bool keepCase)
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
    q.addQueryItem(QStringLiteral("titles"), keepCase ? word.trimmed() : capitalised(word.trimmed()));
    url.setQuery(q);
    return url;
}

Grammar parseVerbWikitext(const QString &wikitext)
{
    Grammar g;
    const QStringList tables = verbTables(wikitext);
    if (tables.isEmpty())
        return g;
    const QString table = tables.first(); // a page with several verbs (umfahren): the first entry
    static const QRegularExpression participle(QStringLiteral(R"(^\s*\|\s*Partizip II(?:\s*\d)?\s*=\s*([^\n|]*))"),
                                               QRegularExpression::MultilineOption);
    static const QRegularExpression auxiliary(QStringLiteral(R"(^\s*\|\s*Hilfsverb\*?\s*=\s*([^\n|]*))"),
                                              QRegularExpression::MultilineOption);
    static const QRegularExpression separators(QStringLiteral(R"([,;/]|\boder\b)"));
    static const QRegularExpression parenthesised(QStringLiteral(R"(\([^)]*\))"));
    static const QRegularExpression auxWord(QStringLiteral(R"(\b(haben|sein)\b)"));

    // "gegangen", "gesendet, gesandt": at most two forms, each a single word
    const auto participlesOf = [&](const QString &t) {
        QStringList forms;
        const QRegularExpressionMatch p = participle.match(t);
        if (p.hasMatch())
            for (const QString &part : cleaned(p.captured(1)).split(separators, Qt::SkipEmptyParts)) {
                const QString form = part.simplified();
                if (!form.isEmpty() && !form.contains(u' ') && !forms.contains(form) && forms.size() < 2)
                    forms << form;
            }
        return forms;
    };
    const QStringList forms = participlesOf(table);
    if (forms.isEmpty())
        return g;
    g.participle = forms.join(QStringLiteral(" / "));

    // "sein", "haben, sein", "sein/haben", "haben (landschaftlich: sein)" -> the words outside brackets, in order
    // A word with two meanings can have a table for each (passieren: "happen" with sein, "pass through" with haben):
    // the auxiliaries of every table with the same participle count, sein first (the common one: es ist passiert)
    for (const QString &t : tables) {
        if (participlesOf(t) != forms)
            continue;
        for (auto m = auxiliary.globalMatch(t); m.hasNext();) {
            const QString aux = cleaned(m.next().captured(1)).remove(parenthesised).toLower();
            for (auto it = auxWord.globalMatch(aux); it.hasNext();) {
                const QString v = it.next().captured(1);
                if (!g.auxiliaries.contains(v))
                    g.auxiliaries << v;
            }
        }
    }
    if (g.auxiliaries.size() > 1 && g.auxiliaries.first() != QLatin1String("sein"))
        g.auxiliaries = QStringList{QStringLiteral("sein"), QStringLiteral("haben")};
    g.examples = verbExamples(wikitext);
    return g;
}

Grammar parseVerb(const QByteArray &json, QString *error)
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
    Grammar g = parseVerbWikitext(content);
    if (g.isVerb())
        g.lemma = page.value(QStringLiteral("title")).toString();
    return g;
}

namespace {
const QStringList &separablePrefixes()
{
    static const QStringList list = {
        QStringLiteral("ab"), QStringLiteral("an"), QStringLiteral("auf"), QStringLiteral("aus"), QStringLiteral("bei"),
        QStringLiteral("dabei"), QStringLiteral("dazu"), QStringLiteral("ein"), QStringLiteral("empor"),
        QStringLiteral("fest"), QStringLiteral("fort"), QStringLiteral("her"), QStringLiteral("heraus"),
        QStringLiteral("herein"), QStringLiteral("herunter"), QStringLiteral("hin"), QStringLiteral("hinaus"),
        QStringLiteral("hinein"), QStringLiteral("hinzu"), QStringLiteral("los"), QStringLiteral("mit"),
        QStringLiteral("nach"), QStringLiteral("statt"), QStringLiteral("teil"), QStringLiteral("vor"),
        QStringLiteral("voran"), QStringLiteral("voraus"), QStringLiteral("vorbei"), QStringLiteral("weg"),
        QStringLiteral("weiter"), QStringLiteral("zu"), QStringLiteral("zurück"), QStringLiteral("zusammen")};
    return list;
}
QString deUmlaut(QString s)
{
    s.replace(QChar(0x00E4), QLatin1Char('a')).replace(QChar(0x00F6), QLatin1Char('o')).replace(QChar(0x00FC), QLatin1Char('u'));
    return s;
}
} // namespace

QStringList verbExamples(const QString &wikitext)
{
    static const QString marker = QStringLiteral("{{Deutsch Verb \u00dcbersicht");
    const int start = int(wikitext.indexOf(marker));
    if (start < 0)
        return {};
    const int b = int(wikitext.indexOf(QStringLiteral("{{Beispiele}}"), start));
    if (b < 0)
        return {};
    const int other = int(wikitext.indexOf(QStringLiteral("=== {{Wortart"), start)); // the next entry's section
    if (other >= 0 && other < b)
        return {};
    static const QRegularExpression blockEnd(QStringLiteral(R"(\n(?=\{\{|=))"));
    const int from = b + 13;
    const QRegularExpressionMatch e = blockEnd.match(wikitext, from);
    const QString block = wikitext.mid(from, e.hasMatch() ? int(e.capturedStart()) - from : -1);

    static const QRegularExpression label(QStringLiteral(R"(^:+\s*(?:\[[^\]]*\]\s*)?)"));
    static const QRegularExpression refs(QStringLiteral(R"(<ref[^>]*/>|<ref[^>]*>.*?</ref>)"),
                                         QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression templates(QStringLiteral(R"(\{\{[^{}]*\}\})"));
    static const QRegularExpression piped(QStringLiteral(R"(\[\[[^\]|]*\|([^\]]*)\]\])"));
    QStringList out;
    for (const QString &raw : block.split(u'\n')) {
        if (!raw.startsWith(u':'))
            continue;
        QString t = raw;
        t.remove(label);
        t.remove(refs);
        t.remove(templates);
        t.replace(piped, QStringLiteral("\\1"));
        t = cleaned(t); // links, bold, italic, tags
        while (!t.isEmpty() && QStringLiteral("\u201E\u201C\u201D\u201A\u2018\u2019\u00AB\u00BB\"' ").contains(t.front()))
            t.remove(0, 1);
        while (!t.isEmpty() && QStringLiteral("\u201E\u201C\u201D\u201A\u2018\u2019\u00AB\u00BB\"' ").contains(t.back()))
            t.chop(1);
        // a whole sentence that stands on its own: several words, not too long, no source line, no markup left
        t = t.simplified();
        const int words = int(t.split(u' ', Qt::SkipEmptyParts).size());
        if (words < 2 || words > 12 || t.size() < 8 || t.contains(QStringLiteral("http")) || t.contains(u'[')
                || t.contains(u'{') || t.contains(u'|') || t.contains(u'<') || out.contains(t))
            continue;
        if (!t.endsWith(u'.') && !t.endsWith(u'!') && !t.endsWith(u'?'))
            t += u'.';
        out << t;
        if (out.size() == 3)
            break;
    }
    return out;
}

QString separablePhrase(const QString &text)
{
    QString t = text.simplified().toLower();
    while (!t.isEmpty() && (t.back() == u'.' || t.back() == u'!' || t.back() == u'?' || t.back() == u',' || t.back() == u';'))
        t.chop(1);
    QStringList words = t.split(u' ', Qt::SkipEmptyParts);
    static const QStringList pronouns = {QStringLiteral("ich"), QStringLiteral("du"), QStringLiteral("er"), QStringLiteral("sie"),
                                         QStringLiteral("es"), QStringLiteral("wir"), QStringLiteral("ihr")};
    if (words.size() == 3 && pronouns.contains(words.first()))
        words.removeFirst();
    if (words.size() != 2 || words.first().size() < 2 || isArticle(words.first()) || pronouns.contains(words.first()))
        return QString();
    // "füge hinzu" (form + prefix) or "an rufen" (prefix + infinitive, written apart)
    const bool formThenPrefix = separablePrefixes().contains(words.last());
    const bool prefixThenInfinitive = separablePrefixes().contains(words.first()) && words.last().size() >= 5
                                      && (words.last().endsWith(u"en") || words.last().endsWith(u"ln") || words.last().endsWith(u"rn"))
                                      && !isArticle(words.last());
    if (!formThenPrefix && !prefixThenInfinitive)
        return QString();
    for (const QString &w : std::as_const(words))
        for (const QChar c : w)
            if (!c.isLetter())
                return QString();
    return words.join(u' ');
}

QStringList separableCandidates(const QString &phrase)
{
    const QStringList t = phrase.split(u' ');
    if (t.size() != 2)
        return {};
    const QString &f = t.first();
    const QString &prefix = t.last();
    if (!separablePrefixes().contains(prefix)) // "an rufen": the prefix first, then the infinitive
        return {f + prefix};
    QStringList stems;
    const auto add = [&](const QString &s) {
        if (s.size() >= 3 && !stems.contains(s))
            stems << s;
    };
    if (f.endsWith(u"en"))                       // wir / sie machen auf
        add(f);
    if (f.endsWith(u"le"))                       // sammle ein -> sammeln
        add(f.chopped(2) + QStringLiteral("eln"));
    if (f.endsWith(u"re"))                       // wandre aus -> wandern
        add(f.chopped(2) + QStringLiteral("ern"));
    if (f.endsWith(u'e'))                        // füge hinzu -> fügen
        add(f + u'n');
    if (f.endsWith(u"st")) {                     // machst auf, fährst ab
        const QString b = f.chopped(2);
        add(b + QStringLiteral("en"));
        add(deUmlaut(b) + QStringLiteral("en"));
        add(b + u'n');
    }
    if (f.endsWith(u't')) {                      // macht auf, fährt ab, arbeitet mit
        const QString b = f.chopped(1);
        add(b + QStringLiteral("en"));
        add(deUmlaut(b) + QStringLiteral("en"));
        add(b + u'n');
    }
    if (!f.endsWith(u'e'))                       // mach auf, komm mit
        add(f + QStringLiteral("en"));
    QStringList out;
    for (const QString &s : std::as_const(stems))
        out << prefix + s;
    while (out.size() > 5)
        out.removeLast();
    return out;
}

QString participleText(const Grammar &g)
{
    if (!g.isVerb())
        return QString();
    if (g.auxiliaries.isEmpty())
        return g.participle;
    // One participle with two auxiliaries ("ist gefahren / hat gefahren"); two participles share the first one
    if (g.auxiliaries.size() > 1 && !g.participle.contains(QLatin1Char('/'))) {
        QStringList out;
        for (const QString &a : g.auxiliaries)
            out << auxiliaryWord(a) + QLatin1Char(' ') + g.participle;
        return out.join(QStringLiteral(" / "));
    }
    return auxiliaryWord(g.auxiliaries.first()) + QLatin1Char(' ') + g.participle;
}

QString participleExample(const Grammar &g)
{
    if (!g.isVerb() || g.auxiliaries.isEmpty())
        return QString();
    const QString part = g.participle.section(QStringLiteral(" / "), 0, 0);
    const bool sein = g.auxiliaries.first() == QLatin1String("sein");
    if (isImpersonal(g.lemma))
        return QStringLiteral("Es ") + (sein ? QStringLiteral("ist ") : QStringLiteral("hat ")) + part + QLatin1Char('.');
    return QStringLiteral("Ich ") + (sein ? QStringLiteral("bin ") : QStringLiteral("habe ")) + part + QLatin1Char('.');
}

Grammar parseWikitext(const QString &wikitext, const QString &articleHint)
{
    Grammar g;
    const QStringList tables = nounTables(wikitext);
    QString table = tables.value(0);
    // A plural form of a noun wins over a noun of the same spelling on the page ("Mauern": plural of "Mauer",
    // but also "das Mauern", the activity) - unless that noun has a plural of its own: "Reise" is also the plural
    // of "Reis", but "die Reise" (plural "Reisen") is a countable noun, so the noun wins.
    QString target = singularTarget(wikitext);
    // With the article the word comes with, the entry of that gender is the word ("der Reis" -> the rice entry,
    // "die Reise" -> the noun, not the plural of "Reis"); "die" with no such entry is a plural ("die Mauern").
    const QString gender = articleHint.trimmed().toLower() == QLatin1String("der") ? QStringLiteral("m")
                         : articleHint.trimmed().toLower() == QLatin1String("die") ? QStringLiteral("f")
                         : articleHint.trimmed().toLower() == QLatin1String("das") ? QStringLiteral("n") : QString();
    bool entryFound = false;
    if (!gender.isEmpty())
        for (const QString &t : tables)
            if (tableGenders(t).contains(gender)) {
                table = t;
                entryFound = true;
                break;
            }
    if (entryFound)
        target.clear();
    else if (gender.isEmpty() && !target.isEmpty() && !table.isEmpty() && tableHasPlural(table))
        target.clear(); // no article: a noun with a plural of its own wins
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

Grammar parse(const QByteArray &json, QString *error, const QString &article)
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
    Grammar g = parseWikitext(content, article);
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
    if (g.isVerb()) {
        QString line = QStringLiteral("Partizip II ") + participleText(g);
        const QString example = participleExample(g);
        if (!example.isEmpty()) {
            line += QStringLiteral(" \u00b7 ") + example;
            if (!g.infinitive.isEmpty()) // typed as "füge hinzu": the verb is "hinzufügen"
                line += QStringLiteral(" \u00b7 ") + g.infinitive;
        }
        lines << line;
        return lines;
    }
    if (!g.singularOf.isEmpty()) {
        const QString art = g.genders.isEmpty() ? QString() : article(g.genders.first()) + QLatin1Char(' ');
        lines << QStringLiteral("Singular ") + art + g.lemma;
    } else if (!g.plurals.isEmpty()) {
        lines << pluralLine(g);
    }
    if (!g.masculine.isEmpty()) {
        lines << QStringLiteral("Singular der ") + g.masculine.first();
        if (!g.masculinePlural.isEmpty())
            lines << QStringLiteral("Plural ") + g.masculinePlural;
    }
    if (!g.feminine.isEmpty()) {
        lines << QStringLiteral("Singular die ") + g.feminine.first();
        if (!g.femininePlural.isEmpty())
            lines << QStringLiteral("Plural ") + g.femininePlural;
    }
    return lines;
}

QString encode(const Grammar &g)
{
    if (!g.valid())
        return QString();
    return QStringList{g.genders.join(u'/'), g.plurals.join(u'|'), g.singularOf, g.masculine.join(u'|'),
                       g.feminine.join(u'|'), g.lemma, g.masculinePlural, g.femininePlural,
                       g.participle, g.auxiliaries.join(u'/'), g.infinitive,
                       g.examples.join(QChar(0x2016))}.join(u'\n');
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
    g.participle = f.value(8);
    g.auxiliaries = list(9, u'/');
    g.infinitive = f.value(10);
    g.examples = list(11, QChar(0x2016));
    return g;
}

} // namespace wiktionary
