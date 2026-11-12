# LearningBox – Application flowcharts

Reference for how the app is put together and how the user moves through it. Companion to
[`ROADMAP.md`](../ROADMAP.md) (what is done / planned). The Mermaid charts render on GitHub and GitLab.

**Keep it current:** when a screen, flow or rule changes, edit the matching chart and update the date below.
Source of truth for navigation: `qml/Main.qml`, `qml/HomePage.qml`. Last checked against the code: **2026-11-08**.

Contents: [1 Screens](#1-screens-and-navigation) · [2 Start-up](#2-start-up) · [3 Learning boxes](#3-learning-boxes-and-level-packs) ·
[4 Review](#4-review-session-leitner) · [5 Add / edit card](#5-add-or-edit-a-card) · [6 Dictionary](#6-dictionary) ·
[7 Lens](#7-lens-photo--words--cards) · [8 Send and get cards](#8-send-and-get-cards) · [9 Data and backup](#9-data-storage-and-backup) ·
[10 Architecture](#10-layers)

---

## 1. Screens and navigation

One `StackView` in `Main.qml`. Home is the root page; every other page is pushed on top and closed with the back arrow.
The toolbar shows **? Help** on every page and **⚙ Settings** on every page except Settings itself.

```mermaid
flowchart TD
    START([App start]) --> SETUP{First run?}
    SETUP -- yes --> SETUPP[SetupPage<br/>wizard]
    SETUP -- no --> HOME
    SETUPP --> HOME

    HOME[HomePage<br/>learning box chooser · box tiles 1-5 + Learned]

    HOME -->|Start review| REVIEW[ReviewPage]
    HOME -->|+ Add card| EDIT[CardEditPage]
    HOME -->|tap a box tile| BOX[BoxPage<br/>cards of one box]
    HOME -->|All cards| LIST[CardListPage<br/>search · sort]
    HOME -->|Lens| LENS[LensPage<br/>photo → OCR]
    HOME -->|Favorite words ★| FAV[FavoritesPage<br/>all boxes]
    HOME -->|Dictionary| DICT[DictionaryPage]
    HOME -->|Export| SEND[SendCardsPage]
    HOME -->|Import| GET[GetCardsPage]
    HOME -->|Word packs| PACKS[WordPacksPage / LevelPacksPage]

    HOME -.->|toolbar| HELP[HelpPage<br/>fa / en / de]
    HOME -.->|toolbar| SET[SettingsPage]

    BOX --> EDIT
    LIST --> EDIT
    FAV --> EDIT
    REVIEW -->|Edit| EDIT
    LENS --> EDIT
    DICT -->|+ Add| CARD[(card in a box)]
    EDIT --> CARD
    PACKS --> PCH[WordPackChapterPage<br/>LevelPage] --> CARD
    EDIT -->|camera| CAM[CameraShotPage]
```

---

## 2. Start-up

```mermaid
flowchart TD
    A([main.cpp]) --> B[Open SQLite learningbox.sqlite<br/>run schema migrations]
    B --> C{Cards restored<br/>from automatic backup?}
    C -- yes --> D[setupDone = true<br/>no wizard]
    C -- no --> E{setupDone?}
    D --> F
    E -- no --> W[SetupPage wizard:<br/>meaning language · Simple or Full app ·<br/>use Lens? · starter words]
    W --> F
    E -- yes --> F[LevelPacks.installBoxes<br/>once per pack group]
    F --> G[Hide boxes of languages that are<br/>switched off in Settings]
    G --> H[HomePage]
    H --> I{Backup permission<br/>asked yet? Android}
    I -- no --> J[One-time all-files-access prompt<br/>for Documents/LearningBox backup]
    I -- yes --> K([Ready])
    J --> K
```

---

## 3. Learning boxes and level packs

A **learning box** (collection) is a separate Leitner system with its own Boxes 1–5 and Learned. Every query is scoped to the
selected learning box. Each box has a **learning language** (German, English, Swedish) and a **meaning language**
(Persian, English, German).

```mermaid
flowchart LR
    subgraph Sources["Where learning boxes come from"]
        S1[User: New learning box]
        S2[Built-in level packs<br/>data/levelpacks/*.tsv]
        S3[Import file / link<br/>into a new learning box]
    end
    S2 --> P1[A1 / A2 levels<br/>starten_wir_levels]
    S2 --> P2[B1 · B2 · C1]
    S2 --> P3[A1_complete]
    S2 --> P4[Alltag]
    S2 --> P5[Job-Wortschatz]
    S2 --> P6[English A1 · Swedish A1/A2]

    S1 & S3 & P1 & P2 & P3 & P4 & P5 & P6 --> BOXES[(Learning boxes<br/>combo on Home)]

    SW{Settings:<br/>German / English / Swedish<br/>switches} -->|on: create missing boxes, show them| BOXES
    SW -->|off: hide every box of that language| BOXES
    BOXES --> SEL[Selected box<br/>remembered across restarts]
    SEL --> HOME[Home tiles · Review · Add · Lens]
```

Pack file format and groups: `src/levelpack.cpp`, `src/levelpacks.cpp` (group 0 A1/A2, 1 B1–C1, 2 "complete", 3 Alltag, 4 Job-Wortschatz).
Each group is installed **once** (a settings flag per group), so a box the user deleted does not come back.

---

## 4. Review session (Leitner)

Boxes 1–5, then Learned (6). Intervals: **1 / 2 / 4 / 8 / 16 days** (`src/leitner.h`).

```mermaid
flowchart TD
    S([Start review]) --> Q{Cards due today<br/>in this learning box?}
    Q -- no --> DONE([Done for today])
    Q -- yes --> F{Review direction<br/>per learning box}
    F -- "German first" --> C1[Show German word<br/>article colour mark · 🔊 auto-speak]
    F -- "Meaning first" --> C2[Show meaning<br/>say the German word]
    F -- Mixed --> C1
    C1 & C2 --> R[Show the meaning<br/>meaning · plural · examples with translation]
    R --> K{Knew it?}
    K -- yes --> UP[Move up one box<br/>1→2→3→4→5→Learned]
    K -- no --> DN[Back to Box 1<br/>re-asked at the end of the session]
    UP --> DUE[Next due date = today + interval of the new box]
    DN --> DUE
    DUE --> Q

    R -.-> EXTRA["Extras on the card:<br/>Edit · ★ favorite · choose box by hand ·<br/>فارسی / English switch · 🔊 · select + copy words ·<br/>◀ previous card (undo answer)"]
```

---

## 5. Add or edit a card

```mermaid
flowchart TD
    A([Add card / Edit]) --> T[Type the word<br/>capital letter fixed · ä ö ü ß key row]
    T --> SUG[Word suggestions while typing<br/>pack → saved → online]
    SUG --> M[Meaning filled in automatically<br/>pack → saved → online]
    M --> G[Article · singular / plural<br/>colour mark: der blue · die red · das green]
    G --> EX[Example sentences<br/>pack + Tatoeba · or type your own · several allowed]
    EX --> BX[Choose learning box and Box 1-5]
    BX --> SAVE{Word already<br/>in this box?}
    SAVE -- no --> NEW[(New card in Box 1)]
    SAVE -- yes --> ASK[Update existing<br/>Open it<br/>Cancel]
    ASK --> NEW
```

---

## 6. Dictionary

The Dictionary does **not** depend on the selected box: a German word is always a German card; "+ Add" goes to a German box.

```mermaid
flowchart TD
    A([Dictionary]) --> FT[From / To combo boxes<br/>Deutsch · English · فارسی]
    FT --> Q[Type a word]
    Q --> L[Look up: pack → saved → online translator]
    L --> RES[Result: meanings · alternatives ·<br/>article, plural, gender mark · examples · 🔊 · ★]
    RES --> BOXSEL[Choose which learning box to save to]
    BOXSEL --> ADD["+ Add: pick example sentences (checkboxes)"]
    ADD --> DUP{Already in box?}
    DUP -- no --> CARD[(Card)]
    DUP -- yes --> ASK[Update / Open / Cancel] --> CARD
```

---

## 7. Lens: photo → words → cards

```mermaid
flowchart TD
    P[Camera or gallery] --> SET{Lens enabled<br/>in setup?}
    SET -- no --> OFF[Message: Lens is off]
    SET -- yes --> L[Load · EXIF-rotate · phone tilt turns landscape upright<br/>scale to 2400 px]
    L --> HW{Handwriting and<br/>Azure key set?}
    HW -- yes --> AZ[Azure AI Vision Read<br/>online] --> M
    HW -- no --> T1
    AZ -.->|fails| T1
    L --> T1[Tesseract passes offline:<br/>page layout · sparse text · half-size sparse]
    T1 --> ENOUGH{Enough confident text?}
    ENOUGH -- no --> CL[Cleaned passes: moiré / glare]
    ENOUGH -- no --> ROT[Orientation probe 0 / 90 / 180 / 270]
    ENOUGH -- yes --> M
    CL --> M
    ROT --> M[Merge lines · reading order · re-join hyphenation]
    M --> V[Photo with word boxes]
    V --> SEL[Tap words · hold = whole sentence · pinch zoom]
    SEL --> TR[Translator: Persian or English<br/>online if enabled, else cache / pack]
    TR --> RESW[Result window: selected words + meanings]
    RESW --> CC[Create card]
    CC --> DUP{Already in box?}
    DUP -- no --> NEW[(New cards)]
    DUP -- yes --> ASK[Update existing / Open / Cancel] --> NEW
```

Dependency check: when Tesseract or its language data is missing, the Lens page shows what is missing with download links.

---

## 8. Send and get cards

```mermaid
flowchart LR
    subgraph Out["Export (SendCardsPage)"]
        E1[Choose: all cards · one learning box · favorites]
        E1 --> E2{Format}
        E2 --> F1[".lbox<br/>zip: cards.json + images/<br/>optional progress"]
        E2 --> F2[CSV]
        E2 --> F3[Anki text / .apkg]
        F1 & F2 & F3 --> SH[Save file · Android share sheet<br/>WhatsApp, Telegram, e-mail …]
    end
    subgraph In["Import (GetCardsPage)"]
        I1[File or link<br/>Google Drive · Dropbox · GitHub → direct download]
        I1 --> I2[Detect format:<br/>.lbox · CSV / text · hand-written card list · Anki .apkg]
        I2 --> I3[Preview · duplicate options]
        I3 --> I4[Into the current box<br/>or into a new learning box]
    end
    SH -. other person's phone .-> I1
```

CSV import reads the first three columns: German, meaning, example (a header row such as `German` is skipped). Code: `src/deckexchange.*`, `src/exchange/deckformats.*`.

---

## 9. Data storage and backup

```mermaid
flowchart TD
    subgraph Phone["AppDataLocation on the phone"]
        DB[(learningbox.sqlite<br/>cards · learning boxes · progress)]
        TR[(translations.sqlite<br/>offline translation cache)]
        IMG[images/<br/>card pictures, 1024 px JPEG]
        TD[tessdata/<br/>OCR model copy]
        QS[QSettings<br/>language, switches, flags]
    end
    DB -->|automatic backup| BK[Documents/LearningBox]
    BK -->|after reinstall| RES[Restore on first start<br/>no setup wizard]
    RES --> DB
    NET{Online enabled<br/>in Settings?}
    NET -- yes --> AZT[Azure AI Translator free tier<br/>own key, optional]
    AZT --> TR
    NET -- no --> TR
```

---

## 10. Layers

```mermaid
flowchart TD
    subgraph QML["qml/ – screens"]
        Q1[Main · HomePage · ReviewPage · CardEditPage · LensPage ·<br/>DictionaryPage · BoxPage · Settings · Help · Send / Get · Packs]
    end
    subgraph CPP["src/ – C++ backend (Qt 6, C++20)"]
        C1[CardStore<br/>SQLite]
        C2[Leitner rules<br/>leitner.h]
        C3[ReviewSession]
        C4[LevelPacks · WordPacks<br/>StarterDeck]
        C5[Translator<br/>Azure / cache]
        C6[Speaker<br/>QTextToSpeech]
        C7[OcrEngine · CloudOcr<br/>Tesseract · Azure Read]
        C8[DeckExchange<br/>lbox · CSV · apkg]
        C9[Backup · AppMode · DeviceTilt]
    end
    subgraph EXT["External"]
        X1[Tesseract + Leptonica<br/>3rdparty]
        X2[Android TTS voice]
        X3[Azure AI services]
        X4[Tatoeba · share sheet]
    end
    Q1 <--> C1 & C3 & C4 & C5 & C6 & C7 & C8 & C9
    C3 --> C2
    C3 --> C1
    C4 --> C1
    C7 --> X1
    C7 --> X3
    C5 --> X3
    C6 --> X2
    C8 --> X4
```

Details per file: see the module table in [`ROADMAP.md`](../ROADMAP.md#2-modules--what-lives-where).

---

## Changelog of this document
| Date | Change |
|---|---|
| 2026-11-08 | First version: screens, start-up, boxes and packs, review, add / edit, dictionary, Lens, exchange, data, layers |
