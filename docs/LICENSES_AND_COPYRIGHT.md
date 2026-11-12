# LearningBox – Licences and copyright check before Google Play

Living document. Audit: **2026-11-09**, code at `14e9072`. Method: looked at every file under `assets/`, `data/`, `3rdparty/`,
the Qt modules in `CMakeLists.txt`, the QML imports, the Gradle dependencies, the online services in `src/`, and checked
the published terms of the services (sources in section 8).
**I am not a lawyer.** This is a technical inventory with risks; for anything marked 🔴 get the permission in writing or remove the item.

Result in one line: **the code libraries are fine to ship; the open problems are images whose origin is unknown (🔴),
the unofficial Google Translate endpoint (🔴), missing attributions (🟠) and a few names that belong to other companies (🟠).**
There is **no LICENSE file and no "open-source licences" screen in the app yet** – both are needed (section 6).

---

## 1. Summary

| ID | Risk | Item | What to do |
|---|---|---|---|
| L-1 | 🔴 | `assets/icons/app_icon.png` = the Standard of Cyrus the Great, 554×554 px, looks like a web image; the launcher icons in `android/res/mipmap-*` come from it | Know the source and its licence, or redraw it yourself (a simple vector redraw of an ancient design is fine) |
| L-2 | 🔴 | `assets/home_bg.png` = photo of a Persepolis relief (600×900, stock-site preview size) | Use your own photo, or a photo whose licence allows commercial use (Wikimedia Commons CC0 / CC BY with credit) |
| L-3 | 🔴 | `assets/maziar.jpg` (shown in Settings) = a signboard **mock-up** "designed by Maziar" with stock plants and an office render | Written permission from Maziar for the logo; do not ship the mock-up (stock content you cannot clear) – show only the logo |
| L-4 | 🔴 | `translate.googleapis.com/translate_a/single?client=gtx` (`src/translation/googletranslate.cpp`) is Google's **undocumented web endpoint**, not the Cloud Translation API | Remove it, or replace it with the official Cloud Translation API using the user's own key (like Azure). Do not ship it on by default |
| L-5 | 🟠 | Tatoeba sentences are shown to users: **CC BY 2.0 FR requires attribution** | Add the credit (text in section 6) in the app and on the Play page |
| L-6 | 🟠 | Wiktionary data (article, plural, forms) is stored and shown: **CC BY-SA 4.0** | Add the credit; do not copy whole entries; keep it as grammar facts |
| L-7 | 🟠 | Icons `lens.png`, `dictionary.png`, `back.png` (and probably `favorites/export/import`) are third-party clip-art styles; the red camera is the "Flaticon / Freepik" style, which needs a credit on the free licence | Know the source of each. Replace with Material Icons (Apache-2.0) or buy/credit properly |
| L-8 | 🟠 | Qt LGPL obligations | Section 3: ship the licence text, say you use Qt, offer the Qt source, keep dynamic linking |
| L-9 | 🟠 | Other companies' names inside the app: "Starten wir!" (Hueber), "Netzwerk neu" (Klett), "Goethe-Zertifikat A1" | Rename the packs to neutral topic names; do not use publisher or exam names as titles |
| L-10 | 🟠 | `deutsch_a1_complete.tsv` follows the **entire** Goethe A1 word list (703 lines, "every entry of the Goethe A1 word list") | Do not claim "complete Goethe list"; either drop the pack, or make it your own selection and do not name Goethe |
| L-11 | 🟡 | MyMemory: free, anonymous 5 000 characters/day per IP, 500 bytes per request; terms forbid circumventing limits | Fine as a fallback; credit "MyMemory"; handle the limit message |
| L-12 | 🟡 | No `LICENSE` for your own code and content | Decide: "All rights reserved" or an open licence (section 5) |
| L-13 | 🟡 | `data/tessdata/` has `eng` and `swe` besides `deu`, but `3rdparty/README.md` lists only `deu` | Update the README; all three are Apache-2.0 (tessdata_fast) |
| L-14 | 🟡 | Name "Learning Box" is generic; there may be other apps/marks with it | Search Google Play, EUIPO and PRV (Sweden) before launch |
| L-15 | 🟡 | `assets/home_bg.jpg` (cyber-cloud stock illustration) is in the repo but not used or packaged | Delete it (it looks like a stock vector) |

---

## 2. Third-party code and data

| Component | Version | Licence | Ships in the APK? | Obligation |
|---|---|---|---|---|
| Qt (Quick, QuickControls2, Sql, TextToSpeech, Multimedia, Concurrent, Network, Sensors, Effects, Dialogs) | 6.11 | LGPL-3.0 (open-source edition) | yes, as shared `.so` | Section 3 |
| Tesseract OCR | 5.5.2 | Apache-2.0 | yes | licence text + NOTICE |
| Leptonica | 1.87.0 | BSD-2-Clause | yes | copyright notice + licence text |
| cpu_features | 0.11.0 | Apache-2.0 | yes | licence text |
| OpenSSL (KDAB `android_openssl`) | 3.1.8 → 3.5.9 | Apache-2.0 | yes | licence text (and update, see SECURITY.md F-2) |
| tessdata_fast `deu`, `eng`, `swe` | 4.1.0 | Apache-2.0 | yes | licence text (`data/tessdata/LICENSE` exists) |
| Playfair Display font | – | SIL OFL 1.1 | yes | keep `OFL.txt`; do not sell the font alone; do not rename a modified copy "Playfair Display" |
| androidx.core | 1.17.0 | Apache-2.0 | yes | licence text |
| Android TTS (system engine) | – | system component | no (app calls the phone's engine) | none |

No GPL-only Qt module is used (no Charts, Virtual Keyboard, Quick 3D, etc. – checked in `CMakeLists.txt` and all `import` lines).
Test code (`Qt6::Test`) is not shipped.

## 3. Qt (LGPL-3.0) on Google Play – what you must do
The LGPL lets you keep your own source closed **if** the app only *uses* Qt as shared libraries. Qt's own page says dynamic linking is the normal way to do this (sources in section 8). Your project already builds Qt as shared libraries for Android.
- [ ] **Keep dynamic linking.** Do not switch Qt to a static build for Android.
- [ ] Do not modify Qt. If you ever patch Qt, you must publish the patched Qt source.
- [ ] Do not use Qt's GPL-only modules.
- [ ] Show in the app (Settings → About → Licences) that the app uses Qt under the **LGPL-3.0**, with the licence text and a link to the Qt source: `https://download.qt.io/` / `https://www.qt.io/download-open-source`.
- [ ] The user must be able to replace the Qt libraries. On Android the Qt `.so` files are inside the APK; Qt's forum answers say you must give the user a way to rebuild/relink. The simplest way is to publish your **build instructions and the object files or the full source of the app** on request, or to offer the app source under a licence you choose (e.g. GPL-3.0, or "source available on request"). **Ask the Qt Company if you want certainty**; many developers instead buy a commercial Qt licence for closed-source mobile apps.
- [ ] Multimedia (Qt 6.11 uses an FFmpeg backend) also brings FFmpeg libraries, which are LGPL as well – list FFmpeg in the notices. If you do not need playback beyond the camera, check whether Qt Multimedia is really required.

Decision for you: **(a)** release the app source (GPL/LGPL-compatible), **(b)** keep it closed and follow the LGPL list above, or **(c)** buy a commercial Qt licence. (b) is common but is the one where a lawyer's opinion helps.

## 4. Online services (terms of the services your app calls)

| Service | Used for | Licence / terms | Status |
|---|---|---|---|
| Google `translate.googleapis.com/translate_a/single?client=gtx` | translations (fallback, **on by default**) | Not a public API. Google says it may suspend use on any suspected violation of its terms; the official route is Cloud Translation (paid, API key) | 🔴 L-4 |
| MyMemory `api.mymemory.translated.net` | translation fallback | Free; anonymous limit 5 000 chars/day by IP; a request is max 500 bytes; terms forbid circumventing limits and reselling | 🟡 credit it |
| Azure Translator / Azure AI Vision | optional, user's own key | Microsoft's terms apply to the key owner (free tier 2 M characters/month) | ✅ fine, user's own account |
| Tatoeba `api.tatoeba.org` | example sentences | Text: **CC BY 2.0 FR** – you must name Tatoeba and link `https://tatoeba.org`, and say the data is CC-BY 2.0 FR | 🟠 L-5 |
| Wiktionary `*.wiktionary.org` | article, plural, forms | **CC BY-SA 4.0** (+ GFDL) – credit + share-alike for adapted text | 🟠 L-6 |
| Google Drive / GitHub raw links (import) | user-supplied links | the user's own responsibility | ✅ |

Notes:
- Words typed or photographed by the user are sent to these services. This must be in the **privacy policy** and the Play **Data safety** form (see section 7).
- Do not store or sell bulk copies of Tatoeba or Wiktionary data; your cache keeps only what the user looked up, which is the intended use.
- Machine translations (Azure/Google/MyMemory) are results for the user's own study; do not publish them as a "dictionary" of yours.

## 5. Your own code and content
- **Word packs** (`data/levelpacks/*.tsv`, `data/wordpacks`, `data/starter`) state that headwords, meanings and examples are **original**. That is the right position; keep the sentence in each file. Write down who produced the English and Persian translations (you, a person, or an AI tool) – you need to be sure you may license them. Course-book **chapter titles** (e.g. "Super! · Begrüßen und sich vorstellen") may be copied from a table of contents – rename them to your own topic titles.
- Do **not** ship text copied from: the Goethe-Institut word list examples, textbooks, telc/ÖSD lists, Duden, DWDS, or Anki shared decks.
- `LearningBox_German_Job_Vocabulary.lbox` in the project root: if it is shared, check that it only holds your original content.
- Add a root **`LICENSE`** (or a "© 2026 Morteza Pourmehdi. All rights reserved." notice) and say what applies to the word packs.

## 6. What to add to the app and repo

**1) `NOTICE` / in-app "Open-source licences" screen** (Settings → About) with this text and the full licence texts:
```
LearningBox uses these open-source components:
- Qt 6 (LGPL-3.0) – https://www.qt.io – source: https://download.qt.io
  Qt is dynamically linked; you may replace the Qt libraries.
- Qt Multimedia FFmpeg libraries (LGPL-2.1+) – https://ffmpeg.org
- Tesseract OCR 5.5.2 (Apache-2.0) – https://github.com/tesseract-ocr/tesseract
- Leptonica 1.87.0 (BSD-2-Clause) – https://github.com/DanBloomberg/leptonica
- cpu_features (Apache-2.0) – https://github.com/google/cpu_features
- tessdata_fast language data: deu, eng, swe (Apache-2.0)
- OpenSSL (Apache-2.0) – https://www.openssl.org ; Android build: KDAB android_openssl
- Playfair Display font, © The Playfair Display Project Authors (SIL OFL 1.1)
- androidx.core (Apache-2.0)

Example sentences come from Tatoeba (https://tatoeba.org), licensed CC BY 2.0 FR.
Word forms (articles, plurals) come from Wiktionary (https://www.wiktionary.org), licensed CC BY-SA 4.0.
Translations may come from MyMemory (https://mymemory.translated.net) and Microsoft Azure Translator (your own key).
Icons: <fill in after L-7>.
```
**2)** Copy the licence texts of Apache-2.0, BSD-2-Clause, LGPL-3.0 and OFL into `licenses/` and show them in the screen.
**3)** In the **help** (3 languages) and the Play description: one line that online lookups send the word to third parties.
**4)** `3rdparty/README.md`: add `eng` and `swe` traineddata (L-13) and the Qt/FFmpeg row.

## 7. Play Store checklist (policy items that often block a first release)
- [ ] **Privacy policy URL** (required because the app uses the camera and the network, and sends text to third parties). Say: what is sent (words/sentences, photos only to Azure with the user's key), to whom, nothing is collected by you, no account.
- [ ] **Data safety form** matches the policy (data shared: text for translation; camera images processed on device, or sent to Azure only if the user turns it on).
- [ ] **Permissions:** `MANAGE_EXTERNAL_STORAGE` ("All files access") needs a Play *declaration form* and is usually refused for a flashcard app – see SECURITY.md F-1. Without it the reinstall backup folder stops working. Decide before submitting.
- [ ] **Intellectual-property policy:** no third-party names/logos as your own (L-9), no copied content (L-10), no unlicensed images (L-1…L-3, L-7).
- [ ] App name "Learning Box" and the Maziar logo: confirm you may use both (L-3, L-14).
- [ ] Content rating questionnaire (all ages; user-imported decks can contain any text – say user-generated content is local only, not shared).
- [ ] Target API level, 64-bit, 16 KB page-size and signed AAB: check Play Console's current requirements at submission time (they change yearly); build an **AAB**, not the debug APK.
- [ ] Play App Signing and your own upload keystore (see SECURITY.md F-10).

## 8. Sources
- Tatoeba – terms for re-users (CC BY 2.0 FR, attribution with a link to tatoeba.org): https://en.wiki.tatoeba.org/articles/show/faq
- Qt – obligations of the (L)GPL: https://www.qt.io/development/open-source-lgpl-obligations · FAQ 3.7: https://www.qt.io/faq/3.7.-what-are-my-obligations-when-using-qt-under-the-lgpl · Qt Wiki on mobile licensing: https://wiki.qt.io/Licensing-talk-about-mobile-platforms · Qt forum on LGPL on Android: https://forum.qt.io/topic/100839/how-to-comply-with-lgpl-on-android
- Google – undocumented `translate_a` endpoint and the Cloud Translation terms: https://discuss.google.dev/t/translate-googleapis-com-translate-a/126639
- MyMemory – terms: https://mymemory.translated.net/terms-and-conditions · API spec: https://mymemory.translated.net/doc/spec.php
- Wiktionary / Wikimedia re-use (CC BY-SA 4.0): https://foundation.wikimedia.org/wiki/Policy:Terms_of_Use (not re-checked in this audit)
- Apache-2.0, BSD-2-Clause, SIL OFL 1.1: the licence texts in the component repositories (not re-checked online in this audit)

## 9. Update log
| Date | Change |
|---|---|
| 2026-11-09 | First licence / copyright audit; findings L-1 … L-15 |
