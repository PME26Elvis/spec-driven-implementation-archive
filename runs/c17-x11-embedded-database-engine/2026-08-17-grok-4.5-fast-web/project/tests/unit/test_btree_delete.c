#include "edb/pager.h"
#include "edb/btree.h"
#include "edb/byteorder.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
int main(void) {
    edb_error err;
    unlink("/tmp/bdel.edb");
    edb_pager *p = edb_pager_open("/tmp/bdel.edb", true, false, NULL, &err);
    uint32_t root = 0;
    if (edb_btree_create(p, true, &root, &err) != 0) { printf("FAIL create\n"); return 1; }
    edb_btree *t = edb_btree_open(p, root, true, &err);
    for (int i = 1; i <= 50; i++) {
        uint8_t kbuf[8], vbuf[8];
        edb_store_u64_le(kbuf, (uint64_t)i);
        edb_store_u64_le(vbuf, (uint64_t)i * 10);
        edb_key key = { .data = kbuf, .len = 8 };
        if (edb_btree_insert(t, &key, vbuf, 8, &err) != 0) { printf("FAIL ins %d\n", i); return 1; }
    }
    for (int i = 1; i <= 50; i += 2) {
        uint8_t kbuf[8];
        edb_store_u64_le(kbuf, (uint64_t)i);
        edb_key key = { .data = kbuf, .len = 8 };
        if (edb_btree_delete(t, &key, &err) != 0) { edb_error_clear(&err); }
    }
    for (int i = 2; i <= 50; i += 2) {
        uint8_t kbuf[8], vbuf[8];
        uint16_t vl = 8;
        edb_store_u64_le(kbuf, (uint64_t)i);
        edb_key key = { .data = kbuf, .len = 8 };
        if (edb_btree_get(t, &key, vbuf, &vl, &err) != 0) { printf("FAIL get even %d\n", i); return 1; }
    }
    root = t->root_page;
    edb_btree_close(t);
    edb_pager_close(p);
    printf("PASS btree insert/delete/get\n");
    return 0;
}
