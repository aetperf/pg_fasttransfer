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
#include "utils/builtins.h" // Nécessaire pour text_to_cstring, CStringGetTextDatum, etc.
#include "lib/stringinfo.h"
#include "executor/spi.h" // Nécessaire pour SPI_connect, SPI_execute_with_args, SPI_tuptable etc.
#include "funcapi.h"
#include "utils/rel.h" // Nécessaire pour les descriptions de tuple, etc.


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
// Déclaration de la fonction decrypt_password pour éviter l'avertissement 'missing-prototypes'
char *decrypt_password(text *cipher_text, const char *key);


// Renvoie un pointeur alloué en mémoire PostgreSQL (pas besoin de free).
// Utilise SPI pour appeler les fonctions SQL de pgcrypto.
char *decrypt_password(text *cipher_text, const char *key) {
    const char *sql = "SELECT pgp_sym_decrypt(decode($1, 'base64'), $2)";
    Oid argtypes[2] = { TEXTOID, TEXTOID };
    Datum values[2] = {
        PointerGetDatum(cipher_text),
        CStringGetTextDatum(key)
    };

    char *decrypted = NULL; // Initialise à NULL
    int ret; // Déclaration déplacée
    bool isnull; // Déclaration déplacée
    Datum result; // Déclaration déplacée
    text *txt; // Déclaration déplacée

    // Tente de se connecter à SPI (Server Programming Interface)
    if (SPI_connect() != SPI_OK_CONNECT) {
        ereport(ERROR, (errmsg("Failed to connect to SPI for decryption")));
    }

    // Exécute la requête SQL pour décrypter
    ret = SPI_execute_with_args(sql, 2, argtypes, values, NULL, true, 1);

    // Vérifie le succès de l'exécution et le nombre de lignes traitées
    if (ret != SPI_OK_SELECT || SPI_processed != 1) {
        SPI_finish(); // Déconnecte SPI en cas d'échec
        ereport(ERROR, (errmsg("Decryption failed via pgp_sym_decrypt. Check encrypted data or key.")));
    }

    // Récupère le résultat de la première colonne de la première ligne
    result = SPI_getbinval(SPI_tuptable->vals[0],
                                 SPI_tuptable->tupdesc,
                                 1, // Récupère la première colonne (index 1)
                                 &isnull);

    if (!isnull) {
        // Convertit le Datum (TEXT *) en char * alloué par PostgreSQL
        txt = DatumGetTextPP(result);
        decrypted = text_to_cstring(txt);
    }

    SPI_finish(); // Déconnecte SPI

    return decrypted;
}

//###########################################################################################
//## Run FastTransfer Function
//###########################################################################################

PG_FUNCTION_INFO_V1(xp_RunFastTransfer_secure);

Datum
xp_RunFastTransfer_secure(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc; // Déclaration déplacée
    Datum values[6]; // Déclaration déplacée
    bool nulls[6] = {false, false, false, false, false, false}; // Déclaration déplacée
    HeapTuple tuple; // Déclaration déplacée
    
    // Variables statiques pour conserver les résultats entre les appels
    static int exit_code = 0;
    static char result_buffer[65536]; // Grand tampon statique au lieu de StringInfo
    static long total_rows = -1;
    static int total_columns = -1;
    static long transfer_time = -1;
    static long total_time = -1;
    
    // Construire la commande et exécuter FastTransfer
    char command[8192]; // Déclaration déplacée
    char binary_path[1024]; // Déclaration déplacée
    const char *arg_names[] = {
        "--sourceconnectiontype", "--sourceconnectstring", "--sourcedsn", "--sourceprovider", "--sourceserver",
        "--sourceuser", "--sourcepassword", "--sourcetrusted", "--sourcedatabase", "--sourceschema",
        "--sourcetable", "--query", "--fileinput", "--targetconnectiontype", "--targetconnectstring",
        "--targetserver", "--targetuser", "--targetpassword", "--targettrusted", "--targetdatabase",
        "--targetschema", "--targettable", "--degree", "--method", "--distributekeycolumn",
        "--datadrivenquery", "--loadmode", "--batchsize", "--useworktables", "--runid",
        "--settingsfile", "--mapmethod", "--license",
        "__fasttransfer_path" // Argument interne spécial pour le chemin binaire si pas de variable d'environnement
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
    
    FILE *fp; // Déclaration déplacée
    char buffer[1024]; // Déclaration déplacée
    const char *pg_path; // Déclaration déplacée
    char numbuf[34]; // Déclaration déplacée
    int i, b, x; // Déclaration déplacée
    bool is_bool, is_int, is_password; // Déclaration déplacée
    const char *val = NULL; // Pointeur pour stocker la valeur de l'argument (dé)chiffrée // Déclaration déplacée
    int status; // Déclaration déplacée
    text *enc; // Déclaration déplacée
    size_t result_len; // Déclaration déplacée
    size_t buf_len; // Déclaration déplacée
    char *token; // Déclaration déplacée

    #ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
    #endif

    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errmsg("The function should return a record")));
    
    // Initialiser le tampon de résultat
    result_buffer[0] = '\0';
    
    // Construire le chemin binaire
    // Le nombre d'arguments pour la fonction pg_fasttransfer est de 34 (0 à 33)
    // Vérifier si l'argument 33 existe et n'est pas NULL pour le chemin binaire
    if (fcinfo->nargs > 33 && !PG_ARGISNULL(33)) { 
        pg_path = text_to_cstring(PG_GETARG_TEXT_PP(33));
#ifdef _WIN32
        snprintf(binary_path, sizeof(binary_path), "%s\\%s", pg_path, BINARY_NAME);
#else
        snprintf(binary_path, sizeof(binary_path), "%s/%s", pg_path, BINARY_NAME);
#endif
    } else {
        // Sinon, utiliser le chemin par défaut (répertoire courant)
#ifdef _WIN32
        snprintf(binary_path, sizeof(binary_path), ".\\%s", BINARY_NAME);
#else
        snprintf(binary_path, sizeof(binary_path), "./%s", BINARY_NAME);
#endif
    }
    
    // Construire la commande
    snprintf(command, sizeof(command), "\"%s\"", binary_path);
    
    // Itérer à travers les arguments (jusqu'à 33, car arg_names a 34 éléments, 0-33)
    // Le dernier arg_name ("__fasttransfer_path" à l'index 33) est interne, non transmis à FastTransfer.exe
    for (i = 0; i < 33; i++) { // Boucler uniquement pour les arguments de ligne de commande réels (index 0 à 32)
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
            continue; // Passer à l'argument suivant si c'est un booléen
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
            // val est déjà défini par le bloc int_params, passer à la concaténation
        } else {
            // Si ce n'est ni un booléen ni un entier, c'est un argument texte (y compris le mot de passe)
            is_password = false;
            
            // Vérification si l'argument est un mot de passe
            for (int p = 0; p < sizeof(password_params) / sizeof(char*); p++) {
                if (strcmp(arg_names[i], password_params[p]) == 0) {
                    is_password = true;
                    break;
                }
            }

            if (is_password) {
                enc = PG_GETARG_TEXT_PP(i); // Utilise la variable enc déclarée en haut
                val = decrypt_password(enc, PGFT_ENCRYPTION_KEY); 
                
                // --- AJOUT POUR LE DÉBOGAGE : IMPRIMER LE MOT DE PASSE DÉCRYPTÉ ---
                if (val) {
                    ereport(LOG, (errmsg("pg_fasttransfer: Decrypted password for %s: '%s'", arg_names[i], val)));
                } else {
                    ereport(LOG, (errmsg("pg_fasttransfer: Decrypted password for %s: NULL or empty", arg_names[i])));
                }
                // --- FIN DE L'AJOUT POUR LE DÉBOGAGE ---

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
    
    // Exécuter la commande
    fp = popen(command, "r");
    if (!fp) {
        strncat(result_buffer, "Error: unable to execute FastTransfer.\n", sizeof(result_buffer) - strlen(result_buffer) - 1);
        exit_code = -1;
    } else {
        result_len = strlen(result_buffer); // Utilise la variable result_len déclarée en haut
        while (fgets(buffer, sizeof(buffer), fp) != NULL && result_len < sizeof(result_buffer) - 1) {
            buf_len = strlen(buffer); // Utilise la variable buf_len déclarée en haut
            if (result_len + buf_len < sizeof(result_buffer) - 1) {
                strcat(result_buffer, buffer);
                result_len += buf_len;
            }
        }
        status = pclose(fp);
        
        // Analyser les résultats
        if (result_buffer[0] != '\0') {
            token = strstr(result_buffer, "Total rows : "); // Utilise la variable token déclarée en haut
            if (token != NULL) {
                total_rows = strtol(token + strlen("Total rows : "), NULL, 10);
            }
            
            // Chercher "Total columns : "
            token = strstr(result_buffer, "Total columns : ");
            if (token != NULL) {
                total_columns = strtol(token + strlen("Total columns : "), NULL, 10);
            }
            
            // Chercher "Transfer time : Elapsed"
            token = strstr(result_buffer, "Transfert time : Elapsed");
            if (token != NULL) {
                transfer_time = strtol(token + strlen("Transfert time : Elapsed="), NULL, 10);
            }
            
            // Chercher "Total time : Elapsed="
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
    text *input_text; // Déclaration déplacée
    const char *key = PGFT_ENCRYPTION_KEY; // Déclaration déplacée
    Datum result; // Déclaration déplacée
    const char *sql; // Déclaration déplacée
    Oid argtypes[2]; // Déclaration déplacée
    Datum values[2]; // Déclaration déplacée
    int ret; // Déclaration déplacée
    bool isnull; // Déclaration déplacée

    if (PG_ARGISNULL(0))
        PG_RETURN_NULL();

    input_text = PG_GETARG_TEXT_PP(0); // Affectation après le retour conditionnel

    SPI_connect();

    sql = "SELECT encode(pgp_sym_encrypt($1, $2), 'base64')"; // Affectation après la déclaration
    argtypes[0] = TEXTOID; // Affectation après la déclaration
    argtypes[1] = TEXTOID; // Affectation après la déclaration
    values[0] = PointerGetDatum(input_text); // Affectation après la déclaration
    values[1] = CStringGetTextDatum(key); // Affectation après la déclaration

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
