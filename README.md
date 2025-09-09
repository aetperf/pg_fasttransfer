
# pg_fasttransfer

A PostgreSQL extension to run the [FastTransfer](https://www.arpe.io/fasttransfer/?v=82a9e4d26595) tool from an SQL function, enabling fast data transfer between databases.

![Tuto](assets/pg_fasttransfer.gif)

---

## Table of Contents

* [FastTransfer Tool Requirement](#fasttransfer-tool-requirement)
* [Prerequisites](#prerequisites)
* [Installation](#installation)
  * [Windows](#windows)
  * [Linux](#linux)
* [SQL Setup](#sql-setup)
* [Function: pg_fasttransfer_encrypt](#function-pg_fasttransfer_encrypt)
* [Function: xp_RunFastTransfer_secure Usage](#function-xp_runfasttransfer_secure-usage)
* [Function Return Structure](#function-return-structure)
* [Notes](#notes)

---

## FastTransfer Tool Requirement

This extension requires the **FastTransfer** tool to be installed separately.

**Download FastTransfer and get a free trial license here:**
[https://www.arpe.io/get-your-fasttransfer-trial](https://www.arpe.io/get-your-fasttransfer-trial)

Once downloaded, extract the archive and provide the folder path using the `fasttransfer_path` parameter when calling the `xp_RunFastTransfer_secure` SQL function.

---

## Prerequisites

Before installing `pg_fasttransfer`, make sure you have:

* **FastTransfer tool**  
  Download, extract, and keep the path ready for `fasttransfer_path`.

* **Administrator privileges**  
  - On **Linux**: use `sudo` for installation/removal steps.  
  - On **Windows**: run scripts with *Run as administrator*.

* **PostgreSQL with development headers**  
  Ensure `pg_config` is available in your PATH.

---

## Installation

### Windows

There are two methods to install `pg_fasttransfer` on Windows:

#### Automated Installation

1. Extract the contents of the ZIP file. It should contain:
```

pg\_fasttransfer.dll
pg\_fasttransfer.control
pg\_fasttransfer--1.0.sql
install-win.bat

```
2. Right-click on `install-win.bat` → **Run as administrator**.  
3. The script will:
- Detect your PostgreSQL installation path  
- Copy the `.dll`, `.control`, and `.sql` files to the correct directories  
4. Restart the PostgreSQL service.

---

#### Manual Installation

If you prefer manual installation or if the script fails:

1. **Stop PostgreSQL service**  
Open *Services* and stop `postgresql-x64-<version>`.

2. **Locate your PostgreSQL installation folder**  
Typically:  
```
C:\Program Files\PostgreSQL\<version>\\
```

3. **Copy files to appropriate directories**
- `pg_fasttransfer.dll` → `lib\`  
- `pg_fasttransfer.control` → `share\extension\`  
- `pg_fasttransfer--1.0.sql` → `share\extension\`

4. **Restart PostgreSQL service**  
Start the service again from *Services* or with:  
```powershell
net start postgresql-x64-<version>
```

---

## SQL Setup

### Drop existing extension (if any)

```sql
DROP EXTENSION IF EXISTS pg_fasttransfer CASCADE;
```

### Create the extension

```sql
CREATE EXTENSION pg_fasttransfer;
```

---

### Linux

*(Instructions will be added later — not yet implemented.)*

---

## Function: pg\_fasttransfer\_encrypt

This function encrypts a given text string using `pgp_sym_encrypt` and encodes the result in base64.
It is useful for storing sensitive information, such as passwords, in a secure manner within your SQL scripts or configuration.

The `xp_RunFastTransfer_secure` function will automatically decrypt any values passed to its `--sourcepassword` and `--targetpassword` arguments using the same encryption key.
**The encryption/decryption key is defined by the `PGFT_ENCRYPTION_KEY` variable in the C source file (`pg_fasttransfer.c`)**

**Syntax:**

```sql
pg_fasttransfer_encrypt(text_to_encrypt text) RETURNS text
```

**Example:**

```sql
SELECT pg_fasttransfer_encrypt('MySecurePassword');
-- Returns: A base64-encoded encrypted string, e.g., "PgP...base64encodedstring=="
```

---

## Function: xp\_RunFastTransfer\_secure Usage

This is the main function to execute the FastTransfer tool.
It takes various parameters to configure the data transfer operation.

Password arguments (`sourcepassword`, `targetpassword`) will be automatically decrypted 

**Syntax:**

```sql
xp_RunFastTransfer_secure(
sourceconnectiontype text DEFAULT NULL,
sourceconnectstring text DEFAULT NULL,
sourcedsn text DEFAULT NULL,
sourceprovider text DEFAULT NULL,
sourceserver text DEFAULT NULL,
sourceuser text DEFAULT NULL,
sourcepassword text DEFAULT NULL, 
sourcetrusted boolean DEFAULT FALSE,
sourcedatabase text DEFAULT NULL,
sourceschema text DEFAULT NULL,
sourcetable text DEFAULT NULL,
query text DEFAULT NULL,
fileinput text DEFAULT NULL,
targetconnectiontype text DEFAULT NULL,
targetconnectstring text DEFAULT NULL,
targetserver text DEFAULT NULL,
targetuser text DEFAULT NULL,
targetpassword text DEFAULT NULL, 
targettrusted boolean DEFAULT FALSE,
targetdatabase text DEFAULT NULL,
targetschema text DEFAULT NULL,
targettable text DEFAULT NULL,
degree integer DEFAULT NULL,
method text DEFAULT NULL,
distributekeycolumn text DEFAULT NULL,
datadrivenquery text DEFAULT NULL,
loadmode text DEFAULT NULL,
batchsize integer DEFAULT NULL,
useworktables boolean DEFAULT FALSE,
runid text DEFAULT NULL,
settingsfile text DEFAULT NULL,
mapmethod text DEFAULT NULL,
license text DEFAULT NULL,
fasttransfer_path text DEFAULT NULL
) RETURNS TABLE
```

---

### Linux example

```sql
SELECT * FROM xp_RunFastTransfer_secure(
targetconnectiontype := 'msbulk',
sourceconnectiontype := 'pgsql',
sourceserver := 'localhost:5432',
sourceuser := 'pytabextract_pguser',
sourcepassword := pg_fasttransfer_encrypt('MyActualPassword'), 
sourcedatabase := 'tpch',
sourceschema := 'tpch_1',
sourcetable := 'orders',
targetserver := 'localhost,1433',
targetuser := 'migadmin',
targetpassword := pg_fasttransfer_encrypt('AnotherSecurePassword'), 
targetdatabase := 'target_db',
targetschema := 'tpch_1',
targettable := 'orders',
loadmode := 'Truncate',
license := '/tmp/FastTransfer_linux-x64_v0.13.5/FastTransfer.lic',
fasttransfer_path := '/tmp/FastTransfer_linux-x64_v0.13.5'
);
```

---

### Windows example

```sql
SELECT * from xp_RunFastTransfer_secure(
sourceconnectiontype := 'mssql',
sourceserver := 'localhost',
sourcepassword := pg_fasttransfer_encrypt('MyWindowsPassword'),
sourceuser := 'FastLogin',
sourcedatabase := 'tpch10',
sourceschema := 'dbo',
sourcetable := 'orders',
targetconnectiontype := 'pgcopy',
targetserver := 'localhost:15433',
targetuser := 'postgres',
targetpassword := pg_fasttransfer_encrypt('MyPostgresPassword'), 
targetdatabase := 'postgres',
targetschema := 'public',
targettable := 'orders',
method := 'Ntile',
degree := 12,
distributekeycolumn := 'o_orderkey',
loadmode := 'Truncate',
batchsize := 1048576,
mapmethod := 'Position',
fasttransfer_path := 'D:\sources\FastTransfer'
);
```

---

## Function Return Structure

| **Column**         | **Type** | **Description**                                 |
| ------------------ | -------- | ----------------------------------------------- |
| exit\_code         | integer  | FastTransfer process exit code                  |
| output             | text     | Full FastTransfer logs                          |
| total\_rows        | bigint   | Total rows transferred (-1 if not found)        |
| total\_columns     | integer  | Total columns transferred (-1 if not found)     |
| transfer\_time\_ms | bigint   | Transfer time in milliseconds (-1 if not found) |
| total\_time\_ms    | bigint   | Total time in milliseconds (-1 if not found)    |

---

## Notes

* On Windows, the `build_for_windows.bat` script must be run from a **Developer Command Prompt for Visual Studio** where the compiler is available.
* The extension uses `pg_config` to locate PostgreSQL paths.
* `.control` and `.sql` files are installed automatically via the Makefile or Windows build script.
* **To change the encryption/decryption key, edit the `PGFT_ENCRYPTION_KEY` static variable in the `pg_fasttransfer.c` source file and recompile the extension.**

