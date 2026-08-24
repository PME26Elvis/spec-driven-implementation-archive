#include "edb/pager.h"
#include "edb/wal.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
int main(void) {
    edb_error err;
    unlink("/tmp/walrt.edb");
    unlink("/tmp/walrt.edb-wal");
    edb_pager *p = edb_pager_open("/tmp/walrt.edb", true, false, NULL, &err);
    if (!p) { printf("FAIL pager\n"); return 1; }
    edb_wal *w = edb_wal_open("/tmp/walrt.edb", true, &err);
    if (!w) { printf("FAIL wal open\n"); return 1; }
    if (edb_wal_begin(w, 1, &err) != 0) { printf("FAIL begin\n"); return 1; }
    uint8_t page[4096];
    memset(page, 0xAB, sizeof page);
    if (edb_wal_log_page(w, 1, page, &err) != 0) { printf("FAIL log\n"); return 1; }
    if (edb_wal_commit(w, 1, &err) != 0) { printf("FAIL commit\n"); return 1; }
    edb_wal_close(w);
    edb_pager_close(p);
    printf("PASS wal begin/log/commit\n");
    return 0;
}
