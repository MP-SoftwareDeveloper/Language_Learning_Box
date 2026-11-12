#include "mymemory.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrlQuery>

namespace MyMemory {

namespace {
// MyMemory sometimes returns HTML entities in the translated text.
QString decodeEntities(QString s)
{
    s.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s.trimmed();
}

// Translation-memory entries can carry the inline tags of translation tools (<g id="1">1</g><g id="2">accessible</g>),
// as real tags or as entities. Tags become spaces and a list number in front ("1 accessible") is dropped, unless the
// asked text starts with a digit too. Text without tags is returned unchanged.
QString cleanMarkup(const QString &text, const QString &asked)
{
    static const QRegularExpression tag(QStringLiteral("</?[A-Za-z][^>]*>"));
    if (!text.contains(tag))
        return text;
    QString s = text;
    s.replace(tag, QStringLiteral(" "));
    s = s.simplified();
    static const QRegularExpression leadingNumber(QStringLiteral("^\\d+[.)]?\\s+"));
    if (asked.isEmpty() || !asked.at(0).isDigit())
        s.remove(leadingNumber);
    static const QRegularExpression onlyNumber(QStringLiteral("^\\d+[.)]?$"));
    if ((asked.isEmpty() || !asked.at(0).isDigit()) && onlyNumber.match(s).hasMatch())
        return QString();
    return s; // empty when only tags / a number were there
}
} // namespace

bool hasMarkup(const QString &text)
{
    static const QRegularExpression tag(QStringLiteral("</?[A-Za-z][^>]*>"));
    return text.contains(tag);
}

bool fits(const QString &text)
{
    return text.toUtf8().size() <= 480;
}

QUrl requestUrl(const QString &text, const QString &source, const QString &target)
{
    QUrl url(QStringLiteral("https://api.mymemory.translated.net/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), text);
    q.addQueryItem(QStringLiteral("langpair"), source + QLatin1Char('|') + target);
    url.setQuery(q);
    return url;
}

bool plausibleFor(const QString &text, const QString &target)
{
    if (hasMarkup(text))
        return false;
    bool arabic = false;
    for (const QChar ch : text) {
        const char32_t u = ch.unicode();
        const bool cjk = (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xAC00 && u <= 0xD7AF) || (u >= 0xF900 && u <= 0xFAFF);
        const bool cyrillic = u >= 0x0400 && u <= 0x052F;
        const bool otherScript = (u >= 0x0590 && u <= 0x05FF) /*Hebrew*/ || (u >= 0x0900 && u <= 0x0DFF) /*Indic*/
                                 || (u >= 0x0E00 && u <= 0x0EFF) /*Thai, Lao*/ || (u >= 0x1000 && u <= 0x109F);
        const bool arabicScript = (u >= 0x0600 && u <= 0x06FF) || (u >= 0x0750 && u <= 0x077F) || (u >= 0xFB50 && u <= 0xFEFF);
        if (cjk || cyrillic || otherScript)
            return false;
        if (arabicScript) {
            if (target != QLatin1String("fa"))
                return false;
            arabic = true;
        }
    }
    return target != QLatin1String("fa") || arabic;
}

Result parse(const QByteArray &json, const QString &sourceText, const QString &target, int maxAlternatives)
{
    Result r;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        r.error = QStringLiteral("unexpected response");
        return r;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("quotaFinished")).toBool()) {
        r.error = QStringLiteral("MyMemory daily limit reached");
        return r;
    }
    // responseStatus is a number or a string ("200")
    const int status = root.value(QStringLiteral("responseStatus")).toVariant().toInt();
    if (status != 200) {
        const QString d = root.value(QStringLiteral("responseDetails")).toString().trimmed();
        r.error = d.isEmpty() ? QStringLiteral("MyMemory status %1").arg(status) : d;
        return r;
    }

    const QJsonObject data = root.value(QStringLiteral("responseData")).toObject();
    const QString asked = sourceText.simplified().toCaseFolded();
    const QString first = cleanMarkup(decodeEntities(data.value(QStringLiteral("translatedText")).toString()), asked);
    if (first.isEmpty() || first.startsWith(QLatin1String("MYMEMORY WARNING"), Qt::CaseInsensitive)
        || first.startsWith(QLatin1String("PLEASE SELECT"), Qt::CaseInsensitive)) {
        r.error = first.isEmpty() ? QStringLiteral("empty translation") : QStringLiteral("MyMemory daily limit reached");
        return r;
    }

    // Candidates, best first: the main answer, then the translation memory entries for exactly this text.
    struct Candidate { QString text; double match; };
    QList<Candidate> candidates;
    candidates.append({first, data.value(QStringLiteral("match")).toVariant().toDouble()});
    for (const QJsonValue &v : root.value(QStringLiteral("matches")).toArray()) {
        const QJsonObject m = v.toObject();
        if (m.value(QStringLiteral("segment")).toString().simplified().toCaseFolded() != asked)
            continue;
        const double match = m.value(QStringLiteral("match")).toVariant().toDouble();
        const QString t = cleanMarkup(decodeEntities(m.value(QStringLiteral("translation")).toString()), asked);
        if (!t.isEmpty() && match >= 0.5)
            candidates.append({t, match});
    }

    QStringList good; // plausible, not just the source repeated, each once
    for (const Candidate &cand : std::as_const(candidates)) {
        const QString folded = cand.text.toCaseFolded();
        if (!plausibleFor(cand.text, target))
            continue;
        if (folded == asked && cand.match < 0.9) // "don't know": the source comes back unchanged
            continue;
        bool dup = false;
        for (const QString &g : std::as_const(good))
            dup = dup || g.toCaseFolded() == folded;
        if (!dup)
            good.append(cand.text);
    }
    if (good.isEmpty()) {
        r.error = QStringLiteral("no translation");
        return r;
    }
    r.text = good.first();
    r.alternatives = good.mid(1, maxAlternatives);
    return r;
}

} // namespace MyMemory
