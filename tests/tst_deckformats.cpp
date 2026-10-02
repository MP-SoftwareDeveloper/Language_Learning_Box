#include <QtTest>
#include <QBuffer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>

#include "exchange/deckformats.h"

#if __has_include(<QtCore/private/qzipwriter_p.h>)
#include <QtCore/private/qzipwriter_p.h>
#else
#include <QtGui/private/qzipwriter_p.h>
#endif

using namespace deckformats;

class TstDeckFormats : public QObject
{
    Q_OBJECT

    // A minimal Anki package: collection.anki2 (notes table) + media map + one picture.
    static QByteArray makeApkg(const QString &dir, const QStringList &notes, bool newFormatOnly = false)
    {
        QByteArray out;
        QBuffer buf(&out);
        buf.open(QIODevice::WriteOnly);
        QZipWriter zip(&buf);
        if (newFormatOnly) {
            zip.addFile(QStringLiteral("collection.anki21b"), "zstd...");
        } else {
            const QString path = dir + QStringLiteral("/c.anki2");
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("mk"));
                db.setDatabaseName(path);
                db.open();
                QSqlQuery q(db);
                q.exec(QStringLiteral("CREATE TABLE notes (id INTEGER PRIMARY KEY, flds TEXT)"));
                int id = 1;
                for (const QString &n : notes) {
                    q.prepare(QStringLiteral("INSERT INTO notes VALUES (?, ?)"));
                    q.addBindValue(id++);
                    q.addBindValue(n);
                    q.exec();
                }
                db.close();
            }
            QSqlDatabase::removeDatabase(QStringLiteral("mk"));
            QFile f(path);
            f.open(QIODevice::ReadOnly);
            zip.addFile(QStringLiteral("collection.anki2"), f.readAll());
            zip.addFile(QStringLiteral("media"), R"({"0": "haus.jpg"})");
            zip.addFile(QStringLiteral("0"), "JPEGBYTES");
        }
        zip.close();
        return out;
    }

private slots:
    void lboxRoundTripWithProgress()
    {
        Item a;
        a.front = QStringLiteral("das Haus");
        a.back = QStringLiteral("خانه\nPl. die Häuser");
        a.example = QStringLiteral("Das Haus ist groß.");
        a.deck = QStringLiteral("Lens");
        a.image = QStringLiteral("x.jpg");
        a.box = 3;
        a.dueAt = QDateTime(QDate(2026, 10, 2), QTime(0, 0), QTimeZone::utc());
        a.reviews = 4;
        a.lapses = 1;
        Item b;
        b.front = QStringLiteral("gehen");
        b.back = QStringLiteral("to go");
        b.box = 6; // learned, no due date
        const QByteArray file = writeLbox({a, b}, {{QStringLiteral("x.jpg"), QByteArray("IMG")}}, QStringLiteral("Mein Deck"), true);

        QTemporaryDir tmp;
        const Package p = read(file, QStringLiteral("x.lbox"), tmp.path());
        QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
        QCOMPARE(p.format, QStringLiteral("lbox"));
        QCOMPARE(p.title, QStringLiteral("Mein Deck"));
        QVERIFY(p.hasProgress);
        QCOMPARE(p.items.size(), 2);
        const Item &ra = p.items[0];
        QCOMPARE(ra.front, a.front);
        QCOMPARE(ra.back, a.back);           // multi-line Persian meaning kept
        QCOMPARE(ra.example, a.example);
        QCOMPARE(ra.deck, a.deck);
        QCOMPARE(p.images.value(ra.image), QByteArray("IMG"));
        QVERIFY(ra.hasProgress);
        QCOMPARE(ra.box, 3);
        QCOMPARE(ra.dueAt, a.dueAt);
        QCOMPARE(ra.reviews, 4);
        QCOMPARE(ra.lapses, 1);
        QCOMPARE(p.items[1].box, 6);
        QVERIFY(!p.items[1].dueAt.isValid());
        QVERIFY(p.items[1].image.isEmpty());
    }
    void lboxWithoutProgress()
    {
        Item a;
        a.front = QStringLiteral("das Brot");
        a.box = 4;
        const Package p = readLbox(writeLbox({a}, {}, QString(), false));
        QVERIFY(p.error.isEmpty());
        QVERIFY(!p.hasProgress);
        QVERIFY(!p.items[0].hasProgress);
        QCOMPARE(p.items[0].box, 1);
        QCOMPARE(p.language, QStringLiteral("de")); // default
    }
    void lboxKeepsLearningLanguage()
    {
        Item a;
        a.front = QStringLiteral("to go");
        a.back = QStringLiteral("رفتن");
        const Package p = readLbox(writeLbox({a}, {}, QStringLiteral("English A1"), false, QStringLiteral("en")));
        QVERIFY(p.error.isEmpty());
        QCOMPARE(p.language, QStringLiteral("en"));
        QCOMPARE(p.title, QStringLiteral("English A1"));
    }
    void csvRoundTrip()
    {
        Item a;
        a.front = QStringLiteral("die Straße, breit");
        a.back = QStringLiteral("خیابان\nPl. die Straßen");
        a.example = QStringLiteral("Er sagt \"Hallo\".");
        const QByteArray csv = writeCsv({a});
        QVERIFY(csv.startsWith("\xEF\xBB\xBF" "German,Meaning,Example\n"));
        const Package p = readCsv(csv);
        QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
        QCOMPARE(p.items.size(), 1);             // header skipped
        QCOMPARE(p.items[0].front, a.front);
        QCOMPARE(p.items[0].back, a.back);
        QCOMPARE(p.items[0].example, a.example);
    }
    void otherTextFormats()
    {
        // Quizlet: tab between term and definition
        Package p = readCsv("Hund\tdog\nKatze\tcat\n");
        QCOMPARE(p.items.size(), 2);
        QCOMPARE(p.items[1].back, QStringLiteral("cat"));
        // Excel (German locale): semicolons
        p = readCsv("Wort;Bedeutung\nHaus;house\n");
        QCOMPARE(p.items.size(), 1);
        QCOMPARE(p.items[0].front, QStringLiteral("Haus"));
        // Anki text export: header lines + HTML
        p = readCsv("#separator:tab\n#html:true\n<b>Apfel</b>\tapple&nbsp;<br>fruit\n");
        QCOMPARE(p.items.size(), 1);
        QCOMPARE(p.items[0].front, QStringLiteral("Apfel"));
        QCOMPARE(p.items[0].back, QStringLiteral("apple\nfruit"));
        QVERIFY(!readCsv("\n\n").error.isEmpty());
    }
    void cardListText()
    {
        // Hand-written list: German / meaning / example on separate lines, each card ends with ';'
        Package p = readCsv("Haus\nhouse\nDas Haus ist groß.;\nBaum\ntree\nDer Baum ist alt.;\n");
        QCOMPARE(p.items.size(), 2);
        QCOMPARE(p.items[0].front, QStringLiteral("Haus"));
        QCOMPARE(p.items[0].back, QStringLiteral("house"));
        QCOMPARE(p.items[0].example, QStringLiteral("Das Haus ist groß."));
        QCOMPARE(p.items[1].front, QStringLiteral("Baum"));
        // One line per card (several cards may share a line); the example keeps its commas;
        // Windows line endings; the last card may leave out the final ';'
        p = readCsv("Haus, house, Das Haus ist groß, nicht klein.; Baum, tree, Der Baum ist alt.;\r\nHund\r\ndog\r\nDer Hund bellt.");
        QCOMPARE(p.items.size(), 3);
        QCOMPARE(p.items[0].example, QStringLiteral("Das Haus ist groß, nicht klein."));
        QCOMPARE(p.items[2].front, QStringLiteral("Hund"));
        QCOMPARE(p.items[2].example, QStringLiteral("Der Hund bellt."));
        // A semicolon-separated Excel table with trailing separators is still a table
        p = readCsv("Haus;house;Das Haus;\nBaum;tree;Der Baum;\n");
        QVERIFY(p.items.isEmpty() || p.items[0].back == QStringLiteral("house"));
    }
    void anki()
    {
        QTemporaryDir tmp;
        const QByteArray apkg = makeApkg(tmp.path(), {
            QStringLiteral("das Haus\x1fhouse\x1f<img src=\"haus.jpg\">[sound:haus.mp3]"),
            QStringLiteral("<div>laufen</div>\x1fto run&#44; walk\x1fIch laufe."),
        });
        const Package p = read(apkg, QStringLiteral("deck.apkg"), tmp.path());
        QVERIFY2(p.error.isEmpty(), qPrintable(p.error));
        QCOMPARE(p.format, QStringLiteral("apkg"));
        QCOMPARE(p.items.size(), 2);
        QCOMPARE(p.items[0].front, QStringLiteral("das Haus"));
        QCOMPARE(p.items[0].back, QStringLiteral("house"));
        QVERIFY(p.items[0].example.isEmpty());     // only a picture + sound in field 3
        QCOMPARE(p.images.value(p.items[0].image), QByteArray("JPEGBYTES"));
        QCOMPARE(p.items[1].front, QStringLiteral("laufen"));
        QCOMPARE(p.items[1].back, QStringLiteral("to run, walk"));
        QCOMPARE(p.items[1].example, QStringLiteral("Ich laufe."));
        QVERIFY(!p.hasProgress);
        // Newest Anki format only -> explain how to export compatibly
        const Package n = read(makeApkg(tmp.path(), {}, true), QStringLiteral("new.apkg"), tmp.path());
        QVERIFY(n.error.contains(QStringLiteral("Support older Anki versions")));
    }
    void webPageInsteadOfFile()
    {
        QTemporaryDir tmp;
        QVERIFY(read("<!DOCTYPE html><html>...", QStringLiteral("x"), tmp.path()).error.contains(QStringLiteral("web page")));
    }
    void links()
    {
        QCOMPARE(directDownloadUrl(QStringLiteral("https://drive.google.com/file/d/ABC123/view?usp=sharing")).toString(),
                 QStringLiteral("https://drive.google.com/uc?export=download&id=ABC123"));
        QCOMPARE(QUrlQuery(directDownloadUrl(QStringLiteral("https://www.dropbox.com/s/xyz/deck.lbox?dl=0"))).queryItemValue(QStringLiteral("dl")),
                 QStringLiteral("1"));
        QCOMPARE(directDownloadUrl(QStringLiteral("https://github.com/u/r/blob/main/decks/a1.csv")).toString(),
                 QStringLiteral("https://raw.githubusercontent.com/u/r/main/decks/a1.csv"));
        QCOMPARE(directDownloadUrl(QStringLiteral("example.com/deck.csv")).toString(), QStringLiteral("https://example.com/deck.csv"));
    }
    void html()
    {
        QCOMPARE(plainText(QStringLiteral("<b>Haus</b>&nbsp;&amp; Hof [sound:x.mp3]")), QStringLiteral("Haus & Hof"));
        QCOMPARE(plainText(QStringLiteral("M&#228;dchen")), QStringLiteral("Mädchen"));
    }
};

QTEST_GUILESS_MAIN(TstDeckFormats)
#include "tst_deckformats.moc"
