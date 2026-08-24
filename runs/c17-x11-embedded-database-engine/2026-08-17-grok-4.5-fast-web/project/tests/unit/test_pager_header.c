#include "edb/pager.h"
#include <stdio.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/phdr.edb");
    edb_pager *p = edb_pager_open("/tmp/phdr.edb", true, false, NULL, &err);
    if (!p) { printf("FAIL open %s\n", err.message); return 1; }
    edb_page *pg = edb_pager_new(p, EDB_PAGE_BTREE_LEAF, &err);
    if (!pg) { printf("FAIL new page %s\n", err.message); return 1; }
    uint32_t n = pg->page_no;
    edb_pager_unpin(p, pg);
    if (edb_pager_sync(p, &err) != 0) { printf("FAIL sync\n"); return 1; }
    edb_pager_close(p);
    p = edb_pager_open("/tmp/phdr.edb", false, false, NULL, &err);
    if (!p) { printf("FAIL reopen %s\n", err.message); return 1; }
    edb_page *pg2 = edb_pager_get(p, n, &err);
    if (!pg2) { printf("FAIL get page %u %s\n", n, err.message); return 1; }
    if (pg2->page_no != n) { printf("FAIL page_no\n"); return 1; }
    edb_pager_unpin(p, pg2);
    edb_pager_close(p);
    printf("PASS pager new/get/sync/reopen\n");
    return 0;
}
