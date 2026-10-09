#include "translation/azuretranslate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

class TstAzureTranslate : public QObject
{
    Q_OBJECT
private slots:
    void urls()
    {
        const QUrl t = AzureTranslate::translateUrl(QStringLiteral("de"), QStringLiteral("fa"));
        QCOMPARE(t.host(), QStringLiteral("api.cognitive.microsofttranslator.com"));
        QCOMPARE(t.path(), QStringLiteral("/translate"));
        const QUrlQuery q(t);
        QCOMPARE(q.queryItemValue(QStringLiteral("api-version")), QStringLiteral("3.0"));
        QCOMPARE(q.queryItemValue(QStringLiteral("from")), QStringLiteral("de"));
        QCOMPARE(q.queryItemValue(QStringLiteral("to")), QStringLiteral("fa"));
        QCOMPARE(AzureTranslate::lookupUrl(QStringLiteral("de"), QStringLiteral("en")).path(),
                 QStringLiteral("/dictionary/lookup"));
    }
    void body()
    {
        const QByteArray b = AzureTranslate::requestBody(QStringLiteral("Straße \"x\""));
        const QJsonArray a = QJsonDocument::fromJson(b).array();
        QCOMPARE(a.size(), 1);
        QCOMPARE(a.first().toObject().value(QStringLiteral("Text")).toString(), QStringLiteral("Straße \"x\""));
    }
    void singleWord()
    {
        QVERIFY(AzureTranslate::isSingleWord(QStringLiteral("Orten")));
        QVERIFY(AzureTranslate::isSingleWord(QStringLiteral("  Weg ")));
        QVERIFY(!AzureTranslate::isSingleWord(QStringLiteral("zu Hause")));
        QVERIFY(!AzureTranslate::isSingleWord(QString()));
    }
    void translation()
    {
        const QByteArray json = R"([{"detectedLanguage":{"language":"de","score":1},
            "translations":[{"text":"locations","to":"en"}]}])";
        const auto r = AzureTranslate::parseTranslation(json);
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.text, QStringLiteral("locations"));
    }
    void persianTranslation()
    {
        const QByteArray json = R"([{"translations":[{"text":"خانه","to":"fa"}]}])";
        QCOMPARE(AzureTranslate::parseTranslation(json).text, QString::fromUtf16(u"خانه"));
    }
    void errors()
    {
        const auto bad = AzureTranslate::parseTranslation(
            R"({"error":{"code":401000,"message":"The request is not authorized because credentials are missing or invalid."}})");
        QVERIFY(bad.text.isEmpty());
        QVERIFY(bad.error.startsWith(QStringLiteral("The request is not authorized")));
        QVERIFY(!AzureTranslate::parseTranslation("<html>").error.isEmpty());
        QVERIFY(!AzureTranslate::parseTranslation(R"([{"translations":[]}])").error.isEmpty());
        QVERIFY(!AzureTranslate::parseTranslation(R"([{"translations":[{"text":"  "}]}])").error.isEmpty());
        QCOMPARE(AzureTranslate::errorMessage(R"({"error":{"code":429001}})"), QStringLiteral("error 429001"));
        QVERIFY(AzureTranslate::errorMessage("[]").isEmpty());
    }
    void lookup()
    {
        const QByteArray json = R"([{"normalizedSource":"orten","displaySource":"Orten","translations":[
            {"normalizedTarget":"places","displayTarget":"places","posTag":"NOUN","confidence":0.4},
            {"normalizedTarget":"locations","displayTarget":"locations","posTag":"NOUN","confidence":0.5},
            {"normalizedTarget":"spots","displayTarget":"spots","posTag":"NOUN","confidence":0.1},
            {"normalizedTarget":"Locations","displayTarget":"Locations","posTag":"NOUN","confidence":0.09},
            {"normalizedTarget":"sites","displayTarget":"sites","posTag":"NOUN","confidence":0.05},
            {"normalizedTarget":"towns","displayTarget":"towns","posTag":"NOUN","confidence":0.01}]}])";
        // best first, the main answer and case duplicates left out, at most three
        QCOMPARE(AzureTranslate::parseLookup(json, QStringLiteral("Places")),
                 QStringList({"locations", "spots", "sites"}));
        QCOMPARE(AzureTranslate::parseLookup(json, QStringLiteral("x"), 1), QStringList({"locations"}));
    }
    void lookupWithoutAnswer()
    {
        QVERIFY(AzureTranslate::parseLookup(R"({"error":{"code":400036}})", QStringLiteral("x")).isEmpty());
        QVERIFY(AzureTranslate::parseLookup("[{\"translations\":[]}]", QStringLiteral("x")).isEmpty());
        QVERIFY(AzureTranslate::parseLookup("nonsense", QStringLiteral("x")).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstAzureTranslate)
#include "tst_azuretranslate.moc"
