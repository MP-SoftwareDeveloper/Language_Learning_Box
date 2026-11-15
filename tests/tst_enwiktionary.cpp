#include "translation/enwiktionary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

namespace {
// Real pages of en.wiktionary.org (trimmed), tests/data/enwiktionary/<name>.txt
QString page(const QString &name)
{
    QFile f(QStringLiteral(LB_ENWIKT_DIR "/") + name + QStringLiteral(".txt"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

QByteArray answer(const QList<QJsonObject> &pages)
{
    QJsonArray a;
    for (const QJsonObject &p : pages)
        a.append(p);
    return QJsonDocument(QJsonObject{{QStringLiteral("query"), QJsonObject{{QStringLiteral("pages"), a}}}})
        .toJson(QJsonDocument::Compact);
}

QJsonObject existing(const QString &title, const QString &content)
{
    return QJsonObject{{QStringLiteral("title"), title},
                       {QStringLiteral("revisions"),
                        QJsonArray{QJsonObject{{QStringLiteral("slots"),
                                                QJsonObject{{QStringLiteral("main"),
                                                             QJsonObject{{QStringLiteral("content"), content}}}}}}}}};
}
} // namespace

class TstEnWiktionary : public QObject
{
    Q_OBJECT
private slots:
    void testDataIsThere()
    {
        QVERIFY(!page(QStringLiteral("ueberzeugt")).isEmpty());
        QVERIFY(!page(QStringLiteral("anziehen")).isEmpty());
        QVERIFY(!page(QStringLiteral("hund")).isEmpty());
    }
    void adjectiveAfterParticiple()
    {
        // The participle entry only says "past participle of überzeugen"; the adjective has the meanings.
        QCOMPARE(EnWiktionary::glossesOf(page(QStringLiteral("ueberzeugt"))),
                 QStringList({"convinced", "staunch, committed"}));
    }
    void verbWithManySenses()
    {
        const QStringList g = EnWiktionary::glossesOf(page(QStringLiteral("anziehen")));
        QCOMPARE(g.mid(0, 6), QStringList({"to get dressed", "to dress", "to put on", "to attract", "to absorb",
                                           "to fasten, tighten"}));
        QVERIFY(!g.contains(QStringLiteral("to wear"))); // the Dutch section is not German
        QVERIFY(!g.contains(QStringLiteral("senses related to dressing"))); // a group heading, not a meaning
        QVERIFY(g.contains(QStringLiteral("to cite")));
    }
    void onlyTheGermanSection()
    {
        // Bavarian and Alemannic German come first on the page of "Hund"; the long explanation is no meaning
        QCOMPARE(EnWiktionary::glossesOf(page(QStringLiteral("hund"))), QStringList({"dog, hound", "scoundrel"}));
    }
    void noGermanSection()
    {
        QVERIFY(EnWiktionary::glossesOf(QStringLiteral("==Dutch==\n\n===Verb===\n# to [[wear]]\n")).isEmpty());
        QVERIFY(EnWiktionary::glossesOf(QString()).isEmpty());
    }
    void parseAnswer()
    {
        // "hund" has no page, "Hund" has: the typed spelling is tried first, then the other one
        const auto r = EnWiktionary::parse(answer({QJsonObject{{QStringLiteral("title"), QStringLiteral("hund")},
                                                               {QStringLiteral("missing"), true}},
                                                   existing(QStringLiteral("Hund"), page(QStringLiteral("hund")))}),
                                           QStringLiteral("hund"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("dog, hound"));
        QCOMPARE(r.alternatives, QStringList({"scoundrel"}));
        // typed spelling first: "anziehen" (verb) before "Anziehen"
        const auto v = EnWiktionary::parse(answer({existing(QStringLiteral("Anziehen"), QStringLiteral("==German==\n===Noun===\n# [[dressing]]\n")),
                                                   existing(QStringLiteral("anziehen"), page(QStringLiteral("anziehen")))}),
                                           QStringLiteral("anziehen"));
        QCOMPARE(v.text, QStringLiteral("to get dressed"));
        QCOMPARE(v.alternatives, QStringList({"to dress", "to put on", "to attract"}));
    }
    void parseErrors()
    {
        QVERIFY(!EnWiktionary::parse("not json", QStringLiteral("x")).error.isEmpty());
        QVERIFY(!EnWiktionary::parse(answer({QJsonObject{{QStringLiteral("title"), QStringLiteral("xyzzy")},
                                                          {QStringLiteral("missing"), true}}}),
                                     QStringLiteral("xyzzy")).error.isEmpty());
        // a page without German meanings (only an inflection entry): no answer, the caller tries the next source
        QVERIFY(!EnWiktionary::parse(answer({existing(QStringLiteral("gegangen"),
                                                      QStringLiteral("==German==\n===Participle===\n# {{past participle of|de|gehen}}\n"))}),
                                     QStringLiteral("gegangen")).error.isEmpty());
    }
    void wordsAndUrl()
    {
        QVERIFY(EnWiktionary::isSingleWord(QStringLiteral("überzeugt")));
        QVERIFY(EnWiktionary::isSingleWord(QStringLiteral("Straße")));
        QVERIFY(EnWiktionary::isSingleWord(QStringLiteral("Kaffee-Maschine")));
        QVERIFY(!EnWiktionary::isSingleWord(QStringLiteral("guten Morgen")));
        QVERIFY(!EnWiktionary::isSingleWord(QStringLiteral("b2")));
        QVERIFY(!EnWiktionary::isSingleWord(QString()));
        const QUrl u = EnWiktionary::requestUrl(QStringLiteral("essen"));
        QCOMPARE(u.host(), QStringLiteral("en.wiktionary.org"));
        QCOMPARE(QUrlQuery(u).queryItemValue(QStringLiteral("titles")), QStringLiteral("essen|Essen"));
        QCOMPARE(QUrlQuery(EnWiktionary::requestUrl(QStringLiteral("Essen"))).queryItemValue(QStringLiteral("titles")),
                 QStringLiteral("Essen|essen"));
    }
};

QTEST_APPLESS_MAIN(TstEnWiktionary)
#include "tst_enwiktionary.moc"
