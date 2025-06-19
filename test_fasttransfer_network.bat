@echo off
REM Test script to debug FastTransfer network connectivity issues

echo.
echo ============================================
echo  FastTransfer Network Connectivity Test
echo ============================================
echo.

REM Test 1: Basic FastTransfer execution
echo Test 1: Basic FastTransfer execution
echo Command: FastTransfer.exe --help
FastTransfer.exe --help
echo Exit code: %errorlevel%
echo.

REM Test 2: Network connectivity from command line
echo Test 2: Testing network connectivity
echo Command: nslookup localhost
nslookup localhost
echo.

REM Test 3: Test with explicit localhost connection
echo Test 3: FastTransfer with localhost (if available)
echo This will likely fail but shows the error:
FastTransfer.exe --sourceserver localhost --targetserver localhost
echo Exit code: %errorlevel%
echo.

REM Test 4: Check if running as service vs interactive
echo Test 4: Current execution context
echo User: %USERNAME%
echo Computer: %COMPUTERNAME%
echo Current directory: %CD%
echo.

REM Test 5: Environment variables that might affect networking
echo Test 5: Network-related environment variables
set | findstr /i "proxy\|dns\|network"
echo.

echo ============================================
echo  Recommended troubleshooting steps:
echo ============================================
echo.
echo 1. Verify PostgreSQL service user has network access
echo 2. Check if localhost resolves to 127.0.0.1
echo 3. Test FastTransfer.exe manually with same parameters
echo 4. Consider running PostgreSQL as LocalSystem for testing
echo.
pause