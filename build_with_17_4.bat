@echo off
echo Building pg_fasttransfer with PostgreSQL 17.4 headers...

REM Check for MSVC
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Run this from Developer Command Prompt for VS
    pause
    exit /b 1
)

REM Clean previous build
del pg_fasttransfer.dll pg_fasttransfer.lib pg_fasttransfer.exp pg_fasttransfer.obj >nul 2>&1

REM Compile using 17.4 headers
cl.exe /LD /MD /O2 /Fe:pg_fasttransfer.dll pg_fasttransfer.c ^
  /I"." ^
  /I"pgsql-build\include" ^
  /I"pgsql-build\include\server" ^
  /DWIN32 /D_WINDOWS ^
  /link /LIBPATH:"pgsql-build\lib" postgres.lib ws2_32.lib

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful! pg_fasttransfer.dll created.
echo This DLL will work with PostgreSQL 17.0 through 17.x
pause