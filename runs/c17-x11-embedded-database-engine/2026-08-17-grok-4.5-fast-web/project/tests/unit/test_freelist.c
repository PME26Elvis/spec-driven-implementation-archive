/* ALLOC/freelist path */
#include "edb/pager.h"
#include "edb/freelist.h"
#include <stdio.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/fl.edb");
    edb_pager *p = edb_pager_open("/tmp/fl.edb", true, false, NULL, &err);
    if (!p) { printf("FAIL open\n"); return 1; }
    edb_freelist *f = edb_freelist_create(p);
    if (!f) { printf("FAIL create\n"); return 1; }
    if (edb_freelist_push(f, 10) != 0 || edb_freelist_push(f, 11) != 0) {
        printf("FAIL push\n"); return 1;
    }
    uint32_t root = 0;
    if (edb_freelist_flush(f, &root, &err) != 0) { printf("FAIL flush\n"); return 1; }
    if (edb_freelist_count(f) != 2) { printf("FAIL count %zu\n", edb_freelist_count(f)); return 1; }
    uint32_t a = edb_freelist_pop(f);
    uint32_t b = edb_freelist_pop(f);
    if (a == 0 || b == 0 || a == b) { printf("FAIL pop %u %u\n", a, b); return 1; }
    printf("PASS freelist push/flush/pop\n");
    edb_freelist_destroy(f);
    edb_pager_close(p);
    return 0;
}
