#include <QtTest>
#include <QUrlQuery>

#include "translation/tatoeba.h"

class TstTatoeba : public QObject
{
    Q_OBJECT
private slots:
    void url()
    {
        const QUrl u = tatoeba::searchUrl(QStringLiteral("das Brot"), QStringLiteral("fa"));
        QCOMPARE(u.host(), QStringLiteral("api.tatoeba.org"));
        QCOMPARE(u.path(), QStringLiteral("/v1/sentences"));
        const QUrlQuery q(u);
        QCOMPARE(q.queryItemValue(QStringLiteral("lang")), QStringLiteral("deu"));
        QCOMPARE(q.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), QStringLiteral("=Brot"));
        QCOMPARE(q.queryItemValue(QStringLiteral("showtrans:lang")), QStringLiteral("pes"));
        QCOMPARE(q.queryItemValue(QStringLiteral("sort")), QStringLiteral("words"));
        QCOMPARE(tatoeba::languageCode(QStringLiteral("en")), QStringLiteral("eng"));
        QCOMPARE(tatoeba::searchWord(QStringLiteral("Guten Morgen!")), QStringLiteral("Guten Morgen"));
    }
    void parsesAndOrders()
    {
        const QByteArray json = R"({"data":[
            {"id":1,"text":"Ich esse Brot.","lang":"deu","translations":[[{"id":9,"text":"I eat bread.","lang":"eng"}]]},
            {"id":2,"text":"Brotteig ist weich.","lang":"deu","translations":[]},
            {"id":3,"text":"Das Brot ist frisch.","lang":"deu","translations":[{"id":8,"text":"نان تازه است.","lang":"pes"}]},
            {"id":4,"text":"Wir kaufen Brot beim Bäcker.","lang":"deu","translations":[]},
            {"id":5,"text":"Das Brot ist frisch.","lang":"deu","translations":[]}
        ],"paging":{}})";
        const auto fa = tatoeba::parse(json, QStringLiteral("das Brot"), QStringLiteral("fa"), 3);
        QCOMPARE(fa.size(), 3);
        QCOMPARE(fa[0].text, QStringLiteral("Das Brot ist frisch."));      // has a Persian translation: first
        QCOMPARE(fa[0].translation, QStringLiteral("نان تازه است."));
        QCOMPARE(fa[1].text, QStringLiteral("Ich esse Brot."));             // English only -> no fa translation
        QVERIFY(fa[1].translation.isEmpty());
        QCOMPARE(fa[2].text, QStringLiteral("Wir kaufen Brot beim Bäcker."));  // "Brotteig" skipped (other word), dup skipped
        const auto en = tatoeba::parse(json, QStringLiteral("Brot"), QStringLiteral("en"), 1);
        QCOMPARE(en.size(), 1);
        QCOMPARE(en[0].translation, QStringLiteral("I eat bread."));
    }
    void errors()
    {
        QString err;
        QVERIFY(tatoeba::parse("<html>", QStringLiteral("Brot"), QStringLiteral("fa"), 3, &err).isEmpty());
        QVERIFY(!err.isEmpty());
        QVERIFY(tatoeba::parse(R"({"data":[]})", QStringLiteral("Brot"), QStringLiteral("fa")).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstTatoeba)
#include "tst_tatoeba.moc"
