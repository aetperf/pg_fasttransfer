# pgfasttransfer

`pgfasttransfer` is a PostgreSQL extension designed to transfer data quickly and efficiently.

## Prerequisites

Before compiling the extension, you need to have PostgreSQL and the necessary development tools installed.

### Dependencies
- PostgreSQL (with `pg_config` available)
- `make`
- A C compiler (such as `gcc` or `clang`)

## Compilation and Installation

1. **Clone the repository** (if not already done):
   
   ```bash
   git clone https://github.com/aetperf/pg_fasttransfer.git
   cd pg_fasttransfer
   ```


2. **Compile the extension**:

Use `make` to compile the extension using the provided `Makefile`. This will build the PostgreSQL module from the source code.
```bash
make
```

3. **Install the extension into PostgreSQL**:

Once the compilation is complete, use `make install` to install the extension into your PostgreSQL installation:
```bash
sudo make install
```

4. **Enable the extension in PostgreSQL**:

Connect to PostgreSQL and run the following SQL command to enable the extension in your database:
```sql
CREATE EXTENSION pgfasttransfer;
```

## Troubleshooting
If you encounter issues during compilation or installation, ensure that PostgreSQL is installed and that `pg_config` is available in your PATH.

You can check if PostgreSQL is correctly installed by running the following command:
```bash
pg_config --version
```

If this command fails, you will need to install PostgreSQL or fix your environment configuration.

## Uninstallation
To uninstall the extension, run the following command in PostgreSQL:

```sql
DROP EXTENSION pgfasttransfer;
```
Then, you can remove any generated files with `make clean`:

```bash
make clean
```

