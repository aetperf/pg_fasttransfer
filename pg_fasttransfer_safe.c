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
    // Simplified version that just returns a test result without using complex memory contexts
    TupleDesc tupdesc;
    Datum values[6];
    bool nulls[6] = {false, false, false, false, false, false};
    HeapTuple tuple;
    
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (errmsg("The function should return a record")));
    
    // Return test values to verify the extension works
    values[0] = Int32GetDatum(0);  // exit_code
    values[1] = CStringGetTextDatum("Extension loaded successfully on Windows!");  // output
    values[2] = Int64GetDatum(-1); // total_rows
    values[3] = Int32GetDatum(-1); // total_columns  
    values[4] = Int64GetDatum(-1); // transfer_time
    values[5] = Int64GetDatum(-1); // total_time
    
    tuple = heap_form_tuple(tupdesc, values, nulls);
    PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}