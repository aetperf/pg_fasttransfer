#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "lib/stringinfo.h"
#include "executor/spi.h"
#include "funcapi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define BINARY_NAME "FastTransfer"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(pg_fasttransfer);

Datum
pg_fasttransfer(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    static int exit_code;
    static StringInfoData result;
    // Parsing variables
    static long total_rows = -1;
    static int total_columns = -1;
    static long transfer_time = -1;
    static long total_time = -1;


    if (SRF_IS_FIRSTCALL()) {
        MemoryContext oldcontext;
        StringInfoData command;
        char binary_path[8192];
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

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // Init result buffer
        initStringInfo(&result);

        if (env_path && strlen(env_path) > 0) {
            snprintf(binary_path, sizeof(binary_path), "%s/%s", env_path, BINARY_NAME);
        } else if (!PG_ARGISNULL(33)) {
            pg_path = text_to_cstring(PG_GETARG_TEXT_PP(33));
            snprintf(binary_path, sizeof(binary_path), "%s/%s", pg_path, BINARY_NAME);
        } else {
            snprintf(binary_path, sizeof(binary_path), "./%s", BINARY_NAME);
        }

        initStringInfo(&command);
        appendStringInfo(&command, "\"%s\"", binary_path);

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
                    appendStringInfo(&command, " %s", arg_names[i]);
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
                appendStringInfo(&command, " %s \"%s\"", arg_names[i], val);
            }
        }

        appendStringInfo(&command, " 2>&1"); // Capture stdout + stderr

        fp = popen(command.data, "r");
        if (!fp) {
            appendStringInfoString(&result, "Error: unable to execute FastTransfer.\n");
            exit_code = -1;
        } else {
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                appendStringInfoString(&result, buffer);
            }
            status = pclose(fp);


            // Code de parsing
            if (result.data != NULL) {
                char *line, *token;
                char *temp_data = pstrdup(result.data); // Copie pour parsing
                
                // Recherche "Total rows : "
                token = strstr(temp_data, "Total rows : ");
                if (token != NULL) {
                    total_rows = strtol(token + strlen("Total rows : "), NULL, 10);
                }
                
                // Recherche "Total columns : "
                token = strstr(temp_data, "Total columns : ");
                if (token != NULL) {
                    total_columns = strtol(token + strlen("Total columns : "), NULL, 10);
                }

                // Recherche "Transfer time : Elapsed"
                token = strstr(temp_data, "Transfert time : Elapsed");
                if (token != NULL) {
                    transfer_time = strtol(token + strlen("Transfert time : Elapsed="), NULL, 10);
                }

                // Recherche "Total time : Elapsed="
                token = strstr(temp_data, "Total time : Elapsed=");
                if (token != NULL) {
                    total_time = strtol(token + strlen("Total time : Elapsed="), NULL, 10);
                }
                
                pfree(temp_data);
            }

            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            } else {
                exit_code = -2;
                appendStringInfoString(&result, "\nUnknown error of FastTransfer\n");
            }
        }

        pfree(command.data);
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    if (funcctx->call_cntr == 0) {
        TupleDesc tupdesc;
        Datum values[6];
        bool nulls[6] = {false, false, false, false, false, false};
        HeapTuple tuple;

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR, (errmsg("The function should return a record")));

        values[0] = Int32GetDatum(exit_code);
        values[1] = CStringGetTextDatum(result.data);
        values[2] = Int64GetDatum(total_rows);
        values[3] = Int32GetDatum(total_columns);
        values[4] = Int64GetDatum(transfer_time);
        values[5] = Int64GetDatum(total_time);

        tuple = heap_form_tuple(tupdesc, values, nulls);
        SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
    } else {
        SRF_RETURN_DONE(funcctx);
    }

}
