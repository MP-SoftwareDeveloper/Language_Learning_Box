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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. Hunde"));
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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. Joghurts / Joghurte"));
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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Pl. Häuser"));
    }
    void missingPageAndErrors()
    {
        QString err;
        QVERIFY(!wiktionary::parse(R"({"query":{"pages":[{"title":"Xyz","missing":true}]}})", &err).valid());
        QVERIFY(err.isEmpty());
        QVERIFY(!wiktionary::parse("<html>", &err).valid());
        QVERIFY(!err.isEmpty());
    }
    void masculineAndFeminineForms()
    {
        const QString wt = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Singular=Lehrer\n|Nominativ Plural=Lehrer\n}}\n\n"
            "{{Weibliche Wortformen}}\n:[1] [[Lehrerin]]\n\n{{Herkunft}}\n:x [[Nein]]\n");
        const wiktionary::Grammar g = wiktionary::parseWikitext(wt);
        QCOMPARE(g.feminine, QStringList{QStringLiteral("Lehrerin")});
        QVERIFY(g.masculine.isEmpty());
        QCOMPARE(wiktionary::formLines(g),
                 (QStringList{QStringLiteral("Pl. Lehrer"), QStringLiteral("Fem. die Lehrerin")}));
        const QString wt2 = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=f\n|Nominativ Singular=Ärztin\n|Nominativ Plural=Ärztinnen\n}}\n"
            "{{Männliche Wortformen}}\n:[1] [[Arzt]]\n");
        const wiktionary::Grammar f = wiktionary::parseWikitext(wt2);
        QCOMPARE(f.masculine, QStringList{QStringLiteral("Arzt")});
        QCOMPARE(wiktionary::formLines(f),
                 (QStringList{QStringLiteral("Pl. Ärztinnen"), QStringLiteral("Mask. der Arzt")}));
    }
    void pluralsOfTheOtherGender()
    {
        wiktionary::Grammar g;
        g.genders = {QStringLiteral("m")};
        g.plurals = {QStringLiteral("Lehrer")};
        g.lemma = QStringLiteral("Lehrer");
        g.feminine = {QStringLiteral("Lehrerin")};
        g.femininePlural = QStringLiteral("Lehrerinnen");
        QCOMPARE(wiktionary::formLines(g),
                 (QStringList{QStringLiteral("Pl. Lehrer"), QStringLiteral("Fem. die Lehrerin"),
                              QStringLiteral("Fem. Pl. Lehrerinnen")}));
    }
    void pluralFormPointsAtItsNoun()
    {
        const wiktionary::Grammar a = wiktionary::parseWikitext(
            QStringLiteral("{{Wortart|Deklinierte Form|Deutsch}}\n{{Grundformverweis Dekl|Hund}}\n"));
        QCOMPARE(a.singularOf, QStringLiteral("Hund"));
        QVERIFY(a.valid());
        const wiktionary::Grammar b = wiktionary::parseWikitext(
            QStringLiteral("Hunde ist die Nominativ-Plural-Form des Substantivs [[Hund]]\n"));
        QCOMPARE(b.singularOf, QStringLiteral("Hund"));
        // adjectives and verbs (lower case targets) are not nouns
        QVERIFY(!wiktionary::parseWikitext(QStringLiteral("{{Grundformverweis Dekl|schön}}")).valid());

        wiktionary::Grammar noun;
        noun.genders = {QStringLiteral("m")};
        noun.plurals = {QStringLiteral("Hunde")};
        noun.lemma = QStringLiteral("Hund");
        const wiktionary::Grammar g = wiktionary::withSingular(a, noun);
        QVERIFY(g.plurals.isEmpty());
        QVERIFY(wiktionary::front(g).isEmpty()); // the typed word stays as typed
        QCOMPARE(wiktionary::formLines(g), QStringList{QStringLiteral("Sg. der Hund")});
        const wiktionary::Grammar back = wiktionary::decode(wiktionary::encode(g), QStringLiteral("x"));
        QCOMPARE(back.singularOf, QStringLiteral("Hund"));
        QCOMPARE(back.lemma, QStringLiteral("Hund"));
        QCOMPARE(wiktionary::formLines(back), wiktionary::formLines(g));
    }
    void cacheRoundTrip()
    {
        wiktionary::Grammar g;
        g.genders = {QStringLiteral("m"), QStringLiteral("n")};
        g.plurals = {QStringLiteral("Joghurts"), QStringLiteral("Joghurte")};
        g.lemma = QStringLiteral("Joghurt");
        g.feminine = {QStringLiteral("Joghurtin")};
        g.femininePlural = QStringLiteral("Joghurtinnen");
        g.masculinePlural = QStringLiteral("Joghurter");
        const QString s = wiktionary::encode(g);
        const wiktionary::Grammar back = wiktionary::decode(s, QStringLiteral("Joghurt"));
        QCOMPARE(back.feminine, g.feminine);
        QCOMPARE(back.femininePlural, g.femininePlural);
        QCOMPARE(back.masculinePlural, g.masculinePlural);
        QCOMPARE(back.genders, g.genders);
        QCOMPARE(back.plurals, g.plurals);
        QVERIFY(wiktionary::encode(wiktionary::Grammar{}).isEmpty());
        QVERIFY(!wiktionary::decode(QString(), QStringLiteral("x")).valid());
    }
};

QTEST_APPLESS_MAIN(TstWiktionary)
#include "tst_wiktionary.moc"
