@echo off
setlocal

:: ============================================================
::  LearningBox - Debug Deploy Tool (Android phone, arm64-v8a)
::
::  Usage:
::    deploy.bat                 full cmake + build + install + launch + log (20s)
::    deploy.bat quick           skip cmake -> build + install + launch + log (20s)
::    deploy.bat quick 120       same, but capture log for 120s instead of 20s
::    deploy.bat log             launch app + capture logcat (no rebuild, 20s)
::    deploy.bat log 120         same, but capture log for 120s
::    deploy.bat watch           launch app + stream logcat LIVE to debug_log.txt
::                               until you press Ctrl+C
::    deploy.bat tts             TTS diagnostics only
:: ============================================================

set SRC_DIR=D:\Projects\LearningBox\LearningBox
set BUILD_DIR=%SRC_DIR%\build\Qt_6_11_1_for_Android_arm64_v8a_Debug
set APK=%BUILD_DIR%\android-build-appLearningBox\build\outputs\apk\debug\android-build-appLearningBox-debug.apk
set SDK=C:\Users\Mazi\AppData\Local\Android\Sdk
set ADB=%SDK%\platform-tools\adb.exe
set CMAKE=C:\Program Files\CMake\bin\cmake.exe
set "JAVA_HOME=C:\Program Files\Java\jdk-23"
set PACKAGE=se.morteza.learningbox
set ACTIVITY=org.qtproject.qt.android.bindings.QtActivity
set LOGFILE=%SRC_DIR%\debug_log.txt
set INSTLOG=%SRC_DIR%\install_log.txt

set MODE=%~1
set WAITSEC=%~2
if "%MODE%"=="" set MODE=full
if "%WAITSEC%"=="" set WAITSEC=20

echo.
echo   LearningBox Deploy  [mode: %MODE%]
echo.

:: ---- Check device -----------------------------------------------------------
"%ADB%" get-state >nul 2>&1
if errorlevel 1 (
    echo ERROR: No device found ^(or more than one - close the emulator^).
    echo        Check USB cable and USB debugging.
    pause
    exit /b 1
)
echo [OK] Device connected.
"%ADB%" devices
echo.

if /i "%MODE%"=="tts"   goto TTS_DIAG
if /i "%MODE%"=="watch" goto WATCH_LOG
if /i "%MODE%"=="log"   goto LAUNCH_AND_LOG
if /i "%MODE%"=="quick" goto BUILD

:: ---- [1/4] CMake ------------------------------------------------------------
echo [1/4] CMake...
"%CMAKE%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -DANDROID_ABI=arm64-v8a -DANDROID_NDK="C:/Users/Mazi/AppData/Local/Android/Sdk/ndk/27.2.12479018" -DANDROID_PLATFORM=android-28 -DANDROID_SDK_ROOT="C:/Users/Mazi/AppData/Local/Android/Sdk" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="C:/Users/Mazi/AppData/Local/Android/Sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/android_arm64_v8a" -DCMAKE_FIND_ROOT_PATH="C:/Qt/6.11.1/android_arm64_v8a" -DQT_HOST_PATH="C:/Qt/6.11.1/mingw_64" -DANDROID_USE_LEGACY_TOOLCHAIN_FILE=OFF -DCMAKE_GENERATOR=Ninja -DCMAKE_MAKE_PROGRAM="C:/mingw64/bin/ninja.exe"
if errorlevel 1 ( echo CMake FAILED. & pause & exit /b 1 )
echo [OK] CMake done.
echo.

:BUILD
:: ---- [2/4] Build APK --------------------------------------------------------
echo [2/4] Build APK...
"%CMAKE%" --build "%BUILD_DIR%" --target apk
if errorlevel 1 ( echo Build FAILED. & pause & exit /b 1 )
:: Qt names the Gradle folder android-build or android-build-appLearningBox (depends on how the
:: build folder was created) - take the newest *-debug.apk under the build folder.
set "APK="
for /f "delims=" %%F in ('powershell -NoProfile -Command "$f = Get-ChildItem -Path '%BUILD_DIR%' -Recurse -Filter '*-debug.apk' | Sort-Object LastWriteTime -Descending | Select-Object -First 1; if ($f) { $f.FullName }"') do set "APK=%%F"
if not defined APK ( echo APK not found under %BUILD_DIR% & pause & exit /b 1 )
echo APK: %APK%
echo [OK] Build done.
echo.

:: ---- [3/4] Install ----------------------------------------------------------
echo [3/4] Install...
echo  WATCH YOUR PHONE - tap Install if a dialog appears.
"%ADB%" shell settings put global verifier_verify_adb_installs 0 >nul 2>&1
"%ADB%" shell settings put global package_verifier_enable 0 >nul 2>&1
"%ADB%" shell am force-stop %PACKAGE% >nul 2>&1
timeout /t 1 /nobreak >nul
"%ADB%" devices -l > "%INSTLOG%" 2>&1
"%ADB%" install -r -t -g "%APK%" >> "%INSTLOG%" 2>&1
type "%INSTLOG%"
findstr /c:"Success" "%INSTLOG%" >nul || ( echo Install FAILED - see install_log.txt & pause & exit /b 1 )
echo [OK] Install done.
echo.

:LAUNCH_AND_LOG
:: ---- [4/4] Launch + capture logcat ------------------------------------------
echo [4/4] Launch + capture log...
"%ADB%" logcat -G 16M >nul 2>&1
"%ADB%" logcat -c
"%ADB%" shell am force-stop %PACKAGE% >nul 2>&1
timeout /t 1 /nobreak >nul
"%ADB%" shell am start -n "%PACKAGE%/%ACTIVITY%"
echo.
echo Waiting %WAITSEC% seconds (watch your phone) - reproduce the issue now...
timeout /t %WAITSEC% /nobreak
echo.
echo Saving log...
"%ADB%" logcat -d -v time > "%LOGFILE%"
echo [OK] Log saved to debug_log.txt
echo.
echo --- App lines (qml / Qt / TTS / CardStore / crash) ---
findstr /i "qml Qt TextToSpeech CardStore learningbox FATAL AndroidRuntime DEBUG" "%LOGFILE%"
echo.
pause
goto :EOF

:: ---- Live log stream (no time limit - Ctrl+C to stop) -----------------------
:WATCH_LOG
echo --- LIVE LOG CAPTURE ---
echo.
echo   1. The app will launch fresh now.
echo   2. Reproduce the issue on the phone.
echo   3. Press Ctrl+C here when done (answer Y to "Terminate batch job").
echo.
pause
"%ADB%" logcat -G 16M >nul 2>&1
"%ADB%" logcat -c
"%ADB%" shell am force-stop %PACKAGE% >nul 2>&1
timeout /t 1 /nobreak >nul
"%ADB%" shell am start -n "%PACKAGE%/%ACTIVITY%"
echo.
echo Streaming to debug_log.txt now - Ctrl+C when done...
"%ADB%" logcat -v time > "%LOGFILE%"
echo.
echo [OK] Log saved to debug_log.txt
pause
goto :EOF

:: ---- TTS diagnostics --------------------------------------------------------
:TTS_DIAG
echo --- TTS DIAGNOSTICS ---
echo.
echo [1] Default TTS engine:
"%ADB%" shell settings get secure tts_default_synth
echo.
echo [2] Installed TTS engines:
"%ADB%" shell cmd package query-services -a android.intent.action.TTS_SERVICE | findstr /i "packageName"
echo.
echo [3] Default TTS locale:
"%ADB%" shell settings get secure tts_default_locale
echo.
echo [4] App installed / running?
"%ADB%" shell pm list packages %PACKAGE%
"%ADB%" shell ps -A | findstr "%PACKAGE%"
echo.
echo [5] Last 150 TTS/Qt logcat lines:
"%ADB%" logcat -d -v time -s QtTextToSpeech:* TextToSpeech:* qml:* Qt:* -t 150
echo.
pause
goto :EOF
