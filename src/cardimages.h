#pragma once

#include <QString>

namespace cardimages {

// Longest edge of a stored picture. Cards only need a phone-screen-sized image,
// and this keeps each file at roughly 100-250 KB.
inline constexpr int kMaxEdge = 1024;

// Reads the image at `source` (a local path, or an Android content:// URI, which
// QFile understands on Android), applies EXIF rotation, scales it down to kMaxEdge
// and writes it as JPEG into `targetDir` under a new unique name.
// `rotation` (0/90/180/270, clockwise) is applied after EXIF, e.g. for a camera shot taken in
// landscape with the rotation lock on.
// Returns the file name (not the full path), or an empty string and sets *error.
QString store(const QString &source, const QString &targetDir, QString *error = nullptr, int rotation = 0);

} // namespace cardimages
