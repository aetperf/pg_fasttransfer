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


#ifdef _WIN32
#include "tiny-aes/aes.h"
#else
#include "aes.h"
#endif

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


static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// Fonction de décodage base64 cohérente avec ton encodage
static int base64_decode(const char *input, uint8_t *output, int output_len) {
    int len = strlen(input);
    int i = 0, j = 0;
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d;
    uint32_t triple;

    while (i < len && input[i] != '=') {
        sextet_a = strchr(base64_chars, input[i++]) - base64_chars;
        sextet_b = strchr(base64_chars, input[i++]) - base64_chars;
        sextet_c = input[i] == '=' ? 0 & i++ : strchr(base64_chars, input[i++]) - base64_chars;
        sextet_d = input[i] == '=' ? 0 & i++ : strchr(base64_chars, input[i++]) - base64_chars;

        triple = (sextet_a << 18) | (sextet_b << 12) | (sextet_c << 6) | sextet_d;

        if (j < output_len) output[j++] = (triple >> 16) & 0xFF;
        if (j < output_len && input[i - 2] != '=') output[j++] = (triple >> 8) & 0xFF;
        if (j < output_len && input[i - 1] != '=') output[j++] = triple & 0xFF;
    }

    return j;  // Nombre d'octets réellement écrits
}

// Fonction de déchiffrement compatible avec aes_encrypt_pg
char *aes_decrypt_pg(const char *base64_input) {
    // Clé AES-256 utilisée dans aes_encrypt_pg
    const uint8_t key[32] = {
        0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
    };

    uint8_t iv[16] = {0};
    size_t input_len = strlen(base64_input);
    size_t decoded_max_len = (input_len * 3) / 4;

    uint8_t *decoded = malloc(decoded_max_len + 1);
    if (!decoded) return NULL;

    int decrypted_len = base64_decode(base64_input, decoded, decoded_max_len);
    if (decrypted_len <= 0 || decrypted_len % 16 != 0) {
        free(decoded);
        return NULL;
    }

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    AES_CBC_decrypt_buffer(&ctx, decoded, decrypted_len);

    // Supprimer le padding PKCS#7
    uint8_t pad_value = decoded[decrypted_len - 1];
    if (pad_value == 0 || pad_value > 16) {
        free(decoded);
        return NULL;
    }

    decoded[decrypted_len - pad_value] = '\0';  // Null-terminate

    char *result = strdup((char *)decoded);
    free(decoded);
    return result;
}


PG_FUNCTION_INFO_V1(xp_RunFastTransfer_secure);

PGDLLEXPORT  Datum
xp_RunFastTransfer_secure(PG_FUNCTION_ARGS)
{
    #ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
    #endif

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
    
    const char *password_params[] = {
        "--sourcepassword", "--targetpassword"
    };

    FILE *fp;
    char buffer[1024];
    const char *env_path = getenv("FASTTRANSFER_PATH");
    const char *pg_path;
    char numbuf[34];
    int i, b, x, p;
    bool is_bool, is_int, is_password;
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
        
        /*
        is_int = false;
        for (x = 0; x < sizeof(int_params) / sizeof(char *); x++) {
            if (strcmp(arg_names[i], int_params[x]) == 0) {
                snprintf(numbuf, sizeof(numbuf), "%d", PG_GETARG_INT32(i));
                val = numbuf;
                is_int = true;
                break;
            }
        }*/
        
        is_int = false;
        
        if (!is_int) {
            is_password = false;
            
            // Vérification si l'argument est un mot de passe
            for (int p = 0; p < sizeof(password_params) / sizeof(char*); p++) {
                if (strcmp(arg_names[i], password_params[p]) == 0) {
                    is_password = true;
                    break;
                }
            }

            if (is_password) {
                // Décryptage du mot de passe uniquement si nécessaire
                text *enc = PG_GETARG_TEXT_PP(i);
                char *enc_cstr = text_to_cstring(enc);  // Convertit text * en char *
                char *decrypted = aes_decrypt_pg(enc_cstr);  // Retourne char * décrypté
                val = decrypted;  // Affecte la valeur décryptée à val
            } else {
                val = text_to_cstring(PG_GETARG_TEXT_PP(i));  // Si ce n'est pas un mot de passe, on récupère directement la valeur
            }
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
        strncat(result_buffer, "Error: unable to execute FastTransfer.\n", sizeof(result_buffer) - strlen(result_buffer) - 1);
        exit_code = -1;
    } else {
        size_t result_len = strlen(result_buffer);
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

//###########################################################################################
//## Encrypt String Function
//###########################################################################################


static void base64_encode(const uint8_t *input, int input_len, char *output, int output_len)
{
    int i = 0, j = 0;
    while (i < input_len) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        if (j + 4 > output_len - 1) // -1 pour le \0
            break;

        output[j++] = base64_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_chars[(triple >> 12) & 0x3F];
        output[j++] = (i - 2) <= input_len ? base64_chars[(triple >> 6) & 0x3F] : '=';
        output[j++] = (i - 1) <= input_len ? base64_chars[triple & 0x3F] : '=';
    }
    output[j] = '\0';
}

PG_FUNCTION_INFO_V1(aes_encrypt_pg);

PGDLLEXPORT  Datum
aes_encrypt_pg(PG_FUNCTION_ARGS)
{
    text *input_text = PG_GETARG_TEXT_PP(0);
    int input_len = VARSIZE_ANY_EXHDR(input_text);
    uint8_t *input_data = (uint8_t *) VARDATA_ANY(input_text);

    const uint8_t key[32] = {
        0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
    };

    int block_size = 16;
    int padded_len = ((input_len / block_size) + 1) * block_size;

    uint8_t *buffer = (uint8_t *) palloc0(padded_len);
    memcpy(buffer, input_data, input_len);

    uint8_t pad_value = padded_len - input_len;
    for (int i = input_len; i < padded_len; i++) {
        buffer[i] = pad_value;
    }

    struct AES_ctx ctx;
    uint8_t iv[16] = {0};

    AES_init_ctx_iv(&ctx, key, iv);
    AES_CBC_encrypt_buffer(&ctx, buffer, padded_len);

    // Taille maximale sortie base64 : 4 * ceil(input/3)
    int base64_max_len = ((padded_len + 2) / 3) * 4 + 1;
    char *base64_output = (char *) palloc(base64_max_len);

    base64_encode(buffer, padded_len, base64_output, base64_max_len);

    pfree(buffer);

    // Retourner en text PostgreSQL
    text *result = cstring_to_text(base64_output);
    pfree(base64_output);

    PG_RETURN_TEXT_P(result);
}






