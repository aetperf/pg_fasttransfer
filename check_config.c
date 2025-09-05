#include <stdio.h>
#include "postgres.h"

int main() {
    printf("SIZEOF_DATUM = %d\n", SIZEOF_DATUM);
    printf("sizeof(Datum) = %zu\n", sizeof(Datum));
    printf("sizeof(int32) = %zu\n", sizeof(int32));
    printf("sizeof(void*) = %zu\n", sizeof(void*));
    return 0;
}