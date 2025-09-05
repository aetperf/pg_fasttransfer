@echo off
echo Setting up PostgreSQL 17.4 build environment...

if exist pgsql-build rd /s /q pgsql-build
curl -L -o pgsql.zip https://get.enterprisedb.com/postgresql/postgresql-17.4-1-windows-x64-binaries.zip
mkdir pgsql-build
tar -xf pgsql.zip -C pgsql-build --strip-components=1
del pgsql.zip

echo Build environment ready in pgsql-build\
pause