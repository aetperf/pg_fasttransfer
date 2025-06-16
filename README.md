# Installation and Update of pg_fasttransfer Extension

## Prerequisites
- PostgreSQL installed with development headers
- Administrator rights (sudo)
- C compiler (gcc or clang)
- Make

## Clean Installation

### Step 1: Stop PostgreSQL
```bash
sudo systemctl stop postgresql
```
**Reason:** Avoids conflicts with files in use and cache artifacts.

### Step 2: Clean Existing Files
```bash
sudo rm -f /usr/lib/postgresql/*/lib/pg_fasttransfer.so
sudo rm -f /usr/share/postgresql/*/extension/pg_fasttransfer*
```
**Reason:** Completely removes the old version to avoid artifact conflicts.

### Step 3: Compilation and Installation
```bash
make clean && make && sudo make install
```
**Command details:**
- `make clean`: Cleans object files
- `make`: Compiles the extension 
- `sudo make install`: Installs into PostgreSQL directories

### Step 4: Restart PostgreSQL
```bash
sudo systemctl start postgresql
```

## SQL Configuration

### Remove Old Extension
```sql
DROP EXTENSION pg_fasttransfer;
```
**Note:** If error occurs, use `DROP EXTENSION pg_fasttransfer CASCADE;`

### Create New Extension
```sql
CREATE EXTENSION pg_fasttransfer;
```

## Return Structure

The function now returns 4 columns:
- `exit_code` (integer): FastTransfer exit code
- `output` (text): Complete logs
- `total_rows` (bigint): Number of transferred rows (-1 if not found)
- `total_columns` (int): Number of transferred columns (-1 if not found)
- `transfer_time_ms` (integer): Transfer time in milliseconds (-1 if not found)
- `total_time_ms` (integer): Total time in milliseconds (-1 if not found)