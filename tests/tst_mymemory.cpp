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
        const auto r = MyMemory::parse(json, QStringLiteral("Orten"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("Places"));
        QCOMPARE(r.alternatives, QStringList({"locations"})); // same segment only, no duplicates, no weak matches
    }
    void stringStatusAndEntities()
    {
        const QByteArray json = R"({"responseData":{"translatedText":"It&#39;s a house","match":1},
            "responseStatus":"200","matches":[]})";
        const auto r = MyMemory::parse(json, QStringLiteral("Es ist ein Haus"));
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("It's a house"));
    }
    void unknownWordRepeatsSource()
    {
        const QByteArray json = R"({"responseData":{"translatedText":"Xyzzy","match":0.5},"responseStatus":200,"matches":[]})";
        QVERIFY(!MyMemory::parse(json, QStringLiteral("xyzzy")).error.isEmpty());
        // identical text with a perfect match (a name) is fine
        const QByteArray name = R"({"responseData":{"translatedText":"Tom","match":1},"responseStatus":200,"matches":[]})";
        QCOMPARE(MyMemory::parse(name, QStringLiteral("Tom")).text, QStringLiteral("Tom"));
    }
    void quotaAndErrors()
    {
        const QByteArray warn = R"({"responseData":{"translatedText":"MYMEMORY WARNING: YOU USED ALL AVAILABLE FREE TRANSLATIONS FOR TODAY."},
            "quotaFinished":true,"responseStatus":429})";
        QVERIFY(!MyMemory::parse(warn, QStringLiteral("Haus")).error.isEmpty());
        const QByteArray bad = R"({"responseData":{"translatedText":"x"},"responseStatus":403,"responseDetails":"INVALID LANGUAGE PAIR"})";
        QCOMPARE(MyMemory::parse(bad, QStringLiteral("Haus")).error, QStringLiteral("INVALID LANGUAGE PAIR"));
        QVERIFY(!MyMemory::parse("<html>oops</html>", QStringLiteral("Haus")).error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstMyMemory)
#include "tst_mymemory.moc"
