@echo off
REM Build script for pg_fasttransfer extension on Windows

echo.
echo ============================================
echo   Windows Build for pg_fasttransfer
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
del pg_fasttransfer.obj >nul 2>&1
del pg_fasttransfer.dll >nul 2>&1
del pg_fasttransfer.lib >nul 2>&1
del pg_fasttransfer.exp >nul 2>&1

REM Compile the object file
echo Compiling pg_fasttransfer.c...
cl /c /MD /O2 /W1 /nologo ^
    /I. ^
    /I"%PG_INCLUDE_SERVER%" ^
    /I"%PG_INCLUDE%" ^
    /DWIN32 /D_WINDOWS /D_WIN32_WINNT=0x0600 ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /wd4005 /wd4996 ^
    pg_fasttransfer.c

if %errorlevel% neq 0 (
    echo ERROR: Compilation failed!
    pause
    exit /b 1
)

REM Link the DLL
echo Linking pg_fasttransfer.dll...
link /DLL /OUT:pg_fasttransfer.dll /nologo ^
    /LIBPATH:"%PG_LIB%" ^
    pg_fasttransfer.obj ^
    postgres.lib ^
    ws2_32.lib kernel32.lib user32.lib advapi32.lib

if %errorlevel% neq 0 (
    echo ERROR: Linking failed!
    pause
    exit /b 1
)

REM Install files
echo Installing extension files...
copy pg_fasttransfer.dll "%PG_PKGLIB%\" >nul
copy pg_fasttransfer.control "%PG_SHARE%\extension\" >nul
copy pg_fasttransfer--1.0.sql "%PG_SHARE%\extension\" >nul

echo.
echo ============================================
echo   Build completed successfully!
echo ============================================
echo.
echo After restarting PostgreSQL, you can test with:
echo CREATE EXTENSION pg_fasttransfer;
echo SELECT * FROM pg_fasttransfer();
echo.
pause
