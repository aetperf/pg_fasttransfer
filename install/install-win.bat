@echo off

setlocal EnableDelayedExpansion

:: Define the extension file names
set "DLL_FILE=pg_fasttransfer.dll"
set "CONTROL_FILE=pg_fasttransfer.control"
set "SQL_FILE=pg_fasttransfer--1.0.sql"

:: Welcome message and instructions
echo.
echo ==========================================================
echo           pg_fasttransfer Installation Wizard
echo ==========================================================
echo.
echo This script will automatically detect your PostgreSQL installation.
echo.

:find_pg
echo.
echo Searching for PostgreSQL installation in common directories...
echo.

set "PG_BIN_PATH="
set "count=0"
for /d %%A in ("C:\Program Files\PostgreSQL\*") do (
    if exist "%%A\bin\pg_config.exe" (
        set /a count+=1
        set "pg_paths[!count!]=%%A"
    )
)
for /d %%A in ("C:\Program Files (x86)\PostgreSQL\*") do (
    if exist "%%A\bin\pg_config.exe" (
        set /a count+=1
        set "pg_paths[!count!]=%%A"
    )
)

if %count% equ 0 (
    goto prompt_for_path
)

if %count% equ 1 (
    echo Only one instance found. Using this path.
    set "pg_dir=!pg_paths[1]!"
    goto found_pg_path
)

echo Multiple instances found. Please select one:
echo.
for /L %%i in (1,1,%count%) do (
    echo [%%i] !pg_paths[%%i]!
)
set /p "choice=Enter the number of the instance to use: "

if not defined pg_paths[%choice%] (
    echo.
    echo Invalid choice. Please try again.
    goto find_pg
)
set "pg_dir=!pg_paths[%choice%]!"
goto found_pg_path

:prompt_for_path
echo No PostgreSQL installation found in standard locations.
echo.
echo Please manually enter the path to the PostgreSQL BIN directory
echo (e.g., C:\Program Files\PostgreSQL\16\bin).
echo.
set /p "PG_BIN_PATH=PostgreSQL BIN path: "
set "pg_dir=!PG_BIN_PATH:\bin=!"

:found_pg_path
if not exist "%pg_dir%\bin\pg_config.exe" (
    echo.
    echo Error: The specified path does not appear to be a valid BIN directory.
    echo.
    goto prompt_for_path
)

echo Valid installation path found: %pg_dir%

:check_files_exist
echo.
echo Checking for existing extension files...
echo.

set "files_exist=false"
if exist "%pg_dir%\lib\%DLL_FILE%" (
    if exist "%pg_dir%\share\extension\%CONTROL_FILE%" (
        if exist "%pg_dir%\share\extension\%SQL_FILE%" (
            set "files_exist=true"
        )
    )
)

if "%files_exist%"=="true" (
    echo Existing files found. This will be an update.
) else (
    echo No existing files found. This will be a new installation.
)
goto check_service

:check_service
echo.
echo Verifying PostgreSQL service status...
echo.

:: Get PostgreSQL version from pg_config.exe
for /f "tokens=2" %%v in ('"%pg_dir%\bin\pg_config.exe" --version') do set "pg_version_full=%%v"

:: Extract the major version (e.g., 17 from 17.5)
for /f "tokens=1 delims=." %%a in ("%pg_version_full%") do set "pg_version=%%a"

:: The service name is typically postgresql-x64-<major_version>
set "service_name=postgresql-x64-%pg_version%"
echo Checking service: %service_name%

sc query "%service_name%" | findstr /I "RUNNING" > nul
if %errorlevel% equ 0 (
    if "%files_exist%"=="true" (
        echo.
        echo ==================== INSTALLATION FAILED ====================
        echo.
        echo The PostgreSQL service '%service_name%' is currently running AND
        echo one or more extension files already exist.
        echo To avoid corruption and installation errors, you MUST stop
        echo the PostgreSQL service.
        echo.
        echo Action required:
        echo 1. Stop the PostgreSQL service manually from the Services control panel.
        echo 2. Rerun this script as an administrator.
        goto :end
    ) else (
        echo Service is running, but no existing files were found.
        echo Proceeding with new file installation.
    )
) else (
    echo Service is stopped. Proceeding with file copy.
)
goto copy_files

:copy_files
echo.
echo Copying files...
echo.

set "lib_path=%pg_dir%\lib"
set "share_path=%pg_dir%\share\extension"

xcopy "%DLL_FILE%" "%lib_path%" /y
xcopy "%CONTROL_FILE%" "%share_path%" /y
xcopy "%SQL_FILE%" "%share_path%" /y

if %errorlevel% neq 0 (
    echo.
    echo ==================== INSTALLATION FAILED ====================
    echo.
    echo An installation error occurred. This is likely because the PostgreSQL
    echo service was not stopped and the files are in use, or permissions are
    echo not sufficient.
    echo.
    echo Action required:
    echo 1. Stop the PostgreSQL service.
    echo 2. Rerun this script as an administrator.
    goto :end
)

echo.
echo ==========================================================
echo             Installation completed successfully!
echo ==========================================================
echo.
echo To finalize the installation, you must now:
echo 1. Restart your PostgreSQL service.

if "%files_exist%"=="true" (
    echo 2. The extension files were updated. To apply the changes, you must
    echo    first drop the old extension, then recreate it in psql:
    echo    DROP EXTENSION pg_fasttransfer;
    echo    CREATE EXTENSION pg_fasttransfer CASCADE;
) else (
    echo 2. Execute the following command in psql:
    echo    CREATE EXTENSION pg_fasttransfer CASCADE;
)
echo.

:end
pause
endlocal
