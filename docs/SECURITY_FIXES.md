# LearningBox – Security fixes: summary and to-do

Date: 2026-11-09 · Details and evidence: [SECURITY.md](SECURITY.md)
Tested on Qt 6.4 (Linux sandbox), **not on a phone**. All changes are uncommitted.

## Fixed
| ID | What | Test |
|---|---|---|
| F-1 | Backup restore validated (`quick_check`, schema version, `cards` table; pictures `<uuid>.jpg` ≤ 8 MB) | `tst_backup` |
| F-3 | `.lbox` text through `plainText()`; `textFormat: Text.PlainText` on 185 QML `Label`/`Text` | `tst_hostile` |
| F-4 | Azure endpoint: `https://` only | `tst_azureread` |
| F-5 | Max 5 redirects; CSV import capped at 100 000 cards | `tst_hostile` |
| F-6 | FileProvider paths reduced to `shared/` | phone only |
| F-7 | `.apkg` opened read-only; `notes` must be a table | `tst_hostile` |
| F-12 | `plainText()`: length cap, static regexes, one-pass entities | `tst_hostile` |

## Still to do (you)
- [ ] **F-2** Run `3rdparty/build_openssl_android.sh` (or Action `openssl-android`), replace `ssl_3/arm64-v8a` + `x86_64`, rebuild, test translate / Tatoeba / link import / Azure *Test*
- [ ] **F-1** Decide: keep or drop `MANAGE_EXTERNAL_STORAGE` (dropping it removes reinstall auto-backup)
- [ ] Phone test: *Share*, *Save file*, camera, *Open folder* (F-6)
- [ ] Phone test: import `tests/security/hostile/*` (`python tests/security/make_hostile_files.py`), incl. `02_html_cards.lbox`
- [ ] First run of `.github/workflows/security.yml` and `openssl-android.yml` (expect small fixes)
- [ ] Run gitleaks, cppcheck, MobSF (SECURITY.md §6)
- [ ] Look at `tst_deckexchange` (`csvExportOfOneBox`, `importIntoNewLearningBox`) – failing before these changes too
- [ ] Review, then commit; delete `_to_delete/`

## Not changed
- Imports still run on the UI thread (worker thread = larger change)
- `ComboBox` / `Button` / `ItemDelegate` text keeps Qt's default format
- `AndroidManifest.xml` unchanged (`requestLegacyExternalStorage` is needed on Android 10)

## Update log
| Date | Change |
|---|---|
| 2026-11-09 | Fixes F-1, F-3, F-4, F-5, F-6, F-7, F-12; F-2 prepared |
