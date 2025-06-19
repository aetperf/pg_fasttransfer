@echo off
REM Installation helper script for pg_fasttransfer extension
REM This script helps with PostgreSQL service restart and extension creation

echo.
echo ============================================
echo  pg_fasttransfer Extension Installation
echo ============================================
echo.

REM Check if running as Administrator
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: Not running as Administrator
    echo Some operations may fail. Consider running as Administrator.
    echo.
)

REM Find PostgreSQL service
echo Looking for PostgreSQL service...
sc query type= service | findstr /i "postgresql" > temp_services.txt
if %errorlevel% neq 0 (
    echo ERROR: No PostgreSQL service found!
    echo Please ensure PostgreSQL is installed and running as a service.
    del temp_services.txt >nul 2>&1
    pause
    exit /b 1
)

REM Try to identify the service name
for /f "tokens=2" %%i in ('sc query type^= service ^| findstr /i "postgresql"') do (
    set PG_SERVICE=%%i
    goto :found_service
)

:found_service
if "%PG_SERVICE%"=="" (
    echo Could not automatically detect PostgreSQL service name.
    echo Common service names: postgresql-x64-15, postgresql-x64-14, postgresql-x64-13
    set /p PG_SERVICE=Enter PostgreSQL service name: 
)

echo Found PostgreSQL service: %PG_SERVICE%
echo.

REM Restart PostgreSQL service
echo Stopping PostgreSQL service...
net stop %PG_SERVICE%
if %errorlevel% neq 0 (
    echo WARNING: Failed to stop PostgreSQL service
    echo The service might not be running or you may lack permissions
)

echo Starting PostgreSQL service...
net start %PG_SERVICE%
if %errorlevel% neq 0 (
    echo ERROR: Failed to start PostgreSQL service!
    pause
    exit /b 1
)

echo PostgreSQL service restarted successfully.
echo.

REM Prompt for database connection
echo ============================================
echo  Database Connection Setup
echo ============================================
echo.
set /p DB_HOST=Enter PostgreSQL host (default: localhost): 
if "%DB_HOST%"=="" set DB_HOST=localhost

set /p DB_PORT=Enter PostgreSQL port (default: 5432): 
if "%DB_PORT%"=="" set DB_PORT=5432

set /p DB_USER=Enter PostgreSQL username (default: postgres): 
if "%DB_USER%"=="" set DB_USER=postgres

set /p DB_NAME=Enter database name: 
if "%DB_NAME%"=="" (
    echo ERROR: Database name is required!
    pause
    exit /b 1
)

echo.
echo Connecting to PostgreSQL to create extension...
echo Host: %DB_HOST%:%DB_PORT%
echo User: %DB_USER%
echo Database: %DB_NAME%
echo.

REM Create SQL commands file
echo DROP EXTENSION IF EXISTS pg_fasttransfer CASCADE; > temp_install.sql
echo CREATE EXTENSION pg_fasttransfer; >> temp_install.sql
echo \dx pg_fasttransfer >> temp_install.sql

REM Execute SQL commands
psql -h %DB_HOST% -p %DB_PORT% -U %DB_USER% -d %DB_NAME% -f temp_install.sql
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to create extension!
    echo Please check:
    echo - Database connection parameters
    echo - User permissions
    echo - Extension files are properly installed
    del temp_install.sql >nul 2>&1
    pause
    exit /b 1
)

REM Clean up
del temp_install.sql >nul 2>&1
del temp_services.txt >nul 2>&1

echo.
echo ============================================
echo  Installation completed successfully!
echo ============================================
echo.
echo The pg_fasttransfer extension is now installed and ready to use.
echo.
echo Example usage:
echo SELECT * FROM pg_fasttransfer(
echo     sourceconnectiontype := 'pgsql',
echo     targetconnectiontype := 'msbulk',
echo     sourceserver := 'localhost:5432',
echo     -- ... other parameters
echo );
echo.
pause