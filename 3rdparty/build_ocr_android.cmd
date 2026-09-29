@echo off
rem ---------------------------------------------------------------------------
rem Builds the OCR libraries for Android as static libraries:
rem   cpu_features (Google)  ->  Leptonica  ->  Tesseract
rem into 3rdparty\install\android-<ABI>, where CMakeLists.txt picks them up.
rem
rem Usage (from any directory):   3rdparty\build_ocr_android.cmd [ABI ...]
rem   default ABIs: x86_64 (emulator) and arm64-v8a (phones)
rem Needs: Android NDK, CMake, Ninja, git (first run downloads the sources). Override paths with ANDROID_NDK / CMAKE_EXE / NINJA.
rem Logs:  3rdparty\build-android-<ABI>\*.log
rem Note: LEPT_TIFF_RESULT=1 answers Tesseract's try_run TIFF probe, which cannot run
rem       when cross-compiling (Leptonica is built without TIFF on purpose).
rem ---------------------------------------------------------------------------
setlocal EnableExtensions
if "%ANDROID_NDK%"=="" set "ANDROID_NDK=%LOCALAPPDATA%\Android\Sdk\ndk\27.2.12479018"
if "%CMAKE_EXE%"=="" set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
if "%NINJA%"=="" set "NINJA=C:\mingw64\bin\ninja.exe"
rem Moderate parallelism keeps a laptop cool; raise if you like.
if "%JOBS%"=="" set "JOBS=4"
set "HERE=%~dp0"

if not exist "%ANDROID_NDK%\build\cmake\android.toolchain.cmake" ( echo NDK not found: %ANDROID_NDK% & exit /b 1 )
if not exist "%CMAKE_EXE%" ( echo CMake not found: %CMAKE_EXE% & exit /b 1 )
if not exist "%NINJA%" ( echo Ninja not found: %NINJA% & exit /b 1 )

rem Sources are not committed: download the pinned versions when missing (needs git).
call :fetch cpu_features https://github.com/google/cpu_features.git v0.11.0 || exit /b 1
call :fetch leptonica https://github.com/DanBloomberg/leptonica.git 1.87.0 || exit /b 1
call :fetch tesseract https://github.com/tesseract-ocr/tesseract.git 5.5.2 || exit /b 1

set "ABIS=%*"
if "%ABIS%"=="" set "ABIS=x86_64 arm64-v8a"
for %%A in (%ABIS%) do (
  call :build_abi %%A
  if errorlevel 1 ( echo *** FAILED for %%A & exit /b 1 )
)
echo.
echo ALL OCR LIBRARIES BUILT: %ABIS%
exit /b 0

:fetch
if exist "%HERE%%~1\CMakeLists.txt" exit /b 0
echo Downloading %~1 %~3 ...
git clone --depth 1 --branch %~3 %~2 "%HERE%%~1" || ( echo git clone failed for %~1 & exit /b 1 )
exit /b 0

:build_abi
set "ABI=%~1"
set "P=%HERE%install\android-%ABI%"
set "B=%HERE%build-android-%ABI%"
if not exist "%B%" mkdir "%B%"
echo.
echo ===== %ABI% =====

echo [1/3] cpu_features
"%CMAKE_EXE%" -S "%HERE%cpu_features" -B "%B%\cpu" -G Ninja "-DCMAKE_MAKE_PROGRAM=%NINJA%" ^
  "-DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%\build\cmake\android.toolchain.cmake" -DANDROID_ABI=%ABI% ^
  -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_INSTALL_PREFIX=%P%" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_EXECUTABLE=OFF ^
  -DENABLE_INSTALL=ON > "%B%\cpu-configure.log" 2>&1 || ( type "%B%\cpu-configure.log" & exit /b 1 )
"%CMAKE_EXE%" --build "%B%\cpu" --target install -j %JOBS% > "%B%\cpu-build.log" 2>&1 || ( type "%B%\cpu-build.log" & exit /b 1 )

echo [2/3] Leptonica
"%CMAKE_EXE%" -S "%HERE%leptonica" -B "%B%\lept" -G Ninja "-DCMAKE_MAKE_PROGRAM=%NINJA%" ^
  "-DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%\build\cmake\android.toolchain.cmake" -DANDROID_ABI=%ABI% ^
  -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_INSTALL_PREFIX=%P%" -DBUILD_SHARED_LIBS=OFF -DBUILD_PROG=OFF -DSW_BUILD=OFF ^
  -DENABLE_ZLIB=OFF -DENABLE_PNG=OFF -DENABLE_GIF=OFF -DENABLE_JPEG=OFF -DENABLE_TIFF=OFF ^
  -DENABLE_WEBP=OFF -DENABLE_OPENJPEG=OFF > "%B%\lept-configure.log" 2>&1 || ( type "%B%\lept-configure.log" & exit /b 1 )
"%CMAKE_EXE%" --build "%B%\lept" --target install -j %JOBS% > "%B%\lept-build.log" 2>&1 || ( type "%B%\lept-build.log" & exit /b 1 )

echo [3/3] Tesseract (a few minutes)
"%CMAKE_EXE%" -S "%HERE%tesseract" -B "%B%\tess" -G Ninja "-DCMAKE_MAKE_PROGRAM=%NINJA%" ^
  "-DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%\build\cmake\android.toolchain.cmake" -DANDROID_ABI=%ABI% ^
  -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release ^
  "-DCMAKE_INSTALL_PREFIX=%P%" -DBUILD_SHARED_LIBS=OFF -DBUILD_TRAINING_TOOLS=OFF -DBUILD_TESTS=OFF ^
  -DDISABLE_ARCHIVE=ON -DDISABLE_CURL=ON -DDISABLE_TIFF=ON -DGRAPHICS_DISABLED=ON -DOPENMP_BUILD=OFF ^
  -DENABLE_LTO=OFF -DSW_BUILD=OFF ^
  -DLEPT_TIFF_RESULT=1 -DLEPT_TIFF_RESULT__TRYRUN_OUTPUT= ^
  "-DLeptonica_DIR=%P%\lib\cmake\leptonica" ^
  "-DCpuFeaturesNdkCompat_DIR=%P%\lib\cmake\CpuFeaturesNdkCompat" ^
  "-DCpuFeatures_DIR=%P%\lib\cmake\CpuFeatures" > "%B%\tess-configure.log" 2>&1 || ( type "%B%\tess-configure.log" & exit /b 1 )
"%CMAKE_EXE%" --build "%B%\tess" --target install -j %JOBS% > "%B%\tess-build.log" 2>&1 || ( powershell -NoProfile -Command "Get-Content '%B%\tess-build.log' -Tail 40" & exit /b 1 )

echo %ABI% OK: %P%
exit /b 0
