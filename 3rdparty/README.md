# Third-party code (OCR)

Sources are **not committed**: `build_ocr_android.cmd` downloads these pinned versions with `git clone` on first run:

| Library | Version | License | Used for |
|---|---|---|---|
| [Tesseract OCR](https://github.com/tesseract-ocr/tesseract) | 5.5.2 | Apache-2.0 | text recognition |
| [Leptonica](https://github.com/DanBloomberg/leptonica) | 1.87.0 | BSD-2-Clause | image library required by Tesseract (built without codecs; Qt decodes photos) |
| [cpu_features](https://github.com/google/cpu_features) | 0.11.0 | Apache-2.0 | CPU feature detection, required by Tesseract on Android |
| [tessdata_fast `deu`](https://github.com/tesseract-ocr/tessdata_fast) ([direct download](https://github.com/tesseract-ocr/tessdata_fast/raw/main/deu.traineddata)) | 4.1.0 | Apache-2.0 | German model, `data/tessdata/deu.traineddata` (1.5 MB, committed) |
| [android_openssl](https://github.com/KDAB/android_openssl) (KDAB) | ssl_3 | Apache-2.0 | HTTPS on Android, `android_openssl/ssl_3/<abi>/` (arm64-v8a committed) |

## Build
- **Android**: `3rdparty\build_ocr_android.cmd` (x86_64 + arm64-v8a) → `3rdparty/install/android-<ABI>/`.
  Uses the NDK from `%LOCALAPPDATA%\Android\Sdk\ndk\27.2.12479018`; about 5 minutes per ABI.
- **Desktop (Linux, later Windows)**: same CMake options without the NDK toolchain, installed to
  `3rdparty/install/<Linux|Windows>/`.
- `CMakeLists.txt` looks in `3rdparty/install/<platform>`; override with `-DLB_OCR_PREFIX=...`.
  If nothing is found, the app builds **without OCR** and the Lens page shows what is missing, with download links.

`build-*`, `install/` and the downloaded sources are not committed. `android_openssl/ssl_3/arm64-v8a` is committed
(phones); the x86_64 copy for the emulator is not — get it from [KDAB/android_openssl](https://github.com/KDAB/android_openssl).
