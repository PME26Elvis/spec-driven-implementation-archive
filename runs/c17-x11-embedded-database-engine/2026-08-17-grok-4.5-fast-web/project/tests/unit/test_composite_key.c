#include "edb/composite_key.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fail = 0;
static void check(const char *n, int ok) {
    if (ok) printf("PASS %s\n", n);
    else { printf("FAIL %s\n", n); fail++; }
}

int main(void) {
    uint8_t buf1[256], buf2[256];
    edb_composite_key k1 = {0}, k2 = {0};

    k1.n = 2;
    k1.comps[0].type = EDB_VAL_TEXT; k1.comps[0].u.bin.p = (const uint8_t*)"ab"; k1.comps[0].u.bin.len = 2;
    k1.comps[1].type = EDB_VAL_TEXT; k1.comps[1].u.bin.p = (const uint8_t*)"c";  k1.comps[1].u.bin.len = 1;
    k2.n = 2;
    k2.comps[0].type = EDB_VAL_TEXT; k2.comps[0].u.bin.p = (const uint8_t*)"a";  k2.comps[0].u.bin.len = 1;
    k2.comps[1].type = EDB_VAL_TEXT; k2.comps[1].u.bin.p = (const uint8_t*)"bc"; k2.comps[1].u.bin.len = 2;
    int n1 = edb_composite_encode(&k1, buf1, sizeof buf1);
    int n2 = edb_composite_encode(&k2, buf2, sizeof buf2);
    check("unambiguous", n1 > 0 && n2 > 0 && !(n1 == n2 && memcmp(buf1, buf2, n1) == 0));

    edb_composite_key out;
    check("roundtrip", edb_composite_decode(buf1, n1, &out) == 0 && out.n == 2 && out.comps[0].u.bin.len == 2);

    edb_composite_key nula = {0}, nulb = {0};
    nula.n = 2; nula.comps[0].type = EDB_VAL_NULL; nula.comps[1].type = EDB_VAL_INTEGER; nula.comps[1].u.i64 = 1;
    nulb = nula;
    check("null no conflict", !edb_composite_unique_conflict(&nula, &nulb));

    edb_composite_key fa = {0}, fb = {0};
    fa.n = 2; fa.comps[0].type = EDB_VAL_INTEGER; fa.comps[0].u.i64 = 1;
    fa.comps[1].type = EDB_VAL_INTEGER; fa.comps[1].u.i64 = 2;
    fb = fa;
    check("full conflict", edb_composite_unique_conflict(&fa, &fb));

    const char *zh = "中文";
    k1.n = 1; k1.comps[0].type = EDB_VAL_TEXT;
    k1.comps[0].u.bin.p = (const uint8_t*)zh; k1.comps[0].u.bin.len = (uint32_t)strlen(zh);
    n1 = edb_composite_encode(&k1, buf1, sizeof buf1);
    edb_composite_decode(buf1, n1, &out);
    check("chinese", out.comps[0].u.bin.len == strlen(zh) &&
          memcmp(out.comps[0].u.bin.p, zh, strlen(zh)) == 0);

    return fail ? 1 : 0;
}
