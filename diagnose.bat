@echo off
echo Preprocessing to see the actual conflict...

cl /E /I"%ProgramFiles%\PostgreSQL\17\include\server" ^
    /I"%ProgramFiles%\PostgreSQL\17\include" ^
    /DWIN32 /D_WINDOWS ^
    pg_fasttransfer.c > preprocessed.txt 2>&1

echo Check preprocessed.txt for the actual enum values
findstr /N "case 4:" preprocessed.txt
pause
