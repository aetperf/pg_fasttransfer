# Windows Installation Guide for pg_fasttransfer Extension

## Prerequisites

### 1. Install PostgreSQL for Windows
- Download PostgreSQL from [postgresql.org](https://www.postgresql.org/download/windows/)
- **Important**: Choose the installer that includes development headers (EnterpriseDB installer recommended)
- During installation:
  - Note the installation path (typically `C:\Program Files\PostgreSQL\[version]`)
  - Remember the superuser password
  - Keep default port (5432)

### 2. Install Visual Studio Build Tools
- Download **Visual Studio Community 2022** or **Build Tools for Visual Studio 2022**
- During installation, select:
  - **C++ build tools**
  - **Windows 10/11 SDK** (latest version)
  - **MSVC v143 compiler toolset**

### 3. Install Git for Windows (if not already installed)
- Download from [git-scm.com](https://git-scm.com/download/win)
- Use default installation options

## Environment Setup

### 4. Set Environment Variables
Open **Command Prompt as Administrator** and run:
```cmd
setx PATH "%PATH%;C:\Program Files\PostgreSQL\15\bin" /M
setx PATH "%PATH%;C:\Program Files\PostgreSQL\15\lib" /M
```
*Replace `15` with your PostgreSQL version number*

### 5. Verify Installation
Open a **new Command Prompt** and verify:
```cmd
pg_config --version
pg_config --pgxs
```
Both commands should return valid paths and version information.

## Compilation Steps

### 6. Prepare Source Files
Copy the following files to your Windows build directory:
- `pg_fasttransfer_win.c` (Windows-optimized source)
- `Makefile.win` (Windows-specific Makefile)
- `pg_fasttransfer.control`
- `pg_fasttransfer--1.0.sql`

### 7. Open Developer Command Prompt
From Start menu, search for and open:
**"Developer Command Prompt for VS 2022"**

### 8. Navigate to Source Directory
```cmd
cd C:\path\to\your\pg_fasttransfer\source
```

### 9. Compile the Extension
Run the compilation commands:
```cmd
nmake /f Makefile.win clean
nmake /f Makefile.win
nmake /f Makefile.win install
```

## Installation and Configuration

### 10. Restart PostgreSQL Service
Open **Services** (services.msc) or use Command Prompt as Administrator:
```cmd
net stop postgresql-x64-15
net start postgresql-x64-15
```
*Replace `15` with your PostgreSQL version*

### 11. Connect to PostgreSQL and Install Extension
Open **pgAdmin** or use **psql**:
```sql
-- Connect to your target database
\c your_database_name

-- Create the extension
CREATE EXTENSION pg_fasttransfer;

-- Verify installation
\dx pg_fasttransfer
```

## Usage Configuration

### 12. Set FastTransfer Path
You have two options:

**Option A: Environment Variable (Recommended)**
```cmd
setx FASTTRANSFER_PATH "C:\path\to\FastTransfer\directory" /M
```

**Option B: Pass path in function call**
Use the `fasttransfer_path` parameter in your SQL calls.

### 13. Test the Extension
```sql
SELECT * FROM pg_fasttransfer(
    targetconnectiontype := 'msbulk',
    sourceconnectiontype := 'pgsql',
    sourceserver := 'localhost:5432',
    sourceuser := 'your_user',
    sourcepassword := 'your_password',
    sourcedatabase := 'source_db',
    sourceschema := 'public',
    sourcetable := 'test_table',
    targetserver := 'localhost,1433',
    targetuser := 'target_user',
    targetpassword := 'target_password',
    targetdatabase := 'target_db',
    targetschema := 'dbo',
    targettable := 'test_table',
    loadmode := 'Truncate',
    license := 'C:\path\to\FastTransfer.lic',
    fasttransfer_path := 'C:\path\to\FastTransfer\directory'
);
```

## Troubleshooting

### Common Issues and Solutions

**Error: "pg_config not found"**
- Ensure PostgreSQL bin directory is in PATH
- Restart Command Prompt after setting PATH
- Verify with: `where pg_config`

**Error: "MSVC compiler not found"**
- Use "Developer Command Prompt for VS" instead of regular Command Prompt
- Ensure Visual Studio Build Tools are properly installed

**Error: "Access denied during installation"**
- Run Developer Command Prompt as Administrator
- Check PostgreSQL service permissions

**Error: "Extension not found after installation"**
- Restart PostgreSQL service
- Check extension was installed in correct directory:
  ```cmd
  dir "C:\Program Files\PostgreSQL\15\lib\pg_fasttransfer.dll"
  dir "C:\Program Files\PostgreSQL\15\share\extension\pg_fasttransfer*"
  ```

**Error: "FastTransfer.exe not found"**
- Verify FASTTRANSFER_PATH environment variable
- Ensure FastTransfer.exe is in the specified directory
- Check file permissions

### Debug Information
To get detailed compilation information:
```cmd
nmake /f Makefile.win VERBOSE=1
```

### Clean Rebuild
If you encounter issues, perform a clean rebuild:
```cmd
nmake /f Makefile.win clean
del *.obj *.dll *.lib *.exp *.pdb 2>NUL
nmake /f Makefile.win
nmake /f Makefile.win install
```

## Windows-Specific Notes

1. **File Paths**: Use backslashes (`\`) or double backslashes (`\\`) in Windows paths
2. **Binary Name**: The extension looks for `FastTransfer.exe` on Windows (not `FastTransfer`)
3. **Permissions**: Some operations may require Administrator privileges
4. **Services**: Use Windows Service Manager or `net start/stop` commands instead of systemctl

## File Structure After Installation
```
C:\Program Files\PostgreSQL\15\
├── lib\
│   └── pg_fasttransfer.dll
└── share\extension\
    ├── pg_fasttransfer.control
    └── pg_fasttransfer--1.0.sql
```

## Support
If you encounter issues:
1. Check PostgreSQL and extension logs
2. Verify all prerequisites are installed
3. Ensure environment variables are set correctly
4. Try compiling with verbose output for detailed error messages