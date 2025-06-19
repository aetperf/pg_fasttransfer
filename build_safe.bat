@echo off
REM Safe build script that creates a minimal working extension to test basic functionality

echo.
echo ============================================
echo  Safe Windows Build for pg_fasttransfer
echo ============================================
echo.

REM Check if we're in a Developer Command Prompt
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Microsoft C compiler not found!
    echo Please run this script from "Developer Command Prompt for VS"
    pause
    exit /b 1
)

REM Check if pg_config is available
where pg_config >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: pg_config not found!
    echo Please ensure PostgreSQL bin directory is in your PATH
    pause
    exit /b 1
)

REM Get PostgreSQL configuration
echo Getting PostgreSQL configuration...
for /f "tokens=*" %%i in ('pg_config --includedir-server') do set PG_INCLUDE_SERVER=%%i
for /f "tokens=*" %%i in ('pg_config --includedir') do set PG_INCLUDE=%%i
for /f "tokens=*" %%i in ('pg_config --libdir') do set PG_LIB=%%i
for /f "tokens=*" %%i in ('pg_config --pkglibdir') do set PG_PKGLIB=%%i
for /f "tokens=*" %%i in ('pg_config --sharedir') do set PG_SHARE=%%i

echo PostgreSQL include server: %PG_INCLUDE_SERVER%
echo PostgreSQL lib: %PG_LIB%
echo.

REM Clean previous build
echo Cleaning previous build...
del pg_fasttransfer_safe.obj >nul 2>&1
del pg_fasttransfer.dll >nul 2>&1
del pg_fasttransfer.lib >nul 2>&1
del pg_fasttransfer.exp >nul 2>&1

REM Compile the safe object file
echo Compiling pg_fasttransfer_safe.c...
cl /c /MD /O2 /W1 /nologo ^
   /I. ^
   /I"%PG_INCLUDE_SERVER%" ^
   /I"%PG_INCLUDE%" ^
   /DWIN32 /D_WINDOWS /D_WIN32_WINNT=0x0600 ^
   /DBUILDING_DLL /D_CRT_SECURE_NO_WARNINGS ^
   /wd4005 /wd4996 ^
   pg_fasttransfer_safe.c

if %errorlevel% neq 0 (
    echo ERROR: Compilation failed!
    pause
    exit /b 1
)

REM Link the DLL - only with postgres.lib, no forcing unresolved symbols
echo Linking pg_fasttransfer.dll...
link /DLL /OUT:pg_fasttransfer.dll /nologo ^
     /LIBPATH:"%PG_LIB%" ^
     pg_fasttransfer_safe.obj ^
     postgres.lib ^
     ws2_32.lib kernel32.lib user32.lib advapi32.lib

if %errorlevel% neq 0 (
    echo ERROR: Linking failed!
    pause
    exit /b 1
)

REM Install files
echo Installing extension files...
copy pg_fasttransfer.dll "%PG_PKGLIB%\"
copy pg_fasttransfer_safe.control "%PG_SHARE%\extension\"
copy pg_fasttransfer_safe--1.0.sql "%PG_SHARE%\extension\"

echo.
echo ============================================
echo  Safe build completed successfully!
echo ============================================
echo.
echo This version provides a minimal test function.
echo After restarting PostgreSQL, you can test with:
echo CREATE EXTENSION pg_fasttransfer_safe;
echo SELECT * FROM pg_fasttransfer_safe();
echo.
pause