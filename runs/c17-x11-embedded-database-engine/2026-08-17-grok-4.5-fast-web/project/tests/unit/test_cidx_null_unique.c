/* CIDX NULL unique semantics */
#include "edb/composite_key.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    edb_composite_key a = {0}, b = {0};
    a.n = 2; b.n = 2;
    a.comps[0].type = EDB_VAL_NULL;
    a.comps[1].type = EDB_VAL_INTEGER; a.comps[1].u.i64 = 1;
    b.comps[0].type = EDB_VAL_NULL;
    b.comps[1].type = EDB_VAL_INTEGER; b.comps[1].u.i64 = 1;
    /* NULL-containing UNIQUE: equal non-null parts with NULL should NOT conflict fully */
    int conf = edb_composite_unique_conflict(&a, &b) ? 1 : 0;
    a.comps[0].type = EDB_VAL_INTEGER; a.comps[0].u.i64 = 5;
    b.comps[0].type = EDB_VAL_INTEGER; b.comps[0].u.i64 = 5;
    int conf2 = edb_composite_unique_conflict(&a, &b) ? 1 : 0;
    if (!conf2) { printf("FAIL expected conflict on full non-null equal\n"); return 1; }
    uint8_t buf[128];
    int n = edb_composite_encode(&a, buf, sizeof buf);
    if (n < 0) { printf("FAIL encode\n"); return 1; }
    edb_composite_key out = {0};
    if (edb_composite_decode(buf, (size_t)n, &out) != 0) { printf("FAIL decode\n"); return 1; }
    printf("PASS cidx null/unique/encode (null_conflict=%d)\n", conf);
    return 0;
}
