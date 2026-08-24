#include "edb/mvcc.h"
#include <stdlib.h>
#include <string.h>

edb_mvcc_mgr *edb_mvcc_create(void) {
    edb_mvcc_mgr *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->next_xid = 1;
    m->active_cap = 32;
    m->active = calloc(m->active_cap, sizeof(edb_xid));
    m->committed_cap = 64;
    m->committed = calloc(m->committed_cap, sizeof(edb_xid));
    m->aborted_cap = 64;
    m->aborted = calloc(m->aborted_cap, sizeof(edb_xid));
    if (!m->active || !m->committed || !m->aborted) {
        free(m->active); free(m->committed); free(m->aborted); free(m);
        return NULL;
    }
    return m;
}

void edb_mvcc_destroy(edb_mvcc_mgr *m) {
    if (!m) return;
    free(m->active); free(m->committed); free(m->aborted); free(m);
}

static int xid_in(const edb_xid *arr, size_t n, edb_xid x) {
    for (size_t i = 0; i < n; i++) if (arr[i] == x) return 1;
    return 0;
}

static void remove_active(edb_mvcc_mgr *m, edb_xid x) {
    for (size_t i = 0; i < m->active_count; i++) {
        if (m->active[i] == x) {
            m->active[i] = m->active[m->active_count - 1];
            m->active_count--;
            return;
        }
    }
}

edb_txn *edb_mvcc_begin(edb_mvcc_mgr *m, bool read_only, edb_error *err) {
    edb_txn *t = calloc(1, sizeof(*t));
    if (!t) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return NULL; }
    t->xid = m->next_xid++;
    t->state = EDB_TXN_ACTIVE;
    t->read_only = read_only;
    /* snapshot: xmax = next_xid after assign; active = copy of current active */
    t->snap.xmax = m->next_xid; /* boundary: xids >= xmax not visible unless own */
    t->snap.xmin = t->xid;
    for (size_t i = 0; i < m->active_count; i++) {
        if (m->active[i] < t->snap.xmin) t->snap.xmin = m->active[i];
    }
    t->snap.active_count = m->active_count;
    if (t->snap.active_count) {
        t->snap.active = malloc(t->snap.active_count * sizeof(edb_xid));
        if (!t->snap.active) {
            free(t); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return NULL;
        }
        memcpy(t->snap.active, m->active, t->snap.active_count * sizeof(edb_xid));
    }
    if (!read_only) {
        if (m->active_count >= m->active_cap) {
            size_t nc = m->active_cap * 2;
            edb_xid *na = realloc(m->active, nc * sizeof(edb_xid));
            if (!na) { edb_mvcc_txn_free(t); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return NULL; }
            m->active = na; m->active_cap = nc;
        }
        m->active[m->active_count++] = t->xid;
    }
    return t;
}

int edb_mvcc_commit(edb_mvcc_mgr *m, edb_txn *txn, edb_error *err) {
    if (!txn || txn->state != EDB_TXN_ACTIVE) {
        edb_error_set(err, EDB_TRANSACTION_STATE, 0, "not active");
        return -1;
    }
    if (m->committed_count >= m->committed_cap) {
        size_t nc = m->committed_cap * 2;
        edb_xid *na = realloc(m->committed, nc * sizeof(edb_xid));
        if (!na) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
        m->committed = na; m->committed_cap = nc;
    }
    m->committed[m->committed_count++] = txn->xid;
    remove_active(m, txn->xid);
    txn->state = EDB_TXN_COMMITTED;
    return 0;
}

int edb_mvcc_abort(edb_mvcc_mgr *m, edb_txn *txn, edb_error *err) {
    if (!txn || txn->state != EDB_TXN_ACTIVE) {
        edb_error_set(err, EDB_TRANSACTION_STATE, 0, "not active");
        return -1;
    }
    if (m->aborted_count >= m->aborted_cap) {
        size_t nc = m->aborted_cap * 2;
        edb_xid *na = realloc(m->aborted, nc * sizeof(edb_xid));
        if (!na) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
        m->aborted = na; m->aborted_cap = nc;
    }
    m->aborted[m->aborted_count++] = txn->xid;
    remove_active(m, txn->xid);
    txn->state = EDB_TXN_ABORTED;
    return 0;
}

void edb_mvcc_txn_free(edb_txn *txn) {
    if (!txn) return;
    free(txn->snap.active);
    free(txn);
}

bool edb_mvcc_visible(const edb_mvcc_mgr *m, const edb_snapshot *snap,
                      edb_xid xmin, edb_xid xmax) {
    /* Version created by xmin, deleted/superseded by xmax (0 = not deleted).
     * Visible if creator committed before snapshot and not deleted before snapshot. */
    if (xmin == 0) return false;
    /* creator still active at snapshot time? */
    if (xid_in(snap->active, snap->active_count, xmin)) return false;
    if (xmin >= snap->xmax) return false;
    /* aborted? */
    if (xid_in(m->aborted, m->aborted_count, xmin)) return false;
    /* must be committed (or be special bootstrap xid) */
    if (xmin > 0 && !xid_in(m->committed, m->committed_count, xmin)) {
        /* if xmin < snap.xmin and not aborted, treat as committed for bootstrap */
        if (!(xmin < snap->xmin)) return false;
    }
    /* deleted? */
    if (xmax != 0) {
        if (xid_in(snap->active, snap->active_count, xmax)) {
            /* deleter still active — version still visible */
        } else if (xmax < snap->xmax && xid_in(m->committed, m->committed_count, xmax)) {
            return false; /* deleted by committed txn in the past */
        }
    }
    return true;
}


edb_xid edb_mvcc_horizon(const edb_mvcc_mgr *m) {
    if (!m) return 0;
    edb_xid h = m->next_xid;
    for (size_t i = 0; i < m->active_count; i++)
        if (m->active[i] < h) h = m->active[i];
    return h;
}

int edb_mvcc_gc_note(edb_mvcc_mgr *m, edb_xid purged_upto) {
    (void)m; (void)purged_upto;
    return 0;
}
