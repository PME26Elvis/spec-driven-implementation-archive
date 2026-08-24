/* Multi-snapshot isolation smoke (MVCCT partial) */
#include "edb/mvcc.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    edb_error err;
    edb_mvcc_mgr *m = edb_mvcc_create();
    edb_txn *readers[8];
    for (int i = 0; i < 8; i++) {
        readers[i] = edb_mvcc_begin(m, true, &err);
        if (!readers[i]) { printf("FAIL begin reader %d\n", i); return 1; }
    }
    edb_txn *w = edb_mvcc_begin(m, false, &err);
    edb_xid v = w->xid;
    /* readers must not see uncommitted */
    for (int i = 0; i < 8; i++) {
        if (edb_mvcc_visible(m, &readers[i]->snap, v, 0)) {
            printf("FAIL reader %d sees uncommitted\n", i); return 1;
        }
    }
    edb_mvcc_commit(m, w, &err);
    /* new reader sees it */
    edb_txn *r9 = edb_mvcc_begin(m, true, &err);
    if (!edb_mvcc_visible(m, &r9->snap, v, 0)) {
        printf("FAIL new reader misses commit\n"); return 1;
    }
    /* old readers still exclude concurrent write */
    for (int i = 0; i < 8; i++) {
        if (edb_mvcc_visible(m, &readers[i]->snap, v, 0)) {
            printf("FAIL old reader %d sees concurrent commit\n", i); return 1;
        }
    }
    printf("horizon=%llu\n", (unsigned long long)edb_mvcc_horizon(m));
    for (int i = 0; i < 8; i++) edb_mvcc_txn_free(readers[i]);
    edb_mvcc_txn_free(w); edb_mvcc_txn_free(r9);
    edb_mvcc_destroy(m);
    printf("PASS 8-reader snapshot isolation\n");
    return 0;
}
