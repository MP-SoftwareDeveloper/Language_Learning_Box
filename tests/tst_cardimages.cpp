#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include "cardimages.h"

class TstCardImages : public QObject
{
    Q_OBJECT
private slots:
    void scalesLargeImageAndWritesJpeg()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QImage big(3000, 2000, QImage::Format_RGB32);
        big.fill(Qt::red);
        const QString src = tmp.filePath(QStringLiteral("big.png"));
        QVERIFY(big.save(src));

        QString err;
        const QString name = cardimages::store(src, tmp.filePath(QStringLiteral("images")), &err);
        QVERIFY2(!name.isEmpty(), qPrintable(err));
        QVERIFY(name.endsWith(QStringLiteral(".jpg")));
        QVERIFY(!name.contains(u'/'));

        QImage out(tmp.filePath(QStringLiteral("images/") + name));
        QVERIFY(!out.isNull());
        QCOMPARE(out.width(), cardimages::kMaxEdge);   // longest edge
        QVERIFY(qAbs(out.height() - 683) <= 1);        // aspect ratio kept (2000*1024/3000 = 682.7)
    }

    void smallImageIsNotUpscaled()
    {
        QTemporaryDir tmp;
        QImage small(200, 100, QImage::Format_ARGB32);
        small.fill(Qt::transparent); // transparent PNG -> flattened on white
        const QString src = tmp.filePath(QStringLiteral("small.png"));
        QVERIFY(small.save(src));
        const QString name = cardimages::store(src, tmp.path());
        QVERIFY(!name.isEmpty());
        QImage out(tmp.filePath(name));
        QCOMPARE(out.size(), QSize(200, 100));
        QVERIFY(qGray(out.pixel(10, 10)) > 240); // white, not black
    }

    void rotatesLandscapeShot()
    {
        QTemporaryDir tmp;
        QImage img(300, 100, QImage::Format_RGB32);
        img.fill(Qt::white);
        const QString src = tmp.filePath(QStringLiteral("shot.jpg"));
        QVERIFY(img.save(src));
        const QString name = cardimages::store(src, tmp.path(), nullptr, 90);
        QVERIFY(!name.isEmpty());
        QCOMPARE(QImage(tmp.filePath(name)).size(), QSize(100, 300));
    }

    void rejectsNonImage()
    {
        QTemporaryDir tmp;
        const QString src = tmp.filePath(QStringLiteral("x.txt"));
        QFile f(src);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
        f.close();
        QString err;
        QVERIFY(cardimages::store(src, tmp.path(), &err).isEmpty());
        QVERIFY(!err.isEmpty());
        QVERIFY(cardimages::store(tmp.filePath(QStringLiteral("missing.png")), tmp.path(), &err).isEmpty());
    }
};

QTEST_MAIN(TstCardImages)
#include "tst_cardimages.moc"
