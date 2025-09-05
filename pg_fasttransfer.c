/*
 * PostgreSQL 17.5 Windows MSVC Compatibility Fix
 * The duplicate case 4 error occurs because sizeof(Datum) == sizeof(int32) == 4
 */

#ifdef _WIN32
#ifdef _MSC_VER

// CRITICAL: Suppress the duplicate case value error in tupmacs.h
#pragma warning(push)
#pragma warning(disable: 2196)  // C2196: case value 'X' already used

// Force specific size values
#define SIZEOF_DATUM 4
#define SIZEOF_VOID_P 8

#endif // _MSC_VER

// Define PGDLLIMPORT before any includes
#define PGDLLIMPORT __declspec(dllimport)

// Prevent Windows.h from defining min/max macros that conflict
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Use lean Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Windows-specific includes - order matters!
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

// Windows compatibility defines
#define popen _popen
#define pclose _pclose
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

#endif // _WIN32

// NOW include PostgreSQL headers - with error suppression active
#include "postgres.h"

#ifdef _MSC_VER
#pragma warning(pop)  // Re-enable the warning after including postgres.h
#endif

#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "utils/rel.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

// Standard C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

// Binary name definition
#ifdef _WIN32
#define BINARY_NAME "FastTransfer.exe"
#else
#define BINARY_NAME "FastTransfer"
#endif

// Encryption key for secure password handling
static const char *PGFT_ENCRYPTION_KEY = "key";

//###########################################################################################
//## Decrypt C Function
//###########################################################################################

// --- PROTOTYPES DE FONCTIONS ---
char *decrypt_password(text *cipher_text, const char *key);

char *decrypt_password(text *cipher_text, const char *key) {
    const char *sql = "SELECT pgp_sym_decrypt(decode($1, 'base64'), $2)";
    Oid argtypes[2] = { TEXTOID, TEXTOID };
    Datum values[2] = {
        PointerGetDatum(cipher_text),
        CStringGetTextDatum(key)
    };

    char *decrypted = NULL;
    int ret;
    bool isnull;
    Datum result;
    text *txt;
    
    char *cipher_string = text_to_cstring(cipher_text);

    if (SPI_connect() != SPI_OK_CONNECT) {
        ereport(ERROR, (errmsg("Failed to connect to SPI for decryption")));
    }
    
    ret = SPI_execute_with_args(sql, 2, argtypes, values, NULL, true, 1);

    if (ret != SPI_OK_SELECT || SPI_processed != 1) {
        SPI_finish();
        ereport(ERROR, (errmsg("Decryption failed via pgp_sym_decrypt. Check encrypted data or key.")));
    }
    
    result = SPI_getbinval(SPI_tuptable->vals[0],
                           SPI_tuptable->tupdesc,
                           1,
                           &isnull);

    if (!isnull) {
        txt = DatumGetTextPP(result);
        decrypted = text_to_cstring(txt);
    } else {
        ereport(LOG, (errmsg("pg_fasttransfer: Decryption returned NULL.")));
    }

    SPI_finish();

    return decrypted;
}

//###########################################################################################
//## Run FastTransfer Function
//###########################################################################################

PG_FUNCTION_INFO_V1(xp_RunFastTransfer_secure);

Datum
xp_RunFastTransfer_secure(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc;
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    HeapTuple tuple;
    
    // Variables statiques pour conserver les résultats entre les appels
    static int exit_code = 0;
    //static char result_buffer[65536];
    static long total_rows = -1;
    static int total_columns = -1;
    static long transfer_time = -1;
    static long total_time = -1;
    
    // Remplacer les tableaux statiques "command" par un StringInfo pour une gestion dynamique de la mémoire
    StringInfo command = makeStringInfo();
    StringInfo result_output = makeStringInfo();

    // Déclarations de variables déplacées en haut de la fonction pour éviter les avertissements de compilation
    char binary_path[1024];
    const char *arg_names[] = {
        "--sourceconnectiontype", "--sourceconnectstring", "--sourcedsn", "--sourceprovider", "--sourceserver",
        "--sourceuser", "--sourcepassword", "--sourcetrusted", "--sourcedatabase", "--sourceschema",
        "--sourcetable", "--query", "--fileinput", "--targetconnectiontype", "--targetconnectstring",
        "--targetserver", "--targetuser", "--targetpassword", "--targettrusted", "--targetdatabase",
        "--targetschema", "--targettable", "--degree", "--method", "--distributekeycolumn",
        "--datadrivenquery", "--loadmode", "--batchsize", "--useworktables", "--runid",
        "--settingsfile", "--mapmethod", "--license",
        "__fasttransfer_path"
    };
    const char *bool_params[] = {
        "--sourcetrusted", "--targettrusted", "--useworktables"
    };
    const char *int_params[] = {
        "--degree", "--batchsize"
    };

    const char *password_params[] = {
        "--sourcepassword", "--targetpassword"
    };
    
    FILE *fp;
    char buffer[1024];
    const char *pg_path;
    char numbuf[34];
    int i, b, x;
    bool is_bool, is_int, is_password;
    const char *val = NULL;
    int status;
    text *enc;
    char *token;

    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errmsg("The function should return a record")));
    
    
    
    // Construire le chemin binaire
    if (fcinfo->nargs > 33 && !PG_ARGISNULL(33)) {
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

    for (i = 0; binary_path[i] != '\0'; i++) {
        if (isspace((unsigned char)binary_path[i])) {
            ereport(ERROR, (errmsg("The path of the executable must not contain spaces.")));
            break;
        }
    }
    
    appendStringInfo(command, "%s", binary_path);

    ereport(LOG, (errmsg("pg_fasttransfer: Final command to be executed: %s", command->data)));

    
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
                appendStringInfo(command, " %s", arg_names[i]);
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
        
        if (is_int) {
            appendStringInfo(command, " %s \"%s\"", arg_names[i], val);
        } else {
            is_password = false;
            
            for (int p = 0; p < sizeof(password_params) / sizeof(char*); p++) {
                if (strcmp(arg_names[i], password_params[p]) == 0) {
                    is_password = true;
                    break;
                }
            }

            if (is_password) {
                enc = PG_GETARG_TEXT_PP(i);
                val = decrypt_password(enc, PGFT_ENCRYPTION_KEY);
            } else {
                val = text_to_cstring(PG_GETARG_TEXT_PP(i));
            }
            
            if (val && strlen(val) > 0) {
                appendStringInfo(command, " %s \"%s\"", arg_names[i], val);
            }
        }
    }
    
    appendStringInfo(command, " 2>&1");
    
    // Log the full command before execution
    ereport(LOG, (errmsg("pg_fasttransfer: Final command to be executed: %s", command->data)));
    
    // Exécuter la commande avec gestion d'erreurs
    PG_TRY();
    {
       fp = popen(command->data, "r");
        if (!fp) {
            ereport(FATAL, (errmsg("pg_fasttransfer: unable to execute FastTransfer. Check the binary path, permissions, and environment variables.")));
        } else {
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                appendStringInfoString(result_output, buffer);
            }
            
            
            status = pclose(fp);
            fp = NULL;

            // Analyser le code de sortie
#ifdef _WIN32
            exit_code = status;
#else
            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            } else {
                exit_code = -2;
            }
#endif
    
           
    
            // Journaliser le code de sortie pour le débogage
            ereport(LOG, (errmsg("pg_fasttransfer: Process exited with status code: %d", exit_code)));
            
            /* Parsing sûr de la sortie : on recherche des labels et on utilise strtol avec endptr */
            char *out = result_output->data;
            char *token = NULL;
            if (out && out[0] != '\0') {
                /* Total rows */
                token = strstr(out, "Total rows : ");
                if (token) {
                    char *p = token + strlen("Total rows : ");
                    char *endptr = NULL;
                    long v = strtol(p, &endptr, 10);
                    if (endptr != p) total_rows = v;
                }

                /* Total columns */
                token = strstr(out, "Total columns : ");
                if (token) {
                    char *p = token + strlen("Total columns : ");
                    char *endptr = NULL;
                    long v = strtol(p, &endptr, 10);
                    if (endptr != p) total_columns = (int)v;
                }

                /* Transfer time : essayer plusieurs variantes (orthographe) */
                token = strstr(out, "Transfert time : Elapsed=");
                if (!token) token = strstr(out, "Transfer time : Elapsed=");
                if (token) {
                    char *p = token;
                    /* trouver le signe '=' dans la ligne */
                    char *eq = strchr(p, '=');
                    if (eq) {
                        char *endptr = NULL;
                        long v = strtol(eq + 1, &endptr, 10);
                        if (endptr != (eq + 1)) transfer_time = v;
                    }
                }

                /* Total time */
                token = strstr(out, "Total time : Elapsed=");
                if (token) {
                    char *eq = strchr(token, '=');
                    if (eq) {
                        char *endptr = NULL;
                        long v = strtol(eq + 1, &endptr, 10);
                        if (endptr != (eq + 1)) total_time = v;
                    }
                }
            }
        }
    }
    PG_CATCH();
    {
       /* En cas d'exception PostgreSQL on journalise et on renvoie un message d'erreur simple */
        ErrorData *errdata;
        MemoryContext oldcxt = MemoryContextSwitchTo(ErrorContext);
        errdata = CopyErrorData();
        MemoryContextSwitchTo(oldcxt);
        /* log l'erreur */
        ereport(LOG, (errmsg("pg_fasttransfer: exception caught during execution: %s", errdata->message)));
        FreeErrorData(errdata);

        /* Sécuriser exit code et message */
        exit_code = -3;
        /* remplacer la sortie par un message d'erreur minimal */
        resetStringInfo(result_output);
        appendStringInfoString(result_output, "An internal error occurred during data transfer. Check PostgreSQL logs for details.\n");
    
    }
    PG_END_TRY();
    
    

    // Retourner les résultats
    values[0] = Int32GetDatum(exit_code);
    //values[1] = CStringGetTextDatum(result_buffer);
    values[1] = CStringGetTextDatum(result_output->data); // direct

    values[2] = Int64GetDatum(total_rows);
    values[3] = Int32GetDatum(total_columns);
    values[4] = Int64GetDatum(transfer_time);
    values[5] = Int64GetDatum(total_time);
    
    tuple = heap_form_tuple(tupdesc, values, nulls);

   
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


//###########################################################################################
//## Encrypt Function
//###########################################################################################

PG_FUNCTION_INFO_V1(pg_fasttransfer_encrypt);

Datum
pg_fasttransfer_encrypt(PG_FUNCTION_ARGS)
{
    text *input_text;
    const char *key = PGFT_ENCRYPTION_KEY;
    Datum result;
    const char *sql;
    Oid argtypes[2];
    Datum values[2];
    int ret;
    bool isnull;

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_text = PG_GETARG_TEXT_PP(0);

    SPI_connect();

    sql = "SELECT encode(pgp_sym_encrypt($1, $2), 'base64')";
    argtypes[0] = TEXTOID;
    argtypes[1] = TEXTOID;
    values[0] = PointerGetDatum(input_text);
    values[1] = CStringGetTextDatum(key);

    ret = SPI_execute_with_args(sql, 2, argtypes, values, NULL, true, 1);

    if (ret != SPI_OK_SELECT || SPI_processed != 1) {
        SPI_finish();
        ereport(ERROR, (errmsg("Encryption with pgp_sym_encrypt failed")));
    }

    result = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);

    SPI_finish();

    if (isnull)
        PG_RETURN_NULL();

    PG_RETURN_DATUM(result);
}
