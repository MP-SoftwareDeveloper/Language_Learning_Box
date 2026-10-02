#include "deckformats.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrlQuery>
#include <QUuid>

// Qt's zip classes are private API; they moved from QtGui to QtCore in newer Qt versions.
#if __has_include(<QtCore/private/qzipreader_p.h>)
#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>
#else
#include <QtGui/private/qzipreader_p.h>
#include <QtGui/private/qzipwriter_p.h>
#endif

namespace deckformats {

namespace {

constexpr int kLboxVersion = 1;

QString csvField(const QString &s)
{
    if (s.contains(u',') || s.contains(u'"') || s.contains(u'\n') || s.contains(u'\r') || s.contains(u';'))
        return u'"' + QString(s).replace(u'"', QStringLiteral("\"\"")) + u'"';
    return s;
}

// RFC-4180-style parser for one separator; quoted fields may contain the separator and newlines.
QList<QStringList> parseDelimited(const QString &text, QChar sep)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false, fieldStarted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == u'"') {
                if (i + 1 < text.size() && text.at(i + 1) == u'"') { field += u'"'; ++i; }
                else quoted = false;
            } else {
                field += c;
            }
            continue;
        }
        if (c == u'"' && !fieldStarted) { quoted = true; fieldStarted = true; continue; }
        if (c == sep) { row << field; field.clear(); fieldStarted = false; continue; }
        if (c == u'\r') continue;
        if (c == u'\n') {
            row << field;
            rows << row;
            row.clear(); field.clear(); fieldStarted = false;
            continue;
        }
        field += c;
        fieldStarted = true;
    }
    if (fieldStarted || !row.isEmpty() || !field.isEmpty()) { row << field; rows << row; }
    return rows;
}

QChar detectSeparator(const QString &text)
{
    // First real line (Anki text exports start with "#separator:..." / "#html:..." lines).
    for (const QString &line : text.split(u'\n')) {
        const QString l = line.trimmed();
        if (l.startsWith(QLatin1String("#separator:"))) {
            const QString v = l.mid(11).trimmed().toLower();
            if (v == QLatin1String("tab")) return u'\t';
            if (v == QLatin1String("semicolon")) return u';';
            if (v == QLatin1String("comma")) return u',';
            if (v == QLatin1String("pipe")) return u'|';
        }
        if (l.isEmpty() || l.startsWith(u'#'))
            continue;
        const int tabs = int(l.count(u'\t')), semis = int(l.count(u';')), commas = int(l.count(u','));
        if (tabs > 0 && tabs >= semis && tabs >= commas) return u'\t';
        if (semis > 0 && semis >= commas) return u';';
        return u',';
    }
    return u',';
}

bool isHeader(const QStringList &row)
{
    static const QStringList names = {
        QStringLiteral("german"), QStringLiteral("deutsch"), QStringLiteral("front"), QStringLiteral("word"),
        QStringLiteral("wort"), QStringLiteral("term"), QStringLiteral("begriff"), QStringLiteral("vorderseite"),
        QStringLiteral("question"), QStringLiteral("frage")};
    return !row.isEmpty() && names.contains(row.first().trimmed().toLower());
}

// "Card list" text written by hand: every card ends with ';'. Inside a card either
//   * one field per line - German word / meaning / example sentence (the example may wrap), or
//   * one line - German word, meaning, example sentence (separated by tab, | or comma;
//     the example keeps any further commas).
// Several cards may share a line. Returns false when the text is not in this format
// (for example a semicolon-separated Excel table), so the normal CSV reader takes over.
bool parseCardList(const QString &text, QList<Item> *out)
{
    bool endsWithSemicolon = false;
    for (const QString &line : text.split(u'\n')) {
        if (line.trimmed().endsWith(u';')) { endsWithSemicolon = true; break; }
    }
    if (!endsWithSemicolon)
        return false;

    int records = 0;
    QList<Item> items;
    for (const QString &record : text.split(u';')) {
        QStringList lines;
        for (const QString &l : record.split(u'\n')) {
            const QString t = l.trimmed();
            if (!t.isEmpty())
                lines << t;
        }
        if (lines.isEmpty())
            continue;
        ++records;

        QString front, back, example;
        if (lines.size() >= 2) {
            front = lines.at(0);
            back = lines.at(1);
            example = lines.mid(2).join(u' ');
        } else {
            const QString &l = lines.first();
            QChar sep;
            if (l.contains(u'\t')) sep = u'\t';
            else if (l.contains(u'|')) sep = u'|';
            else if (l.contains(u',')) sep = u',';
            else continue; // a single word with nothing to split
            const int a = int(l.indexOf(sep));
            const int b = int(l.indexOf(sep, a + 1));
            front = l.left(a).trimmed();
            if (b < 0) {
                back = l.mid(a + 1).trimmed();
            } else {
                back = l.mid(a + 1, b - a - 1).trimmed();
                example = l.mid(b + 1).trimmed();
            }
        }
        Item it;
        it.front = plainText(front).replace(u'\n', u' ').trimmed();
        it.back = plainText(back).trimmed();
        it.example = plainText(example).replace(u'\n', u' ').trimmed();
        if (!it.front.isEmpty() && !it.back.isEmpty())
            items << it;
    }
    if (items.isEmpty() || items.size() * 2 < records)
        return false;
    *out = items;
    return true;
}

QJsonValue dateOrNull(const QDateTime &d)
{
    return d.isValid() ? QJsonValue(d.toUTC().toString(Qt::ISODate)) : QJsonValue();
}

} // namespace

// ------------------------------------------------------------------ writing

QByteArray writeLbox(const QList<Item> &items, const QHash<QString, QByteArray> &images,
                     const QString &title, bool withProgress, const QString &language)
{
    QJsonArray cards;
    QHash<QString, QString> pathOf; // image key -> path in the zip
    for (const Item &it : items) {
        QJsonObject c{
            {QStringLiteral("front"), it.front},
            {QStringLiteral("back"), it.back},
            {QStringLiteral("example"), it.example},
            {QStringLiteral("deck"), it.deck},
        };
        if (!it.image.isEmpty() && images.contains(it.image)) {
            QString &p = pathOf[it.image];
            if (p.isEmpty())
                p = QStringLiteral("images/%1.jpg").arg(pathOf.size());
            c.insert(QStringLiteral("image"), p);
        }
        if (withProgress) {
            c.insert(QStringLiteral("box"), it.box);
            c.insert(QStringLiteral("due"), dateOrNull(it.dueAt));
            c.insert(QStringLiteral("reviews"), it.reviews);
            c.insert(QStringLiteral("lapses"), it.lapses);
        }
        cards.append(c);
    }
    const QJsonObject root{
        {QStringLiteral("format"), QStringLiteral("learningbox")},
        {QStringLiteral("version"), kLboxVersion},
        {QStringLiteral("title"), title},
        {QStringLiteral("language"), language},
        {QStringLiteral("exported"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("withProgress"), withProgress},
        {QStringLiteral("cards"), cards},
    };
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    QZipWriter zip(&buf);
    zip.setCompressionPolicy(QZipWriter::AutoCompress);
    zip.addFile(QStringLiteral("cards.json"), QJsonDocument(root).toJson(QJsonDocument::Indented));
    for (auto it = pathOf.cbegin(); it != pathOf.cend(); ++it)
        zip.addFile(it.value(), images.value(it.key()));
    zip.close();
    return out;
}

QByteArray writeCsv(const QList<Item> &items)
{
    QString s = QStringLiteral("German,Meaning,Example\n");
    for (const Item &it : items)
        s += csvField(it.front) + u',' + csvField(it.back) + u',' + csvField(it.example) + u'\n';
    return QByteArray("\xEF\xBB\xBF") + s.toUtf8(); // BOM: Excel then reads UTF-8 (Persian, umlauts)
}

// ------------------------------------------------------------------ reading

QString plainText(const QString &html)
{
    QString s = html;
    s.replace(QRegularExpression(QStringLiteral("\\[sound:[^\\]]*\\]")), QString());
    s.replace(QRegularExpression(QStringLiteral("<br\\s*/?>|</div>|</p>|</li>"), QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\n"));
    s.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    static const QList<std::pair<QString, QString>> entities = {
        {QStringLiteral("&nbsp;"), QStringLiteral(" ")}, {QStringLiteral("&lt;"), QStringLiteral("<")},
        {QStringLiteral("&gt;"), QStringLiteral(">")},   {QStringLiteral("&quot;"), QStringLiteral("\"")},
        {QStringLiteral("&#39;"), QStringLiteral("'")},  {QStringLiteral("&apos;"), QStringLiteral("'")},
        {QStringLiteral("&amp;"), QStringLiteral("&")}};
    for (const auto &[e, r] : entities)
        s.replace(e, r, Qt::CaseInsensitive);
    // Numeric entities (&#228; / &#xE4;)
    static const QRegularExpression num(QStringLiteral("&#(x?)([0-9a-fA-F]+);"));
    QRegularExpressionMatch m;
    while ((m = num.match(s)).hasMatch()) {
        bool ok = false;
        const uint code = m.captured(1).isEmpty() ? m.captured(2).toUInt(&ok) : m.captured(2).toUInt(&ok, 16);
        s.replace(m.capturedStart(), m.capturedLength(), ok ? QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1) : QString());
    }
    // Tidy lines
    QStringList lines;
    for (const QString &l : s.split(u'\n')) {
        const QString t = l.simplified();
        if (!t.isEmpty())
            lines << t;
    }
    return lines.join(u'\n');
}

Package readCsv(const QByteArray &bytes)
{
    Package p;
    p.format = QStringLiteral("csv");
    QString text = QString::fromUtf8(bytes);
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    QList<Item> listed;
    if (parseCardList(text, &listed)) {
        p.items = listed;
        return p;
    }
    const QChar sep = detectSeparator(text);
    bool first = true;
    for (QStringList row : parseDelimited(text, sep)) {
        if (row.isEmpty() || (row.size() == 1 && row.first().trimmed().isEmpty()))
            continue;
        if (row.first().startsWith(u'#') && row.size() == 1)
            continue; // Anki "#html:true" etc.
        if (row.first().startsWith(QLatin1String("#separator:")) || row.first().startsWith(QLatin1String("#html:"))
            || row.first().startsWith(QLatin1String("#columns:")) || row.first().startsWith(QLatin1String("#tags")))
            continue;
        if (first && isHeader(row)) { first = false; continue; }
        first = false;
        Item it;
        it.front = plainText(row.value(0)).replace(u'\n', u' ');
        it.back = plainText(row.value(1));
        it.example = plainText(row.value(2)).replace(u'\n', u' ');
        if (!it.front.isEmpty())
            p.items << it;
    }
    if (p.items.isEmpty())
        p.error = QStringLiteral("No cards found in this text file.");
    return p;
}

Package readLbox(const QByteArray &data)
{
    Package p;
    p.format = QStringLiteral("lbox");
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    if (!zip.isReadable()) {
        p.error = QStringLiteral("Not a LearningBox file.");
        return p;
    }
    const QJsonObject root = QJsonDocument::fromJson(zip.fileData(QStringLiteral("cards.json"))).object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("learningbox")) {
        p.error = QStringLiteral("Not a LearningBox file.");
        return p;
    }
    if (root.value(QStringLiteral("version")).toInt() > kLboxVersion) {
        p.error = QStringLiteral("This file was made by a newer LearningBox. Please update the app.");
        return p;
    }
    p.title = root.value(QStringLiteral("title")).toString();
    p.hasProgress = root.value(QStringLiteral("withProgress")).toBool();
    p.language = root.value(QStringLiteral("language")).toString(QStringLiteral("de")); // version 1 files: German
    for (const QJsonValue &v : root.value(QStringLiteral("cards")).toArray()) {
        const QJsonObject c = v.toObject();
        Item it;
        it.front = c.value(QStringLiteral("front")).toString().trimmed();
        it.back = c.value(QStringLiteral("back")).toString();
        it.example = c.value(QStringLiteral("example")).toString();
        it.deck = c.value(QStringLiteral("deck")).toString();
        const QString img = c.value(QStringLiteral("image")).toString();
        if (!img.isEmpty() && !p.images.contains(img)) {
            const QByteArray bytes = zip.fileData(img);
            if (!bytes.isEmpty())
                p.images.insert(img, bytes);
        }
        if (p.images.contains(img))
            it.image = img;
        if (c.contains(QStringLiteral("box"))) {
            it.hasProgress = true;
            it.box = qBound(1, c.value(QStringLiteral("box")).toInt(1), 6);
            it.dueAt = QDateTime::fromString(c.value(QStringLiteral("due")).toString(), Qt::ISODate);
            it.reviews = c.value(QStringLiteral("reviews")).toInt();
            it.lapses = c.value(QStringLiteral("lapses")).toInt();
        }
        if (!it.front.isEmpty())
            p.items << it;
    }
    if (p.items.isEmpty())
        p.error = QStringLiteral("This LearningBox file has no cards.");
    return p;
}

Package readApkg(const QByteArray &data, const QString &tempDir)
{
    Package p;
    p.format = QStringLiteral("apkg");
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    if (!zip.isReadable()) {
        p.error = QStringLiteral("Not an Anki package.");
        return p;
    }
    QStringList names;
    for (const QZipReader::FileInfo &fi : zip.fileInfoList())
        names << fi.filePath;
    const QString oldFormatHint = QStringLiteral(
        "This Anki deck uses Anki's newest format. In Anki: File → Export → tick "
        "\"Support older Anki versions\", export again and import that file.");
    QString dbName;
    if (names.contains(QStringLiteral("collection.anki21")))
        dbName = QStringLiteral("collection.anki21");
    else if (names.contains(QStringLiteral("collection.anki2")))
        dbName = QStringLiteral("collection.anki2");
    if (dbName.isEmpty()) {
        p.error = names.contains(QStringLiteral("collection.anki21b")) ? oldFormatHint
                                                                        : QStringLiteral("Not an Anki package.");
        return p;
    }

    QDir().mkpath(tempDir);
    const QString dbPath = tempDir + QStringLiteral("/anki-") + QUuid::createUuid().toString(QUuid::Id128) + QStringLiteral(".db");
    {
        QFile f(dbPath);
        if (!f.open(QIODevice::WriteOnly) || f.write(zip.fileData(dbName)) <= 0) {
            p.error = QStringLiteral("Could not unpack the Anki deck.");
            return p;
        }
    }
    // media: {"0": "haus.jpg", ...} (older/compatible exports; newer binary lists are skipped)
    QHash<QString, QString> entryOfName;
    const QJsonObject media = QJsonDocument::fromJson(zip.fileData(QStringLiteral("media"))).object();
    for (auto it = media.begin(); it != media.end(); ++it)
        entryOfName.insert(it.value().toString(), it.key());

    const QString conn = QStringLiteral("anki-import-") + QUuid::createUuid().toString(QUuid::Id128);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            p.error = QStringLiteral("Could not open the Anki deck.");
        } else {
            QSqlQuery q(db);
            static const QRegularExpression imgRe(QStringLiteral("<img[^>]*src=[\"']?([^\"' >]+)"),
                                                  QRegularExpression::CaseInsensitiveOption);
            if (q.exec(QStringLiteral("SELECT flds FROM notes ORDER BY id"))) {
                while (q.next()) {
                    const QStringList f = q.value(0).toString().split(QChar(0x1f));
                    Item it;
                    it.front = plainText(f.value(0)).replace(u'\n', u' ');
                    it.back = plainText(f.value(1));
                    it.example = plainText(f.value(2)).replace(u'\n', u' ');
                    for (const QString &field : f) {
                        const auto m = imgRe.match(field);
                        if (!m.hasMatch())
                            continue;
                        const QString name = QUrl::fromPercentEncoding(m.captured(1).toUtf8());
                        const QString entry = entryOfName.value(name);
                        if (!entry.isEmpty() && !p.images.contains(name)) {
                            const QByteArray bytes = zip.fileData(entry);
                            if (!bytes.isEmpty())
                                p.images.insert(name, bytes);
                        }
                        if (p.images.contains(name))
                            it.image = name;
                        break;
                    }
                    if (!it.front.isEmpty())
                        p.items << it;
                }
            }
            // Anki's placeholder in new-format packages
            if (p.items.size() == 1 && p.items.first().front.contains(QLatin1String("update to the latest Anki"), Qt::CaseInsensitive))
                p.error = oldFormatHint;
            else if (p.items.isEmpty())
                p.error = QStringLiteral("No cards found in this Anki deck.");
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    QFile::remove(dbPath);
    if (!p.error.isEmpty()) {
        p.items.clear();
        p.images.clear();
    }
    return p;
}

Package read(const QByteArray &data, const QString &fileName, const QString &tempDir)
{
    if (data.startsWith("PK\x03\x04")) {
        QBuffer buf;
        buf.setData(data);
        buf.open(QIODevice::ReadOnly);
        QZipReader zip(&buf);
        for (const QZipReader::FileInfo &fi : zip.fileInfoList()) {
            if (fi.filePath == QLatin1String("cards.json"))
                return readLbox(data);
            if (fi.filePath.startsWith(QLatin1String("collection.anki")))
                return readApkg(data, tempDir);
        }
        Package p;
        p.error = QStringLiteral("This zip file is neither a LearningBox nor an Anki file.");
        return p;
    }
    const QByteArray head = data.left(512).trimmed().toLower();
    if (head.startsWith("<!doctype html") || head.startsWith("<html")) {
        Package p;
        p.error = QStringLiteral("The link opened a web page, not a file. Use a direct download link.");
        return p;
    }
    Q_UNUSED(fileName);
    return readCsv(data);
}

QUrl directDownloadUrl(const QString &link)
{
    QString s = link.trimmed();
    if (s.isEmpty())
        return {};
    if (!s.contains(QLatin1String("://")))
        s.prepend(QStringLiteral("https://"));
    QUrl url(s);
    const QString host = url.host().toLower();
    if (host == QLatin1String("drive.google.com")) {
        static const QRegularExpression id(QStringLiteral("/file/d/([^/]+)"));
        const auto m = id.match(url.path());
        QString fileId = m.hasMatch() ? m.captured(1) : QUrlQuery(url).queryItemValue(QStringLiteral("id"));
        if (!fileId.isEmpty())
            return QUrl(QStringLiteral("https://drive.google.com/uc?export=download&id=") + fileId);
    }
    if (host.endsWith(QLatin1String("dropbox.com"))) {
        QUrlQuery q(url);
        q.removeAllQueryItems(QStringLiteral("dl"));
        q.addQueryItem(QStringLiteral("dl"), QStringLiteral("1"));
        url.setQuery(q);
        return url;
    }
    if (host == QLatin1String("github.com") && url.path().contains(QLatin1String("/blob/"))) {
        QString path = url.path();
        path.replace(QLatin1String("/blob/"), QLatin1String("/"));
        return QUrl(QStringLiteral("https://raw.githubusercontent.com") + path);
    }
    return url;
}

} // namespace deckformats
