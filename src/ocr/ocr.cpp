#include "ocr.h"

#include <algorithm>
#include <QFile>
#include <QHash>
#include <QTransform>
#include <QFileInfo>

#ifdef LB_HAVE_OCR
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#include <leptonica/allheaders.h>
#include <future>
#include <vector>
#include <memory>
#endif

namespace ocr {

bool compiledIn()
{
#ifdef LB_HAVE_OCR
    return true;
#else
    return false;
#endif
}

QString engineVersion()
{
#ifdef LB_HAVE_OCR
    return QString::fromLatin1(tesseract::TessBaseAPI::Version());
#else
    return {};
#endif
}

#ifdef LB_HAVE_OCR
namespace {

constexpr double kGoodEnough = 60; // qualityScore of roughly 10+ confident words

// ---- Leptonica <-> QImage (8-bit gray) ----
using PixPtr = std::unique_ptr<PIX, void (*)(PIX *)>;
void destroyPix(PIX *p) { pixDestroy(&p); }

PixPtr toPix(const QImage &gray)
{
    PIX *p = pixCreate(gray.width(), gray.height(), 8);
    const int wpl = pixGetWpl(p);
    l_uint32 *data = pixGetData(p);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *src = gray.constScanLine(y);
        l_uint32 *line = data + y * wpl;
        for (int x = 0; x < gray.width(); ++x)
            SET_DATA_BYTE(line, x, src[x]);
    }
    return PixPtr(p, destroyPix);
}

QImage fromPix(PIX *p)
{
    QImage out(int(pixGetWidth(p)), int(pixGetHeight(p)), QImage::Format_Grayscale8);
    const int wpl = pixGetWpl(p);
    l_uint32 *data = pixGetData(p);
    for (int y = 0; y < out.height(); ++y) {
        uchar *dst = out.scanLine(y);
        l_uint32 *line = data + y * wpl;
        for (int x = 0; x < out.width(); ++x)
            dst[x] = uchar(GET_DATA_BYTE(line, x));
    }
    return out;
}

} // namespace

QImage cleanForOcr(const QImage &gray, int blurRadius)
{
    // Box blur removes screen moire / paper texture; background normalisation evens out
    // shadows and uneven light; local contrast normalisation brings faint print back.
    PixPtr pix = toPix(gray);
    PixPtr cur(nullptr, destroyPix);
    PIX *src = pix.get();
    if (blurRadius > 0) {
        cur.reset(pixBlockconvGray(src, nullptr, blurRadius, blurRadius));
        if (cur) src = cur.get();
    }
    PixPtr norm(pixBackgroundNormSimple(src, nullptr, nullptr), destroyPix);
    if (norm) src = norm.get();
    PixPtr contrast(pixContrastNorm(nullptr, src, 50, 50, 130, 2, 2), destroyPix);
    if (contrast) src = contrast.get();
    return fromPix(src);
}

namespace {

// One Tesseract pass over an 8-bit gray image.
OcrResult runTesseract(const QImage &gray, const QString &tessdataDir, const QString &lang,
                       tesseract::PageSegMode mode = tesseract::PSM_AUTO)
{
    OcrResult result;
    result.imageSize = gray.size();

    auto api = std::make_unique<tesseract::TessBaseAPI>();
    // LSTM-only engine: the "fast" models contain no legacy data.
    if (api->Init(QFile::encodeName(tessdataDir).constData(), lang.toLatin1().constData(),
                  tesseract::OEM_LSTM_ONLY) != 0) {
        result.error = QStringLiteral("Tesseract could not load %1").arg(lang);
        return result;
    }
    api->SetPageSegMode(mode);
    api->SetVariable("user_defined_dpi", "300"); // photos have no DPI; avoids a warning
    api->SetImage(gray.constBits(), gray.width(), gray.height(), 1, int(gray.bytesPerLine()));
    if (api->Recognize(nullptr) != 0) {
        result.error = QStringLiteral("recognition failed");
        api->End();
        return result;
    }

    std::unique_ptr<tesseract::ResultIterator> it(api->GetIterator());
    int line = -1, block = -1;
    if (it) {
        do {
            if (it->IsAtBeginningOf(tesseract::RIL_BLOCK) || it->IsAtBeginningOf(tesseract::RIL_PARA))
                ++block;
            if (it->IsAtBeginningOf(tesseract::RIL_TEXTLINE))
                ++line;
            std::unique_ptr<char[]> txt(it->GetUTF8Text(tesseract::RIL_WORD));
            if (!txt)
                continue;
            const QString text = QString::fromUtf8(txt.get()).trimmed();
            const float conf = it->Confidence(tesseract::RIL_WORD);
            int x1, y1, x2, y2;
            if (text.isEmpty() || !it->BoundingBox(tesseract::RIL_WORD, &x1, &y1, &x2, &y2))
                continue;
            // Skip noise: no letters/digits, very low confidence, or short unsure fragments
            // ("Il", "N", "HN" from picture edges and texture).
            int letters = 0;
            for (QChar c : text)
                if (c.isLetterOrNumber()) ++letters;
            if (letters == 0 || conf < 25.0f || (letters <= 2 && conf < 75.0f))
                continue;
            result.words.append(OcrWord{text, QRect(QPoint(x1, y1), QPoint(x2 - 1, y2 - 1)),
                                        qMax(line, 0), qMax(block, 0), conf});
        } while (it->Next(tesseract::RIL_WORD));
    }
    api->End();
    return result;
}

} // namespace
#endif

double qualityScore(const OcrResult &r)
{
    // Confident, real-looking words count; noise barely does.
    double score = 0;
    for (const OcrWord &w : r.words) {
        int letters = 0;
        for (QChar c : w.text)
            if (c.isLetter()) ++letters;
        if (letters >= 3 && w.confidence >= 60.0f)
            score += w.confidence / 100.0 * qMin(letters, 8);
    }
    return score;
}

OcrResult joinLineFragments(const OcrResult &r)
{
    // Sparse-text mode reports each text fragment separately and in no useful order
    // ("heute. Ja, gehen" before "So, das ist genug"). Rebuild visual rows: fragments whose
    // vertical centres are close form one line, ordered left to right; rows stacked closely
    // with horizontal overlap (a two-line subtitle) share a block.
    struct Frag { QList<OcrWord> words; QRect box; };
    QList<Frag> frags;
    for (const OcrWord &w : r.words) {
        if (frags.isEmpty() || frags.last().words.last().line != w.line)
            frags.append(Frag{});
        frags.last().words.append(w);
        frags.last().box |= w.box;
    }
    std::stable_sort(frags.begin(), frags.end(), [](const Frag &a, const Frag &b) {
        return a.box.center().y() < b.box.center().y();
    });
    QList<Frag> rows;
    for (const Frag &f : std::as_const(frags)) {
        if (!rows.isEmpty()) {
            Frag &row = rows.last();
            // Same row if the vertical ranges overlap by a third of the smaller height; this
            // also holds for a slightly tilted photo, where a long line drifts up or down.
            const int overlap = qMin(row.box.bottom(), f.box.bottom()) - qMax(row.box.top(), f.box.top());
            if (overlap * 3 >= qMin(row.box.height(), f.box.height())) {
                row.words += f.words;
                row.box |= f.box;
                continue;
            }
        }
        rows.append(f);
    }
    OcrResult out;
    out.imageSize = r.imageSize;
    out.error = r.error;
    int block = -1;
    QRect prev;
    for (int k = 0; k < rows.size(); ++k) {
        Frag &row = rows[k];
        std::stable_sort(row.words.begin(), row.words.end(), [](const OcrWord &a, const OcrWord &b) {
            return a.box.left() < b.box.left();
        });
        const bool continues = k > 0 && row.box.top() - prev.bottom() < prev.height() * 1.5
                               && row.box.left() < prev.right() && prev.left() < row.box.right();
        if (!continues)
            ++block;
        for (OcrWord w : std::as_const(row.words)) {
            w.line = k;
            w.block = block;
            out.words.append(w);
        }
        prev = row.box;
    }
    return out;
}

OcrResult mergeResults(const OcrResult &base, const OcrResult &extra)
{
    // Group `extra` into its text lines; keep a line only if it holds real text and does not
    // overlap anything `base` already found.
    struct Line { QList<OcrWord> words; QRect box; };
    QList<Line> added;
    for (const OcrWord &w : extra.words) {
        if (added.isEmpty() || added.last().words.last().line != w.line)
            added.append(Line{});
        added.last().words.append(w);
        added.last().box |= w.box;
    }
    auto overlapsBase = [&](const QRect &r) {
        const QRect grown = r.adjusted(-2, -2, 2, 2);
        for (const OcrWord &w : base.words)
            if (grown.intersects(w.box))
                return true;
        return false;
    };
    QList<Line> keep;
    for (const Line &l : std::as_const(added)) {
        OcrResult probe;
        probe.words = l.words;
        if (qualityScore(probe) > 0 && !overlapsBase(l.box))
            keep.append(l);
    }
    if (keep.isEmpty())
        return base;

    // Base lines, in Tesseract's reading order.
    QList<Line> lines;
    QList<int> blockOf; // block id per line (base ids, or new ids for added lines)
    for (const OcrWord &w : base.words) {
        if (lines.isEmpty() || lines.last().words.last().line != w.line) {
            lines.append(Line{});
            blockOf.append(w.block);
        }
        lines.last().words.append(w);
        lines.last().box |= w.box;
    }
    // Insert each added line before the first base line that starts below its middle, so a
    // subtitle lands between the text above and below it. Stacked added lines (a two-line
    // subtitle) share a block, so a sentence can run across them.
    int nextBlock = 1000000;
    QRect prevBox;
    int prevBlock = -1;
    for (const Line &l : std::as_const(keep)) {
        const bool continues = prevBlock >= 0
            && l.box.top() - prevBox.bottom() < prevBox.height() * 1.5
            && l.box.left() < prevBox.right() && prevBox.left() < l.box.right();
        const int block = continues ? prevBlock : nextBlock++;
        int at = int(lines.size());
        for (int k = 0; k < lines.size(); ++k) {
            if (lines.at(k).box.top() > l.box.center().y()) { at = k; break; }
        }
        lines.insert(at, l);
        blockOf.insert(at, block);
        prevBox = l.box;
        prevBlock = block;
    }

    // Renumber lines and blocks in the new order.
    OcrResult out;
    out.imageSize = base.imageSize;
    QHash<int, int> blockIds;
    for (int k = 0; k < lines.size(); ++k) {
        if (!blockIds.contains(blockOf.at(k))) {
            const int id = int(blockIds.size());
            blockIds.insert(blockOf.at(k), id);
        }
        const int b = blockIds.value(blockOf.at(k));
        for (OcrWord w : lines.at(k).words) {
            w.line = k;
            w.block = b;
            out.words.append(w);
        }
    }
    return out;
}

#ifdef LB_HAVE_OCR
namespace {
// All passes on one orientation of the (gray) picture; see recognize().
OcrResult readUpright(const QImage &gray, const QString &tessdataDir, const QString &lang)
{
    // Always, in parallel (separate Tesseract instances):
    //  * page layout (PSM_AUTO): books, worksheets, anything with columns and paragraphs;
    //  * sparse text: text anywhere, e.g. subtitles/captions inside a picture, which the page
    //    layout discards as "image" - even when it finds other text on the same photo.
    auto sparseRun = std::async(std::launch::async, [&] {
        return runTesseract(gray, tessdataDir, lang, tesseract::PSM_SPARSE_TEXT);
    });
    //  * sparse text on a half-size copy: large text (subtitles or signs photographed close up)
    //    is read better near Tesseract's preferred size; boxes are scaled back.
    auto halfRun = std::async(std::launch::async, [&] {
        const QImage half = gray.scaled(gray.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        OcrResult r = runTesseract(half, tessdataDir, lang, tesseract::PSM_SPARSE_TEXT);
        const double fx = double(gray.width()) / half.width(), fy = double(gray.height()) / half.height();
        for (OcrWord &w : r.words)
            w.box = QRect(int(w.box.x() * fx), int(w.box.y() * fy), int(w.box.width() * fx), int(w.box.height() * fy));
        r.imageSize = gray.size();
        return joinLineFragments(r);
    });
    OcrResult best = runTesseract(gray, tessdataDir, lang);
    OcrResult sparse = joinLineFragments(sparseRun.get());
    OcrResult half = halfRun.get();
    if (!best.error.isEmpty())
        return best;
    QList<OcrResult> others;
    if (sparse.error.isEmpty())
        others.append(std::move(sparse));
    if (half.error.isEmpty())
        others.append(std::move(half));

    // Little confident text: also try cleaned-up versions (photos of screens with moire,
    // glossy or unevenly lit pages). Blur scaled to the picture size, then normalised.
    if (qualityScore(best) < kGoodEnough) {
        const int longEdge = qMax(gray.width(), gray.height());
        const int radius = qMax(1, longEdge / 1200);
        auto pass = [&](int r) { return runTesseract(cleanForOcr(gray, r), tessdataDir, lang); };
        auto light = std::async(std::launch::async, pass, radius);
        auto strong = std::async(std::launch::async, pass, radius * 2);
        for (OcrResult cand : {light.get(), strong.get()})
            if (cand.error.isEmpty())
                others.append(std::move(cand));
    }

    // The pass with the most confident words is the base; lines that only the other passes
    // found (and that don't overlap it) are merged in. All images have the same size, so
    // boxes are comparable.
    std::sort(others.begin(), others.end(), [](const OcrResult &a, const OcrResult &b) {
        return qualityScore(a) > qualityScore(b);
    });
    if (!others.isEmpty() && qualityScore(others.first()) > qualityScore(best))
        std::swap(best, others.first());
    for (const OcrResult &o : std::as_const(others))
        best = mergeResults(best, o);
    best.imageSize = gray.size();
    return best;
}
} // namespace
#endif

OcrResult recognize(const QImage &image, const QString &tessdataDir, const QString &lang, int rotationHint)
{
#ifndef LB_HAVE_OCR
    Q_UNUSED(image);
    Q_UNUSED(tessdataDir);
    Q_UNUSED(lang);
    Q_UNUSED(rotationHint);
    OcrResult result;
    result.error = QStringLiteral("This build has no OCR engine (Tesseract).");
    return result;
#else
    OcrResult result;
    result.imageSize = image.size();
    if (image.isNull()) {
        result.error = QStringLiteral("empty image");
        return result;
    }
    if (!QFileInfo::exists(tessdataDir + u'/' + lang + QStringLiteral(".traineddata"))) {
        result.error = QStringLiteral("language data %1.traineddata not found").arg(lang);
        return result;
    }

    // Leptonica is built without image codecs (Qt decodes the photos); Tesseract then
    // encodes line images as BMP instead of PNG in memory and Leptonica warns about it.
    // Harmless, so keep the log clean.
    setMsgSeverity(L_SEVERITY_NONE);

    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    auto turned = [&](int degrees) {
        return degrees ? gray.transformed(QTransform().rotate(degrees)) : gray;
    };

    // Start with the turn the caller knows (phone tilt at capture, like Google Lens), else upright.
    const int start = (rotationHint == 90 || rotationHint == 180 || rotationHint == 270) ? rotationHint : 0;
    OcrResult best = readUpright(turned(start), tessdataDir, lang);
    best.rotation = start;
    if (!best.error.isEmpty() || qualityScore(best) >= kGoodEnough)
        return best;

    // Little text found: the turn may be wrong (gallery picture without orientation info, phone
    // held flat so the tilt was unclear). Probe the other turns cheaply (sparse pass on a
    // half-size copy, in parallel); only a clearly better turn is read in full. Without a hint
    // only sideways turns are tried (upside-down photos are rare). The caller shows the picture
    // turned the same way so the boxes line up.
    auto probe = [&](int degrees) {
        QImage g = turned(degrees);
        g = g.scaled(g.size() / 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return qualityScore(runTesseract(g, tessdataDir, lang, tesseract::PSM_SPARSE_TEXT));
    };
    QList<int> others;
    for (int d : {0, 90, 270, 180})
        if (d != start && (rotationHint >= 0 || d != 180))
            others.append(d);
    auto base = std::async(std::launch::async, probe, start);
    std::vector<std::future<double>> runs;
    for (int d : std::as_const(others))
        runs.push_back(std::async(std::launch::async, probe, d));
    const double sStart = base.get();
    int turn = start;
    double sTurn = -1;
    for (int k = 0; k < others.size(); ++k) {
        const double s = runs[k].get();
        if (s > sTurn) { sTurn = s; turn = others.at(k); }
    }
    if (turn != start && sTurn > sStart * 1.5 + 5) {
        // The probe is decisive enough: take the turned reading (even if it is weak too, the
        // picture is then at least shown upright for the user).
        OcrResult r = readUpright(turned(turn), tessdataDir, lang);
        if (r.error.isEmpty()) {
            r.rotation = turn;
            best = std::move(r);
        }
    }
    return best;
#endif
}

} // namespace ocr
