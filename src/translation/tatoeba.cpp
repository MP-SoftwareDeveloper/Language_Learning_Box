#include "tatoeba.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

namespace tatoeba {

QString languageCode(const QString &target)
{
    if (target == QLatin1String("en"))
        return QStringLiteral("eng");
    if (target == QLatin1String("de"))
        return QStringLiteral("deu");
    return QStringLiteral("pes");
}

QString searchWord(const QString &front, const QString &source)
{
    static const QRegularExpression article(QStringLiteral("^(der|die|das)\\s+"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression englishArticle(QStringLiteral("^(to|the|a|an)\\s+"),
                                                   QRegularExpression::CaseInsensitiveOption);
    QString w = front.simplified();
    w.remove(source == QLatin1String("en") ? englishArticle : article);
    w.remove(QRegularExpression(QStringLiteral("[!?.,;:]+$")));
    return w;
}

QUrl searchUrl(const QString &word, const QString &target, int limit, const QString &source)
{
    QUrl url(QStringLiteral("https://api.tatoeba.org/v1/sentences"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("lang"), languageCode(source));
    q.addQueryItem(QStringLiteral("q"), QStringLiteral("=") + searchWord(word, source)); // "=" = exact word form
    q.addQueryItem(QStringLiteral("sort"), QStringLiteral("words"));           // shortest first
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    q.addQueryItem(QStringLiteral("showtrans:lang"), languageCode(target));
    url.setQuery(q);
    return url;
}

namespace {

// Translations may come as a flat array of objects or as groups (arrays of objects).
void collectTranslations(const QJsonValue &v, const QString &lang, QString *out)
{
    if (!out->isEmpty())
        return;
    if (v.isArray()) {
        for (const QJsonValue &x : v.toArray())
            collectTranslations(x, lang, out);
        return;
    }
    const QJsonObject o = v.toObject();
    if (o.value(QStringLiteral("lang")).toString() == lang)
        *out = o.value(QStringLiteral("text")).toString().trimmed();
}

bool containsWord(const QString &sentence, const QString &word)
{
    const QRegularExpression re(QStringLiteral("(^|[^\\p{L}])%1($|[^\\p{L}])").arg(QRegularExpression::escape(word)),
                                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return re.match(sentence).hasMatch();
}

} // namespace

QList<Example> parse(const QByteArray &json, const QString &front, const QString &target, int max, QString *error,
                     const QString &source)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();
    if (!doc.isObject() || !root.contains(QStringLiteral("data"))) {
        if (error)
            *error = root.value(QStringLiteral("message")).toString(QStringLiteral("unexpected answer from Tatoeba"));
        return {};
    }
    const QString word = searchWord(front, source);
    const QString lang = languageCode(target);
    QList<Example> withTr, without;
    QSet<QString> seen;
    for (const QJsonValue &v : root.value(QStringLiteral("data")).toArray()) {
        const QJsonObject s = v.toObject();
        const QString text = s.value(QStringLiteral("text")).toString().simplified();
        const QString key = text.toCaseFolded();
        if (text.isEmpty() || seen.contains(key) || !containsWord(text, word)
            || text.split(u' ', Qt::SkipEmptyParts).size() > 12)
            continue;
        seen.insert(key);
        Example e{text, {}};
        collectTranslations(s.value(QStringLiteral("translations")), lang, &e.translation);
        (e.translation.isEmpty() ? without : withTr).append(e);
    }
    QList<Example> out = withTr + without;
    if (out.size() > max)
        out.resize(max);
    return out;
}

} // namespace tatoeba
