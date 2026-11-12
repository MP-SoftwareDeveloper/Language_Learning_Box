#!/usr/bin/env python3
"""Makes malformed / hostile deck files to test the LearningBox import code (Get cards, Anki, CSV, links).

    python tests/security/make_hostile_files.py            # small files -> tests/security/hostile/
    python tests/security/make_hostile_files.py --big      # + large ones (100 001 cards, 1 M CSV lines)
    python tests/security/make_hostile_files.py --corpus DIR   # valid seed files for the fuzzer

Pass = the app shows a clear message or imports safely. Never: crash, freeze (ANR), a file written outside the
app's folders, a network request caused by card text. Expected result per file: docs/SECURITY.md, section 4.
Everything here is standard library only. The files are harmless on their own; they only target this app's parsers.
"""
import argparse, io, json, os, sqlite3, struct, tempfile, zipfile, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "hostile")
SLOW = os.path.join(OUT, "slow")  # files that hang today (finding F-7): the Qt test skips this folder


def png(width, height):
    """A tiny valid PNG that only *claims* to be width x height (the pixel data is 16 bytes)."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"\x00" * 16)) + chunk(b"IEND", b""))


def write(folder, name, data):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  {os.path.relpath(path, HERE):60s} {len(data):>12,} bytes")


def lbox_bytes(cards, entries=None, raw_cards_json=None, title="Hostile test"):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("cards.json", raw_cards_json if raw_cards_json is not None else
                   json.dumps({"format": "learningbox", "version": 1, "title": title, "cards": cards}))
        for name, data in (entries or {}).items():
            z.writestr(name, data)
    return buf.getvalue()


def patch_zip_sizes(zip_bytes, fake_size):
    """Make the zip *state* a small uncompressed size while the deflate stream still inflates to much more."""
    b = bytearray(zip_bytes)
    i = b.find(b"PK\x03\x04")          # local header: uncompressed size at +22
    struct.pack_into("<I", b, i + 22, fake_size)
    j = b.find(b"PK\x01\x02")          # central directory: uncompressed size at +24
    struct.pack_into("<I", b, j + 24, fake_size)
    return bytes(b)


def anki_db(notes_sql_rows=None, view_sql=None):
    """An Anki-like collection.anki2 (SQLite). notes_sql_rows: list of flds strings."""
    fd, path = tempfile.mkstemp(suffix=".anki2")
    os.close(fd)
    try:
        con = sqlite3.connect(path)
        if view_sql:
            con.execute(view_sql)
        else:
            con.execute("CREATE TABLE notes (id integer primary key, guid text, mid integer, mod integer, usn integer,"
                        " tags text, flds text, sfld text, csum integer, flags integer, data text)")
            for n, flds in enumerate(notes_sql_rows or [], 1):
                con.execute("INSERT INTO notes VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                            (n, f"g{n}", 1, 0, -1, "", flds, flds.split("\x1f")[0], 0, 0, ""))
        con.commit()
        con.close()
        with open(path, "rb") as f:
            return f.read()
    finally:
        os.remove(path)


def apkg_bytes(db, media=None, entries=None):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("collection.anki2", db)
        z.writestr("media", json.dumps(media or {}))
        for name, data in (entries or {}).items():
            z.writestr(name, data)
    return buf.getvalue()


def make_small():
    print("small files ->", os.path.relpath(OUT, HERE))
    # 01 zip-slip: entry names and the card's "image" use ../ and an absolute path
    write(OUT, "01_zipslip.lbox", lbox_bytes(
        [{"front": "Haus", "back": "house", "image": "../../evil.jpg"},
         {"front": "Baum", "back": "tree", "image": "/etc/passwd"}],
        {"../../evil.jpg": png(8, 8), "/etc/passwd": b"x"}))
    # 02 HTML / links / remote + local images inside the card text
    write(OUT, "02_html_cards.lbox", lbox_bytes([{
        "front": '<img src="https://example.invalid/pixel.png">Haus',
        "back": '<font size="7" color="red">TAP HERE</font><br><a href="https://example.invalid/login">Sign in</a>',
        "example": '<h1>BIG</h1><img src="file:///etc/hostname">'}]))
    # 03 the zip states 100 bytes but the data inflates to 6 MB
    big = '{"format":"learningbox","version":1,"cards":[' + ('{"front":"a","back":"b"},' * 250_000) + '{"front":"z"}]}'
    write(OUT, "03_lying_size.lbox", patch_zip_sizes(lbox_bytes([], raw_cards_json=big), 100))
    # 04 / 05 broken and absurdly nested JSON
    write(OUT, "04_truncated_json.lbox", lbox_bytes([], raw_cards_json='{"format":"learningbox","version":1,"cards":[{"front":"a"'))
    write(OUT, "05_deep_json.lbox", lbox_bytes([], raw_cards_json="[" * 100_000))
    # 06 a picture that claims 30000 x 30000 px (3.6 GB when decoded) but holds 16 bytes of data
    write(OUT, "06_huge_dimension_image.lbox", lbox_bytes(
        [{"front": "Haus", "back": "house", "image": "images/0.png"}], {"images/0.png": png(30000, 30000)}))
    # 07 Anki deck: note text with <img src="../../..">, media map pointing at ../../
    write(OUT, "07_traversal.apkg", apkg_bytes(
        anki_db(["Haus\x1fhouse\x1f<img src=\"../../pwn.jpg\">"]), {"0": "../../pwn.jpg"}, {"0": png(8, 8)}))
    # 08 CSV: one row with 200 000 columns, one 10 MB field, HTML in fields
    write(OUT, "08_wide.csv", (";".join("x" for _ in range(200_000)) + "\n").encode())
    write(OUT, "09_long_field.csv", b'"' + b"a" * (10 * 1024 * 1024) + b'";meaning;example\n')
    write(OUT, "13_entities.csv", b'"' + b"&#65;" * 2_000_000 + b'";x;y\n')       # 10 MB of numeric entities in one field
    write(OUT, "14_open_tags.csv", b'"' + b"<" * 5_000_000 + b'";x;y\n')           # 5 M '<' and no '>'
    write(OUT, "15_tags_then_gt.csv", b'">' + b"<a " * 3_000_000 + b'";x;y\n')     # a '>' only at the very start
    write(OUT, "10_html.csv", '<b>Haus</b>;<script>alert(1)</script><img src=x onerror=1>;<a href="https://example.invalid">x</a>\n'.encode())
    # 11 not what its extension says
    write(OUT, "11_not_a_zip.lbox", b"PK\x03\x04" + os.urandom(2048))
    write(OUT, "12_empty.apkg", b"")
    # slow/: a view that produces 50 million rows. The importer's "SELECT flds FROM notes ORDER BY id" has to sort them all
    # before the 100 000-card limit can apply -> the screen freezes (finding F-7). Skipped by tst_hostile.
    view = ("CREATE VIEW notes AS WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x+1 FROM c WHERE x<50000000) "
            "SELECT x AS id, 'w'||x||char(31)||'m' AS flds FROM c")
    write(SLOW, "20_view_50M_rows.apkg", apkg_bytes(anki_db(view_sql=view)))


def make_big():
    print("big files ->", os.path.relpath(OUT, HERE))
    cards = [{"front": f"w{n}", "back": "m"} for n in range(100_001)]
    write(OUT, "30_100001_cards.lbox", lbox_bytes(cards))
    write(OUT, "31_1M_lines.csv", ("a;b;c\n" * 1_000_000).encode())  # readCsv has no card limit (finding F-5)


def make_corpus(folder):
    """Seeds for tests/fuzz/fuzz_deckformats.cpp: the first byte picks the parser (0 = read(), 4 = plainText, 5 = URL)."""
    print("seed corpus ->", folder)
    write(folder, "seed_lbox", b"\x00" + lbox_bytes([{"front": "das Haus", "back": "house", "example": "Das Haus ist groß.",
                                                      "box": 2, "due": "2026-11-10T00:00:00", "reviews": 3, "lapses": 1},
                                                     {"front": "der Baum", "back": "tree"}]))
    write(folder, "seed_csv", b"\x00German,Meaning,Example\nHaus,house,Das Haus ist gross.\nBaum;tree;\n")
    write(folder, "seed_apkg", b"\x00" + apkg_bytes(anki_db(["Haus\x1fhouse\x1fDas Haus.", "Baum\x1ftree\x1f"])))
    write(folder, "seed_html", b"\x04<div>Haus</div><br>&amp; &#228; &auml;")
    write(folder, "seed_url", b"\x05https://drive.google.com/file/d/ABC123/view?usp=sharing")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--big", action="store_true", help="also write the large files")
    ap.add_argument("--corpus", metavar="DIR", help="only write valid seed files for the fuzzer into DIR")
    a = ap.parse_args()
    if a.corpus:
        make_corpus(a.corpus)
    else:
        make_small()
        if a.big:
            make_big()
