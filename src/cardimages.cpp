#include "cardimages.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QTransform>
#include <QUuid>

namespace cardimages {

QString store(const QString &source, const QString &targetDir, QString *error, int rotation)
{
    auto fail = [&](const QString &msg) {
        if (error)
            *error = msg;
        return QString();
    };

    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot open image: %1").arg(in.errorString()));

    QImageReader reader(&in);
    reader.setAutoTransform(true); // honour EXIF orientation from phone cameras
    // Decode at reduced size when the format supports it (saves memory on 50 MP photos).
    const QSize full = reader.size();
    if (full.isValid() && qMax(full.width(), full.height()) > kMaxEdge)
        reader.setScaledSize(full.scaled(kMaxEdge, kMaxEdge, Qt::KeepAspectRatio));

    QImage img = reader.read();
    if (img.isNull())
        return fail(QStringLiteral("not a readable image: %1").arg(reader.errorString()));
    if (qMax(img.width(), img.height()) > kMaxEdge)
        img = img.scaled(kMaxEdge, kMaxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (rotation == 90 || rotation == 180 || rotation == 270)
        img = img.transformed(QTransform().rotate(rotation));
    if (img.hasAlphaChannel()) {
        // JPEG has no alpha: flatten transparent PNGs onto white instead of black.
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter(&flat).drawImage(0, 0, img);
        img = flat;
    }

    if (!QDir().mkpath(targetDir))
        return fail(QStringLiteral("cannot create %1").arg(targetDir));
    const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".jpg");
    QImageWriter writer(QDir(targetDir).filePath(name), "jpg");
    writer.setQuality(85);
    if (!writer.write(img))
        return fail(QStringLiteral("cannot save image: %1").arg(writer.errorString()));
    return name;
}

} // namespace cardimages
