// Safer Windows version that avoids problematic PostgreSQL memory functions
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

#define popen _popen
#define pclose _pclose
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "executor/spi.h"
#include "funcapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define BINARY_NAME "FastTransfer.exe"
#else
#define BINARY_NAME "FastTransfer"
#endif

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pg_fasttransfer_safe);

Datum
pg_fasttransfer_safe(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc;
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    HeapTuple tuple;
    
    // Static variables to hold results across calls
    static int exit_code = 0;
    static char result_buffer[65536]; // Large static buffer instead of StringInfo
    static long total_rows = -1;
    static int total_columns = -1;
    static long transfer_time = -1;
    static long total_time = -1;
    
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errmsg("The function should return a record")));
    
    // Build command and execute FastTransfer
    char command[8192];
    char binary_path[1024];
    const char *arg_names[] = {
        "--sourceconnectiontype", "--sourceconnectstring", "--sourcedsn", "--sourceprovider", "--sourceserver",
        "--sourceuser", "--sourcepassword", "--sourcetrusted", "--sourcedatabase", "--sourceschema",
        "--sourcetable", "--query", "--fileinput", "--targetconnectiontype", "--targetconnectstring",
        "--targetserver", "--targetuser", "--targetpassword", "--targettrusted", "--targetdatabase",
        "--targetschema", "--targettable", "--degree", "--method", "--distributekeycolumn",
        "--datadrivenquery", "--loadmode", "--batchsize", "--useworktables", "--runid",
        "--settingsfile", "--mapmethod", "--license"
    };
    const char *bool_params[] = {
        "--sourcetrusted", "--targettrusted", "--useworktables"
    };
    const char *int_params[] = {
        "--degree", "--batchsize"
    };
    
    FILE *fp;
    char buffer[1024];
    const char *env_path = getenv("FASTTRANSFER_PATH");
    const char *pg_path;
    char numbuf[34];
    int i, b, x;
    bool is_bool, is_int;
    const char *val = NULL;
    int status;
    
    // Initialize result buffer
    result_buffer[0] = '\0';
    
    // Build binary path
    if (env_path && strlen(env_path) > 0) {
#ifdef _WIN32
        snprintf(binary_path, sizeof(binary_path), "%s\\%s", env_path, BINARY_NAME);
#else
        snprintf(binary_path, sizeof(binary_path), "%s/%s", env_path, BINARY_NAME);
#endif
    } else if (!PG_ARGISNULL(33)) {
        pg_path = text_to_cstring(PG_GETARG_TEXT_PP(33));
#ifdef _WIN32
        snprintf(binary_path, sizeof(binary_path), "%s\\%s", pg_path, BINARY_NAME);
#else
        snprintf(binary_path, sizeof(binary_path), "%s/%s", pg_path, BINARY_NAME);
#endif
    } else {
#ifdef _WIN32
        snprintf(binary_path, sizeof(binary_path), ".\\%s", BINARY_NAME);
#else
        snprintf(binary_path, sizeof(binary_path), "./%s", BINARY_NAME);
#endif
    }
    
    // Build command
    snprintf(command, sizeof(command), "\"%s\"", binary_path);
    
    for (i = 0; i < 33; i++) {
        if (PG_ARGISNULL(i)) continue;
        
        is_bool = false;
        for (b = 0; b < sizeof(bool_params) / sizeof(char *); b++) {
            if (strcmp(arg_names[i], bool_params[b]) == 0) {
                is_bool = true;
                break;
            }
        }
        
        if (is_bool) {
            if (PG_GETARG_BOOL(i)) {
                strncat(command, " ", sizeof(command) - strlen(command) - 1);
                strncat(command, arg_names[i], sizeof(command) - strlen(command) - 1);
            }
            continue;
        }
        
        is_int = false;
        for (x = 0; x < sizeof(int_params) / sizeof(char *); x++) {
            if (strcmp(arg_names[i], int_params[x]) == 0) {
                snprintf(numbuf, sizeof(numbuf), "%d", PG_GETARG_INT32(i));
                val = numbuf;
                is_int = true;
                break;
            }
        }
        
        if (!is_int) {
            val = text_to_cstring(PG_GETARG_TEXT_PP(i));
        }
        
        if (val && strlen(val) > 0) {
            strncat(command, " ", sizeof(command) - strlen(command) - 1);
            strncat(command, arg_names[i], sizeof(command) - strlen(command) - 1);
            strncat(command, " \"", sizeof(command) - strlen(command) - 1);
            strncat(command, val, sizeof(command) - strlen(command) - 1);
            strncat(command, "\"", sizeof(command) - strlen(command) - 1);
        }
    }
    
    strncat(command, " 2>&1", sizeof(command) - strlen(command) - 1);
    
    // Execute command
    fp = popen(command, "r");
    if (!fp) {
        strncpy(result_buffer, "Error: unable to execute FastTransfer.\n", sizeof(result_buffer) - 1);
        exit_code = -1;
    } else {
        size_t result_len = 0;
        while (fgets(buffer, sizeof(buffer), fp) != NULL && result_len < sizeof(result_buffer) - 1) {
            size_t buf_len = strlen(buffer);
            if (result_len + buf_len < sizeof(result_buffer) - 1) {
                strcat(result_buffer, buffer);
                result_len += buf_len;
            }
        }
        status = pclose(fp);
        
        // Parse results
        if (result_buffer[0] != '\0') {
            char *token;
            
            // Search for "Total rows : "
            token = strstr(result_buffer, "Total rows : ");
            if (token != NULL) {
                total_rows = strtol(token + strlen("Total rows : "), NULL, 10);
            }
            
            // Search for "Total columns : "
            token = strstr(result_buffer, "Total columns : ");
            if (token != NULL) {
                total_columns = strtol(token + strlen("Total columns : "), NULL, 10);
            }
            
            // Search for "Transfer time : Elapsed"
            token = strstr(result_buffer, "Transfert time : Elapsed");
            if (token != NULL) {
                transfer_time = strtol(token + strlen("Transfert time : Elapsed="), NULL, 10);
            }
            
            // Search for "Total time : Elapsed="
            token = strstr(result_buffer, "Total time : Elapsed=");
            if (token != NULL) {
                total_time = strtol(token + strlen("Total time : Elapsed="), NULL, 10);
            }
        }
        
#ifdef _WIN32
        exit_code = status;
#else
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
        } else {
            exit_code = -2;
            strncat(result_buffer, "\nUnknown error of FastTransfer\n", sizeof(result_buffer) - strlen(result_buffer) - 1);
        }
#endif
    }
    
    // Return results
    values[0] = Int32GetDatum(exit_code);
    values[1] = CStringGetTextDatum(result_buffer);
    values[2] = Int64GetDatum(total_rows);
    values[3] = Int32GetDatum(total_columns);
    values[4] = Int64GetDatum(transfer_time);
    values[5] = Int64GetDatum(total_time);
    
    tuple = heap_form_tuple(tupdesc, values, nulls);
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}