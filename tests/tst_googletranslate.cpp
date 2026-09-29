#include "translation/googletranslate.h"

#include <QTest>
#include <QUrlQuery>

class TstGoogleTranslate : public QObject
{
    Q_OBJECT
private slots:
    void url()
    {
        const QUrl u = GoogleTranslate::requestUrl(QStringLiteral("de"), QStringLiteral("fa"));
        QCOMPARE(u.host(), QStringLiteral("translate.googleapis.com"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue(QStringLiteral("sl")), QStringLiteral("de"));
        QCOMPARE(q.queryItemValue(QStringLiteral("tl")), QStringLiteral("fa"));
        QCOMPARE(q.allQueryItemValues(QStringLiteral("dt")), QStringList({"t", "bd"}));
    }
    void body()
    {
        QCOMPARE(GoogleTranslate::requestBody(QStringLiteral("Straße & Brot")),
                 QByteArray("q=Stra%C3%9Fe%20%26%20Brot"));
    }
    void singleWordWithDictionary()
    {
        const QByteArray json = R"([[["bread","Brot",null,null,10]],[["noun",["bread","loaf","Bread"],[["bread",["Brot"]]],"Brot",1]],"de"])";
        const auto r = GoogleTranslate::parse(json);
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("bread"));
        QCOMPARE(r.alternatives, QStringList({"loaf"})); // main and case-duplicates removed
    }
    void sentenceSegmentsJoined()
    {
        const QByteArray json = R"([[["من خانه دارم. ","Ich habe ein Haus. ",null],["هوا خوب است.","Das Wetter ist gut.",null]],null,"de"])";
        const auto r = GoogleTranslate::parse(json);
        QCOMPARE(r.text, QStringLiteral("من خانه دارم. هوا خوب است."));
        QVERIFY(r.alternatives.isEmpty());
    }
    void alternativesLimited()
    {
        const QByteArray json = R"([[["go","gehen"]],[["verb",["go","walk","leave","move","head"]]],"de"])";
        QCOMPARE(GoogleTranslate::parse(json, 2).alternatives, QStringList({"walk", "leave"}));
    }
    void garbage()
    {
        QVERIFY(!GoogleTranslate::parse("<html>429 Too Many Requests</html>").error.isEmpty());
        QVERIFY(!GoogleTranslate::parse("[[],null,\"de\"]").error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstGoogleTranslate)
#include "tst_googletranslate.moc"
