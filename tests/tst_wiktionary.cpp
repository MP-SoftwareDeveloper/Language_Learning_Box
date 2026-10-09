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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Plural Hunde"));
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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Plural Joghurts / Joghurte"));
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
        QCOMPARE(wiktionary::pluralLine(g), QStringLiteral("Plural Häuser"));
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
                 (QStringList{QStringLiteral("Plural Lehrer"), QStringLiteral("Singular die Lehrerin")}));
        const QString wt2 = QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=f\n|Nominativ Singular=Ärztin\n|Nominativ Plural=Ärztinnen\n}}\n"
            "{{Männliche Wortformen}}\n:[1] [[Arzt]]\n");
        const wiktionary::Grammar f = wiktionary::parseWikitext(wt2);
        QCOMPARE(f.masculine, QStringList{QStringLiteral("Arzt")});
        QCOMPARE(wiktionary::formLines(f),
                 (QStringList{QStringLiteral("Plural Ärztinnen"), QStringLiteral("Singular der Arzt")}));
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
                 (QStringList{QStringLiteral("Plural Lehrer"), QStringLiteral("Singular die Lehrerin"),
                              QStringLiteral("Plural Lehrerinnen")}));
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
        // a page with its own noun table and a plural-form entry ("Mauern"): the plural form wins
        const wiktionary::Grammar both = wiktionary::parseWikitext(QStringLiteral(
            "{{Grundformverweis Dekl|Mauer}}\n{{Deutsch Substantiv Übersicht\n|Genus=n\n|Nominativ Singular=Mauern\n}}\n"));
        QCOMPARE(both.singularOf, QStringLiteral("Mauer"));
        // ... but a noun with a plural of its own wins: "Reise" (Reisen) is also the plural of "Reis"
        const wiktionary::Grammar reise = wiktionary::parseWikitext(QStringLiteral(
            "{{Grundformverweis Dekl|Reis}}\n{{Deutsch Substantiv Übersicht\n|Genus=f\n|Nominativ Singular=Reise\n"
            "|Nominativ Plural=Reisen\n}}\n"));
        QVERIFY(reise.singularOf.isEmpty());
        QCOMPARE(reise.plurals, QStringList{QStringLiteral("Reisen")});
        QCOMPARE(wiktionary::front(reise), QStringLiteral("die Reise"));
        // Several entries on one page ("Reis": der Reis = rice, das Reis = twig, plural of "Real"): the article decides
        const QString reisPage = QStringLiteral(
            "{{Grundformverweis Dekl|Real}}\n"
            "{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Singular=Reis\n|Nominativ Plural=—\n}}\n"
            "{{Deutsch Substantiv Übersicht\n|Genus=n\n|Nominativ Singular=Reis\n|Nominativ Plural=Reiser\n}}\n");
        const wiktionary::Grammar derReis = wiktionary::parseWikitext(reisPage, QStringLiteral("der"));
        QVERIFY(derReis.singularOf.isEmpty());
        QCOMPARE(wiktionary::front(derReis), QStringLiteral("der Reis"));
        const wiktionary::Grammar dasReis = wiktionary::parseWikitext(reisPage, QStringLiteral("das"));
        QCOMPARE(wiktionary::front(dasReis), QStringLiteral("das Reis"));
        QCOMPARE(dasReis.plurals, QStringList{QStringLiteral("Reiser")});
        // "die" has no entry of its own here: it is the plural form of "Real"
        QCOMPARE(wiktionary::parseWikitext(reisPage, QStringLiteral("die")).singularOf, QStringLiteral("Real"));
        // "die Reise" (noun, f) next to the plural form of "Reis": the article picks the noun, even without a plural
        const wiktionary::Grammar dieReise = wiktionary::parseWikitext(QStringLiteral(
            "{{Grundformverweis Dekl|Reis}}\n{{Deutsch Substantiv Übersicht\n|Genus=f\n|Nominativ Singular=Reise\n"
            "|Nominativ Plural=—\n}}\n"), QStringLiteral("die"));
        QVERIFY(dieReise.singularOf.isEmpty());
        QCOMPARE(wiktionary::front(dieReise), QStringLiteral("die Reise"));
        // "die Mauern": the only entry is neuter, so "die" means the plural of "Mauer"
        QCOMPARE(wiktionary::parseWikitext(QStringLiteral(
            "{{Grundformverweis Dekl|Mauer}}\n{{Deutsch Substantiv Übersicht\n|Genus=n\n|Nominativ Singular=Mauern\n}}\n"),
            QStringLiteral("die")).singularOf, QStringLiteral("Mauer"));
        // adjectives and verbs (lower case targets) are not nouns
        QVERIFY(!wiktionary::parseWikitext(QStringLiteral("{{Grundformverweis Dekl|schön}}")).valid());

        wiktionary::Grammar noun;
        noun.genders = {QStringLiteral("m")};
        noun.plurals = {QStringLiteral("Hunde")};
        noun.lemma = QStringLiteral("Hund");
        const wiktionary::Grammar g = wiktionary::withSingular(a, noun);
        QVERIFY(g.plurals.isEmpty());
        QVERIFY(wiktionary::front(g).isEmpty()); // the typed word stays as typed
        QCOMPARE(wiktionary::formLines(g), QStringList{QStringLiteral("Singular der Hund")});
        const wiktionary::Grammar back = wiktionary::decode(wiktionary::encode(g), QStringLiteral("x"));
        QCOMPARE(back.singularOf, QStringLiteral("Hund"));
        QCOMPARE(back.lemma, QStringLiteral("Hund"));
        QCOMPARE(wiktionary::formLines(back), wiktionary::formLines(g));
    }
    void lowercasePageCheck()
    {
        const QUrl u = wiktionary::existsUrl(QStringLiteral(" kellner "));
        QVERIFY(u.toString().contains(QStringLiteral("titles=kellner")));
        QVERIFY(u.toString().contains(QStringLiteral("prop=info")));
        QString error;
        QVERIFY(wiktionary::pageExists(R"({"query":{"pages":[{"title":"gehen","pageid":1}]}})", &error));
        QVERIFY(!wiktionary::pageExists(R"({"query":{"pages":[{"title":"kellner","missing":true}]}})", &error));
        QVERIFY(error.isEmpty());
        QVERIFY(!wiktionary::pageExists("<html>", &error));
        QVERIFY(!error.isEmpty());
    }

    void suggestions()
    {
        const QUrl u = wiktionary::suggestUrl(QStringLiteral("de"), QStringLiteral(" hau "));
        QCOMPARE(u.host(), QStringLiteral("de.wiktionary.org"));
        QCOMPARE(QUrlQuery(u).queryItemValue(QStringLiteral("action")), QStringLiteral("opensearch"));
        QCOMPARE(QUrlQuery(u).queryItemValue(QStringLiteral("search")), QStringLiteral("hau"));
        QCOMPARE(wiktionary::suggestVariants(QStringLiteral("hau")), (QStringList{QStringLiteral("hau"), QStringLiteral("Hau")}));
        QCOMPARE(wiktionary::suggestVariants(QStringLiteral("Hau")), (QStringList{QStringLiteral("Hau"), QStringLiteral("hau")}));
        const QStringList a = wiktionary::parseSuggestions(
            R"(["hau",["Haus","Hausaufgabe","Kategorie:Haus","a/b"],[],[]])");
        QCOMPARE(a, (QStringList{QStringLiteral("Haus"), QStringLiteral("Hausaufgabe")}));
        QVERIFY(wiktionary::parseSuggestions("<html>").isEmpty());
        const QStringList merged = wiktionary::mergeSuggestions(
            {{QStringLiteral("hau"), QStringLiteral("Haut")}, {QStringLiteral("Haus"), QStringLiteral("Haut"), QStringLiteral("Bau")}},
            QStringLiteral("hau"), 3);
        QCOMPARE(merged, (QStringList{QStringLiteral("hau"), QStringLiteral("Haut"), QStringLiteral("Haus")}));
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
