@echo off
:: Emulator test (adb -e only): Dictionary -> type "selbständig" (ä from the in-app key row) -> tap the suggestion -> wait -> screenshot + log.
set ADB=C:\Users\Mazi\AppData\Local\Android\Sdk\platform-tools\adb.exe
set OUT=D:\Projects\LearningBox\LearningBox\emu_out
"%ADB%" -e logcat -c
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
"%ADB%" -e exec-out screencap -p > "%OUT%\s7.png"
"%ADB%" -e logcat -d -v time > "%OUT%\log7.txt"
echo step done > "%OUT%\step7.txt"
