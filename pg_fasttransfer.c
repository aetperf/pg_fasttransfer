#ifdef _WIN32
#define PGDLLIMPORT __declspec(dllimport)
#endif
// Version Windows plus sûre qui évite les fonctions mémoire PostgreSQL problématiques
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
#include "utils/rel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

#ifdef _WIN32
#define BINARY_NAME "FastTransfer.exe"
#else
#define BINARY_NAME "FastTransfer"
#endif

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

// Clé symétrique partagée pour le chiffrement/déchiffrement
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
    static char result_buffer[65536];
    static long total_rows = -1;
    static int total_columns = -1;
    static long transfer_time = -1;
    static long total_time = -1;
    
    // Remplacer les tableaux statiques "command" par un StringInfo pour une gestion dynamique de la mémoire
    StringInfo command;
    StringInfo result_output;

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
    
    // Initialiser le tampon de résultat statique
    result_buffer[0] = '\0';
    
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
    
    // Initialiser le StringInfo pour la commande
    command = makeStringInfo();
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

    ereport(LOG, (errmsg("pg_fasttransfer: Final command to be executed: %s", command->data)));
    
    // Exécuter la commande avec gestion d'erreurs
    PG_TRY();
    {
       fp = popen(command->data, "r");
        if (!fp) {
            snprintf(result_buffer, sizeof(result_buffer), "Error: unable to execute FastTransfer.\n");
            exit_code = -1;
        } else {
            result_output = makeStringInfo();
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                appendStringInfoString(result_output, buffer);
            }
            
            // Copier le contenu du StringInfo dans le tampon statique pour l'analyse
            strlcpy(result_buffer, result_output->data, sizeof(result_buffer));
            pfree(result_output->data); // Libère la mémoire allouée par StringInfo
            pfree(result_output);
            
            status = pclose(fp);

            // Analyser le code de sortie
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
    
            // Journaliser le code de sortie pour le débogage
            ereport(LOG, (errmsg("pg_fasttransfer: Process exited with status code: %d", exit_code)));
            
            // Analyser les résultats
            if (result_buffer[0] != '\0') {
                token = strstr(result_buffer, "Total rows : ");
                if (token != NULL) {
                    total_rows = strtol(token + strlen("Total rows : "), NULL, 10);
                }
                
                token = strstr(result_buffer, "Total columns : ");
                if (token != NULL) {
                    total_columns = strtol(token + strlen("Total columns : "), NULL, 10);
                }
                
                token = strstr(result_buffer, "Transfert time : Elapsed");
                if (token != NULL) {
                    transfer_time = strtol(token + strlen("Transfert time : Elapsed="), NULL, 10);
                }
                
                token = strstr(result_buffer, "Total time : Elapsed=");
                if (token != NULL) {
                    total_time = strtol(token + strlen("Total time : Elapsed="), NULL, 10);
                }
            }
        }
    }
    PG_CATCH();
    {
        // Une erreur s'est produite, probablement due à FastTransfer
        // Journaliser le message d'erreur et définir le code de sortie sur -3
        exit_code = -3;
        ereport(LOG, (errmsg("pg_fasttransfer: An exception occurred during execution of FastTransfer. The process likely crashed.")));
        ereport(LOG, (errmsg("pg_fasttransfer: Process exited with status code: %d", exit_code)));
        strlcpy(result_buffer, "An internal error occurred during data transfer. Check PostgreSQL logs for details.\n", sizeof(result_buffer));
    }
    PG_END_TRY();
    
    // Libérer la mémoire allouée pour la commande
    pfree(command->data);
    pfree(command);

    // Retourner les résultats
    values[0] = Int32GetDatum(exit_code);
    values[1] = CStringGetTextDatum(result_buffer);
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
