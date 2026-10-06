# LearningBox

A Leitner box for German vocabulary. Qt 6.10+ / C++20 / QML, with no Java. Android is the first target.

## Milestone 1 (done)
- SQLite card store (`AppDataLocation/learningbox.sqlite`), schema versioned via `PRAGMA user_version` (v8)
- **First-run setup** (`qml/SetupPage.qml`, once, before Home): meaning language (English / فارسی) → **Simple or Full
  app** (comparison table `qml/ModeComparison.qml`) → **100 starter words** into the learning box
  *Starter – 100 words* → German voice check (+ online translation switch in Full). APKs have no installer wizard,
  so this is where the "installation" questions are asked. Installs that already have cards skip it (Full mode).
- **Simple / Full app** (`src/appmode.*`, QML singleton `AppMode`, QSettings `app/mode`, `setup/done`): Simple =
  Leitner flashcards only, offline (`Translator::useOnline()` is false), no Lens / word pack A1 / pictures /
  suggestions / links / Anki import. Settings → *App* switches any time; cards and progress are kept; Home shows a
  *More features* card in Simple mode.
- **100 starter words** (`data/starter/starter_100.tsv`, parser `src/starterdeck.*`, `tst_starterdeck`): common A1
  words — greetings, numbers, family, food, home, time, verbs, adjectives, city — with article + plural, English and
  Persian meaning and an example sentence in all three languages. `WordPacks::addStarterCards()` adds them to the
  selected learning box (duplicates skipped) with the meaning in the chosen language and saves both languages plus the
  example translations in the translation cache, so the fa / en switch in review also works offline.
  Also offered in *+ New learning box* and on the Word packs page.
- **Start over** (`qml/ResetDialog.qml`): Home → ⋮ → *Start over (all cards to Box 1)*, or *Move all … to Box 1* on a
  box page. Options: include learned cards, all today or spread over N days, clear statistics; *Undo* for 10 s.
  `CardStore::resetCollection / resetBox / resetCount / undoReset`; the previous state is kept in the table
  `reset_undo` (schema v5, emptied at the next start).
- **Learning boxes** (schema v4): several independent collections, e.g. one per textbook (*Netzwerk neu A2*).
  Home has a ComboBox of learning boxes ordered by last use; closed, it already shows the first two (the selected
  one in bold, and the previous one — tap to switch); the arrow opens the full list.
  *+* creates one (empty, or started with a word pack), ⋮ *Rename / Delete*. The selection is remembered
  (`last_used_at` + QSettings), so it is first again after a restart. Box tiles 1–5 + Learned below are unchanged. Review, Lens, Add, All cards, word packs and export all work on the selected box
  (`CardStore` scopes every query by `collection_id`). Existing cards are migrated into *My learning box*.
- 5 Leitner boxes: intervals 1/2/4/8/16 days (day-aligned); wrong → box 1; correct in box 5 → *Learned*
- Review session: failed cards are re-asked at the end of the session (the DB is updated only once)
- **Favorite words** (schema v8: `cards.favorite` = time starred, 0 = not): ★ (`qml/StarButton.qml`, drawn
  `qml/StarIcon.qml`) on the review card (top row), in box pages, in the card editor (existing cards) and in Lens
  (selected words; words that are no cards yet are added first, like *Add N words*; all starred → unstar).
  Home → *★ Favorite words (N)* opens `qml/FavoritesPage.qml` (whole cards like the box pages via `qml/CardFace.qml`: meaning, example, 🔊; newest star first; tap = edit, ★ = remove from the list).
  Per learning box. A star emits only `favoritesChanged` (not `changed`), so lists are not rebuilt (no flicker); on
  Favorite words an unstarred card stays (empty ★, tap to undo) until the page is shown again.
  `CardStore::favoriteCount / isFavorite / setFavorite / favorites`; `card()` and `cardsInBox()` carry
  `favorite`. Not in `.lbox` exports yet.
- **Select several cards** on a box page: press and hold a card or *Select* → check boxes; the bar has select-all
  (tri-state), count, ★ (star all), *Delete* (confirmation, `CardStore::removeCards`, one transaction, pictures removed)
  and *Done*. Tap = toggle while selecting, otherwise edit. The Learned tab shows 🎓 (★ means favorite now).
- **Box pages**: tap a box on Home to list its cards — the whole card: front, every line of the meaning and the
  example, each with 🔊 (meaning only when English / German), plus ★ (tabs switch between boxes 1–5 and Learned). ◀ / ▶ move a card
  one box back / forward; it is then scheduled with the new box's interval (box 1 = tomorrow … box 5 = 16 days,
  Learned = not reviewed), review history is kept (`leitner::moveTo`, `CardStore::moveCard`). *Undo* for 4 s.
- **Learning English** (schema v6: `collections.language` "de" / "en"): each learning box learns German or English,
  chosen in *+ New learning box* (and in the first-run setup); a flag shows it in the chooser. For the selected box
  `CardStore::learningLanguage` drives: speech (`Speaker`: German, or **American English only** — no other English
  voice is used; `voiceAvailable` / `voiceName` / `hasVoice`), Lens OCR (`eng.traineddata` next to `deu`), online
  translation and the translation cache (`Translator::sourceLanguage`; English keys are stored as `en|…`), Tatoeba
  examples (`lang=eng`, "to/the/a/an" dropped). German word packs / starter words / der-die-das hints are not used there.
- **Meaning language per learning box** (schema v7: `collections.meaning`): Persian, English or German — never the
  language being learned (German boxes: fa / en, English boxes: fa / de). Chosen in *+ New learning box*, the setup,
  Settings → Translation or the switch on the answer side (`CardStore::meaningLanguage`, `Translator::targetLanguage`
  writes it; the last German-box choice is the default for new German boxes). English and German meanings, example
  translations and Lens translations have 🔊 (American / German voice); Persian is not read aloud.
  `.lbox` files carry `"language"`, so an imported English file becomes an English box.
- **Dictionary** (`qml/DictionaryPage.qml`, last button on Home): looks a word or sentence up between the learning
  language and a second language — *With: فارسی / English / Deutsch* (never the learning language; default the box's
  meaning language, the choice is remembered per learning language, QSettings `dictionary/`), both directions (drawn swap button);
  word pack first (German → Persian, offline), then `Translator::translateBetween` (online, saved for offline;
  cache keys get a source prefix except German), alternatives, Tatoeba examples, 🔊 for German / English,
  *+ Add* opens the card editor prefilled (with another language than the box's meaning, the editor fills in the
  box's meaning). Example translations follow the chosen language (`ExampleText.target`).
  For German nouns the result also shows the **gender** (maskulin / feminin / neutral) and the **plural** ("maskulin · Pl. die Hunde"), looked up online in the German Wiktionary (`src/translation/wiktionary.*`, `Translator::lookupGrammar`, saved in the translation cache for offline use, `tst_wiktionary`); *+ Add* puts that line on the card's back and the article on its front.
- **Review direction** (*Ask:* on the review card): *German first* (the meaning is the answer), *Meaning first* (the
  meaning's first line is the question — the plural line would give the word away; say the German word, it is read
  aloud with the answer) or *Mixed* (random per card). Stored per learning box (`CardStore::reviewDirection`, QSettings
  `review/direction/<id>`); a card without any meaning is always asked German first. Same boxes and progress.
- **Edit while reviewing**: *✎ Edit* on the review card opens the editor; back in the review the card is reloaded
  (`ReviewSession::reloadCurrent`), a deleted card is skipped.
- **Box by hand**: the box chooser on the review card moves the card to any box (scheduled with that box's
  interval, `ReviewSession::moveCurrent`) and shows the next card; the editor has the same chooser for existing cards.
- Add / edit / delete / reset cards, search. Saving a word that is already in the box asks: *Update existing card*
  (keeps its box and progress; empty fields keep the old value), *Open existing card* or *Cancel*. Lens *Add N words*
  asks the same for words already in the box (update their meaning / add only new words)
- German TTS via `QTextToSpeech` (system engine on Android), with auto-speak
- Persian: free-text back side, per-paragraph bidi (RTL renders correctly)

## Help (? icon)
The **?** icon next to the gear (every page) opens *Help*: how to use the app — Leitner idea, learning boxes, review,
boxes, adding cards, Lens, word packs, All cards, import & export, settings, tips — in **English** or **فارسی**
(switch at the top; starts in Persian when Persian is the translation language). Text lives in `qml/HelpPage.qml`
(`enSections` / `faSections`); update both when a feature changes. German is planned.

## Help while adding a card
- **Word suggestions** while typing (≥ 2 letters): matching word-pack words with their article ("woh" → woher,
  wohnen, die Wohnung …). Tap one: word, meaning (Persian) and example are filled in.
- **Meaning filled in automatically** in the translation language (Settings / review switch): word pack (Persian),
  else a saved translation, else online (Google endpoint) with up to two alternatives. It stops as soon as you type your
  own meaning; clearing the field turns it back on.
- **Example suggestions**: word-pack examples containing the word (offline, with Persian translation) plus real sentences
  from **Tatoeba** when online (`api.tatoeba.org/v1/sentences`, shortest first, with a Persian/English translation where
  Tatoeba has one; those translations are saved). Tap one to use it. Code: `src/translation/tatoeba.*` (unit-tested,
  `tst_tatoeba`), `Translator::suggestExamples`, `WordPacks::suggest / examplesContaining`.
  Tatoeba sentences are licensed CC BY 2.0 FR (tatoeba.org).
- The keyboard's own word predictions are allowed on the German field.

## Send and get cards (Home → *⬆ Send or back up cards* / *⬇ Get cards from a file or link*)
Plain words and a grey hint under every choice (`qml/HintLabel.qml`).
- **Send** (`qml/SendCardsPage.qml`): 1. *Who is it for?* — another LearningBox app (.lbox, keeps pictures) or
  Excel / Anki / Quizlet (.csv); 2. *Which cards?* — all or one box, with the count; 3. *Keep my progress?* (.lbox).
  **Share…** exports into the app folder and opens the Android share sheet (WhatsApp, Telegram, e-mail, Drive …;
  `DeckExchange::shareCards`, JNI `Intent.ACTION_SEND` through the manifest's FileProvider, C++ only; on desktop the
  folder is opened). **Save file…** uses the system save dialog. File names: `learningbox-<learning box>-<date>.lbox`.
  A one-time tip explains how to give cards to a friend.
- **Get** (`qml/GetCardsPage.qml`): *Choose a file* or (Full app) *paste a link*; the preview shows title, card and
  picture count and the first cards; options with hints: German in the second column, **new learning box** (default
  when the file has a title) or this one, keep mine / use the file's meaning, keep the file's progress or start box.
  The button says what happens (*Add 110 cards*, *Add 110 · update 14*); afterwards *Open "…"*.
- **Import** from a file on the phone or a **link** (Google Drive / Dropbox / GitHub share links are turned into
  direct downloads): `.lbox`, Anki `.apkg`, CSV/TSV/text (Quizlet, Excel, Anki text export). A preview shows how many
  cards are new / already in the box and the first cards; options: swap sides, skip or update existing words, keep the
  file's progress or put new cards into a chosen box.
- **`.lbox` format** (zip): `cards.json` = `{"format":"learningbox","version":1,"title","exported","withProgress",
  "cards":[{"front","back","example","deck","image":"images/0.jpg","box","due" (ISO date or null),"reviews","lapses"}]}`
  plus the pictures. Progress fields are only present with *withProgress*.
- **CSV**: `German,Meaning,Example` (UTF-8 with BOM, quoted fields). Import auto-detects comma / semicolon / tab and
  skips a header row, Anki `#separator:` lines and HTML.
- **Anki `.apkg`**: first three fields of each note (word, meaning, example) and the first picture; Anki progress is not
  used (different algorithm). Decks in Anki's newest format (`collection.anki21b`, zstd) must be exported with
  *Support older Anki versions*; the app says so.
- Code: `src/exchange/deckformats.*` (formats, pure, `tst_deckformats`), `src/deckexchange.*` (QML `DeckExchange`,
  `tst_deckexchange`: export → delete → import round trips), `CardStore::insertCards / storeImageData`,
  `qml/SendCardsPage.qml`, `qml/GetCardsPage.qml`. Zip via Qt's private `QZipReader/QZipWriter` (Qt6::CorePrivate/GuiPrivate).

## Word packs
- `data/wordpacks/netzwerk_neu_a1.tsv`: 597 A1 words in 12 chapters, following the chapter **themes** of *Netzwerk neu A1*
  (Kursbuch/Übungsbuch). The words, Persian meanings and example sentences are original, not copied from the book.
  Format: `chapter<TAB>German<TAB>Persian meaning · grammar note<TAB>example<TAB>example in Persian`, plus `@pack` /
  `@chapter` lines (the 5th field is optional for the parser; the shipped pack has it for all 597 examples).
- **Example translations**: with Persian selected in Settings, every example sentence shows its Persian translation
  underneath (`ExampleText.qml`: review answer, pack preview, card editor). Source: the pack's own translation (also for
  cards already in the box, looked up by the example text), else a saved translation, else online (debounced, saved).
- In the app: Home → ⋮ (learning box menu) → *Word packs* → add a chapter (skips words already in the box, case-insensitive) or preview it.
  Imported cards get `deck = "Netzwerk neu A1 · K<n>"` (schema v2), so searching "K3" in *All cards* finds a chapter.
- The meaning is stored as two lines, Persian meaning and then the German grammar note, and shown with `MeaningText.qml`,
  so each line gets its own direction (RTL/LTR).

## Pictures on cards
- Editor: *Add picture* opens the system picker (Android photo/file picker via Qt's `FileDialog`, no permission
- Editor: *📷 Take photo* opens the camera (`CameraShotPage.qml`); the shot is stored like a picked picture
  (`CardStore::cameraFilePath` / `importCameraShot`), turned upright for landscape shots (phone tilt).
  and no Java needed). The picture is copied into `AppDataLocation/images/<uuid>.jpg`, EXIF-rotated and scaled to
  max 1024 px (JPEG q85), so the original can be deleted from the gallery.
- Shown on the answer side in review, as a thumbnail in *All cards*, and in the editor (Change / Remove).
- Files are deleted with their card or when replaced; pictures picked but never saved are removed on the next start.
- Schema v3 adds `cards.image` (file name only; paths are rejected).
- HEIC photos aren't decoded by Qt's default image plugins; the editor then asks for a JPG/PNG.

## Lens (camera → words → cards)
- Home → **Lens**: live camera (Qt Multimedia, no Java) with a shutter and a gallery button.
- The photo is EXIF-rotated, scaled to max 2400 px and recognized **offline** by Tesseract 5.5 with the German
  `tessdata_fast` model, on a worker thread (`OcrEngine`, QtConcurrent). Typical page: well under a second on a desktop.
  Two passes always run in parallel: page layout (books, worksheets) and **sparse text** (text anywhere, e.g.
  subtitles/captions inside a picture, which page layout drops as "image" even when it finds other text). Sparse
  fragments are re-joined into visual rows, left to right (`ocr::joinLineFragments`). If there is little confident
  text, two cleaned-up versions also run (photos of screens with moiré, glossy or unevenly lit pages: Leptonica box
  blur + background/contrast normalisation, light and strong). The best pass is the base; lines only the others found
  are merged in by position (`ocr::mergeResults`, unit-tested in `tst_ocrmerge`).
- **Landscape photos (like Google Lens)**: `DeviceTilt` reads the accelerometer (Qt Sensors, works with rotation lock)
  while the camera is open; at the shutter the phone's tilt (upright / top left / top right / upside down, minus the UI's
  own rotation) is passed as a hint and the photo is turned upright *before* OCR - no guessing, no extra passes. The
  viewfinder shows "↻ Landscape" turned the way the phone is held. Held flat over a page, the last clear tilt is kept.
  Fallback (gallery pictures, no sensor, or little text with the hinted turn): a cheap probe (sparse pass on a half-size copy, 0°/90°/270° in parallel) picks a
  clearly better turn, the photo is read turned, and Lens shows it turned (`OcrResult::rotation`) (`ocr::cleanForOcr`, `ocr::qualityScore`; covered by
  `tst_ocr::recognizesScreenPhotoWithMoire`, where plain Tesseract reads nothing, and `recognizesCaptionInsidePicture`). Short, unsure fragments are dropped.
- Recognized words are highlighted. **Tap** words to select them, **press and hold** a word to select its sentence,
  and **pinch** to zoom. The selection bar shows the text (hyphenated line breaks "Woh-/nung" are rejoined, trailing
  commas are dropped) with a 🔊 button.
- **Create card**: opens the editor pre-filled. A single word found in the A1 word pack gets its article,
  Persian meaning and example ("Brot" → "das Brot · نان"). **Add N words**: adds each selected word as its own card
  (deck "Lens"), filled from the word pack where possible, skipping words already in the box.
- **Online recognition (handwriting)**, Settings ⚙ → *Text recognition*: with the switch on and your own **Azure AI
  Vision** resource on the free **F0** tier (5000 photos/month, 20/min; beyond that Azure refuses instead of charging),
  Lens sends the prepared photo (upright, max 2400 px, JPEG) to Image Analysis 4.0 *Read*
  (`<endpoint>/computervision/imageanalysis:analyze?api-version=2024-02-01&features=read`, header
  `Ocp-Apim-Subscription-Key`), which reads handwriting like Google Lens. Words without Latin letters/digits (Persian
  notes) are skipped. No connection, wrong key/endpoint, quota used up or no text → the offline Tesseract reader is used
  and Lens says why. *Test* checks endpoint + key with a tiny picture. Code: `src/cloudocr.*` (settings + HTTPS),
  `src/ocr/azureread.*` (URL + response parsing, unit-tested in `tst_azureread`). Endpoint and key are stored in the
  app's settings on the phone.
  Setup: portal.azure.com → *Create a resource* → **Computer Vision** → region e.g. *West Europe*, pricing tier
  **Free F0** → after creation *Keys and Endpoint* → copy *Endpoint* and *KEY 1* into the app.
- **Translation** (Settings ⚙ → *Translation*: Persian or English). After recognition the whole text is shown
  translated in a collapsible panel under the picture (above it: *Text in the photo* with 🔊, the recognized text read
  aloud in the learning language); every selected word gets its own translation (and a
  multi-word/sentence selection its phrase translation). *Add N words* / *Create card* put that translation on the
  card's back. Persian keeps the word pack's curated meaning (+ grammar note) when the word is in the pack; English
  adds the pack's grammar note after the translation.
- **Online vs offline**: with *Translate online when connected* on and a network (`QNetworkInformation`), the app
  uses Google's public endpoint (`translate.googleapis.com`, `client=gtx`, no key; unofficial and rate-limited,
  fine for personal use). Every online result is saved to `AppDataLocation/translations.sqlite`, so the same
  word or text is translated offline later. Offline (or online failure) → saved translations, then the word
  pack's Persian meaning. Settings shows the current mode and can clear saved translations.
- Dependency check: if the build has no Tesseract or the language data is missing, Lens says so and offers
  download links (Tesseract, tessdata_fast). Camera permission is requested at runtime; the gallery works without it.
- Code: `src/ocr/` (Tesseract wrapper, pure selection helpers), `src/ocrengine.*`, `qml/LensPage.qml`;
  third-party libs and the Android build script are in `3rdparty/` (see `3rdparty/README.md`).

## Layout
```
src/leitner.h         pure scheduling rules (unit-tested)
src/cardstore.*       SQLite, QML singleton `CardStore`
src/cardlistmodel.*   QAbstractListModel `CardListModel` with filter
src/reviewsession.*   `ReviewSession` queue/state, reloadCurrent (after edit), moveCurrent (box by hand)
src/speaker.*         QML singleton `Speaker` (QTextToSpeech wrapper)
src/wordpack.*        pure TSV word-pack parser (unit-tested, incl. the shipped pack)
src/wordpacks.*       QML singleton `WordPacks`: chapters, add chapter / all, preview, starter words
src/starterdeck.*     pure parser for the 100 starter words (unit-tested, incl. the shipped file)
src/appmode.*         QML singleton `AppMode`: Simple / Full, first-run setup done
src/cardimages.*      picture import: EXIF rotate, scale to 1024 px, save JPEG (unit-tested)
src/ocr/              Tesseract wrapper (ocr.*), word selection helpers (textselect.*, unit-tested)
src/ocrengine.*       QML `OcrEngine`: async recognition, word boxes, sentence selection
src/translator.*      QML singleton `Translator`: settings, online/offline, SQLite cache of translations
src/translation/      Google endpoint URL/body + response parser (pure, unit-tested)
3rdparty/             build_ocr_android.cmd (downloads Tesseract, Leptonica, cpu_features at pinned versions);
                      android_openssl/ (KDAB prebuilt libssl_3/libcrypto_3 for HTTPS on Android)
data/tessdata/        German OCR model (deu.traineddata, tessdata_fast)
qml/                  Main, HomePage, ReviewPage, CardEditPage, CardListPage, SpeakButton,
                      WordPacksPage, WordPackChapterPage, MeaningText, LensPage, SettingsPage, BoxPage,
                      SendCardsPage, GetCardsPage, SetupPage, ModeComparison, ResetDialog, HintLabel,
                      CameraShotPage, HelpPage, ExampleText; icons GearIcon, HelpIcon, EditIcon
tests/                Qt Test: tst_leitner, tst_wordpack, tst_starterdeck, tst_cardimages, tst_cardstore, tst_cardstore_migrate,
                      tst_deckformats, tst_deckexchange, tst_textselect, tst_googletranslate, tst_tatoeba,
                      tst_azureread, tst_devicetilt, tst_ocrmerge,
                      tst_ocr (real OCR on rendered German text; only when Tesseract is available)
```

## Dependencies & downloads
Everything the app and its build need, with links. Items marked *committed* are already in the repository.

**Development tools (Windows PC)**
| What | Version | Link |
|---|---|---|
| Qt (Online Installer) — kits *Android arm64-v8a* (+ *x86_64* for the emulator) and *MinGW 64-bit* (host); Additional Libraries: **Qt Speech**, **Qt Multimedia**, **Qt Sensors** (optional) | 6.11.x | [qt.io/download-qt-installer-oss](https://www.qt.io/download-qt-installer-oss) |
| Android Studio (SDK, platform tools / adb, emulator) | latest | [developer.android.com/studio](https://developer.android.com/studio) |
| Android NDK (the scripts use exactly this one) | r27c = 27.2.12479018 | Android Studio → SDK Manager → SDK Tools → *Show Package Details* → NDK 27.2.12479018, or the r27 downloads on the [NDK wiki](https://github.com/android/ndk/wiki) (another r27 version needs the path in `deploy.bat` / `build_ocr_android.cmd` changed) |
| JDK (Gradle build only — the app has no Java code) | 17 | [adoptium.net — Temurin 17](https://adoptium.net/temurin/releases/?version=17) |
| CMake | ≥ 3.16 | [cmake.org/download](https://cmake.org/download/) |
| Ninja | ≥ 1.11 | [github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases) |
| Git (also used by `build_ocr_android.cmd`) | any | [git-scm.com/download/win](https://git-scm.com/download/win) |

**Libraries and data used by the app**
| What | Version | Link | How it gets here |
|---|---|---|---|
| Tesseract OCR | 5.5.2 | [github.com/tesseract-ocr/tesseract (5.5.2)](https://github.com/tesseract-ocr/tesseract/tree/5.5.2) | downloaded + built by `3rdparty\build_ocr_android.cmd` |
| Leptonica | 1.87.0 | [github.com/DanBloomberg/leptonica (1.87.0)](https://github.com/DanBloomberg/leptonica/tree/1.87.0) | same script |
| cpu_features | v0.11.0 | [github.com/google/cpu_features (v0.11.0)](https://github.com/google/cpu_features/tree/v0.11.0) | same script |
| German OCR model `deu.traineddata` (tessdata_fast) | 4.1.0 | [deu.traineddata](https://github.com/tesseract-ocr/tessdata_fast/raw/main/deu.traineddata) · [tessdata_fast](https://github.com/tesseract-ocr/tessdata_fast) | *committed* in `data/tessdata/` |
| OpenSSL 3 for Android (HTTPS) | ssl_3 | [github.com/KDAB/android_openssl](https://github.com/KDAB/android_openssl) | *committed* for arm64-v8a; copy `ssl_3/x86_64` for the emulator |
| Word pack *Netzwerk neu A1* | — | — | *committed* in `data/wordpacks/` |

**Online services (optional, free)**
| What | Link |
|---|---|
| Translation (Google endpoint, no key) | [translate.google.com](https://translate.google.com) |
| Example sentences (Tatoeba API, no key) | [tatoeba.org](https://tatoeba.org) · [API docs](https://api.tatoeba.org/) |
| Handwriting recognition: Azure AI Vision, free F0 tier (endpoint + key in Settings) | [Create resource](https://portal.azure.com/#create/Microsoft.CognitiveServicesComputerVision) · [Read OCR docs](https://learn.microsoft.com/azure/ai-services/computer-vision/overview-ocr) |

**On the phone**
| What | Link |
|---|---|
| German text-to-speech voice: *Speech Recognition and Synthesis from Google*, then Android Settings → Text-to-speech → install German | [Google Play](https://play.google.com/store/apps/details?id=com.google.android.tts) |

The app itself checks what is missing and shows these links: Lens (Tesseract / language data) and Home (German voice).

## Required Qt components
Qt Quick, Quick Controls 2, SQL, Concurrent, Network, **Qt Sensors** (optional: phone tilt for Lens), **Qt TextToSpeech** and **Qt Multimedia**
(Maintenance Tool → Qt 6.11.x → Additional Libraries → Qt Speech / Qt Multimedia).

## Android
- App icon: the Achaemenid *Shahbaz* standard. Source `assets/icons/app_icon.png`; Android icons in `android/res/`:
  the full flag with its triangle border everywhere — adaptive icon (`mipmap-anydpi-v26`, flag at 58 dp so it fits every
  launcher shape, on red `#AC0F0A`), `ic_launcher_round` and legacy `ic_launcher` (48–192 px, rounded corners). The manifest uses `@mipmap/ic_launcher` / `@mipmap/ic_launcher_round`.
- `android/` was created by Qt Creator (Build Android APK → Create Templates). `AndroidManifest.xml` adds
  `<queries><intent><action android:name="android.intent.action.TTS_SERVICE"/></intent></queries>`,
  so QTextToSpeech can find the system TTS engine on Android 11+. **Keep that block** if you regenerate the templates.
- HTTPS: Android has no OpenSSL for apps, so `CMakeLists.txt` bundles `3rdparty/android_openssl/ssl_3/<abi>/`
  via `QT_ANDROID_EXTRA_LIBS` (INTERNET/ACCESS_NETWORK_STATE permissions come from Qt Network automatically).
- `deploy.bat` (project root): build + install + launch on the phone (arm64-v8a) — `deploy.bat`, `quick`, `log`, `watch`, `tts`.
- The launcher name is set by `QT_ANDROID_APP_NAME` ("Learning Box").
- UI is edge-to-edge-safe (Android 15): the toolbar pads with `SafeArea.margins.top` and the page area with `SafeArea.margins.bottom`.
- Emulator: AVD `Samsung_Galaxy_S25` (Android 15, Google Play, x86_64, 1080×2340 @ 420 dpi) with Google TTS German voice data installed.
  Use the **Qt 6.11.1 for Android x86_64** kit for the emulator and **arm64-v8a** for a real phone.
- `build/lb.cmd` (debug helper, not committed): `lb build` (APK + install), `lb start`, `lb tap X Y`, `lb text S`, `lb shot NAME`.

## Known limitations
- Android shows "Learning Box pasted from your clipboard" when a text field gets focus. Qt reads the clipboard to decide
  whether to offer Paste; this can't be changed from QML/C++. The edit page no longer auto-focuses a field, to reduce it.

## Next milestones
2. ~~OCR / Lens~~ (done)
3. Sentence suggestions: offline Tatoeba German corpus in SQLite
