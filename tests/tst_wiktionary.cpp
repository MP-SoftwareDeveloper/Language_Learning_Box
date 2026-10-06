#include <QtTest>
#include <QUrlQuery>

#include "translation/wiktionary.h"

class TstWiktionary : public QObject
{
    Q_OBJECT
private slots:
    void lemma()
    {
        QCOMPARE(wiktionary::lemmaOf(QStringLiteral("der Hund")), QStringLiteral("Hund"));
        QCOMPARE(wiktionary::lemmaOf(QStringLiteral("  Die  Frau, ")), QStringLiteral("Frau"));
        QCOMPARE(wiktionary::lemmaOf(QStringLiteral("Hund")), QStringLiteral("Hund"));
        QVERIFY(wiktionary::lemmaOf(QStringLiteral("guten Morgen")).isEmpty());
        QVERIFY(wiktionary::lemmaOf(QStringLiteral("")).isEmpty());
        QVERIFY(wiktionary::lemmaOf(QStringLiteral("der")).isEmpty());
    }
    void url()
    {
        const QUrl u = wiktionary::requestUrl(QStringLiteral("hund"));
        QCOMPARE(u.host(), QStringLiteral("de.wiktionary.org"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue(QStringLiteral("titles")), QStringLiteral("Hund")); // nouns are capitalised
        QCOMPARE(q.queryItemValue(QStringLiteral("rvprop")), QStringLiteral("content"));
        QCOMPARE(q.queryItemValue(QStringLiteral("redirects")), QStringLiteral("1"));
    }
    void simpleNoun()
    {
        const QString wt = QStringLiteral(
            "== Hund ({{Sprache|Deutsch}}) ==\n=== {{Wortart|Substantiv|Deutsch}}, {{m}} ===\n\n"
            "{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Singular=Hund\n|Nominativ Plural=Hunde\n"
            "|Genitiv Singular=Hundes\n|Genitiv Plural=Hunde\n}}\n\n{{Bedeutungen}}\n");
        const wiktionary::Grammar g = wiktionary::parseWikitext(wt);
        QVERIFY(g.valid());
        QCOMPARE(g.genders, QStringList{QStringLiteral("m")});
        QCOMPARE(g.plurals, QStringList{QStringLiteral("Hunde")});
        QCOMPARE(g.lemma, QStringLiteral("Hund"));
        QCOMPARE(wiktionary::front(g), QStringLiteral("der Hund"));
        QCOMPARE(wiktionary::pluralText(g), QStringLiteral("die Hunde"));
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. die Hunde"));
    }
    void twoGendersAndPlurals()
    {
        const QString wt = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus 1=m\n|Genus 2=n\n|Nominativ Singular=Joghurt\n"
            "|Nominativ Plural 1=Joghurts\n|Nominativ Plural 2=[[Joghurte]]\n}}");
        const wiktionary::Grammar g = wiktionary::parseWikitext(wt);
        QCOMPARE(g.genders, (QStringList{QStringLiteral("m"), QStringLiteral("n")}));
        QCOMPARE(g.plurals, (QStringList{QStringLiteral("Joghurts"), QStringLiteral("Joghurte")}));
        QCOMPARE(wiktionary::front(g), QStringLiteral("der Joghurt"));
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. die Joghurts / Joghurte"));
    }
    void noPlural()
    {
        const QString wt = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=n\n|Nominativ Singular=Wasser\n|Nominativ Plural=—\n}}");
        const wiktionary::Grammar g = wiktionary::parseWikitext(wt);
        QVERIFY(g.valid());
        QVERIFY(g.plurals.isEmpty());
        QVERIFY(wiktionary::pluralLine(g).isEmpty());
        QCOMPARE(wiktionary::front(g), QStringLiteral("das Wasser"));
    }
    void notANoun()
    {
        const QString wt = QStringLiteral("{{Deutsch Verb Übersicht\n|Präsens_ich=gehe\n}}");
        QVERIFY(!wiktionary::parseWikitext(wt).valid());
        QVERIFY(!wiktionary::parseWikitext(QString()).valid());
    }
    void nestedBracesStayInsideTheTable()
    {
        const QString wt = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=f\n|Nominativ Singular=Frau\n|Nominativ Plural=Frauen\n"
            "|Bild={{x|y}}\n}}\n{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Plural=Nope\n}}");
        const wiktionary::Grammar g = wiktionary::parseWikitext(wt);
        QCOMPARE(g.genders, QStringList{QStringLiteral("f")});
        QCOMPARE(g.plurals, QStringList{QStringLiteral("Frauen")});
    }
    void parsesApiAnswer()
    {
        const QByteArray json = QJsonDocument(QJsonObject{
            {QStringLiteral("query"), QJsonObject{{QStringLiteral("pages"), QJsonArray{QJsonObject{
                {QStringLiteral("title"), QStringLiteral("Haus")},
                {QStringLiteral("revisions"), QJsonArray{QJsonObject{{QStringLiteral("slots"), QJsonObject{
                    {QStringLiteral("main"), QJsonObject{{QStringLiteral("content"),
                        QStringLiteral("{{Deutsch Substantiv Übersicht\n|Genus=n\n|Nominativ Plural=Häuser\n}}")}}}}}}}}}}}}}
        }).toJson();
        QString err;
        const wiktionary::Grammar g = wiktionary::parse(json, &err);
        QVERIFY(err.isEmpty());
        QCOMPARE(g.lemma, QStringLiteral("Haus")); // from the page title when the table has no singular
        QCOMPARE(wiktionary::front(g), QStringLiteral("das Haus"));
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. die Häuser"));
    }
    void missingPageAndErrors()
    {
        QString err;
        QVERIFY(!wiktionary::parse(R"({"query":{"pages":[{"title":"Xyz","missing":true}]}})", &err).valid());
        QVERIFY(err.isEmpty());
        QVERIFY(!wiktionary::parse("<html>", &err).valid());
        QVERIFY(!err.isEmpty());
    }
    void cacheRoundTrip()
    {
        wiktionary::Grammar g;
        g.genders = {QStringLiteral("m"), QStringLiteral("n")};
        g.plurals = {QStringLiteral("Joghurts"), QStringLiteral("Joghurte")};
        g.lemma = QStringLiteral("Joghurt");
        const QString s = wiktionary::encode(g);
        const wiktionary::Grammar back = wiktionary::decode(s, QStringLiteral("Joghurt"));
        QCOMPARE(back.genders, g.genders);
        QCOMPARE(back.plurals, g.plurals);
        QVERIFY(wiktionary::encode(wiktionary::Grammar{}).isEmpty());
        QVERIFY(!wiktionary::decode(QString(), QStringLiteral("x")).valid());
    }
};

QTEST_APPLESS_MAIN(TstWiktionary)
#include "tst_wiktionary.moc"
