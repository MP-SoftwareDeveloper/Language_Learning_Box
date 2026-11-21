@echo off
setlocal
cd /d "%~dp0"
:: Emulator test run (x86_64).
:: RULE (keep this): the emulator window is always shown in the TOP-LEFT QUARTER of the screen
:: (emu_place.ps1 does the placement) so the process can be watched while working on the rest.
:: The test is driven only through adb (adb -e = emulator only, never the phone), so the mouse is never used.
:: Starts the emulator, builds + installs the app, launches it, then types "selbstaendig"
:: in the Dictionary through adb (adb -e = emulator only, never the phone) and saves screenshots + log.
:: Everything is written to emu_out\  - read status.txt there; the run closes itself when done.
set SRC_DIR=D:\Projects\LearningBox\LearningBox
set BUILD_DIR=%SRC_DIR%\build\Qt_6_11_1_for_Android_x86_64_Debug
set OUT=%SRC_DIR%\emu_out
set SDK=C:\Users\Mazi\AppData\Local\Android\Sdk
set ADB=%SDK%\platform-tools\adb.exe
set CMAKE=C:\Program Files\CMake\bin\cmake.exe
set "JAVA_HOME=C:\Program Files\Java\jdk-23"
set PACKAGE=se.morteza.learningbox
set ACTIVITY=org.qtproject.qt.android.bindings.QtActivity
if not exist "%OUT%" mkdir "%OUT%"
del /q "%OUT%\*.png" "%OUT%\*.txt" 2>nul
echo started %date% %time% > "%OUT%\status.txt"

:: reuse a running emulator, otherwise start one without a window
"%ADB%" -e get-state >nul 2>&1
if errorlevel 1 (
    start "emulator" /min "%SDK%\emulator\emulator.exe" -avd Samsung_Galaxy_S25 -no-snapshot-load -no-audio
    set STARTED=1
    echo emulator started >> "%OUT%\status.txt"
) else (
    echo emulator already running >> "%OUT%\status.txt"
)

:: move the emulator window to the top-left quarter of the screen (runs in background)
start "" /b powershell -NoProfile -ExecutionPolicy Bypass -File "%SRC_DIR%\emu_place.ps1"

"%CMAKE%" -S "%SRC_DIR%" -B "%BUILD_DIR%" -DANDROID_ABI=x86_64 -DANDROID_NDK="C:/Users/Mazi/AppData/Local/Android/Sdk/ndk/27.2.12479018" -DANDROID_PLATFORM=android-28 -DANDROID_SDK_ROOT="C:/Users/Mazi/AppData/Local/Android/Sdk" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="C:/Users/Mazi/AppData/Local/Android/Sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/android_x86_64" -DCMAKE_FIND_ROOT_PATH="C:/Qt/6.11.1/android_x86_64" -DQT_HOST_PATH="C:/Qt/6.11.1/mingw_64" -DANDROID_USE_LEGACY_TOOLCHAIN_FILE=OFF -DCMAKE_GENERATOR=Ninja -DCMAKE_MAKE_PROGRAM="C:/mingw64/bin/ninja.exe" > "%OUT%\cmake.log" 2>&1
if errorlevel 1 ( echo CMAKE FAILED >> "%OUT%\status.txt" & pause & exit /b 1 )
echo cmake ok >> "%OUT%\status.txt"

"%CMAKE%" --build "%BUILD_DIR%" --target apk > "%OUT%\build.log" 2>&1
if errorlevel 1 ( echo BUILD FAILED >> "%OUT%\status.txt" & pause & exit /b 1 )
set "APK="
for /f "delims=" %%F in ('powershell -NoProfile -Command "$f = Get-ChildItem -Path '%BUILD_DIR%' -Recurse -Filter '*-debug.apk' | Sort-Object LastWriteTime -Descending | Select-Object -First 1; if ($f) { $f.FullName }"') do set "APK=%%F"
if not defined APK ( echo NO APK >> "%OUT%\status.txt" & pause & exit /b 1 )
echo build ok >> "%OUT%\status.txt"

"%ADB%" -e wait-for-device
:BOOT
set BOOTED=
for /f "delims=" %%B in ('"%ADB%" -e shell getprop sys.boot_completed 2^>nul') do set BOOTED=%%B
if not "%BOOTED%"=="1" ( timeout /t 3 /nobreak >nul & goto BOOT )
echo booted >> "%OUT%\status.txt"
:: a freshly started emulator is busy for a while (System UI may show "isn't responding"): let it settle, then clear dialogs
if defined STARTED timeout /t 45 /nobreak >nul
"%ADB%" -e shell input keyevent 4
"%ADB%" -e shell input keyevent 3
timeout /t 2 /nobreak >nul

"%ADB%" -e install -r -t -g "%APK%" > "%OUT%\install.log" 2>&1
findstr /c:"Success" "%OUT%\install.log" >nul || ( echo INSTALL FAILED >> "%OUT%\status.txt" & pause & exit /b 1 )
echo installed >> "%OUT%\status.txt"

:: ---- the lookup test: Home -> Dictionary -> "selbstaendig" (a-umlaut from the in-app key row) -> tap suggestion ----
"%ADB%" -e shell am force-stop %PACKAGE%
"%ADB%" -e logcat -c
"%ADB%" -e shell am start -n "%PACKAGE%/%ACTIVITY%" >nul
timeout /t 10 /nobreak >nul
"%ADB%" -e shell input tap 612 1932
timeout /t 3 /nobreak >nul
"%ADB%" -e shell input tap 468 521
timeout /t 2 /nobreak >nul
"%ADB%" -e shell input text selbst
timeout /t 1 /nobreak >nul
"%ADB%" -e shell input tap 99 667
timeout /t 1 /nobreak >nul
"%ADB%" -e shell input text ndig
timeout /t 4 /nobreak >nul
"%ADB%" -e shell input tap 263 805
timeout /t 14 /nobreak >nul
"%ADB%" -e exec-out screencap -p > "%OUT%\result.png"
"%ADB%" -e logcat -d -v time > "%OUT%\log.txt"
echo DONE result.png + log.txt >> "%OUT%\status.txt"
exit /b 0
