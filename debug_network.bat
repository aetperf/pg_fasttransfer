@echo off
REM Debug script to test network connectivity from PostgreSQL context

echo.
echo ============================================
echo  Network Connectivity Debug
echo ============================================
echo.

echo Current user context:
whoami
echo.

echo Network configuration:
ipconfig /all | findstr /i "DNS"
echo.

echo Testing DNS resolution:
nslookup localhost
echo.

echo Testing basic connectivity:
ping localhost -n 2
echo.

echo PostgreSQL service user:
sc qc postgresql-x64-17 | findstr SERVICE_START_NAME
echo.

echo Current working directory:
cd
echo.

echo Environment variables:
set | findstr /i "path\|user\|computername"
echo.

pause