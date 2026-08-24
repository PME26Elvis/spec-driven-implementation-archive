#ifndef EDB_MVCC_H
#define EDB_MVCC_H

#include "edb/common.h"
#include <stdbool.h>

/* MVCC transaction ids and snapshot (MVCC-001.. partial skeleton) */

typedef uint64_t edb_xid;

typedef struct edb_snapshot {
    edb_xid xmin;          /* oldest active when snapshot taken */
    edb_xid xmax;          /* next xid to assign (= snapshot boundary) */
    edb_xid *active;       /* active xids concurrent with snapshot */
    size_t  active_count;
} edb_snapshot;

typedef enum edb_txn_state {
    EDB_TXN_ACTIVE = 1,
    EDB_TXN_COMMITTED = 2,
    EDB_TXN_ABORTED = 3
} edb_txn_state;

typedef struct edb_txn {
    edb_xid        xid;
    edb_txn_state  state;
    edb_snapshot   snap;
    bool           read_only;
} edb_txn;

typedef struct edb_mvcc_mgr {
    edb_xid next_xid;
    edb_xid *active;
    size_t  active_count;
    size_t  active_cap;
    /* commit log: xid -> state (simple array for bootstrap) */
    edb_xid *committed;
    size_t  committed_count;
    size_t  committed_cap;
    edb_xid *aborted;
    size_t  aborted_count;
    size_t  aborted_cap;
} edb_mvcc_mgr;

edb_mvcc_mgr *edb_mvcc_create(void);
void          edb_mvcc_destroy(edb_mvcc_mgr *m);

edb_txn *edb_mvcc_begin(edb_mvcc_mgr *m, bool read_only, edb_error *err);
int      edb_mvcc_commit(edb_mvcc_mgr *m, edb_txn *txn, edb_error *err);
int      edb_mvcc_abort(edb_mvcc_mgr *m, edb_txn *txn, edb_error *err);
void     edb_mvcc_txn_free(edb_txn *txn);

/* Visibility: is version created by xmin visible to snapshot? */
bool edb_mvcc_visible(const edb_mvcc_mgr *m, const edb_snapshot *snap,
                      edb_xid xmin, edb_xid xmax);

/* Oldest xmin among active snapshots; versions with xmax < horizon may be purged */
edb_xid edb_mvcc_horizon(const edb_mvcc_mgr *m);
int     edb_mvcc_gc_note(edb_mvcc_mgr *m, edb_xid purged_upto);

#endif
