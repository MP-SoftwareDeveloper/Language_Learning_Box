#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    void singularGuesses()
    {
        QCOMPARE(wiktionary::singularCandidates(QStringLiteral("Nudeln")).value(0), QStringLiteral("Nudel"));
        QCOMPARE(wiktionary::singularCandidates(QStringLiteral("Frauen")).value(0), QStringLiteral("Frau"));
        QCOMPARE(wiktionary::singularCandidates(QStringLiteral("Lehrerinnen")).value(0), QStringLiteral("Lehrerin"));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("Kinder")).contains(QStringLiteral("Kind")));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("B\u00fccher")).contains(QStringLiteral("Buch")));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("H\u00e4nde")).contains(QStringLiteral("Hand")));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("H\u00e4user")).contains(QStringLiteral("Haus")));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("Autos")).contains(QStringLiteral("Auto")));
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("gehen")).isEmpty()); // not capitalised: not a noun
        QVERIFY(wiktionary::singularCandidates(QStringLiteral("Hund")).isEmpty());
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

    // ---- verbs: Partizip II + auxiliary from "{{Deutsch Verb Übersicht" ----
    static QString verbPage(const QString &partizip, const QString &hilfsverb)
    {
        QString t = QStringLiteral("== x ({{Sprache|Deutsch}}) ==\n=== {{Wortart|Verb|Deutsch}} ===\n\n"
                                   "{{Deutsch Verb Übersicht\n|Präsens_ich=gehe\n|Präsens_du=gehst\n"
                                   "|Präteritum_ich=ging\n");
        if (!partizip.isEmpty())
            t += QStringLiteral("|Partizip II=") + partizip + QLatin1Char('\n');
        t += QStringLiteral("|Konjunktiv II_ich=ginge\n|Imperativ Singular=geh\n");
        if (!hilfsverb.isEmpty())
            t += QStringLiteral("|Hilfsverb=") + hilfsverb + QLatin1Char('\n');
        t += QStringLiteral("|Weitere_Konjugationen=gehen (Konjugation)\n}}\n\n{{Bedeutungen}}\n");
        return t;
    }
    void verbSein()
    {
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("gegangen"), QStringLiteral("sein")));
        QVERIFY(g.valid());
        QVERIFY(g.isVerb());
        g.lemma = QStringLiteral("gehen");
        QCOMPARE(g.participle, QStringLiteral("gegangen"));
        QCOMPARE(g.auxiliaries, QStringList{QStringLiteral("sein")});
        QCOMPARE(wiktionary::participleText(g), QStringLiteral("ist gegangen"));
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich bin gegangen."));
        QVERIFY(wiktionary::front(g).isEmpty()); // no article: a verb is not a noun
        QCOMPARE(wiktionary::formLines(g), QStringList{QStringLiteral("Partizip II ist gegangen · Ich bin gegangen.")});
    }
    void verbHaben()
    {
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("[[gemacht]]"), QStringLiteral("haben")));
        g.lemma = QStringLiteral("machen");
        QCOMPARE(wiktionary::participleText(g), QStringLiteral("hat gemacht"));
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich habe gemacht."));
    }
    void verbBothAuxiliaries()
    {
        for (const QString &aux : {QStringLiteral("sein, haben"), QStringLiteral("sein/haben")}) {
            wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("gefahren"), aux));
            g.lemma = QStringLiteral("fahren");
            QCOMPARE(g.auxiliaries, (QStringList{QStringLiteral("sein"), QStringLiteral("haben")}));
            QCOMPARE(wiktionary::participleText(g), QStringLiteral("ist gefahren / hat gefahren"));
            QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich bin gefahren.")); // the first one
        }
        // a regional note in brackets is not a second auxiliary
        const wiktionary::Grammar r = wiktionary::parseVerbWikitext(
            verbPage(QStringLiteral("gesessen"), QStringLiteral("haben (landschaftlich: sein)")));
        QCOMPARE(r.auxiliaries, QStringList{QStringLiteral("haben")});
    }
    void verbTwoTables()
    {
        // passieren: one table for "pass through" (haben), one for "happen" (sein): both, sein first
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(
            verbPage(QStringLiteral("passiert"), QStringLiteral("haben")) + verbPage(QStringLiteral("passiert"), QStringLiteral("sein")));
        g.lemma = QStringLiteral("passieren");
        QCOMPARE(g.auxiliaries, (QStringList{QStringLiteral("sein"), QStringLiteral("haben")}));
        QCOMPARE(wiktionary::participleText(g), QStringLiteral("ist passiert / hat passiert"));
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich bin passiert."));
        // another verb on the page with another participle (umfahren) is not mixed in
        const wiktionary::Grammar u = wiktionary::parseVerbWikitext(
            verbPage(QStringLiteral("umgefahren"), QStringLiteral("haben")) + verbPage(QStringLiteral("umfahren"), QStringLiteral("sein")));
        QCOMPARE(u.auxiliaries, QStringList{QStringLiteral("haben")});
        // "Hilfsverb*" is read as well
        const QString star = QStringLiteral("{{Deutsch Verb Übersicht\n|Partizip II=x\n|Hilfsverb=haben\n|Hilfsverb*=sein\n}}");
        QCOMPARE(wiktionary::parseVerbWikitext(star).auxiliaries, (QStringList{QStringLiteral("sein"), QStringLiteral("haben")}));
    }
    void verbTwoParticiples()
    {
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(
            verbPage(QStringLiteral("gesendet, gesandt"), QStringLiteral("haben")));
        g.lemma = QStringLiteral("senden");
        QCOMPARE(g.participle, QStringLiteral("gesendet / gesandt"));
        QCOMPARE(wiktionary::participleText(g), QStringLiteral("hat gesendet / gesandt"));
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich habe gesendet."));
    }
    void verbImpersonal()
    {
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("geregnet"), QStringLiteral("haben")));
        g.lemma = QStringLiteral("regnen");
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Es hat geregnet."));
    }
    void verbWithoutAuxiliary()
    {
        // no "Hilfsverb": the participle is shown, but no sentence is guessed
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("gekonnt"), QString()));
        g.lemma = QStringLiteral("können");
        QVERIFY(g.isVerb());
        QCOMPARE(wiktionary::participleText(g), QStringLiteral("gekonnt"));
        QVERIFY(wiktionary::participleExample(g).isEmpty());
        QCOMPARE(wiktionary::formLines(g), QStringList{QStringLiteral("Partizip II gekonnt")});
    }
    void notAVerb()
    {
        // a noun page, a page without tables, a verb table without a Partizip II
        QVERIFY(!wiktionary::parseVerbWikitext(QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Singular=Hund\n|Nominativ Plural=Hunde\n}}")).valid());
        QVERIFY(!wiktionary::parseVerbWikitext(QStringLiteral("== ging ==\n{{Grundformverweis Konj|gehen}}\n")).valid());
        QVERIFY(!wiktionary::parseVerbWikitext(verbPage(QString(), QStringLiteral("sein"))).valid());
        QVERIFY(wiktionary::participleText(wiktionary::Grammar{}).isEmpty());
        QVERIFY(wiktionary::participleExample(wiktionary::Grammar{}).isEmpty());
        // nouns are unchanged
        const wiktionary::Grammar n = wiktionary::parseWikitext(QStringLiteral(
            "{{Deutsch Substantiv Übersicht\n|Genus=m\n|Nominativ Singular=Hund\n|Nominativ Plural=Hunde\n}}"));
        QVERIFY(!n.isVerb());
        QCOMPARE(wiktionary::formLines(n), QStringList{QStringLiteral("Plural Hunde")});
    }
    void verbFromJson()
    {
        const QString wt = verbPage(QStringLiteral("gegangen"), QStringLiteral("sein"));
        QJsonObject slot{{QStringLiteral("content"), wt}};
        QJsonObject rev{{QStringLiteral("slots"), QJsonObject{{QStringLiteral("main"), slot}}}};
        QJsonObject page{{QStringLiteral("title"), QStringLiteral("gehen")}, {QStringLiteral("revisions"), QJsonArray{rev}}};
        const QByteArray json = QJsonDocument(QJsonObject{{QStringLiteral("query"),
            QJsonObject{{QStringLiteral("pages"), QJsonArray{page}}}}}).toJson();
        QString error;
        const wiktionary::Grammar g = wiktionary::parseVerb(json, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(g.lemma, QStringLiteral("gehen"));
        QCOMPARE(wiktionary::participleExample(g), QStringLiteral("Ich bin gegangen."));

        // missing page, and an answer that is not JSON
        const QByteArray missing = R"({"query":{"pages":[{"title":"gehenx","missing":true}]}})";
        QVERIFY(!wiktionary::parseVerb(missing, &error).valid());
        QVERIFY(error.isEmpty());
        QVERIFY(!wiktionary::parseVerb("<html>", &error).valid());
        QVERIFY(!error.isEmpty());
    }
    void verbUrlKeepsCase()
    {
        const QUrlQuery q(wiktionary::requestUrl(QStringLiteral("gehen"), true));
        QCOMPARE(q.queryItemValue(QStringLiteral("titles")), QStringLiteral("gehen"));
        const QUrlQuery n(wiktionary::requestUrl(QStringLiteral("gehen")));
        QCOMPARE(n.queryItemValue(QStringLiteral("titles")), QStringLiteral("Gehen")); // unchanged: nouns
    }
    void verbCacheRoundTrip()
    {
        wiktionary::Grammar g = wiktionary::parseVerbWikitext(verbPage(QStringLiteral("gefahren"), QStringLiteral("sein, haben")));
        g.lemma = QStringLiteral("fahren");
        const wiktionary::Grammar back = wiktionary::decode(wiktionary::encode(g), QStringLiteral("fahren"));
        QVERIFY(back.isVerb());
        QCOMPARE(back.participle, g.participle);
        QCOMPARE(back.auxiliaries, g.auxiliaries);
        QCOMPARE(wiktionary::formLines(back), wiktionary::formLines(g));
        // an answer saved before verbs were read (8 fields) still decodes as a noun
        const wiktionary::Grammar old = wiktionary::decode(QStringLiteral("m\nHunde\n\n\n\nHund\n\n"), QStringLiteral("Hund"));
        QVERIFY(!old.isVerb());
        QCOMPARE(old.plurals, QStringList{QStringLiteral("Hunde")});
    }
};

QTEST_APPLESS_MAIN(TstWiktionary)
#include "tst_wiktionary.moc"
