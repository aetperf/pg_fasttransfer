@echo off
REM Windows build script for pg_fasttransfer extension
REM Run this from Developer Command Prompt for Visual Studio

echo.
echo ============================================
echo  Building pg_fasttransfer for Windows
echo ============================================
echo.

REM Check if we're in a Developer Command Prompt
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Microsoft C compiler not found!
    echo Please run this script from "Developer Command Prompt for VS"
    echo.
    pause
    exit /b 1
)

REM Check if pg_config is available
where pg_config >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: pg_config not found!
    echo Please ensure PostgreSQL bin directory is in your PATH
    echo Example: set PATH=%%PATH%%;C:\Program Files\PostgreSQL\15\bin
    echo.
    pause
    exit /b 1
)

echo Found PostgreSQL version:
pg_config --version
echo.

REM Clean previous build
echo Cleaning previous build...
nmake /f Makefile.win clean >nul 2>&1
del *.obj *.dll *.lib *.exp *.pdb >nul 2>&1

echo Building extension...
nmake /f Makefile.win
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    echo Check the error messages above.
    pause
    exit /b 1
)

echo.
echo Installing extension...
nmake /f Makefile.win install
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Installation failed!
    echo Make sure you're running as Administrator if needed.
    pause
    exit /b 1
)

echo.
echo ============================================
echo  Build completed successfully!
echo ============================================
echo.
echo Next steps:
echo 1. Restart PostgreSQL service
echo 2. Connect to your database
echo 3. Run: CREATE EXTENSION pg_fasttransfer;
echo.
pause