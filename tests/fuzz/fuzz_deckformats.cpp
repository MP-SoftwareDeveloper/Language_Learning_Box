// libFuzzer harness for the import parsers (src/exchange/deckformats.cpp). The first input byte picks the parser,
// the rest is the file. Clang only; not compiled yet: see docs/SECURITY.md section 6 for the CMake block and the run line.
#include "exchange/deckformats.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    static int argc = 1;
    static char name[] = "fuzz_deckformats";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 || size > 2 * 1024 * 1024)
        return 0;
    const QByteArray body(reinterpret_cast<const char *>(data) + 1, int(size) - 1);
    static const QString tmp = QDir::tempPath() + QStringLiteral("/lb-fuzz");
    switch (data[0] % 6) {
    case 0: deckformats::read(body, QStringLiteral("fuzz"), tmp); break;
    case 1: deckformats::readLbox(body); break;
    case 2: deckformats::readCsv(body); break;
    case 3: deckformats::readApkg(body, tmp); break;
    case 4: deckformats::plainText(QString::fromUtf8(body)); break;
    case 5: deckformats::directDownloadUrl(QString::fromUtf8(body)); break;
    }
    return 0;
}
