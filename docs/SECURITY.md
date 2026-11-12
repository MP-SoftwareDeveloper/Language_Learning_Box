# LearningBox – Security checklist and audit

Living document. When a finding is fixed, change its status to ✅ and add a line to the [update log](#9-update-log).
Audit: **2026-11-09**, code at commit `14e9072` (main). Method: static review + searches of the source tree and the
git history (132 commits), plus small experiments. Fixes: **2026-11-09**, same day (section 1a).
**Not done:** running the app on a phone, running gitleaks / cppcheck / MobSF (not installed where the audit ran) –
section 6 says how.

Result in one line: **no high-severity problem found**; the 7 findings (2 medium, 5 low) are fixed in the working tree
(not yet committed), except two parts that need you: the OpenSSL rebuild (F-2) and the decision about the
all-files permission (F-1). The fixes were built and tested on Qt 6.4 in a Linux sandbox, **not** on a phone.

---

## 1. Findings

| ID | Severity | Area | Finding | Status |
|---|---|---|---|---|
| F-1 | Medium | Backup / permissions | `MANAGE_EXTERNAL_STORAGE` + unvalidated restore from a folder other apps can write | ✅ restore is validated · ⚠️ permission kept (your decision) |
| F-2 | Medium | Dependencies | Bundled OpenSSL 3.1.8 is end-of-life (since 2025-03-14); it carries all HTTPS traffic | 🟡 rebuild script + CI job ready, **needs one run** |
| F-3 | Low–Medium | QML / import | Card text from `.lbox` files is shown as rich text (HTML) | ✅ |
| F-4 | Low | Network | Azure Read endpoint accepts `http://` (key + photo in clear text) | ✅ |
| F-5 | Low | Import | Link import: unlimited redirects; CSV import has no card limit | ✅ |
| F-6 | Low | Android | FileProvider paths cover the whole app folder and all external storage | ✅ (test Share on the phone) |
| F-7 | Low | Import | A crafted `.apkg` can freeze the app (view with 50 M rows, run on the UI thread) – **tested** | ✅ |
| F-8 | Info | Secrets | Azure keys are stored in plain text in `QSettings` (app-private) | accepted |
| F-9 | Info | Privacy | Online lookups are on by default and send text to third parties | accepted, document it |
| F-10 | Info | Release | Debug APK in the project folder; release signing is outside the repo | your process |
| F-11 | Info | Android | `requestLegacyExternalStorage` | no change: **needed** on Android 10 |
| F-12 | Low | Import | *(found while fixing)* `plainText()` took quadratic time on numeric entities / open tags | ✅ |

### 1a. What was changed (2026-11-09)
| Finding | Change | Test |
|---|---|---|
| F-3 | `deckformats.cpp` `readLbox`: title, front, back, example, deck go through `plainText()`; `textFormat: Text.PlainText` added to all 185 `Label`/`Text` items in `qml/` (no QML used rich text anywhere) | `tst_hostile::htmlInLboxIsPlainText`, `qmllint` on all 49 QML files |
| F-4 | `azureread.cpp`: only `https://` endpoints; anything else → no URL | `tst_azureread::onlyEncryptedEndpoints` |
| F-5 | `deckexchange.cpp`: `setMaximumRedirectsAllowed(5)`; `readCsv`: stops at 100 000 cards | `tst_hostile::csvIsLimitedToMaxCards` |
| F-6 | `qtprovider_paths.xml`: only `files-path shared/` | not testable off the phone |
| F-7 | `readApkg`: database opened read-only; `notes` must be a table in `sqlite_master`, else "Not an Anki package." | `tst_hostile::ankiNotesViewIsRefused` (was 8 s+, now instant) |
| F-12 | `plainText()`: fields cut at 20 000 characters, regexes built once, skipped when the text has no `<`/`&`, numeric entities decoded in one pass | `tst_hostile` (49 s → 2 s on the first hostile files) |
| F-1 | `backup.cpp`: a restore first copies the shared file into the app's private folder and only uses it if `PRAGMA quick_check` is `ok`, `user_version` is 1…current schema, and the `cards` table exists (`CardStore::schemaVersion()`); pictures are only taken if named `<uuid>.jpg` and ≤ 8 MB; the restore offer (card count) is only shown for a valid file | `tst_backup` (9 tests) |
| F-2 | `3rdparty/build_openssl_android.sh` (OpenSSL 3.5.9 LTS, sha256-pinned, KDAB's patch embedded, 16 KB pages) + `.github/workflows/openssl-android.yml`; README row updated | download + checksum + patch tested; the compile needs the NDK (see F-2) |

**Test run (Linux sandbox, Qt 6.4.2, 21 test programs):** all pass except `tst_deckexchange` (2 checks:
`csvExportOfOneBox`, `importIntoNewLearningBox`), which fails **identically before any change** – look at it separately.
`tst_backup`, `tst_hostile`, `tst_deckformats`, `tst_azureread`, `tst_cardstore`, `tst_cardstore_migrate` are clean under
AddressSanitizer + UBSan. The QML files were syntax-checked only (`qmllint`), not run. Nothing was run on Android.

### F-1 Backup in `Documents/LearningBox` (Medium) – restore validated ✅, permission kept ⚠️
**Done:** step 2 below. **Left for you:** step 1 – removing `MANAGE_EXTERNAL_STORAGE` also removes the automatic backup after a reinstall (a product decision, so I did not do it). With the validation in place the tampering risk is closed; the remaining reasons to drop the permission are privacy (other apps can read your vocabulary) and Google Play's policy.
**Where:** `android/AndroidManifest.xml` (`MANAGE_EXTERNAL_STORAGE`), `src/backup.cpp` (`restoreIfFresh`, `restoreNow`).
**Why it matters:**
- Any app with storage access can read the whole card database in `Documents/LearningBox` (no secrets – Azure keys are not in it – but it is your data) and **replace** it.
- On a fresh install the file is copied straight into the app (`QFile::copy` in `restoreIfFresh`) with no check. A planted or corrupt database then gets opened with the app's own queries.
- The pictures copied back from `images/` are decoded with Qt's image readers: a malformed picture is attack surface.
- Google Play restricts "All files access" to apps whose core purpose needs it; a flashcard app with a backup feature is likely to be refused.

**Fix (best first):**
1. Drop `MANAGE_EXTERNAL_STORAGE`. Android Auto Backup is already configured correctly (`backup_rules.xml`, `data_extraction_rules.xml`: cards + pictures only) and *Send / back up cards* exports a `.lbox`. If you still want a visible copy, write it through the system file picker (Storage Access Framework, `ACTION_CREATE_DOCUMENT`) on request, not automatically.
2. If you keep the folder: validate before any restore – open the copy read-only, require `PRAGMA quick_check` = `ok`, `PRAGMA user_version` ≤ the app's schema version, and the expected tables; refuse otherwise. Check picture file names against `[0-9a-f-]+\.jpg`.

### F-2 OpenSSL is end-of-life (Medium) – 🟡 ready to run
**Status:** KDAB's `android_openssl` (checked 2026-11-09) still ships 3.1.8, so there is no ready-made newer build. I prepared `3rdparty/build_openssl_android.sh` (OpenSSL 3.5.9, tarball checksum pinned, KDAB's patch applies cleanly – tested) and the manual GitHub Action `openssl-android`. The compile itself needs the Android NDK, which I could not download here. **To finish:** run the Action (or the script in WSL with `ANDROID_NDK_ROOT` set), copy `ssl_3/arm64-v8a` and `ssl_3/x86_64` over the old libraries, rebuild, and test on the phone: a translation lookup, Tatoeba examples, a link import, Azure *Test*. Rollback = `git checkout 3rdparty/android_openssl`.
**Evidence:** `3rdparty/android_openssl/ssl_3/arm64-v8a/libcrypto_3.so` reports `OpenSSL 3.1.8 11 Feb 2025`. The 3.1 branch reached end of life on 2025-03-14 and gets no security fixes; 3.0 LTS ended 2026-09-07; the current long-term-support branch is 3.5 (sources in section 8).
**Why it matters:** this library does the TLS for translation, Tatoeba, Wiktionary, MyMemory, Azure and link import.
**Fix:** get a newer build of KDAB `android_openssl` (check its repository for a build of a supported OpenSSL release) or build OpenSSL 3.5 LTS with the NDK. Replace **both** `arm64-v8a` (committed) and `x86_64` (emulator, not committed). Then test: online translation, Tatoeba examples, a link import, Azure *Test*. Put a reminder in this file to re-check every 6 months.

### F-3 HTML in card text is rendered (Low–Medium) – ✅ fixed
**Note:** `plainText()` decodes entities after removing tags, so `&lt;b&gt;` becomes the text `<b>`; that is why the QML side is also plain text now. Not covered: texts that Qt Quick Controls draw themselves (`ComboBox`, `Button`, `ItemDelegate`: e.g. learning-box names) keep Qt's default; the import strips tags from deck titles, which are the only outside source of those names.
**Where:** `src/exchange/deckformats.cpp` `readLbox` keeps `front`/`back`/`example`/`deck` as is (the CSV and Anki importers run `plainText()` on them); in QML only 3 places set `textFormat: …PlainText`, every other `Label`/`Text` (`CardFace`, `MeaningText`, `ReviewPage`, `BoxPage`, `CardListPage`, `FavoritesPage`, `GetCardsPage`, `DictionaryPage`) uses Qt's default `AutoText`, which switches to rich text when the string looks like HTML.
**Why it matters:** a shared deck can contain `<img src="https://…">` (Qt rich text can fetch images → the sender learns when and from which IP you opened the card), `<font size=7>` / `<h1>` (fake or oversized text, UI spoofing) and links. Links are not clickable (no `onLinkActivated`), which limits the damage. **Not yet confirmed on a device** – test with `02_html_cards.lbox` (section 4).
**Fix (both, defence in depth):**
```cpp
// deckformats.cpp, readLbox()
it.front   = plainText(c.value(QStringLiteral("front")).toString()).replace(u'\n', u' ').trimmed();
it.back    = plainText(c.value(QStringLiteral("back")).toString());
it.example = plainText(c.value(QStringLiteral("example")).toString()).replace(u'\n', u' ');
it.deck    = plainText(c.value(QStringLiteral("deck")).toString());
p.title    = plainText(root.value(QStringLiteral("title")).toString());
```
and `textFormat: Text.PlainText` on every `Label`/`Text` whose text comes from cards, pictures' OCR text, imports or the network (a one-line wrapper component `PlainLabel.qml` makes this easy to keep).
`tests/tst_hostile.cpp` has a `QEXPECT_FAIL` for this; remove it when fixed.

### F-4 `http://` accepted for the Azure Read endpoint (Low) – ✅ fixed
**Where:** `src/ocr/azureread.cpp:15` – an endpoint typed with `http://` is used as is, so the subscription key and the photo would travel in clear text.
**Fix:**
```cpp
if (!base.contains(QLatin1String("://")))
    base.prepend(QLatin1String("https://"));
if (!base.startsWith(QLatin1String("https://")))
    return {};                       // only encrypted endpoints
```
and a test in `tst_azureread` (`http://x` → empty URL). Qt does its own networking on Android, so the manifest's cleartext flag does **not** protect you here; the code is the only guard. (Every other host in the app is hard-coded `https`.)

### F-5 Link import redirects, CSV card limit (Low) – ✅ fixed (private-address filter not added)
**Where:** `src/deckexchange.cpp` `openLink`: `NoLessSafeRedirectPolicy` only blocks https→http; Qt's default allows 50 redirects, to any host (including `https://192.168…`). `src/exchange/deckformats.cpp` `readCsv` has no `kMaxCards` check (a 200 MB file of `a;b;c` lines is ~30 million cards → out-of-memory crash). The download itself is capped (200 MB) and https-only, and there is a test for the plain-http refusal.
**Fix:** `req.setMaximumRedirectsAllowed(5);` and in `readCsv`'s loop `if (p.items.size() >= kMaxCards) break;`. Optional: refuse loopback / private addresses after DNS resolution.

### F-6 FileProvider paths too wide (Low) – ✅ fixed, **test Share on the phone**
**Where:** `android/res/xml/qtprovider_paths.xml` lists `files-path "/"`, `external-path "/"`, … The provider is `exported="false"` and `androidShare()` only hands out the single file in `…/shared/`, so nothing is exploitable today. But if a future bug ever shares a wrong path, `files-path "/"` includes the settings file with the Azure keys and the database.
**Fix:** reduce to what is used, then re-test *Share…*, *Save file…*, camera shots and *Open folder*:
```xml
<paths xmlns:android="http://schemas.android.com/apk/res/android">
    <files-path name="shared" path="shared/"/>
</paths>
```
(Qt's own code may use more entries; if something breaks, add back only that entry.)

### F-7 Crafted `.apkg` freezes the app (Low) – tested, ✅ fixed
**Note:** a legitimate huge deck is still read on the UI thread; moving imports to a worker thread (`QtConcurrent`, like `OcrEngine`) is a separate, larger change that I did not make.
**Where:** `src/exchange/deckformats.cpp` `readApkg`: `SELECT flds FROM notes ORDER BY id` on a database from the file; `DeckExchange::openFile` / the download handler call it **on the UI thread**.
**Experiment (2026-11-09, SQLite 3.37, Python):** `tests/security/hostile/slow/20_view_50M_rows.apkg` is 433 bytes; its `notes` is a *view* over a recursive query with 50 million rows. Both `… ORDER BY id`, `… ORDER BY id LIMIT 100000` and even `… LIMIT 100000` were still running when I stopped them after 8 s – SQLite builds the whole recursive result first, so the 100 000-card limit and `LIMIT` do **not** help. On a phone this is an ANR / crash. Impact is availability only, and only if you import a hostile file.
**Fix:**
```cpp
// before db.open():
db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
// after db.open(), before the SELECT:
QSqlQuery chk(db);
if (!chk.exec(QStringLiteral("SELECT type FROM sqlite_master WHERE name = 'notes'")) || !chk.next()
        || chk.value(0).toString() != QLatin1String("table")) {
    p.error = QStringLiteral("Not an Anki package.");   // then close/removeDatabase as the existing error path does
}
```
Also run imports in `QtConcurrent` (like `OcrEngine`) so no hostile file can freeze the screen.

### F-8 Azure keys in plain text (Info)
`QSettings` keys `ocr/azureKey`, `translation/azureKey` live in the app's private storage (other apps cannot read them without root). Verified: `backup_rules.xml` and `data_extraction_rules.xml` use include-lists with only the database and `images/`, so the keys are not in cloud backups or device transfer; `.lbox` / CSV exports contain cards only; no code logs a key. Android Keystore would need JNI/Java glue (against the no-Java rule) – accept the risk, and use **free-tier keys only** (as the app already advises).

### F-9 Online lookups on by default (Info)
`translation/online` defaults to `true` (`translator.cpp:74`): words you look up or add go to Google's unofficial translate endpoint, MyMemory, Wiktionary and Tatoeba; photos go to Azure only when you switch handwriting recognition on and enter a key. The setup wizard asks about it in Full mode. If you publish the app, say this in the Play *Data safety* form and in the Help text.

### F-10 Release hygiene (Info)
`LearningBox-debug.apk`, `debug_log.txt`, `install_log.txt` sit in the project folder (all git-ignored, none committed – good). A **debuggable** APK lets `adb run-as` read the whole app folder: never hand it to anyone. The package name is `se.morteza.learningbox` (not the Qt default). Keystores (`*.keystore`, `*.jks`) are git-ignored; keep the keystore and its password in a password manager, not in the folder.

### F-11 Manifest notes (Info) – no change
`requestLegacyExternalStorage` is ignored from Android 11, but on **Android 10** the backup folder only works with it – keep it as long as you support Android 10. (Correction to the first version of this document, which said to remove it.) The launcher activity is the only exported component (correct); `allowBackup="true"` is fine given the include-only rules.

---

### F-12 `plainText()` could be made very slow (Low) – ✅ fixed
Found by `tst_hostile` after F-3 put `.lbox` text through `plainText()`: a field of 2 million `&#65;` made the numeric-entity loop quadratic, and 100 000-card decks were slow because the regexes were rebuilt on every call. See the change table in 1a.

---

## 2. Verified OK (evidence)
| Check | Result |
|---|---|
| SQL injection | 61 query sites in `src/`. Every user-supplied value is bound with `?`; every `%n` substitution is an integer id, a box number or a constant column list / fixed `ORDER BY`; `VACUUM INTO` escapes quotes; no `LIKE`. |
| TLS validation | No `ignoreSslErrors`, `VerifyNone`, custom CA or SSL configuration anywhere. |
| Hosts | Google translate, MyMemory, Wiktionary, Tatoeba, Azure Translator: hard-coded `https`. Link import: https only (test `plainHttpLinkIsRefused`). |
| Zip import | Entry count ≤ 100 000, declared unpacked total ≤ 1 GB, `cards.json` ≤ 64 MB, database ≤ 128 MB, picture ≤ 8 MB, cards ≤ 100 000, download ≤ 200 MB. Entry names are only dictionary keys: nothing is extracted by name, so zip-slip does not apply; pictures are re-encoded and stored under generated names. Test `oversizedZipEntryIsRefused` exists. |
| Pictures | Decoded scaled (`setScaledSize`), re-saved as JPEG; HEIC refused with a message. |
| Secrets in repo | No keys, keystores, `.env`, `.sqlite`, APK or log ever committed (history search over all 132 commits; the only hits are the header *name* `Ocp-Apim-Subscription-Key`). Run gitleaks to confirm (section 6). |
| Logging | No `qDebug`/`qWarning` prints a key, token or endpoint. |
| Android | Only the launcher activity is exported; provider not exported; backup rules exclude settings; camera permission asked at runtime; `<queries>` limited to the TTS intent. |
| Share | Exports only the one file being shared into `…/shared/` (older files removed first). |

---

## 3. Threat model in short
Offline-first, no server, no accounts → small attack surface:
1. **Files you import** (`.lbox`, `.apkg`, CSV, links) – hostile files: F-3, F-5, F-7.
2. **Photos and OCR** – decoder bugs in Qt / Tesseract / Leptonica: keep them updated (section 5).
3. **Network** – OpenSSL and TLS use: F-2, F-4.
4. **Other apps on the phone** – the shared backup folder: F-1.
5. **Your own keys and release process** – F-8, F-10.

---

## 4. Test plan on the phone / emulator
Generate the files: `python tests/security/make_hostile_files.py` (add `--big` for the large ones), then
`adb push tests\security\hostile /sdcard/Download/hostile` and import each with *Get cards from a file*.
The `slow/` file is harmless now, but try it on the emulator first anyway.

| File | What it tests | Expected | Result in the sandbox (Qt 6.4, desktop) |
|---|---|---|---|
| `01_zipslip.lbox` | `../` and absolute names for pictures | 2 cards imported; `adb shell ls /sdcard/evil.jpg` → not found; nothing new outside the app folder | ✅ passes |
| `02_html_cards.lbox` | HTML, `<img>` (remote + `file://`), `<a>`, `<font size=7>` in card text | tags shown as text or stripped; no request to `example.invalid` (watch with mitmproxy / logcat) | ✅ tags removed (`tst_hostile`); QML not run |
| `03_lying_size.lbox` | zip says 100 bytes, data is 6 MB | error message, no crash | ✅ passes |
| `04_truncated_json.lbox`, `05_deep_json.lbox` | broken / 100 000-deep JSON | "Not a LearningBox file." | ✅ passes |
| `06_huge_dimension_image.lbox` | picture claims 30 000 × 30 000 px | card imported without picture or an error; memory stays normal | ✅ passes |
| `07_traversal.apkg` | `../../` in Anki media names | card imported; `adb shell ls /sdcard/pwn.jpg` → not found | ✅ passes |
| `08_wide.csv`, `09_long_field.csv` | 200 000 columns, 10 MB field | finishes in seconds, app responsive | ✅ passes |
| `10_html.csv` | tags in CSV | `Haus` shown without tags | ✅ passes |
| `11_not_a_zip.lbox`, `12_empty.apkg` | not a zip / empty file | clear error | ✅ passes |
| `13_entities.csv`, `14_open_tags.csv`, `15_tags_then_gt.csv` | 10 MB of `&#65;`, 5 M `<` without `>`, `>` then 3 M `<a ` | finish in < 1 s | ✅ passes (F-12) |
| `slow/20_view_50M_rows.apkg` | `notes` is a view with 50 M rows | instant "Not an Anki package." | ✅ refused at once |
| `30_100001_cards.lbox` (`--big`) | card limit | exactly 100 000 cards, or a "too large" message | ✅ passes |
| `31_1M_lines.csv` (`--big`) | CSV has no card limit | stops at 100 000 | ✅ capped (`csvIsLimitedToMaxCards`) |

Other manual checks:
- **Release build is not debuggable:** `adb shell run-as se.morteza.learningbox ls` must say *not debuggable*; `apkanalyzer manifest print app-release.apk` must not contain `debuggable="true"`.
- **Permissions:** `adb shell dumpsys package se.morteza.learningbox | findstr /i "permission"` – after F-1 only `CAMERA` + `INTERNET`.
- **Proxy test:** run the emulator through mitmproxy **without** installing its certificate; every online feature (translate, Tatoeba, link import) must fail with a connection error. If it works, TLS validation is broken.
- **Keys:** after entering an Azure key, `adb bugreport` / `logcat` must not contain it (search for the first 6 characters).
- **Backup rules:** `adb shell bmgr backupnow se.morteza.learningbox` on an emulator with a backup transport, or `adb backup` on an old API level – the settings file must not be in the archive.

---

## 5. Dependencies and versions
| Component | Version in the project | Status / what to do |
|---|---|---|
| OpenSSL (KDAB `android_openssl`, `ssl_3`) | 3.1.8 (Feb 2025) | **End of life** → F-2 (KDAB's repository still has 3.1.8; use `3rdparty/build_openssl_android.sh`, 3.5.9) |
| Qt | 6.11.x | Keep on the latest patch release; read the Qt security advisories (qt.io/blog, "Security Advisory") after every update. |
| Tesseract | 5.5.2 | Not checked against advisories in this audit → github.com/tesseract-ocr/tesseract/security |
| Leptonica | 1.87.0 | Not checked → github.com/DanBloomberg/leptonica/security |
| cpu_features | v0.11.0 | Low risk (CPU detection only). |
| `deu.traineddata` | tessdata_fast 4.1.0 | Data file, no code. |
| androidx.core | 1.17.0 | Dependabot (`.github/dependabot.yml`) proposes updates. |
| Android Gradle plugin / Kotlin / NDK | AGP 9.0.0, Kotlin 2.3.0, NDK r27c | Build tools; update when Qt requires. |

Re-check this table every 6 months and after every Qt update.

---

## 6. How to run the tools (Windows PC)
| Tool | Command | What it finds |
|---|---|---|
| **gitleaks** | `winget install Gitleaks.Gitleaks`, then in the project folder `gitleaks detect --source . -v --redact` | keys / tokens anywhere in the git history |
| **cppcheck** | `cppcheck --enable=warning,portability --inline-suppr -I src src main.cpp` | memory / UB mistakes |
| **clang-tidy** | configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then `clang-tidy -p build -checks="bugprone-*,clang-analyzer-security.*,cert-*" src/exchange/deckformats.cpp src/deckexchange.cpp src/backup.cpp` | bug-prone patterns |
| **ASan + UBSan** | easiest in GitHub Actions (job `sanitizers` in `.github/workflows/security.yml`); locally in WSL / Linux: `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"` | memory errors in the unit tests |
| **CodeQL** | job `codeql` in the same workflow (free for public repos) | data-flow security bugs |
| **MobSF** | `docker run -it --rm -p 8000:8000 opensecurity/mobile-security-framework-mobsf:latest`, open `localhost:8000`, upload the **release** APK | manifest, permissions, native-library hardening, hard-coded secrets |
| **apkanalyzer / aapt2** | `apkanalyzer manifest print app-release.apk` | debuggable, backup, permissions |
| **mitmproxy** | `mitmproxy --listen-port 8080`, emulator `-http-proxy 10.0.2.2:8080` | what the app sends, TLS validation (section 4) |
| **Fuzzing** | build with clang (below), then `fuzz_deckformats corpus -max_len=1048576 -timeout=10 -rss_limit_mb=2048` | crashes / hangs in the import parsers |

**CMake block for `tst_hostile`** (already in `CMakeLists.txt` next to `tst_deckformats`; shown for reference):
```cmake
qt_add_executable(tst_hostile tests/tst_hostile.cpp src/exchange/deckformats.cpp)
target_include_directories(tst_hostile PRIVATE src)
target_compile_definitions(tst_hostile PRIVATE LB_HOSTILE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/security/hostile")
target_link_libraries(tst_hostile PRIVATE Qt6::Test Qt6::Gui Qt6::Sql Qt6::CorePrivate Qt6::GuiPrivate)
set_target_properties(tst_hostile PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME tst_hostile COMMAND tst_hostile)
```
**CMake block for the fuzzer** (clang only; run `python tests/security/make_hostile_files.py --corpus corpus` first):
```cmake
option(LB_FUZZ "Build libFuzzer targets (clang)" OFF)
if(LB_FUZZ)
    qt_add_executable(fuzz_deckformats tests/fuzz/fuzz_deckformats.cpp src/exchange/deckformats.cpp)
    target_include_directories(fuzz_deckformats PRIVATE src)
    target_compile_options(fuzz_deckformats PRIVATE -fsanitize=fuzzer-no-link,address,undefined -g)
    target_link_options(fuzz_deckformats PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_libraries(fuzz_deckformats PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::CorePrivate Qt6::GuiPrivate)
endif()
```
**Status of the prepared files:** `tst_hostile` and `tst_backup` are registered in `CMakeLists.txt` and pass (section 1a). `fuzz_deckformats.cpp` (needs clang), `security.yml` and `openssl-android.yml` were **not run** – expect small fixes on the first run (Qt version, package names). `tst_hostile` needs the files from `make_hostile_files.py` first (the CI job does this).

---

## 7. Pre-release checklist
- [ ] All findings above either fixed (✅) or consciously accepted (write why) – open today: F-2 (run the OpenSSL build), F-1 (decide about all-files access)
- [ ] `gitleaks detect` clean; no keystore, `.env`, log or APK in `git status --ignored` that you did not expect
- [ ] OpenSSL / Qt / Tesseract / Leptonica versions re-checked (section 5)
- [ ] Release build signed with your own keystore, **not debuggable**, version code raised
- [ ] Manifest: only needed permissions; decide about `MANAGE_EXTERNAL_STORAGE` (F-1)
- [ ] `tst_hostile` and all other unit tests pass with ASan + UBSan (CI)
- [ ] Hostile files from section 4 imported on a real phone: no crash, no freeze
- [ ] Proxy test: online features fail against an untrusted certificate
- [ ] MobSF report on the release APK reviewed
- [ ] Data-safety / privacy text matches F-9 (what leaves the phone, and when)
- [ ] Windows / iOS (phase 2): installer and app signing, iOS App Transport Security, no secrets in the installer

---

## 8. Sources
- OpenSSL 3.1 end of life: https://endoflife.ai/openssl/3.1 · https://versionlog.com/openssl/3.1/
- OpenSSL release timeline (3.0 LTS ended 2026-09-07, LTS lines 3.0 and 3.5): https://hidekazu-konishi.com/entry/openssl_release_and_support_timeline.html · https://endoflife.ai/article-openssl-eol
- Checklists: OWASP MASVS / MASTG (mas.owasp.org)

## 9. Update log
| Date | Change |
|---|---|
| 2026-11-09 | First audit (code at `14e9072`): findings F-1 … F-11, hostile-file generator, `tst_hostile`, fuzz harness, CI workflow, Dependabot config |
| 2026-11-09 | Fixes F-1 (restore validation), F-3, F-4, F-5, F-6, F-7, F-12; F-11 corrected (keep the legacy-storage flag); `tst_backup`; OpenSSL rebuild script + Action prepared (F-2 still open) |
