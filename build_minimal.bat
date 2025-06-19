@echo off
REM Minimal build script using the simplified source file
REM This avoids PostgreSQL headers that conflict with Windows

echo.
echo ============================================
echo  Minimal Windows Build for pg_fasttransfer
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
echo PostgreSQL include: %PG_INCLUDE%
echo PostgreSQL lib: %PG_LIB%
echo PostgreSQL pkg lib: %PG_PKGLIB%
echo PostgreSQL share: %PG_SHARE%
echo.

REM Clean previous build
echo Cleaning previous build...
del pg_fasttransfer_win.obj >nul 2>&1
del pg_fasttransfer.dll >nul 2>&1
del pg_fasttransfer.lib >nul 2>&1
del pg_fasttransfer.exp >nul 2>&1

REM Compile the Windows-specific object file
echo Compiling pg_fasttransfer_win.c...
cl /c /MD /O2 /W1 /nologo ^
   /I"%PG_INCLUDE_SERVER%" ^
   /I"%PG_INCLUDE%" ^
   /DWIN32 /D_WINDOWS /D_WIN32_WINNT=0x0600 ^
   /DBUILDING_DLL /D_CRT_SECURE_NO_WARNINGS ^
   /wd4005 /wd4996 ^
   pg_fasttransfer_win.c

if %errorlevel% neq 0 (
    echo ERROR: Compilation failed!
    echo.
    echo Try using the minimal version which avoids problematic headers
    pause
    exit /b 1
)

REM Link the DLL
echo Linking pg_fasttransfer.dll...
link /DLL /OUT:pg_fasttransfer.dll /nologo ^
     /LIBPATH:"%PG_LIB%" ^
     pg_fasttransfer_win.obj ^
     postgres.lib ws2_32.lib kernel32.lib user32.lib advapi32.lib

if %errorlevel% neq 0 (
    echo ERROR: Linking failed!
    echo.
    echo Make sure postgres.lib exists in %PG_LIB%
    pause
    exit /b 1
)

REM Install files
echo Installing extension files...
copy pg_fasttransfer.dll "%PG_PKGLIB%\"
if %errorlevel% neq 0 (
    echo ERROR: Failed to copy DLL to %PG_PKGLIB%
    echo Make sure you have write permissions or run as Administrator
    pause
    exit /b 1
)

copy pg_fasttransfer.control "%PG_SHARE%\extension\"
if %errorlevel% neq 0 (
    echo ERROR: Failed to copy control file
    pause
    exit /b 1
)

copy pg_fasttransfer--1.0.sql "%PG_SHARE%\extension\"
if %errorlevel% neq 0 (
    echo ERROR: Failed to copy SQL file
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Minimal build completed successfully!
echo ============================================
echo.
echo Files installed:
echo - %PG_PKGLIB%\pg_fasttransfer.dll
echo - %PG_SHARE%\extension\pg_fasttransfer.control
echo - %PG_SHARE%\extension\pg_fasttransfer--1.0.sql
echo.
echo Next steps:
echo 1. Restart PostgreSQL service
echo 2. Connect to your database  
echo 3. Run: CREATE EXTENSION pg_fasttransfer;
echo.
pause