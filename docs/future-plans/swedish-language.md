# Future plan: Swedish as a learning language

Status: idea, not started. No code has been changed for this.
Written: 2026-10-03

## Starting point

The app already supports more than one learning language. Each learning box has a
`language` ("de" or "en") and a meaning language, and TTS, translation and OCR follow the
selected box (`CardStore::learningLanguage()` / `meaningLanguage()`). English was added this
way, so Swedish ("sv") would be the third language, not a redesign.

## Work needed (rough order of effort)

1. **Language plumbing (small)**
   - `CardStore`: allow "sv" next to "de" and "en"; define which meaning languages go with it
     (Persian, English, maybe German).
   - `Speaker`: add the "sv-SE" voice tag.
   - `Translator`: "sv" as a source language; cache keys get an "sv|" prefix like "en|" today.

2. **TTS (small, depends on the phone)**
   - The app uses the Android voice, so a Swedish voice must be installed.
   - Reuse the existing "no voice installed" message that German has.

3. **OCR for Lens (medium)**
   - Add `swe.traineddata` (fast model, about 1-4 MB) to `data/tessdata`.
   - The OCR language picker (`tesseractLanguage()`) must choose it.
   - Adds to the APK size. The model covers å, ä, ö.

4. **Translation backend (check first)**
   - The online service used by `Translator` must support Swedish <-> Persian and English.
   - Persian is the weak spot for many services: test before committing.

5. **Content (the real work)**
   - The German word packs, level packs ("Starten wir!" A1/A2), starter deck and dictionary
     are German-specific. Swedish needs its own, written from scratch.
   - A native-speaker check of the wording is recommended.

6. **UI and help text (medium, tedious)**
   - Strings that say "German" (review modes such as "German first", help pages, Settings
     text) must become language-neutral or per-language.
   - Swedish as an interface or help language is a separate decision (help/Settings
     dictionaries are en/fa/de today).

## Decisions still open

- Swedish only as a language to learn (the German way), or also as an interface/help language?
  The first is much less work.
- Which meaning languages should Swedish boxes offer?
- First release with word/level packs, or an empty Swedish box with Lens, TTS and manual cards?

## Suggested first version

Items 1-4 only, no packs: Swedish learning boxes with Lens, TTS and manual cards. Useful for
learning from real Swedish text.

## Updating this file

Add findings under the matching item and move decisions from "open" to a "Decided" section
when they are made.
