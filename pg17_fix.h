#ifndef PG17_FIX_H
#define PG17_FIX_H

#ifdef _MSC_VER

// Completely bypass the problematic tupmacs.h functions
#define TUPMACS_H 1  // Prevent tupmacs.h from being included

// Provide our own minimal implementations that avoid the switch statements
typedef uintptr_t Datum;

#define PointerGetDatum(X) ((Datum) (X))
#define DatumGetPointer(X) ((void *) (X))
#define CharGetDatum(X) ((Datum) (X))
#define Int16GetDatum(X) ((Datum) (X))
#define Int32GetDatum(X) ((Datum) (X))
#define DatumGetChar(X) ((char) (X))
#define DatumGetInt16(X) ((int16) (X))
#define DatumGetInt32(X) ((int32) (X))

// Dummy fetch_att that avoids the switch statement
static inline Datum
fetch_att(const void *T, bool attbyval, int attlen)
{
    if (!attbyval)
        return PointerGetDatum(T);
    
    // Avoid switch, use if-else
    if (attlen == 1)
        return CharGetDatum(*((const char *) T));
    else if (attlen == 2)
        return Int16GetDatum(*((const int16 *) T));
    else if (attlen == 4)
        return Int32GetDatum(*((const int32 *) T));
    else if (attlen == 8)
        return *((const Datum *) T);
    else
        return 0; // Error case
}

// Dummy store_att_byval that avoids the switch statement  
static inline void
store_att_byval(void *T, Datum newdatum, int attlen)
{
    // Avoid switch, use if-else
    if (attlen == 1)
        *(char *) T = DatumGetChar(newdatum);
    else if (attlen == 2)
        *(int16 *) T = DatumGetInt16(newdatum);
    else if (attlen == 4)
        *(int32 *) T = DatumGetInt32(newdatum);
    else if (attlen == 8)
        *(Datum *) T = newdatum;
}

// Provide other necessary macros from tupmacs.h
#define att_isnull(ATT, BITS) (!(BITS[ATT >> 3] & (1 << (ATT & 0x07))))
#define fetchatt(A,T) fetch_att(T, (A)->attbyval, (A)->attlen)

#endif // _MSC_VER

#endif // PG17_FIX_H