@echo off
REM Script to check available PostgreSQL libraries

echo Checking PostgreSQL lib directory...
for /f "tokens=*" %%i in ('pg_config --libdir') do set PG_LIB=%%i
echo PostgreSQL lib directory: %PG_LIB%
echo.

echo Available .lib files:
dir "%PG_LIB%\*.lib" /b
echo.

echo Available .dll files:
dir "%PG_LIB%\*.dll" /b
echo.

pause