#!/bin/bash

#==============================================================================
#                             pg_fasttransfer Installation Wizard
#==============================================================================
# This script automatically detects your PostgreSQL installation and installs
# the pg_fasttransfer extension.

# Define the extension file names. The .dll is replaced by .so on Linux.
SO_FILE="pg_fasttransfer.so"
CONTROL_FILE="pg_fasttransfer.control"
SQL_FILE="pg_fasttransfer--1.0.sql"

echo ""
echo "=========================================================="
echo "        pg_fasttransfer Linux Installation Wizard"
echo "=========================================================="
echo ""
echo "This script will find your PostgreSQL installation and copy the necessary files."
echo "You will be asked for your password to use 'sudo' for file copying."
echo ""

# Function to find the pg_config executable
find_pg_config() {
    echo "Searching for pg_config..."
    # Check if pg_config is in the system's PATH
    if command -v pg_config &> /dev/null; then
        PG_CONFIG_PATH=$(command -v pg_config)
        echo "Found pg_config at: $PG_CONFIG_PATH"
    else
        echo "Error: pg_config was not found in your system's PATH."
        echo "Please provide the full path to your PostgreSQL 'bin' directory"
        echo "(e.g., /usr/lib/postgresql/16/bin):"
        read -p "PostgreSQL BIN path: " USER_BIN_PATH

        if [ -x "$USER_BIN_PATH/pg_config" ]; then
            PG_CONFIG_PATH="$USER_BIN_PATH/pg_config"
            echo "Using specified pg_config at: $PG_CONFIG_PATH"
        else
            echo "Error: The specified path does not contain a valid pg_config executable."
            exit 1
        fi
    fi
}

# Find the correct installation directories using pg_config
get_pg_paths() {
    echo ""
    echo "Determining PostgreSQL installation paths..."
    LIB_PATH=$("$PG_CONFIG_PATH" --pkglibdir)
    SHARE_PATH=$("$PG_CONFIG_PATH" --sharedir)/extension
    echo "Library directory: $LIB_PATH"
    echo "Extension directory: $SHARE_PATH"
}

# Function to check if the extension files already exist
check_existing_files() {
    echo ""
    echo "Checking for existing extension files..."
    FILES_EXIST=false
    if [ -f "$LIB_PATH/$SO_FILE" ] && [ -f "$SHARE_PATH/$CONTROL_FILE" ] && [ -f "$SHARE_PATH/$SQL_FILE" ]; then
        FILES_EXIST=true
    fi

    if [ "$FILES_EXIST" = true ]; then
        echo "Existing files found. This will be an update."
    else
        echo "No existing files found. This will be a new installation."
    fi
}

# Function to check for a running PostgreSQL service
check_service_status() {
    echo ""
    echo "Verifying PostgreSQL service status..."
    if pgrep -x "postgres" > /dev/null; then
        POSTGRES_RUNNING=true
        echo "A PostgreSQL process is currently running."
    else
        POSTGRES_RUNNING=false
        echo "No PostgreSQL process is running."
    fi
}

# --- Main Script Logic ---
find_pg_config
get_pg_paths
check_existing_files
check_service_status

# Safety check: if updating and service is running, abort.
if [ "$FILES_EXIST" = true ] && [ "$POSTGRES_RUNNING" = true ]; then
    echo ""
    echo "==================== INSTALLATION FAILED ===================="
    echo ""
    echo "The PostgreSQL service is currently running AND the extension files already exist."
    echo "To avoid corruption during an update, you MUST stop the service first."
    echo ""
    echo "Action required:"
    echo "1. Stop the PostgreSQL service (e.g., 'sudo systemctl stop postgresql')."
    echo "2. Rerun this script."
    exit 1
fi

# Copy the files using sudo
echo ""
echo "Copying files..."
sudo cp "$SO_FILE" "$LIB_PATH"
if [ $? -ne 0 ]; then
    echo "Error copying '$SO_FILE'. Check permissions."
    exit 1
fi

sudo cp "$CONTROL_FILE" "$SHARE_PATH"
if [ $? -ne 0 ]; then
    echo "Error copying '$CONTROL_FILE'. Check permissions."
    exit 1
fi

sudo cp "$SQL_FILE" "$SHARE_PATH"
if [ $? -ne 0 ]; then
    echo "Error copying '$SQL_FILE'. Check permissions."
    exit 1
fi

echo ""
echo "=========================================================="
echo "          Installation completed successfully!"
echo "=========================================================="
echo ""
echo "To finalize the installation:"
echo "1. Restart your PostgreSQL service (e.g., 'sudo systemctl restart postgresql')."
echo "2. Then, connect to psql and execute:"

if [ "$FILES_EXIST" = true ]; then
    echo "   DROP EXTENSION pg_fasttransfer;"
    echo "   CREATE EXTENSION pg_fasttransfer CASCADE;"
else
    echo "   CREATE EXTENSION pg_fasttransfer CASCADE;"
fi

echo ""
exit 0
