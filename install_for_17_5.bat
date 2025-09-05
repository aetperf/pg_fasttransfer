@echo off
echo Installing to PostgreSQL 17.5...

for /f "tokens=*" %%i in ('pg_config --pkglibdir') do set PGLIB=%%i
for /f "tokens=*" %%i in ('pg_config --sharedir') do set PGSHARE=%%i

copy pg_fasttransfer.dll "%PGLIB%\" /Y
copy pg_fasttransfer.control "%PGSHARE%\extension\" /Y
copy pg_fasttransfer--1.0.sql "%PGSHARE%\extension\" /Y

echo Installed to PostgreSQL 17.5
echo Restart PostgreSQL and run: CREATE EXTENSION pg_fasttransfer;
pause