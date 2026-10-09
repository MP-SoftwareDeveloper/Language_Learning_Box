#include "translation/mymemory.h"

#include <QTest>
#include <QUrlQuery>

class TstMyMemory : public QObject
{
    Q_OBJECT
private slots:
    void url()
    {
        const QUrl u = MyMemory::requestUrl(QStringLiteral("Straße & Brot"), QStringLiteral("de"), QStringLiteral("fa"));
        QCOMPARE(u.host(), QStringLiteral("api.mymemory.translated.net"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue(QStringLiteral("langpair")), QStringLiteral("de|fa"));
        QCOMPARE(QUrl::fromPercentEncoding(q.queryItemValue(QStringLiteral("q"), QUrl::FullyEncoded).toUtf8()),
                 QStringLiteral("Straße & Brot"));
    }
    void length()
    {
        QVERIFY(MyMemory::fits(QStringLiteral("Orten")));
        QVERIFY(!MyMemory::fits(QString(500, QLatin1Char('a'))));
    }
    void word()
    {
        const QByteArray json = R"({"responseData":{"translatedText":"Places","match":0.99},"quotaFinished":false,
            "responseStatus":200,"responseDetails":"","matches":[
            {"segment":"Orten","translation":"Places","match":0.99},
            {"segment":"Orten","translation":"locations","match":0.8},
            {"segment":"Orten","translation":"PLACES","match":0.7},
            {"segment":"Wir gingen zu Orten","translation":"We went to places","match":0.9},
            {"segment":"Orten","translation":"spots","match":0.2}]})";
        const auto r = MyMemory::parse(json, QStringLiteral("Orten"), QStringLiteral("en"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("Places"));
        QCOMPARE(r.alternatives, QStringList({"locations"})); // same segment only, no duplicates, no weak matches
    }
    void stringStatusAndEntities()
    {
        const QByteArray json = R"({"responseData":{"translatedText":"It&#39;s a house","match":1},
            "responseStatus":"200","matches":[]})";
        const auto r = MyMemory::parse(json, QStringLiteral("Es ist ein Haus"), QStringLiteral("en"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("It's a house"));
    }
    void unknownWordRepeatsSource()
    {
        const QByteArray json = R"({"responseData":{"translatedText":"Xyzzy","match":0.5},"responseStatus":200,"matches":[]})";
        QVERIFY(!MyMemory::parse(json, QStringLiteral("xyzzy"), QStringLiteral("en")).error.isEmpty());
        // identical text with a perfect match (a name) is fine
        const QByteArray name = R"({"responseData":{"translatedText":"Tom","match":1},"responseStatus":200,"matches":[]})";
        QCOMPARE(MyMemory::parse(name, QStringLiteral("Tom"), QStringLiteral("en")).text, QStringLiteral("Tom"));
    }
    void wrongScriptIsSkipped()
    {
        QVERIFY(MyMemory::plausibleFor(QStringLiteral("Pictures"), QStringLiteral("en")));
        QVERIFY(MyMemory::plausibleFor(QStringLiteral("Bilder"), QStringLiteral("de")));
        QVERIFY(!MyMemory::plausibleFor(QStringLiteral("\u56fe\u50cf"), QStringLiteral("en")));
        QVERIFY(!MyMemory::plausibleFor(QStringLiteral("\u0438\u0437\u043e\u0431\u0440\u0430\u0436\u0435\u043d\u0438\u0435"), QStringLiteral("en")));
        QVERIFY(!MyMemory::plausibleFor(QStringLiteral("\u062a\u0635\u0648\u06cc\u0631"), QStringLiteral("en")));
        QVERIFY(MyMemory::plausibleFor(QStringLiteral("\u062a\u0635\u0648\u06cc\u0631"), QStringLiteral("fa")));
        QVERIFY(!MyMemory::plausibleFor(QStringLiteral("Pictures"), QStringLiteral("fa")));
        // The main answer is Chinese; the translation memory has the English one: it becomes the translation
        const QByteArray json = "{\"responseData\":{\"translatedText\":\"\xe5\x9b\xbe\xe5\x83\x8f\",\"match\":0.85},\"responseStatus\":200,"
            "\"matches\":[{\"segment\":\"Bildern\",\"translation\":\"\xe5\x9b\xbe\xe5\x83\x8f\",\"match\":0.85},"
            "{\"segment\":\"Bildern\",\"translation\":\"Pictures\",\"match\":0.8},"
            "{\"segment\":\"Bildern\",\"translation\":\"Images\",\"match\":0.7}]}";
        const auto r = MyMemory::parse(json, QStringLiteral("Bildern"), QStringLiteral("en"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("Pictures"));
        QCOMPARE(r.alternatives, QStringList({"Images"}));
        // Nothing usable in the right script: an error, so the app shows "no translation" instead of Chinese
        const QByteArray onlyChinese = "{\"responseData\":{\"translatedText\":\"\xe5\x9b\xbe\xe5\x83\x8f\",\"match\":0.85},\"responseStatus\":200,\"matches\":[]}";
        QVERIFY(!MyMemory::parse(onlyChinese, QStringLiteral("Bildern"), QStringLiteral("en")).error.isEmpty());
    }
    void quotaAndErrors()
    {
        const QByteArray warn = R"({"responseData":{"translatedText":"MYMEMORY WARNING: YOU USED ALL AVAILABLE FREE TRANSLATIONS FOR TODAY."},
            "quotaFinished":true,"responseStatus":429})";
        QVERIFY(!MyMemory::parse(warn, QStringLiteral("Haus"), QStringLiteral("en")).error.isEmpty());
        const QByteArray bad = R"({"responseData":{"translatedText":"x"},"responseStatus":403,"responseDetails":"INVALID LANGUAGE PAIR"})";
        QCOMPARE(MyMemory::parse(bad, QStringLiteral("Haus"), QStringLiteral("en")).error, QStringLiteral("INVALID LANGUAGE PAIR"));
        QVERIFY(!MyMemory::parse("<html>oops</html>", QStringLiteral("Haus"), QStringLiteral("en")).error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstMyMemory)
#include "tst_mymemory.moc"
