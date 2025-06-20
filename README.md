# pg_fasttransfer

A PostgreSQL extension to run the [FastTransfer](https://www.arpe.io/fasttransfer/?v=82a9e4d26595) tool from an SQL function, enabling fast data transfer between databases.

---

## Prerequisites

- PostgreSQL installed with development headers
- Administrator privileges (sudo on Linux, appropriate rights on Windows)

### Linux

* `gcc` or `clang` (C compiler)
* `make`


---

### Windows

You need to install the **C/C++ compiler** and required build tools using **Build Tools for Visual Studio 2022**.

#### Required Workload

* **C++ build tools** (main workload)

#### Individual Components to Select

| Component Group      | Specific Items to Select                                                  | Description                                     |
| -------------------- | ------------------------------------------------------------------------- | ----------------------------------------------- |
| **MSVC Compiler**    | ✅ *MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)*                  | The core C++ compiler                           |
| **Windows SDK**      | ✅ *Windows 10 SDK (10.0.19041.0)* or *Windows 11 SDK*                     | Headers and libraries                           |
| **Additional Tools** | ✅ *CMake tools for Visual Studio*<br>✅ *MSBuild support for LLVM toolset* | Optional, but helpful for broader compatibility |

#### Installation Steps

1. Download **[Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/)** from the official Microsoft website.
2. Launch the installer.
3. Select the **C++ build tools** workload.
4. On the right-hand panel, check the individual components listed above.
5. Install.

---

## Installation

### Linux

1. **Stop PostgreSQL**

```bash
sudo systemctl stop postgresql
```

2. **Remove any existing installation**

```bash
sudo rm -f /usr/lib/postgresql/*/lib/pg_fasttransfer.so
sudo rm -f /usr/share/postgresql/*/extension/pg_fasttransfer*
```

3. **Build and install**

```bash
make clean && make && sudo make install
```

4. **Restart PostgreSQL**

```bash
sudo systemctl start postgresql
```

---

### Windows

1. Open **x64 Native Tools Command Prompt for VS 2022** (or equivalent).

2. Ensure `pg_config` is available in your `PATH`.

3. Run the build script in the project directory:

```batch
build_for_windows.bat
```

This script will:

* Verify required tools (compiler, pg\_config)
* Compile and link the extension DLL
* Copy the extension files to the appropriate PostgreSQL directories

4. Restart the PostgreSQL service.

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

## Main Function Usage

### Linux exemple
```sql
SELECT * FROM pg_fasttransfer(
    targetconnectiontype := 'msbulk',
    sourceconnectiontype := 'pgsql',
    sourceserver := 'localhost:5432',
    sourceuser := 'pytabextract_pguser',
    sourcepassword := '*****',
    sourcedatabase := 'tpch',
    sourceschema := 'tpch_1',
    sourcetable := 'orders',
    targetserver := 'localhost,1433',
    targetuser := 'migadmin',
    targetpassword := '*****',
    targetdatabase := 'target_db',
    targetschema := 'tpch_1',
    targettable := 'orders',
    loadmode := 'Truncate',
    license := '/tmp/FastTransfer_linux-x64_v0.13.5/FastTransfer.lic',
    fasttransfer_path := '/tmp/FastTransfer_linux-x64_v0.13.5'
);
```

### Windows exemple
```sql
SELECT * from pg_fasttransfer(
  sourceconnectiontype := 'mssql',
  sourceserver := 'localhost',
  sourcepassword := '******',
  sourceuser := 'FastLogin',
  sourcedatabase := 'tpch10',
  sourceschema := 'dbo',
  sourcetable := 'orders',
  targetconnectiontype := 'pgcopy',
  targetserver := 'localhost:15433',
  targetuser := 'postgres',
  targetpassword := '*******',
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

| Column             | Type    | Description                                     |
| ------------------ | ------- | ----------------------------------------------- |
| exit\_code         | integer | FastTransfer process exit code                  |
| output             | text    | Full FastTransfer logs                          |
| total\_rows        | bigint  | Total rows transferred (-1 if not found)        |
| total\_columns     | integer | Total columns transferred (-1 if not found)     |
| transfer\_time\_ms | bigint  | Transfer time in milliseconds (-1 if not found) |
| total\_time\_ms    | bigint  | Total time in milliseconds (-1 if not found)    |

---

## Notes

* On Windows, the `build_for_windows.bat` script must be run from a **Developer Command Prompt for Visual Studio** where the compiler is available.
* The extension uses `pg_config` to locate PostgreSQL paths.
* The FastTransfer binary should be accessible via `fasttransfer_path` argument to the function.
* `.control` and `.sql` files are installed automatically via the Makefile or Windows build script.


