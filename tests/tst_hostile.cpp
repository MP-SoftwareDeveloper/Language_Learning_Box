// Feeds the files from tests/security/hostile/ (made by tests/security/make_hostile_files.py) to the import code.
// Pass = no crash, no hang (< 5 s), no leftovers in the temp folder, hostile card text is plain text.
#include <QtTest>

#include "exchange/deckformats.h"

class TstHostile : public QObject
{
    Q_OBJECT
    QTemporaryDir tmp;

    static QString hostileDir()
    {
        const QByteArray env = qgetenv("LB_HOSTILE_DIR");
        return env.isEmpty() ? QStringLiteral(LB_HOSTILE_DIR) : QString::fromLocal8Bit(env);
    }
    static QByteArray readAll(const QString &name)
    {
        QFile f(QDir(hostileDir()).filePath(name));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmp.isValid());
        QVERIFY2(QDir(hostileDir()).exists(), "run: python tests/security/make_hostile_files.py");
    }

    void importsDoNotCrashOrHang_data()
    {
        QTest::addColumn<QString>("file");
        const QDir d(hostileDir());
        const auto files = d.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : files)
            QTest::newRow(qPrintable(fi.fileName())) << fi.fileName();
        const QDir slow(d.filePath(QStringLiteral("slow"))); // files that used to hang the app (finding F-7)
        const auto slowFiles = slow.entryInfoList(QDir::Files, QDir::Name);
        for (const QFileInfo &fi : slowFiles)
            QTest::newRow(qPrintable(QStringLiteral("slow/") + fi.fileName())) << QStringLiteral("slow/") + fi.fileName();
    }

    void importsDoNotCrashOrHang()
    {
        QFETCH(QString, file);
        const QByteArray data = readAll(file);
        QElapsedTimer t;
        t.start();
        const deckformats::Package p = deckformats::read(data, file, tmp.path());
        QVERIFY2(t.elapsed() < 5000, "the import took longer than 5 s: on the phone the screen would freeze");
        QVERIFY(p.items.size() <= 100000);
        QVERIFY2(QDir(tmp.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty(), "temp files left behind");
    }

    void zipSlipNamesAreOnlyLookupKeys()
    {
        const deckformats::Package p = deckformats::read(readAll("01_zipslip.lbox"), "01_zipslip.lbox", tmp.path());
        QCOMPARE(p.items.size(), 2); // imported; the names are never used as paths (CardStore::storeImageData makes its own)
    }

    void htmlInLboxIsPlainText()
    {
        const deckformats::Package p = deckformats::read(readAll("02_html_cards.lbox"), "02_html_cards.lbox", tmp.path());
        QVERIFY(!p.items.isEmpty());
        const deckformats::Item &it = p.items.first(); // F-3: no markup survives the import
        QVERIFY2(!it.front.contains(QLatin1Char('<')), qPrintable(it.front));
        QVERIFY2(!it.back.contains(QLatin1Char('<')), qPrintable(it.back));
        QVERIFY2(!it.example.contains(QLatin1Char('<')), qPrintable(it.example));
        QVERIFY(it.front.contains(QLatin1String("Haus")));
    }

    void ankiNotesViewIsRefused()
    {
        QElapsedTimer t;
        t.start();
        const deckformats::Package p = deckformats::read(readAll("slow/20_view_50M_rows.apkg"), "x.apkg", tmp.path());
        QVERIFY2(t.elapsed() < 2000, "F-7: a view named notes must be refused at once");
        QVERIFY(!p.error.isEmpty());
        QVERIFY(p.items.isEmpty());
    }

    void csvIsLimitedToMaxCards()
    {
        QByteArray csv;
        for (int i = 0; i < 100500; ++i)
            csv += "a" + QByteArray::number(i) + ";b;c\n";
        const deckformats::Package p = deckformats::readCsv(csv);
        QCOMPARE(p.items.size(), 100000); // F-5
    }
};

QTEST_MAIN(TstHostile)
#include "tst_hostile.moc"
