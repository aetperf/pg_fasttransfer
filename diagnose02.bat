@echo off
echo Looking for the actual tupmacs.h content...

REM First, let's find where tupmacs.h appears in the preprocessed output
findstr /N "tupmacs.h" preprocessed.txt > tupmacs_location.txt
echo === Location of tupmacs.h references ===
type tupmacs_location.txt
echo.

REM Now let's look for switch statements and enums
echo === Looking for switch/case structures ===
findstr /N "switch\|case\|enum" preprocessed.txt | findstr /I "4" > switch_conflicts.txt
type switch_conflicts.txt | more
echo.

REM Let's also directly examine tupmacs.h
echo === Direct examination of tupmacs.h lines 60-70 and 190-200 ===
set TUPMACS_H=C:\PROGRA~1\POSTGR~1\17\include\server\access\tupmacs.h
if exist "%TUPMACS_H%" (
    powershell -Command "Get-Content '%TUPMACS_H%' | Select-Object -Skip 60 -First 10"
    echo.
    echo Lines 190-200:
    powershell -Command "Get-Content '%TUPMACS_H%' | Select-Object -Skip 190 -First 10"
) else (
    echo Could not find tupmacs.h at expected location
)

pause
