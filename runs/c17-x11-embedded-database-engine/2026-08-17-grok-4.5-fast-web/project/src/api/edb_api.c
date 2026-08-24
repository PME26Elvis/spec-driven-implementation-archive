#include "edb/edb_api.h"
#include "edb/pager.h"
#include "edb/btree.h"
#include "edb/sql_lexer.h"
#include "edb/sql_parser.h"
#include "edb/schema.h"
#include "edb/pbkdf2.h"
#include "edb/byteorder.h"
#include "edb/composite_key.h"
#include "edb/wal.h"
#include "edb/mvcc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

typedef struct edb_undo_entry {
    char table[128];
    uint64_t rowid;
    /* optional before-image for UPDATE/DELETE (not always filled) */
    uint8_t *before;
    uint16_t before_len;
    int kind; /* 1=insert 2=update 3=delete */
} edb_undo_entry;

struct edb_db {
    edb_pager   *pager;
    edb_catalog *catalog;
    edb_wal     *wal;
    edb_mvcc_mgr *mvcc;
    edb_txn     *txn;
    uint8_t      key[32];
    bool         has_key;
    bool         in_txn;
    char        *path;
    edb_undo_entry *undo;
    size_t undo_count;
    size_t undo_cap;
};

struct edb_stmt {
    edb_db *db;
    edb_ast *ast;
    /* simple result buffer for SELECT */
    int row_count;
    int col_count;
    char **col_names;
    edb_value *rows; /* row_count * col_count */
    int cur_row;
};

static int undo_push(edb_db *db, const char *table, uint64_t rowid, int kind,
                     const uint8_t *before, uint16_t before_len) {
    if (!db->in_txn) return 0;
    if (db->undo_count >= db->undo_cap) {
        size_t nc = db->undo_cap ? db->undo_cap * 2 : 32;
        edb_undo_entry *n = realloc(db->undo, nc * sizeof(*n));
        if (!n) return -1;
        db->undo = n; db->undo_cap = nc;
    }
    edb_undo_entry *e = &db->undo[db->undo_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->table, table, sizeof e->table - 1);
    e->rowid = rowid;
    e->kind = kind;
    if (before && before_len) {
        e->before = malloc(before_len);
        if (e->before) { memcpy(e->before, before, before_len); e->before_len = before_len; }
    }
    return 0;
}

static void undo_clear(edb_db *db) {
    for (size_t i = 0; i < db->undo_count; i++) free(db->undo[i].before);
    free(db->undo);
    db->undo = NULL; db->undo_count = 0; db->undo_cap = 0;
}


edb_db *edb_open(const char *path, bool create, bool read_only,
                 const char *password_or_null, edb_error *err) {
    edb_error_clear(err);
    uint8_t key[32];
    const uint8_t *keyp = NULL;
    if (password_or_null && password_or_null[0]) {
        uint8_t salt[16] = {0};
        if (edb_pbkdf2_hmac_sha256((const uint8_t*)password_or_null,
                strlen(password_or_null), salt, 16, 100000, key, 32) != 0) {
            edb_error_set(err, EDB_INTERNAL_INVARIANT, 0, "kdf failed");
            return NULL;
        }
        keyp = key;
    }
    edb_pager *pager = edb_pager_open(path, create, read_only, keyp, err);
    if (!pager) return NULL;
    edb_db *db = calloc(1, sizeof(*db));
    if (!db) {
        edb_pager_close(pager);
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }
    db->pager = pager;
    if (keyp) { memcpy(db->key, key, 32); db->has_key = true; }
    db->catalog = edb_catalog_open(pager, err);
    if (!db->catalog) {
        edb_pager_close(pager);
        free(db);
        return NULL;
    }
    db->path = strdup(path);
    db->wal = edb_wal_open(path, true, err);
    if (!db->wal) {
        /* non-fatal for create path if WAL open fails hard — still try */
        edb_error_clear(err);
    } else if (!create) {
        if (edb_wal_recover(db->wal, pager, err) != 0) {
            /* recovery failure is fatal on open */
            edb_wal_close(db->wal);
            edb_catalog_close(db->catalog);
            edb_pager_close(pager);
            free(db->path); free(db);
            return NULL;
        }
    }
    db->mvcc = edb_mvcc_create();
    return db;
}

void edb_close(edb_db *db) {
    if (!db) return;
    if (db->catalog) {
        if (db->catalog->dirty)
            edb_catalog_save(db->catalog, &(edb_error){0});
        edb_catalog_close(db->catalog);
    }
    if (db->txn) { edb_mvcc_txn_free(db->txn); db->txn = NULL; }
    undo_clear(db);
    if (db->mvcc) edb_mvcc_destroy(db->mvcc);
    if (db->wal) {
        edb_wal_checkpoint(db->wal, db->pager, &(edb_error){0});
        edb_wal_close(db->wal);
    }
    edb_pager_close(db->pager);
    edb_secure_zero(db->key, 32);
    free(db->path);
    free(db);
}

int edb_begin(edb_db *db, edb_error *err) {
    if (db->in_txn) {
        edb_error_set(err, EDB_TRANSACTION_STATE, 0, "already in transaction");
        return -1;
    }
    if (db->mvcc) {
        db->txn = edb_mvcc_begin(db->mvcc, false, err);
        if (!db->txn) return -1;
        if (db->wal && edb_wal_begin(db->wal, db->txn->xid, err) != 0) {
            edb_mvcc_abort(db->mvcc, db->txn, err);
            edb_mvcc_txn_free(db->txn);
            db->txn = NULL;
            return -1;
        }
    }
    db->in_txn = true;
    return 0;
}

int edb_commit(edb_db *db, edb_error *err) {
    if (!db->in_txn) {
        edb_error_set(err, EDB_TRANSACTION_STATE, 0, "not in transaction");
        return -1;
    }
    if (db->catalog->dirty && edb_catalog_save(db->catalog, err) != 0) return -1;
    if (edb_pager_sync(db->pager, err) != 0) return -1;
    if (db->txn) {
        if (db->wal && edb_wal_commit(db->wal, db->txn->xid, err) != 0) return -1;
        if (db->mvcc && edb_mvcc_commit(db->mvcc, db->txn, err) != 0) return -1;
        edb_mvcc_txn_free(db->txn);
        db->txn = NULL;
    }
    undo_clear(db);
    db->in_txn = false;
    return 0;
}

int edb_rollback(edb_db *db, edb_error *err) {
    edb_error_clear(err);
    /* Apply undo log in reverse: remove inserts */
    for (size_t i = db->undo_count; i > 0; i--) {
        edb_undo_entry *e = &db->undo[i - 1];
        edb_table *tb = edb_catalog_find_table(db->catalog, e->table);
        if (!tb) continue;
        if (e->kind == 1) { /* insert -> delete rowid */
            edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
            if (!t) continue;
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, e->rowid);
            edb_key key = { .data = keybuf, .len = 8 };
            edb_btree_delete(t, &key, err);
            tb->data_root = t->root_page;
            edb_btree_close(t);
        } else if (e->kind == 2 && e->before) { /* update -> restore before image */
            edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
            if (!t) continue;
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, e->rowid);
            edb_key key = { .data = keybuf, .len = 8 };
            edb_btree_delete(t, &key, err);
            edb_btree_insert(t, &key, e->before, e->before_len, err);
            tb->data_root = t->root_page;
            edb_btree_close(t);
        }
    }
    db->catalog->dirty = true;
    edb_catalog_save(db->catalog, err);
    if (db->txn) {
        if (db->wal) edb_wal_abort(db->wal, db->txn->xid, err);
        if (db->mvcc) edb_mvcc_abort(db->mvcc, db->txn, err);
        edb_mvcc_txn_free(db->txn);
        db->txn = NULL;
    }
    undo_clear(db);
    db->in_txn = false;
    return 0;
}


/* Serialize a row as: col_count u16 | for each: type u8 | payload */


int edb_savepoint(edb_db *db, const char *name, edb_error *err) {
    if (!db->in_txn) {
        edb_error_set(err, EDB_CONSTRAINT, 0, "no active transaction");
        return -1;
    }
    /* marker kind=0, table field holds name */
    return undo_push(db, name, 0, 0, NULL, 0);
}

int edb_release_savepoint(edb_db *db, const char *name, edb_error *err) {
    if (!db->in_txn) {
        edb_error_set(err, EDB_CONSTRAINT, 0, "no active transaction");
        return -1;
    }
    for (size_t i = db->undo_count; i > 0; i--) {
        edb_undo_entry *e = &db->undo[i - 1];
        if (e->kind == 0 && strcmp(e->table, name) == 0) {
            /* drop marker only */
            free(e->before);
            memmove(e, e + 1, (db->undo_count - i) * sizeof(*e));
            db->undo_count--;
            return 0;
        }
    }
    edb_error_set(err, EDB_NOT_FOUND, 0, "savepoint not found");
    return -1;
}

int edb_rollback_to(edb_db *db, const char *name, edb_error *err) {
    if (!db->in_txn) {
        edb_error_set(err, EDB_CONSTRAINT, 0, "no active transaction");
        return -1;
    }
    ssize_t mark = -1;
    for (size_t i = db->undo_count; i > 0; i--) {
        edb_undo_entry *e = &db->undo[i - 1];
        if (e->kind == 0 && strcmp(e->table, name) == 0) { mark = (ssize_t)(i - 1); break; }
    }
    if (mark < 0) {
        edb_error_set(err, EDB_NOT_FOUND, 0, "savepoint not found");
        return -1;
    }
    /* undo entries after marker */
    for (size_t i = db->undo_count; i > (size_t)mark + 1; i--) {
        edb_undo_entry *e = &db->undo[i - 1];
        if (e->kind == 1) {
            edb_table *tb = edb_catalog_find_table(db->catalog, e->table);
            if (tb) {
                edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
                if (t) {
                    uint8_t keybuf[8];
                    edb_store_u64_le(keybuf, e->rowid);
                    edb_key key = { .data = keybuf, .len = 8 };
                    edb_btree_delete(t, &key, err);
                    tb->data_root = t->root_page;
                    edb_btree_close(t);
                    edb_error_clear(err);
                }
            }
        }
        free(e->before);
    }
    db->undo_count = (size_t)mark + 1; /* keep marker */
    return 0;
}

static int encode_row_mvcc(const edb_value *vals, int n, uint8_t *buf, size_t cap,
                           uint64_t xmin, uint64_t xmax) {
    size_t pos = 0;
    if (pos + 18 > cap) return -1;
    buf[pos++] = 'R'; buf[pos++] = 'V';
    edb_store_u64_le(buf + pos, xmin); pos += 8;
    edb_store_u64_le(buf + pos, xmax); pos += 8;
    if (pos + 2 > cap) return -1;
    edb_store_u16_le(buf + pos, (uint16_t)n); pos += 2;
    for (int i = 0; i < n; i++) {
        if (pos + 1 > cap) return -1;
        buf[pos++] = (uint8_t)vals[i].type;
        switch (vals[i].type) {
        case EDB_VAL_NULL: break;
        case EDB_VAL_INTEGER:
            if (pos + 8 > cap) return -1;
            edb_store_u64_le(buf + pos, (uint64_t)vals[i].u.i64); pos += 8;
            break;
        case EDB_VAL_REAL: {
            if (pos + 8 > cap) return -1;
            uint64_t bits; memcpy(&bits, &vals[i].u.real, 8);
            edb_store_u64_le(buf + pos, bits); pos += 8;
            break;
        }
        case EDB_VAL_TEXT:
        case EDB_VAL_BLOB:
            if (pos + 4 + vals[i].u.bin.len > cap) return -1;
            edb_store_u32_le(buf + pos, vals[i].u.bin.len); pos += 4;
            memcpy(buf + pos, vals[i].u.bin.p, vals[i].u.bin.len); pos += vals[i].u.bin.len;
            break;
        }
    }
    return (int)pos;
}

static int decode_row_mvcc(const uint8_t *buf, size_t len, edb_value *vals, int maxn, int *out_n,
                            uint64_t *out_xmin, uint64_t *out_xmax) {
    size_t pos = 0;
    uint64_t xmin = 1, xmax = 0;
    if (len >= 18 && buf[0] == 'R' && buf[1] == 'V') {
        xmin = edb_load_u64_le(buf + 2);
        xmax = edb_load_u64_le(buf + 10);
        pos = 18;
    }
    if (out_xmin) *out_xmin = xmin;
    if (out_xmax) *out_xmax = xmax;
    if (pos + 2 > len) return -1;
    int n = edb_load_u16_le(buf + pos); pos += 2;
    if (n > maxn) return -1;
    for (int i = 0; i < n; i++) {
        if (pos + 1 > len) return -1;
        vals[i].type = (edb_val_type)buf[pos++];
        switch (vals[i].type) {
        case EDB_VAL_NULL: break;
        case EDB_VAL_INTEGER:
            if (pos + 8 > len) return -1;
            vals[i].u.i64 = (int64_t)edb_load_u64_le(buf + pos); pos += 8;
            break;
        case EDB_VAL_REAL: {
            if (pos + 8 > len) return -1;
            uint64_t bits = edb_load_u64_le(buf + pos);
            memcpy(&vals[i].u.real, &bits, 8); pos += 8;
            break;
        }
        case EDB_VAL_TEXT:
        case EDB_VAL_BLOB: {
            if (pos + 4 > len) return -1;
            uint32_t l = edb_load_u32_le(buf + pos); pos += 4;
            if (pos + l > len) return -1;
            /* allocate copy so it outlives the page */
            uint8_t *cp = malloc(l + 1);
            if (!cp) return -1;
            memcpy(cp, buf + pos, l); cp[l] = 0;
            vals[i].u.bin.p = cp;
            vals[i].u.bin.len = l;
            pos += l;
            break;
        }
        default: return -1;
        }
    }
    *out_n = n;
    return 0;
}


static int encode_row(const edb_value *vals, int n, uint8_t *buf, size_t cap) {
    return encode_row_mvcc(vals, n, buf, cap, 1, 0);
}
static int decode_row(const uint8_t *buf, size_t len, edb_value *vals, int maxn, int *out_n) {
    return decode_row_mvcc(buf, len, vals, maxn, out_n, NULL, NULL);
}

static int exec_insert(edb_db *db, edb_ast *ast, edb_error *err) {
    edb_table *tb = edb_catalog_find_table(db->catalog, ast->table);
    if (!tb) {
        edb_error_set(err, EDB_NOT_FOUND, 0, "table not found");
        return -1;
    }
    if (ast->value_count != tb->col_count) {
        edb_error_set(err, EDB_CONSTRAINT, 0, "column count mismatch");
        return -1;
    }
    for (int i = 0; i < tb->col_count; i++) {
        if (tb->cols[i].not_null && ast->values[i].type == EDB_VAL_NULL) {
            edb_error_set(err, EDB_CONSTRAINT, 0, "NOT NULL violation");
            return -1;
        }
    }
    uint8_t rowbuf[2048];
    uint64_t xmin = (db->txn) ? db->txn->xid : 1;
    int nrows = ast->insert_nrows > 0 ? ast->insert_nrows : 1;
    for (int row = 0; row < nrows; row++) {
    edb_value *rv = &ast->values[row * ast->value_count];
    uint64_t rowid = tb->next_rowid++;
    uint8_t keybuf[8];
    edb_store_u64_le(keybuf, rowid);
    edb_key key = { .data = keybuf, .len = 8 };
    int rlen = encode_row_mvcc(rv, ast->value_count, rowbuf, sizeof rowbuf, xmin, 0);
    if (rlen < 0) {
        edb_error_set(err, EDB_LIMIT, 0, "row too large");
        return -1;
    }

    edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
    if (!t) return -1;
    int rc = edb_btree_insert(t, &key, rowbuf, (uint16_t)rlen, err);
    /* update root if grown */
    tb->data_root = t->root_page;
    edb_btree_close(t);
    if (rc != 0) return -1;
    if (undo_push(db, tb->name, rowid, 1, NULL, 0) != 0) {
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "undo oom");
        return -1;
    }

    /* update secondary indexes */
    for (int i = 0; i < db->catalog->index_count; i++) {
        edb_index *ix = &db->catalog->indexes[i];
        if (strcasecmp(ix->table, tb->name) != 0) continue;
        edb_composite_key ck = {0};
        ck.n = ix->col_count;
        ck.has_rowid = !ix->unique;
        ck.rowid = rowid;
        for (int c = 0; c < ix->col_count; c++) {
            int ci = ix->col_idxs[c];
            ck.comps[c] = rv[ci];
        }
        uint8_t ikey[EDB_MAX_KEY_BYTES];
        int klen = edb_composite_encode(&ck, ikey, sizeof ikey);
        if (klen < 0) {
            edb_error_set(err, EDB_LIMIT, 0, "index key too large");
            return -1;
        }
        /* uniqueness check for unique indexes without NULL */
        if (ix->unique) {
            /* simplified: try insert; btree unique will reject exact key match */
        }
        edb_btree *it = edb_btree_open(db->pager, ix->root, ix->unique, err);
        if (!it) return -1;
        edb_key ik = { .data = ikey, .len = (uint16_t)klen };
        uint8_t rowidbuf[8];
        edb_store_u64_le(rowidbuf, rowid);
        rc = edb_btree_insert(it, &ik, rowidbuf, 8, err);
        ix->root = it->root_page;
        edb_btree_close(it);
        if (rc != 0) return -1;
    }

    } /* end multi-row */
    db->catalog->dirty = true;
    if (!db->in_txn)
        return edb_catalog_save(db->catalog, err);
    return 0;
}

/* Very simple full table scan SELECT * */

/* Try resolve equality WHERE via secondary index: returns rowids packed, count; -1 on fail */
static int index_eq_lookup(edb_db *db, edb_table *tb, const char *col, const edb_value *lit,
                           uint64_t *out_rids, int max_rids, edb_error *err) {
    for (int i = 0; i < db->catalog->index_count; i++) {
        edb_index *ix = &db->catalog->indexes[i];
        if (strcasecmp(ix->table, tb->name) != 0) continue;
        if (ix->col_count < 1 || strcasecmp(ix->col_names[0], col) != 0) continue;
        if (ix->root == 0) continue;
        edb_composite_key ck = {0};
        ck.n = 1;
        ck.comps[0] = *lit;
        ck.has_rowid = false;
        uint8_t kbuf[EDB_MAX_KEY_BYTES];
        int klen = edb_composite_encode(&ck, kbuf, sizeof kbuf);
        if (klen < 0) continue;
        edb_key key = { .data = kbuf, .len = (uint16_t)klen };
        edb_btree *ibt = edb_btree_open(db->pager, ix->root, ix->unique, err);
        if (!ibt) { edb_error_clear(err); continue; }
        uint8_t vbuf[64];
        uint16_t vlen = sizeof vbuf;
        int rc = edb_btree_get(ibt, &key, vbuf, &vlen, err);
        edb_btree_close(ibt);
        if (rc != 0) { edb_error_clear(err); return 0; }
        if (vlen >= 8 && max_rids > 0) {
            out_rids[0] = edb_load_u64_le(vbuf);
            return 1;
        }
        return 0;
    }
    return -1;
}

static int exec_select(edb_db *db, edb_ast *ast, edb_stmt *stmt, edb_error *err) {
    edb_table *tb = edb_catalog_find_table(db->catalog, ast->table);
    if (!tb) {
        edb_error_set(err, EDB_NOT_FOUND, 0, "table not found");
        return -1;
    }
    if (ast->has_join) {
        edb_table *tb2 = edb_catalog_find_table(db->catalog, ast->join_table);
        if (!tb2) { edb_error_set(err, EDB_NOT_FOUND, 0, "join table not found"); return -1; }
        int lc = -1, rc = -1;
        for (int c = 0; c < tb->col_count; c++)
            if (strcasecmp(tb->cols[c].name, ast->join_left_col) == 0) { lc = c; break; }
        for (int c = 0; c < tb2->col_count; c++)
            if (strcasecmp(tb2->cols[c].name, ast->join_right_col) == 0) { rc = c; break; }
        if (lc < 0 || rc < 0) { edb_error_set(err, EDB_NOT_FOUND, 0, "join column not found"); return -1; }
        int out_cols = tb->col_count + tb2->col_count;
        int max_rows = 4096;
        stmt->col_count = out_cols;
        stmt->col_names = calloc((size_t)out_cols, sizeof(char*));
        for (int c = 0; c < tb->col_count; c++) stmt->col_names[c] = strdup(tb->cols[c].name);
        for (int c = 0; c < tb2->col_count; c++) stmt->col_names[tb->col_count + c] = strdup(tb2->cols[c].name);
        stmt->rows = calloc((size_t)max_rows * (size_t)out_cols, sizeof(edb_value));
        edb_btree *t1 = edb_btree_open(db->pager, tb->data_root, true, err);
        edb_btree *t2 = edb_btree_open(db->pager, tb2->data_root, true, err);
        if (!t1 || !t2) { if (t1) edb_btree_close(t1); if (t2) edb_btree_close(t2); return -1; }
        int got = 0;
        for (uint64_t r1 = 1; r1 < tb->next_rowid && got < max_rows; r1++) {
            int matched = 0;
            uint8_t kb1[8]; edb_store_u64_le(kb1, r1);
            edb_key k1 = { .data = kb1, .len = 8 };
            uint8_t rb1[2048]; uint16_t rl1 = sizeof rb1;
            if (edb_btree_get(t1, &k1, rb1, &rl1, err) != 0) { edb_error_clear(err); continue; }
            edb_value v1[EDB_MAX_COLS]; int n1 = 0;
            if (decode_row(rb1, rl1, v1, EDB_MAX_COLS, &n1) != 0) continue;
            for (uint64_t r2 = 1; r2 < tb2->next_rowid && got < max_rows; r2++) {
                uint8_t kb2[8]; edb_store_u64_le(kb2, r2);
                edb_key k2 = { .data = kb2, .len = 8 };
                uint8_t rb2[2048]; uint16_t rl2 = sizeof rb2;
                if (edb_btree_get(t2, &k2, rb2, &rl2, err) != 0) { edb_error_clear(err); continue; }
                edb_value v2[EDB_MAX_COLS]; int n2 = 0;
                if (decode_row(rb2, rl2, v2, EDB_MAX_COLS, &n2) != 0) continue;
                int match = 0;
                if (lc < n1 && rc < n2 && v1[lc].type == EDB_VAL_INTEGER && v2[rc].type == EDB_VAL_INTEGER)
                    match = (v1[lc].u.i64 == v2[rc].u.i64);
                if (!match) {
                    for (int c = 0; c < n2; c++)
                        if (v2[c].type == EDB_VAL_TEXT || v2[c].type == EDB_VAL_BLOB) free((void*)v2[c].u.bin.p);
                    continue;
                }
                for (int c = 0; c < n1; c++) {
                    stmt->rows[got * out_cols + c] = v1[c];
                    if (v1[c].type == EDB_VAL_TEXT || v1[c].type == EDB_VAL_BLOB) {
                        uint32_t L = v1[c].u.bin.len;
                        uint8_t *cp = malloc(L ? L : 1);
                        if (cp && L) memcpy(cp, v1[c].u.bin.p, L);
                        stmt->rows[got * out_cols + c].u.bin.p = cp;
                    }
                }
                for (int c = 0; c < n2; c++) {
                    stmt->rows[got * out_cols + tb->col_count + c] = v2[c];
                    if (v2[c].type == EDB_VAL_TEXT || v2[c].type == EDB_VAL_BLOB) {
                        uint32_t L = v2[c].u.bin.len;
                        uint8_t *cp = malloc(L ? L : 1);
                        if (cp && L) memcpy(cp, v2[c].u.bin.p, L);
                        stmt->rows[got * out_cols + tb->col_count + c].u.bin.p = cp;
                    }
                }
                for (int c = 0; c < n2; c++)
                    if (v2[c].type == EDB_VAL_TEXT || v2[c].type == EDB_VAL_BLOB)
                        free((void*)v2[c].u.bin.p);
                matched = 1;
                got++;
            }
            if (!matched && ast->is_left_join && got < max_rows) {
                for (int c = 0; c < n1; c++) {
                    stmt->rows[got * out_cols + c] = v1[c];
                    if (v1[c].type == EDB_VAL_TEXT || v1[c].type == EDB_VAL_BLOB) {
                        uint32_t L = v1[c].u.bin.len;
                        uint8_t *cp = malloc(L ? L : 1);
                        if (cp && L) memcpy(cp, v1[c].u.bin.p, L);
                        stmt->rows[got * out_cols + c].u.bin.p = cp;
                    }
                }
                for (int c = 0; c < tb2->col_count; c++)
                    stmt->rows[got * out_cols + tb->col_count + c].type = EDB_VAL_NULL;
                got++;
            }
            for (int c = 0; c < n1; c++)
                if (v1[c].type == EDB_VAL_TEXT || v1[c].type == EDB_VAL_BLOB)
                    free((void*)v1[c].u.bin.p);
        }
        edb_btree_close(t1); edb_btree_close(t2);
        stmt->row_count = got;
        stmt->cur_row = -1;
        return 0;
    }
    if (ast->is_count_star && !ast->has_group) {
        int cnt = 0;
        edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
        if (!t) return -1;
        for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rid);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[16];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(t, &key, rowbuf, &rlen, err) == 0) cnt++;
            else edb_error_clear(err);
        }
        edb_btree_close(t);
        stmt->col_count = 1;
        stmt->col_names = calloc(1, sizeof(char*));
        stmt->col_names[0] = strdup("COUNT(*)");
        stmt->row_count = 1;
        stmt->rows = calloc(1, sizeof(edb_value));
        stmt->rows[0].type = EDB_VAL_INTEGER;
        stmt->rows[0].u.i64 = cnt;
        stmt->cur_row = -1;
        return 0;
    }
    if (ast->is_count_star && ast->has_group) {
        int gcol = -1;
        for (int c = 0; c < tb->col_count; c++)
            if (strcasecmp(tb->cols[c].name, ast->group_col) == 0) { gcol = c; break; }
        if (gcol < 0) { edb_error_set(err, EDB_NOT_FOUND, 0, "group column not found"); return -1; }
        /* collect groups: fixed max 256 groups */
        typedef struct { edb_value key; int64_t cnt; } grp_t;
        grp_t grps[256];
        int ng = 0;
        edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
        if (!t) return -1;
        for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rid);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[2048];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
            edb_value vals[EDB_MAX_COLS];
            int n = 0;
            if (decode_row(rowbuf, rlen, vals, EDB_MAX_COLS, &n) != 0) continue;
            if (gcol >= n) continue;
            edb_value *gk = &vals[gcol];
            int found = -1;
            for (int g = 0; g < ng; g++) {
                if (grps[g].key.type == gk->type && gk->type == EDB_VAL_INTEGER &&
                    grps[g].key.u.i64 == gk->u.i64) { found = g; break; }
                if (grps[g].key.type == gk->type && gk->type == EDB_VAL_TEXT &&
                    grps[g].key.u.bin.len == gk->u.bin.len &&
                    memcmp(grps[g].key.u.bin.p, gk->u.bin.p, gk->u.bin.len)==0) { found = g; break; }
            }
            if (found >= 0) grps[found].cnt++;
            else if (ng < 256) {
                grps[ng].key = *gk;
                if (gk->type == EDB_VAL_TEXT && gk->u.bin.len) {
                    uint8_t *cp = malloc(gk->u.bin.len);
                    memcpy(cp, gk->u.bin.p, gk->u.bin.len);
                    grps[ng].key.u.bin.p = cp;
                }
                grps[ng].cnt = 1;
                ng++;
            }
            for (int c = 0; c < n; c++)
                if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                    free((void*)vals[c].u.bin.p);
        }
        edb_btree_close(t);
        /* HAVING filter */
        int out_n = 0;
        for (int g = 0; g < ng; g++) {
            if (ast->has_having && ast->having_lit.type == EDB_VAL_INTEGER) {
                int64_t hv = ast->having_lit.u.i64;
                int ok = 1;
                if (ast->having_op == EDB_TOK_GT) ok = grps[g].cnt > hv;
                else if (ast->having_op == EDB_TOK_LT) ok = grps[g].cnt < hv;
                else if (ast->having_op == EDB_TOK_EQ) ok = grps[g].cnt == hv;
                if (!ok) continue;
            }
            grps[out_n++] = grps[g];
        }
        ng = out_n;
        stmt->col_count = 2;
        stmt->col_names = calloc(2, sizeof(char*));
        stmt->col_names[0] = strdup(ast->group_col);
        stmt->col_names[1] = strdup("COUNT(*)");
        stmt->row_count = ng;
        stmt->rows = calloc((size_t)ng * 2, sizeof(edb_value));
        for (int g = 0; g < ng; g++) {
            stmt->rows[g*2] = grps[g].key;
            stmt->rows[g*2+1].type = EDB_VAL_INTEGER;
            stmt->rows[g*2+1].u.i64 = grps[g].cnt;
        }
        stmt->cur_row = -1;
        return 0;
    }
    if (ast->is_sum || ast->is_avg || ast->is_min || ast->is_max) {
        int scol = -1;
        for (int c = 0; c < tb->col_count; c++)
            if (strcasecmp(tb->cols[c].name, ast->sum_col) == 0) { scol = c; break; }
        if (scol < 0) { edb_error_set(err, EDB_NOT_FOUND, 0, "aggregate column not found"); return -1; }
        int64_t total = 0, mn = 0, mx = 0;
        int cnt = 0;
        edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
        if (!t) return -1;
        for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rid);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[2048];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
            edb_value vals[EDB_MAX_COLS];
            int n = 0;
            if (decode_row(rowbuf, rlen, vals, EDB_MAX_COLS, &n) != 0) continue;
            /* apply WHERE on aggregate scan */
            if (ast->where && ast->where->kind == EXPR_BINOP && ast->where->left && ast->where->right) {
                int wci = -1;
                for (int c = 0; c < tb->col_count; c++)
                    if (strcasecmp(tb->cols[c].name, ast->where->left->name) == 0) { wci = c; break; }
                if (wci >= 0 && wci < n) {
                    edb_value *wcv = &vals[wci];
                    edb_value *wlv = &ast->where->right->lit;
                    int wmatch = 0;
                    if (wcv->type == EDB_VAL_INTEGER && wlv->type == EDB_VAL_INTEGER)
                        wmatch = (wcv->u.i64 == wlv->u.i64);
                    else if (wcv->type == EDB_VAL_TEXT && wlv->type == EDB_VAL_TEXT)
                        wmatch = (wcv->u.bin.len == wlv->u.bin.len &&
                                  memcmp(wcv->u.bin.p, wlv->u.bin.p, wcv->u.bin.len)==0);
                    if (!wmatch) {
                        for (int c = 0; c < n; c++)
                            if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                                free((void*)vals[c].u.bin.p);
                        continue;
                    }
                }
            }
            if (scol < n && vals[scol].type == EDB_VAL_INTEGER) {
                int64_t v = vals[scol].u.i64;
                total += v;
                if (cnt == 0 || v < mn) mn = v;
                if (cnt == 0 || v > mx) mx = v;
                cnt++;
            }
            for (int c = 0; c < n; c++)
                if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                    free((void*)vals[c].u.bin.p);
        }
        edb_btree_close(t);
        stmt->col_count = 1;
        stmt->col_names = calloc(1, sizeof(char*));
        const char *nm = ast->is_sum ? "SUM" : ast->is_avg ? "AVG" : ast->is_min ? "MIN" : "MAX";
        stmt->col_names[0] = strdup(nm);
        stmt->row_count = 1;
        stmt->rows = calloc(1, sizeof(edb_value));
        stmt->rows[0].type = EDB_VAL_INTEGER;
        if (ast->is_sum) stmt->rows[0].u.i64 = total;
        else if (ast->is_avg) stmt->rows[0].u.i64 = cnt ? total / cnt : 0;
        else if (ast->is_min) stmt->rows[0].u.i64 = mn;
        else stmt->rows[0].u.i64 = mx;
        stmt->cur_row = -1;
        return 0;
    }
    stmt->col_count = tb->col_count;
    stmt->col_names = calloc(tb->col_count, sizeof(char*));
    for (int i = 0; i < tb->col_count; i++)
        stmt->col_names[i] = strdup(tb->cols[i].name);

    /* scan by probing rowids 1..next_rowid-1 (simple, not production scan) */
    int max_rows = (int)(tb->next_rowid - 1);
    if (max_rows < 0) max_rows = 0;
    if (ast->has_limit && ast->limit < max_rows) max_rows = ast->limit;
    stmt->rows = calloc((size_t)max_rows * (size_t)tb->col_count, sizeof(edb_value));
    if (!stmt->rows && max_rows > 0) {
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return -1;
    }
    edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
    if (!t) return -1;
    int got = 0;
    int skipped = 0;
    int need_skip = (ast->has_offset && ast->offset > 0) ? ast->offset : 0;
    /* Secondary-index equality fast path */
    if (ast->where && ast->where->kind == EXPR_BINOP && ast->where->op == EDB_TOK_EQ &&
        ast->where->left && ast->where->right && ast->where->right->kind == EXPR_LIT) {
        uint64_t rids[1];
        int nr = index_eq_lookup(db, tb, ast->where->left->name, &ast->where->right->lit, rids, 1, err);
        if (nr > 0) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rids[0]);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[2048];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(t, &key, rowbuf, &rlen, err) == 0) {
                edb_value *rowvals = &stmt->rows[0];
                int n = 0;
                if (decode_row(rowbuf, rlen, rowvals, tb->col_count, &n) == 0) {
                    got = 1;
                    stmt->row_count = 1;
                    stmt->cur_row = -1;
                    edb_btree_close(t);
                    return 0;
                }
            } else edb_error_clear(err);
        } else if (nr == 0) {
            stmt->row_count = 0;
            stmt->cur_row = -1;
            edb_btree_close(t);
            return 0;
        }
        edb_error_clear(err);
    }
    for (uint64_t rid = 1; rid < tb->next_rowid && got < max_rows; rid++) {
        uint8_t keybuf[8];
        edb_store_u64_le(keybuf, rid);
        edb_key key = { .data = keybuf, .len = 8 };
        uint8_t rowbuf[2048];
        uint16_t rlen = sizeof rowbuf;
        if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) {
            edb_error_clear(err);
            continue; /* deleted or missing */
        }
        int n = 0;
        edb_value *rowvals = &stmt->rows[got * tb->col_count];
        uint64_t rxmin = 1, rxmax = 0;
        if (decode_row_mvcc(rowbuf, rlen, rowvals, tb->col_count, &n, &rxmin, &rxmax) != 0)
            continue;
        /* MVCC visibility */
        if (db->txn && db->mvcc) {
            if (!edb_mvcc_visible(db->mvcc, &db->txn->snap, rxmin, rxmax))
                continue;
        } else if (rxmax != 0) {
            /* no snapshot: hide deleted versions */
            continue;
        }
        /* WHERE comparison / IN */
        if (ast->where && ast->where->kind == EXPR_BINOP && ast->where->left && ast->where->right) {
            int ci = -1;
            for (int c = 0; c < tb->col_count; c++)
                if (strcasecmp(tb->cols[c].name, ast->where->left->name) == 0) { ci = c; break; }
            if (ci >= 0) {
                edb_value *cv = &rowvals[ci];
                edb_value *lv = &ast->where->right->lit;
                int match = 0;
                int op = ast->where->op;
                if (op == EDB_TOK_NULL) {
                    match = (cv->type == EDB_VAL_NULL);
                } else if (op == EDB_TOK_KW_NOT && ast->where->right->lit.type == EDB_VAL_NULL) {
                    match = (cv->type != EDB_VAL_NULL);
                } else if (ast->where->right->kind == EXPR_SUBQUERY && ast->where->right->subquery_sql[0]) {
                    /* Scalar / correlated subquery: substitute @col with outer row values */
                    char sqlbuf[640];
                    const char *src = ast->where->right->subquery_sql;
                    size_t sp = 0;
                    for (size_t i = 0; src[i] && sp + 32 < sizeof sqlbuf; ) {
                        if (src[i] == '@') {
                            i++;
                            char cname[128]; size_t cn = 0;
                            while (src[i] && ((src[i]>='a'&&src[i]<='z')||(src[i]>='A'&&src[i]<='Z')||
                                   (src[i]>='0'&&src[i]<='9')||src[i]=='_') && cn + 1 < sizeof cname)
                                cname[cn++] = src[i++];
                            cname[cn] = 0;
                            int oc = -1;
                            for (int c = 0; c < tb->col_count; c++)
                                if (strcasecmp(tb->cols[c].name, cname) == 0) { oc = c; break; }
                            if (oc >= 0 && oc < n) {
                                edb_value *ov = &rowvals[oc];
                                if (ov->type == EDB_VAL_INTEGER)
                                    sp += (size_t)snprintf(sqlbuf + sp, sizeof sqlbuf - sp, "%lld", (long long)ov->u.i64);
                                else if (ov->type == EDB_VAL_TEXT)
                                    sp += (size_t)snprintf(sqlbuf + sp, sizeof sqlbuf - sp, "'%.*s'",
                                                           (int)ov->u.bin.len, (const char*)ov->u.bin.p);
                                else if (ov->type == EDB_VAL_NULL)
                                    sp += (size_t)snprintf(sqlbuf + sp, sizeof sqlbuf - sp, "NULL");
                                else
                                    sp += (size_t)snprintf(sqlbuf + sp, sizeof sqlbuf - sp, "NULL");
                            }
                        } else {
                            sqlbuf[sp++] = src[i++];
                        }
                    }
                    sqlbuf[sp] = 0;
                    edb_stmt sst; memset(&sst, 0, sizeof sst); sst.db = db;
                    edb_error serr;
                    edb_ast *sast = edb_parse_statement(sqlbuf, strlen(sqlbuf), &serr);
                    if (sast && sast->kind == EDB_AST_SELECT) {
                        if (exec_select(db, sast, &sst, &serr) == 0 && sst.row_count > 0) {
                            edb_value *sv = &sst.rows[0];
                            if (cv->type == EDB_VAL_INTEGER && sv->type == EDB_VAL_INTEGER) {
                                int64_t a = cv->u.i64, b = sv->u.i64;
                                switch (op) {
                                case EDB_TOK_EQ: match = a == b; break;
                                case EDB_TOK_NE: match = a != b; break;
                                case EDB_TOK_LT: match = a < b; break;
                                case EDB_TOK_GT: match = a > b; break;
                                case EDB_TOK_LE: match = a <= b; break;
                                case EDB_TOK_GE: match = a >= b; break;
                                default: match = a == b; break;
                                }
                            } else if (cv->type == EDB_VAL_TEXT && sv->type == EDB_VAL_TEXT) {
                                int cmp = (cv->u.bin.len == sv->u.bin.len &&
                                           memcmp(cv->u.bin.p, sv->u.bin.p, cv->u.bin.len)==0);
                                match = (op == EDB_TOK_NE) ? !cmp : cmp;
                            }
                        }
                        for (int r = 0; r < sst.row_count; r++)
                            for (int c = 0; c < sst.col_count; c++) {
                                edb_value *v = &sst.rows[r*sst.col_count+c];
                                if (v->type == EDB_VAL_TEXT || v->type == EDB_VAL_BLOB) free((void*)v->u.bin.p);
                            }
                        free(sst.rows);
                        for (int i = 0; i < sst.col_count; i++) free(sst.col_names[i]);
                        free(sst.col_names);
                    }
                    if (sast) edb_ast_free(sast);
                } else if (op == EDB_TOK_KW_BETWEEN && lv->type == EDB_VAL_BLOB && lv->u.bin.p && lv->u.bin.len >= 17) {
                    int64_t lo = (int64_t)edb_load_u64_le(lv->u.bin.p + 1);
                    int64_t hi = (int64_t)edb_load_u64_le(lv->u.bin.p + 9);
                    if (cv->type == EDB_VAL_INTEGER)
                        match = (cv->u.i64 >= lo && cv->u.i64 <= hi);
                } else if (op == EDB_TOK_KW_LIKE && lv->type == EDB_VAL_TEXT && cv->type == EDB_VAL_TEXT) {
                    /* simple % suffix/prefix/contains */
                    const char *pat = (const char*)lv->u.bin.p;
                    size_t plen = lv->u.bin.len;
                    const char *s = (const char*)cv->u.bin.p;
                    size_t slen = cv->u.bin.len;
                    if (plen == 0) match = (slen == 0);
                    else if (pat[0] == '%' && plen > 1 && pat[plen-1] == '%') {
                        /* contains */
                        size_t mid = plen - 2;
                        for (size_t i = 0; i + mid <= slen; i++)
                            if (memcmp(s+i, pat+1, mid)==0) { match = 1; break; }
                    } else if (pat[0] == '%') {
                        size_t mid = plen - 1;
                        match = (slen >= mid && memcmp(s + slen - mid, pat + 1, mid)==0);
                    } else if (plen > 0 && pat[plen-1] == '%') {
                        size_t mid = plen - 1;
                        match = (slen >= mid && memcmp(s, pat, mid)==0);
                    } else {
                        match = (slen == plen && memcmp(s, pat, plen)==0);
                    }
                } else if (op == EDB_TOK_KW_IN && lv->type == EDB_VAL_BLOB && lv->u.bin.p) {
                    int nlit = lv->u.bin.p[0];
                    if (cv->type == EDB_VAL_INTEGER) {
                        for (int i = 0; i < nlit; i++) {
                            int64_t iv = (int64_t)edb_load_u64_le(lv->u.bin.p + 1 + i * 8);
                            if (cv->u.i64 == iv) { match = 1; break; }
                        }
                    }
                } else if (cv->type == EDB_VAL_INTEGER && lv->type == EDB_VAL_INTEGER) {
                    int64_t a = cv->u.i64, b = lv->u.i64;
                    switch (op) {
                    case EDB_TOK_EQ: match = a == b; break;
                    case EDB_TOK_NE: match = a != b; break;
                    case EDB_TOK_LT: match = a < b; break;
                    case EDB_TOK_GT: match = a > b; break;
                    case EDB_TOK_LE: match = a <= b; break;
                    case EDB_TOK_GE: match = a >= b; break;
                    default: match = a == b; break;
                    }
                } else if (cv->type == EDB_VAL_TEXT && lv->type == EDB_VAL_TEXT) {
                    int cmp = (cv->u.bin.len == lv->u.bin.len &&
                               memcmp(cv->u.bin.p, lv->u.bin.p, cv->u.bin.len) == 0);
                    match = (op == EDB_TOK_NE) ? !cmp : cmp;
                }
                if (!match) {
                    for (int c = 0; c < n; c++)
                        if (rowvals[c].type == EDB_VAL_TEXT || rowvals[c].type == EDB_VAL_BLOB)
                            free((void*)rowvals[c].u.bin.p);
                    continue;
                }
            }
        }
        if (skipped < need_skip) { skipped++; continue; }
        got++;
    }
    edb_btree_close(t);
    /* ORDER BY */
    if (ast->has_order && got > 1) {
        int ocol = -1;
        for (int c = 0; c < tb->col_count; c++)
            if (strcasecmp(tb->cols[c].name, ast->order_col) == 0) { ocol = c; break; }
        if (ocol >= 0) {
            for (int i = 1; i < got; i++) {
                edb_value *tmp = malloc((size_t)tb->col_count * sizeof(edb_value));
                if (!tmp) break;
                memcpy(tmp, &stmt->rows[i * tb->col_count], (size_t)tb->col_count * sizeof(edb_value));
                int j = i - 1;
                while (j >= 0) {
                    edb_value *a = &stmt->rows[j * tb->col_count + ocol];
                    edb_value *b = &tmp[ocol];
                    int cmp = 0;
                    if (a->type == EDB_VAL_INTEGER && b->type == EDB_VAL_INTEGER)
                        cmp = (a->u.i64 > b->u.i64) - (a->u.i64 < b->u.i64);
                    else if (a->type == EDB_VAL_TEXT && b->type == EDB_VAL_TEXT) {
                        uint32_t n = a->u.bin.len < b->u.bin.len ? a->u.bin.len : b->u.bin.len;
                        cmp = memcmp(a->u.bin.p, b->u.bin.p, n);
                        if (cmp == 0) cmp = (a->u.bin.len > b->u.bin.len) - (a->u.bin.len < b->u.bin.len);
                    } else {
                        cmp = (int)a->type - (int)b->type;
                    }
                    if (ast->order_desc) cmp = -cmp;
                    if (cmp <= 0) break;
                    memcpy(&stmt->rows[(j + 1) * tb->col_count], &stmt->rows[j * tb->col_count],
                           (size_t)tb->col_count * sizeof(edb_value));
                    j--;
                }
                memcpy(&stmt->rows[(j + 1) * tb->col_count], tmp, (size_t)tb->col_count * sizeof(edb_value));
                free(tmp);
            }
        }
    }
    if (ast->is_distinct && got > 1) {
        int w = 0;
        for (int r = 0; r < got; r++) {
            int dup = 0;
            for (int prev = 0; prev < w; prev++) {
                int same = 1;
                for (int c = 0; c < tb->col_count; c++) {
                    edb_value *a = &stmt->rows[r * tb->col_count + c];
                    edb_value *b = &stmt->rows[prev * tb->col_count + c];
                    if (a->type != b->type) { same = 0; break; }
                    if (a->type == EDB_VAL_INTEGER && a->u.i64 != b->u.i64) { same = 0; break; }
                    if (a->type == EDB_VAL_TEXT && (a->u.bin.len != b->u.bin.len ||
                        memcmp(a->u.bin.p, b->u.bin.p, a->u.bin.len))) { same = 0; break; }
                }
                if (same) { dup = 1; break; }
            }
            if (!dup) {
                if (w != r)
                    memcpy(&stmt->rows[w * tb->col_count], &stmt->rows[r * tb->col_count],
                           (size_t)tb->col_count * sizeof(edb_value));
                w++;
            }
        }
        got = w;
    }
    stmt->row_count = got;
    stmt->cur_row = -1;
    return 0;
}

static int exec_delete_all(edb_db *db, edb_ast *ast, edb_error *err) {
    edb_table *tb = edb_catalog_find_table(db->catalog, ast->table);
    if (!tb) { edb_error_set(err, EDB_NOT_FOUND, 0, "table not found"); return -1; }
    if (!ast->where) {
        uint32_t root;
        if (edb_btree_create(db->pager, true, &root, err) != 0) return -1;
        tb->data_root = root;
        tb->next_rowid = 1;
        db->catalog->dirty = true;
        if (!db->in_txn) return edb_catalog_save(db->catalog, err);
        return 0;
    }
    int where_col = -1;
    for (int c = 0; c < tb->col_count; c++)
        if (strcasecmp(tb->cols[c].name, ast->where->left->name) == 0) { where_col = c; break; }
    if (where_col < 0) { edb_error_set(err, EDB_NOT_FOUND, 0, "where column not found"); return -1; }

    edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
    if (!t) return -1;
    uint64_t del_xid = db->txn ? db->txn->xid : 0;
    edb_xid horizon = (db->mvcc) ? edb_mvcc_horizon(db->mvcc) : 0;

    typedef struct { uint64_t rid; edb_value vals[EDB_MAX_COLS]; int n; uint64_t xmin, xmax; int drop; } row_t;
    int cap = (int)tb->next_rowid + 8;
    row_t *rows = calloc((size_t)cap, sizeof(row_t));
    if (!rows) { edb_btree_close(t); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
    int nrows = 0;
    for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
        uint8_t keybuf[8];
        edb_store_u64_le(keybuf, rid);
        edb_key key = { .data = keybuf, .len = 8 };
        uint8_t rowbuf[2048];
        uint16_t rlen = sizeof rowbuf;
        if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
        int n = 0; uint64_t xmin = 1, xmax = 0;
        if (decode_row_mvcc(rowbuf, rlen, rows[nrows].vals, EDB_MAX_COLS, &n, &xmin, &xmax) != 0) continue;
        rows[nrows].rid = rid; rows[nrows].n = n; rows[nrows].xmin = xmin; rows[nrows].xmax = xmax; rows[nrows].drop = 0;
        /* GC dead versions */
        if (xmax != 0 && horizon != 0 && xmax < horizon) {
            for (int c = 0; c < n; c++)
                if (rows[nrows].vals[c].type == EDB_VAL_TEXT || rows[nrows].vals[c].type == EDB_VAL_BLOB)
                    free((void*)rows[nrows].vals[c].u.bin.p);
            continue;
        }
        edb_value *cv = &rows[nrows].vals[where_col];
        edb_value *lv = &ast->where->right->lit;
        int match = 0;
        if (cv->type == EDB_VAL_INTEGER && lv->type == EDB_VAL_INTEGER)
            match = (cv->u.i64 == lv->u.i64);
        else if (cv->type == EDB_VAL_TEXT && lv->type == EDB_VAL_TEXT)
            match = (cv->u.bin.len == lv->u.bin.len && memcmp(cv->u.bin.p, lv->u.bin.p, cv->u.bin.len)==0);
        if (match) {
            if (db->txn && del_xid)
                rows[nrows].xmax = del_xid; /* soft-delete */
            else {
                for (int c = 0; c < n; c++)
                    if (rows[nrows].vals[c].type == EDB_VAL_TEXT || rows[nrows].vals[c].type == EDB_VAL_BLOB)
                        free((void*)rows[nrows].vals[c].u.bin.p);
                continue; /* physical drop */
            }
        }
        nrows++;
    }
    edb_btree_close(t);
    uint32_t root;
    if (edb_btree_create(db->pager, true, &root, err) != 0) { free(rows); return -1; }
    t = edb_btree_open(db->pager, root, true, err);
    if (!t) { free(rows); return -1; }
    for (int i = 0; i < nrows; i++) {
        uint8_t keybuf[8];
        edb_store_u64_le(keybuf, rows[i].rid);
        edb_key key = { .data = keybuf, .len = 8 };
        uint8_t rowbuf[2048];
        int rlen = encode_row_mvcc(rows[i].vals, rows[i].n, rowbuf, sizeof rowbuf, rows[i].xmin, rows[i].xmax);
        for (int c = 0; c < rows[i].n; c++)
            if (rows[i].vals[c].type == EDB_VAL_TEXT || rows[i].vals[c].type == EDB_VAL_BLOB)
                free((void*)rows[i].vals[c].u.bin.p);
        if (rlen < 0) { edb_btree_close(t); free(rows); return -1; }
        if (edb_btree_insert(t, &key, rowbuf, (uint16_t)rlen, err) != 0) {
            edb_btree_close(t); free(rows); return -1;
        }
    }
    tb->data_root = t->root_page;
    edb_btree_close(t);
    free(rows);
    db->catalog->dirty = true;
    if (!db->in_txn) return edb_catalog_save(db->catalog, err);
    return 0;
}

static int exec_update(edb_db *db, edb_ast *ast, edb_error *err) {
    edb_table *tb = edb_catalog_find_table(db->catalog, ast->table);
    if (!tb) { edb_error_set(err, EDB_NOT_FOUND, 0, "table not found"); return -1; }
    int set_col = -1;
    for (int c = 0; c < tb->col_count; c++)
        if (strcasecmp(tb->cols[c].name, ast->select_cols[0]) == 0) { set_col = c; break; }
    if (set_col < 0) { edb_error_set(err, EDB_NOT_FOUND, 0, "set column not found"); return -1; }
    int where_col = -1;
    if (ast->where) {
        for (int c = 0; c < tb->col_count; c++)
            if (strcasecmp(tb->cols[c].name, ast->where->left->name) == 0) { where_col = c; break; }
    }
    /* Load all rows into memory, apply updates, rebuild btree */
    typedef struct { uint64_t rid; edb_value vals[EDB_MAX_COLS]; int n; } row_t;
    int cap = (int)tb->next_rowid + 8;
    row_t *rows = calloc((size_t)cap, sizeof(row_t));
    if (!rows) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
    edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
    if (!t) { free(rows); return -1; }
    int nrows = 0;
    for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
        uint8_t keybuf[8];
        edb_store_u64_le(keybuf, rid);
        edb_key key = { .data = keybuf, .len = 8 };
        uint8_t rowbuf[2048];
        uint16_t rlen = sizeof rowbuf;
        if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
        int n = 0;
        if (decode_row(rowbuf, rlen, rows[nrows].vals, EDB_MAX_COLS, &n) != 0) continue;
        rows[nrows].rid = rid;
        rows[nrows].n = n;
        /* apply where + set */
        int match = 1;
        if (where_col >= 0) {
            edb_value *cv = &rows[nrows].vals[where_col];
            edb_value *lv = &ast->where->right->lit;
            match = 0;
            if (cv->type == EDB_VAL_INTEGER && lv->type == EDB_VAL_INTEGER)
                match = (cv->u.i64 == lv->u.i64);
            else if (cv->type == EDB_VAL_TEXT && lv->type == EDB_VAL_TEXT)
                match = (cv->u.bin.len == lv->u.bin.len && memcmp(cv->u.bin.p, lv->u.bin.p, cv->u.bin.len)==0);
        }
        if (match) {
            if (rows[nrows].vals[set_col].type == EDB_VAL_TEXT || rows[nrows].vals[set_col].type == EDB_VAL_BLOB)
                free((void*)rows[nrows].vals[set_col].u.bin.p);
            rows[nrows].vals[set_col] = ast->values[0];
            if (rows[nrows].vals[set_col].type == EDB_VAL_TEXT || rows[nrows].vals[set_col].type == EDB_VAL_BLOB) {
                uint32_t L = rows[nrows].vals[set_col].u.bin.len;
                uint8_t *cp = malloc(L ? L : 1);
                if (cp) { if (L) memcpy(cp, rows[nrows].vals[set_col].u.bin.p, L); rows[nrows].vals[set_col].u.bin.p = cp; }
            }
        }
        nrows++;
    }
    edb_btree_close(t);
    /* rebuild empty tree and reinsert */
    uint32_t root;
    if (edb_btree_create(db->pager, true, &root, err) != 0) { free(rows); return -1; }
    t = edb_btree_open(db->pager, root, true, err);
    if (!t) { free(rows); return -1; }
    for (int i = 0; i < nrows; i++) {
        uint8_t keybuf[8];
        edb_store_u64_le(keybuf, rows[i].rid);
        edb_key key = { .data = keybuf, .len = 8 };
        uint8_t rowbuf[2048];
        int rlen = encode_row(rows[i].vals, rows[i].n, rowbuf, sizeof rowbuf);
        for (int c = 0; c < rows[i].n; c++)
            if (rows[i].vals[c].type == EDB_VAL_TEXT || rows[i].vals[c].type == EDB_VAL_BLOB)
                free((void*)rows[i].vals[c].u.bin.p);
        if (rlen < 0) { edb_btree_close(t); free(rows); edb_error_set(err, EDB_LIMIT, 0, "row too large"); return -1; }
        if (edb_btree_insert(t, &key, rowbuf, (uint16_t)rlen, err) != 0) {
            edb_btree_close(t); free(rows); return -1;
        }
    }
    tb->data_root = t->root_page;
    edb_btree_close(t);
    free(rows);
    db->catalog->dirty = true;
    if (!db->in_txn) return edb_catalog_save(db->catalog, err);
    return 0;
}




static int exec_vacuum(edb_db *db, edb_error *err) {
    /* Full rewrite VACUUM: export all tables into a new DB file, then atomic replace. */
    if (!db->path) {
        edb_error_set(err, EDB_IO, 0, "vacuum requires path");
        return -1;
    }
    if (db->catalog->dirty && edb_catalog_save(db->catalog, err) != 0) return -1;
    if (edb_pager_sync(db->pager, err) != 0) return -1;

    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s.vacuum-%d", db->path, (int)getpid());
    unlink(tmp);

    edb_db *ndb = edb_open(tmp, true, false, db->has_key ? (const char*)"\0" : NULL, err);
    /* password path is awkward; for unencrypted DBs open works. For encrypted, skip full rewrite. */
    if (!ndb) {
        /* fallback: checkpoint only */
        edb_error_clear(err);
        if (db->wal) edb_wal_checkpoint(db->wal, db->pager, err);
        return edb_pager_sync(db->pager, err);
    }
    /* Recreate schema + copy rows */
    for (int ti = 0; ti < db->catalog->table_count; ti++) {
        edb_table *tb = &db->catalog->tables[ti];
        char ddl[2048];
        int pos = snprintf(ddl, sizeof ddl, "CREATE TABLE %s (", tb->name);
        for (int c = 0; c < tb->col_count; c++) {
            const char *ty = "TEXT";
            if (tb->cols[c].type == EDB_VAL_INTEGER) ty = "INTEGER";
            else if (tb->cols[c].type == EDB_VAL_REAL) ty = "REAL";
            else if (tb->cols[c].type == EDB_VAL_BLOB) ty = "BLOB";
            pos += snprintf(ddl + pos, sizeof ddl - (size_t)pos, "%s%s %s%s",
                            c ? ", " : "", tb->cols[c].name, ty,
                            tb->cols[c].primary_key ? " PRIMARY KEY" : "");
        }
        snprintf(ddl + pos, sizeof ddl - (size_t)pos, ");");
        if (edb_exec(ndb, ddl, err) != 0) { edb_close(ndb); unlink(tmp); return -1; }

        edb_btree *src = edb_btree_open(db->pager, tb->data_root, true, err);
        if (!src) { edb_close(ndb); unlink(tmp); return -1; }
        for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rid);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[2048];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(src, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
            edb_value vals[EDB_MAX_COLS];
            int n = 0;
            if (decode_row(rowbuf, rlen, vals, EDB_MAX_COLS, &n) != 0) continue;
            /* Build INSERT statement */
            char sql[4096];
            int sp = snprintf(sql, sizeof sql, "INSERT INTO %s VALUES (", tb->name);
            for (int c = 0; c < n; c++) {
                if (c) sp += snprintf(sql + sp, sizeof sql - (size_t)sp, ", ");
                if (vals[c].type == EDB_VAL_NULL)
                    sp += snprintf(sql + sp, sizeof sql - (size_t)sp, "NULL");
                else if (vals[c].type == EDB_VAL_INTEGER)
                    sp += snprintf(sql + sp, sizeof sql - (size_t)sp, "%lld", (long long)vals[c].u.i64);
                else if (vals[c].type == EDB_VAL_TEXT) {
                    sp += snprintf(sql + sp, sizeof sql - (size_t)sp, "'");
                    /* escape single quotes simply by skipping for vacuum of fixture data */
                    for (uint32_t i = 0; i < vals[c].u.bin.len && sp + 2 < (int)sizeof sql; i++) {
                        char ch = (char)vals[c].u.bin.p[i];
                        if (ch == '\'') sql[sp++] = '\'';
                        sql[sp++] = ch;
                    }
                    sp += snprintf(sql + sp, sizeof sql - (size_t)sp, "'");
                } else
                    sp += snprintf(sql + sp, sizeof sql - (size_t)sp, "NULL");
            }
            snprintf(sql + sp, sizeof sql - (size_t)sp, ");");
            for (int c = 0; c < n; c++)
                if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                    free((void*)vals[c].u.bin.p);
            if (edb_exec(ndb, sql, err) != 0) {
                edb_btree_close(src);
                edb_close(ndb);
                unlink(tmp);
                return -1;
            }
        }
        edb_btree_close(src);
    }
    edb_close(ndb);

    /* Atomic replace: rename tmp over original path. Caller must reopen. */
    char bak[4096];
    snprintf(bak, sizeof bak, "%s.bak-vacuum", db->path);
    unlink(bak);
    if (rename(db->path, bak) != 0) {
        edb_error_set(err, EDB_IO, errno, "vacuum rename bak failed");
        unlink(tmp);
        return -1;
    }
    if (rename(tmp, db->path) != 0) {
        rename(bak, db->path); /* try restore */
        edb_error_set(err, EDB_IO, errno, "vacuum rename new failed");
        return -1;
    }
    unlink(bak);
    /* Note: current db handle still points at old fd; recommended to close/reopen.
       Mark catalog dirty cleared. */
    return 0;
}


static int exec_drop_table(edb_db *db, edb_ast *ast, edb_error *err) {
    edb_table *tb = edb_catalog_find_table(db->catalog, ast->name[0] ? ast->name : ast->table);
    if (!tb) {
        /* try name field */
        for (int i = 0; i < db->catalog->table_count; i++)
            if (strcasecmp(db->catalog->tables[i].name, ast->name) == 0) {
                tb = &db->catalog->tables[i]; break;
            }
    }
    if (!tb) { edb_error_set(err, EDB_NOT_FOUND, 0, "table not found"); return -1; }
    /* remove indexes on table */
    for (int i = 0; i < db->catalog->index_count; ) {
        if (strcasecmp(db->catalog->indexes[i].table, tb->name) == 0) {
            memmove(&db->catalog->indexes[i], &db->catalog->indexes[i+1],
                    (size_t)(db->catalog->index_count - i - 1) * sizeof(edb_index));
            db->catalog->index_count--;
        } else i++;
    }
    /* remove table slot */
    int idx = (int)(tb - db->catalog->tables);
    memmove(&db->catalog->tables[idx], &db->catalog->tables[idx+1],
            (size_t)(db->catalog->table_count - idx - 1) * sizeof(edb_table));
    db->catalog->table_count--;
    db->catalog->dirty = true;
    if (!db->in_txn) return edb_catalog_save(db->catalog, err);
    return 0;
}

static int exec_explain(edb_db *db, edb_ast *ast, edb_error *err) {
    (void)err;
    printf("QUERY PLAN\n");
    if (ast->has_join)
        printf("-- nested loop join %s %s %s\n", ast->table, ast->is_left_join?"LEFT":"INNER", ast->join_table);
    else
        printf("-- sequential scan on %s\n", ast->table);
    if (ast->where) printf("-- filter WHERE\n");
    if (ast->has_group) printf("-- group by %s\n", ast->group_col);
    if (ast->has_order) printf("-- order by %s %s\n", ast->order_col, ast->order_desc?"DESC":"ASC");
    if (ast->has_limit) printf("-- limit %d\n", ast->limit);
    /* check index opportunity */
    if (ast->where && ast->where->left) {
        for (int i = 0; i < db->catalog->index_count; i++) {
            edb_index *ix = &db->catalog->indexes[i];
            if (strcasecmp(ix->table, ast->table)==0 && ix->col_count>0 &&
                strcasecmp(ix->col_names[0], ast->where->left->name)==0)
                printf("-- possible index %s on %s\n", ix->name, ix->col_names[0]);
        }
    }
    return 0;
}


int edb_dump_sql(edb_db *db, FILE *out, edb_error *err) {
    if (!db || !out) { edb_error_set(err, EDB_IO, 0, "bad args"); return -1; }
    fprintf(out, "-- edb full SQL dump\nBEGIN;\n");
    for (int ti = 0; ti < db->catalog->table_count; ti++) {
        edb_table *tb = &db->catalog->tables[ti];
        fprintf(out, "CREATE TABLE %s (", tb->name);
        for (int c = 0; c < tb->col_count; c++) {
            const char *ty = "TEXT";
            if (tb->cols[c].type == EDB_VAL_INTEGER) ty = "INTEGER";
            else if (tb->cols[c].type == EDB_VAL_REAL) ty = "REAL";
            else if (tb->cols[c].type == EDB_VAL_BLOB) ty = "BLOB";
            fprintf(out, "%s%s %s%s", c?", ":"", tb->cols[c].name, ty,
                    tb->cols[c].primary_key ? " PRIMARY KEY" : "");
        }
        fprintf(out, ");\n");
        edb_btree *t = edb_btree_open(db->pager, tb->data_root, true, err);
        if (!t) continue;
        for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
            uint8_t keybuf[8];
            edb_store_u64_le(keybuf, rid);
            edb_key key = { .data = keybuf, .len = 8 };
            uint8_t rowbuf[2048];
            uint16_t rlen = sizeof rowbuf;
            if (edb_btree_get(t, &key, rowbuf, &rlen, err) != 0) { edb_error_clear(err); continue; }
            edb_value vals[EDB_MAX_COLS];
            int n = 0;
            if (decode_row(rowbuf, rlen, vals, EDB_MAX_COLS, &n) != 0) continue;
            fprintf(out, "INSERT INTO %s VALUES (", tb->name);
            for (int c = 0; c < n; c++) {
                if (c) fputc(',', out);
                if (vals[c].type == EDB_VAL_NULL) fputs("NULL", out);
                else if (vals[c].type == EDB_VAL_INTEGER) fprintf(out, "%lld", (long long)vals[c].u.i64);
                else if (vals[c].type == EDB_VAL_TEXT) {
                    fputc('\'', out);
                    for (uint32_t i = 0; i < vals[c].u.bin.len; i++) {
                        char ch = (char)vals[c].u.bin.p[i];
                        if (ch == '\'') fputc('\'', out);
                        fputc(ch, out);
                    }
                    fputc('\'', out);
                } else fputs("NULL", out);
            }
            fprintf(out, ");\n");
            for (int c = 0; c < n; c++)
                if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                    free((void*)vals[c].u.bin.p);
        }
        edb_btree_close(t);
    }
    for (int i = 0; i < db->catalog->index_count; i++) {
        edb_index *ix = &db->catalog->indexes[i];
        fprintf(out, "CREATE %sINDEX %s ON %s (", ix->unique?"UNIQUE ":"", ix->name, ix->table);
        for (int c = 0; c < ix->col_count; c++)
            fprintf(out, "%s%s", c?", ":"", ix->col_names[c]);
        fprintf(out, ");\n");
    }
    fprintf(out, "COMMIT;\n");
    return 0;
}

static int edb_exec_one(edb_db *db, const char *sql, size_t len, edb_error *err) {
    edb_error_clear(err);
    edb_ast *ast = edb_parse_statement(sql, len, err);
    if (!ast) return -1;
    int rc = 0;
    switch (ast->kind) {
    case EDB_AST_BEGIN: rc = edb_begin(db, err); break;
    case EDB_AST_COMMIT: rc = edb_commit(db, err); break;
    case EDB_AST_ROLLBACK: rc = edb_rollback(db, err); break;
    case EDB_AST_SAVEPOINT: rc = edb_savepoint(db, ast->name, err); break;
    case EDB_AST_RELEASE: rc = edb_release_savepoint(db, ast->name, err); break;
    case EDB_AST_ROLLBACK_TO: rc = edb_rollback_to(db, ast->name, err); break;
    case EDB_AST_VACUUM: rc = exec_vacuum(db, err); break;
    case EDB_AST_CREATE_TABLE:
        rc = edb_catalog_create_table(db->catalog, ast, err);
        break;
    case EDB_AST_CREATE_INDEX:
        rc = edb_catalog_create_index(db->catalog, ast, err);
        if (rc == 0) {
            /* backfill index from existing table rows */
            edb_index *ix = edb_catalog_find_index(db->catalog, ast->name);
            edb_table *tb = edb_catalog_find_table(db->catalog, ast->table);
            if (ix && tb && ix->root) {
                edb_btree *data = edb_btree_open(db->pager, tb->data_root, true, err);
                edb_btree *it = edb_btree_open(db->pager, ix->root, ix->unique, err);
                if (data && it) {
                    for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
                        uint8_t keybuf[8];
                        edb_store_u64_le(keybuf, rid);
                        edb_key key = { .data = keybuf, .len = 8 };
                        uint8_t rowbuf[2048];
                        uint16_t rlen = sizeof rowbuf;
                        if (edb_btree_get(data, &key, rowbuf, &rlen, err) != 0) {
                            edb_error_clear(err); continue;
                        }
                        edb_value vals[EDB_MAX_COLS];
                        int n = 0;
                        if (decode_row(rowbuf, rlen, vals, EDB_MAX_COLS, &n) != 0) continue;
                        edb_composite_key ck = {0};
                        ck.n = ix->col_count;
                        ck.has_rowid = !ix->unique;
                        ck.rowid = rid;
                        for (int c = 0; c < ix->col_count; c++) {
                            int ci = ix->col_idxs[c];
                            if (ci >= 0 && ci < n) ck.comps[c] = vals[ci];
                        }
                        uint8_t ikey[EDB_MAX_KEY_BYTES];
                        int klen = edb_composite_encode(&ck, ikey, sizeof ikey);
                        if (klen > 0) {
                            edb_key ik = { .data = ikey, .len = (uint16_t)klen };
                            uint8_t rowidbuf[8];
                            edb_store_u64_le(rowidbuf, rid);
                            edb_btree_insert(it, &ik, rowidbuf, 8, err);
                            edb_error_clear(err);
                        }
                        for (int c = 0; c < n; c++)
                            if (vals[c].type == EDB_VAL_TEXT || vals[c].type == EDB_VAL_BLOB)
                                free((void*)vals[c].u.bin.p);
                    }
                    ix->root = it->root_page;
                    edb_btree_close(it);
                    edb_btree_close(data);
                    db->catalog->dirty = true;
                    edb_catalog_save(db->catalog, err);
                    edb_error_clear(err);
                } else {
                    if (data) edb_btree_close(data);
                    if (it) edb_btree_close(it);
                    edb_error_clear(err);
                }
            }
        }
        break;
    case EDB_AST_DROP_TABLE:
        rc = exec_drop_table(db, ast, err);
        break;
    case EDB_AST_INSERT:
        rc = exec_insert(db, ast, err);
        break;
    case EDB_AST_DELETE:
        rc = exec_delete_all(db, ast, err);
        break;
    case EDB_AST_UPDATE:
        rc = exec_update(db, ast, err);
        break;
    case EDB_AST_SELECT: {
        /* for exec path without prepared stmt: run and discard */
        edb_stmt st;
        memset(&st, 0, sizeof st);
        st.db = db;
        rc = exec_select(db, ast, &st, err);
        if (rc == 0) {
            /* print simple result to stdout for CLI convenience */
            for (int r = 0; r < st.row_count; r++) {
                for (int c = 0; c < st.col_count; c++) {
                    edb_value *v = &st.rows[r * st.col_count + c];
                    if (c) printf("|");
                    switch (v->type) {
                    case EDB_VAL_NULL: printf("NULL"); break;
                    case EDB_VAL_INTEGER: printf("%lld", (long long)v->u.i64); break;
                    case EDB_VAL_REAL: printf("%g", v->u.real); break;
                    case EDB_VAL_TEXT:
                        printf("%.*s", (int)v->u.bin.len, (const char*)v->u.bin.p);
                        free((void*)v->u.bin.p);
                        break;
                    case EDB_VAL_BLOB: printf("<blob:%u>", v->u.bin.len); free((void*)v->u.bin.p); break;
                    }
                }
                printf("\n");
            }
            free(st.rows);
            for (int i = 0; i < st.col_count; i++) free(st.col_names[i]);
            free(st.col_names);
        }
        break;
    }
    case EDB_AST_ANALYZE:
        printf("ANALYZE: tables=%d indexes=%d\n", db->catalog->table_count, db->catalog->index_count);
        rc = 0;
        break;
    case EDB_AST_EXPLAIN:
        rc = exec_explain(db, ast, err);
        break;
    default:
        edb_error_set(err, EDB_SQL_PARSE, 0, "statement not implemented yet");
        rc = -1;
    }
    edb_ast_free(ast);
    return rc;
}

int edb_exec(edb_db *db, const char *sql, edb_error *err) {
    /* multi-statement: split on ';' outside of strings */
    const char *p = sql;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        const char *start = p;
        int in_str = 0;
        while (*p) {
            if (*p == '\'' ) in_str = !in_str;
            else if (*p == ';' && !in_str) break;
            p++;
        }
        size_t len = (size_t)(p - start);
        while (len && (start[len-1]==' '||start[len-1]=='\n'||start[len-1]=='\r')) len--;
        if (len > 0) {
            if (edb_exec_one(db, start, len, err) != 0) return -1;
        }
        if (*p == ';') p++;
    }
    return 0;
}

int edb_prepare(edb_db *db, const char *sql, edb_stmt **out, edb_error *err) {
    edb_ast *ast = edb_parse_statement(sql, strlen(sql), err);
    if (!ast) return -1;
    edb_stmt *s = calloc(1, sizeof(*s));
    if (!s) { edb_ast_free(ast); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
    s->db = db;
    s->ast = ast;
    if (ast->kind == EDB_AST_SELECT) {
        if (exec_select(db, ast, s, err) != 0) {
            edb_ast_free(ast); free(s); return -1;
        }
    }
    *out = s;
    return 0;
}

int edb_step(edb_stmt *stmt, edb_error *err) {
    edb_error_clear(err);
    if (!stmt->ast || stmt->ast->kind != EDB_AST_SELECT) {
        edb_error_set(err, EDB_SQL_PARSE, 0, "not a select");
        return -1;
    }
    stmt->cur_row++;
    if (stmt->cur_row >= stmt->row_count) return 0; /* done */
    return 1; /* row available */
}

int edb_finalize(edb_stmt *stmt) {
    if (!stmt) return 0;
    if (stmt->rows) {
        for (int r = 0; r < stmt->row_count; r++)
            for (int c = 0; c < stmt->col_count; c++) {
                edb_value *v = &stmt->rows[r * stmt->col_count + c];
                if (v->type == EDB_VAL_TEXT || v->type == EDB_VAL_BLOB)
                    free((void*)v->u.bin.p);
            }
        free(stmt->rows);
    }
    if (stmt->col_names) {
        for (int i = 0; i < stmt->col_count; i++) free(stmt->col_names[i]);
        free(stmt->col_names);
    }
    edb_ast_free(stmt->ast);
    free(stmt);
    return 0;
}

int edb_column_count(edb_stmt *stmt) { return stmt ? stmt->col_count : 0; }
const char *edb_column_name(edb_stmt *stmt, int i) {
    if (!stmt || i < 0 || i >= stmt->col_count) return NULL;
    return stmt->col_names[i];
}
int edb_column_type(edb_stmt *stmt, int i) {
    if (!stmt || stmt->cur_row < 0 || stmt->cur_row >= stmt->row_count) return 0;
    return (int)stmt->rows[stmt->cur_row * stmt->col_count + i].type;
}
int64_t edb_column_int64(edb_stmt *stmt, int i) {
    if (!stmt || stmt->cur_row < 0) return 0;
    edb_value *v = &stmt->rows[stmt->cur_row * stmt->col_count + i];
    return v->type == EDB_VAL_INTEGER ? v->u.i64 : 0;
}
double edb_column_double(edb_stmt *stmt, int i) {
    if (!stmt || stmt->cur_row < 0) return 0;
    edb_value *v = &stmt->rows[stmt->cur_row * stmt->col_count + i];
    return v->type == EDB_VAL_REAL ? v->u.real : 0;
}
const char *edb_column_text(edb_stmt *stmt, int i) {
    if (!stmt || stmt->cur_row < 0) return NULL;
    edb_value *v = &stmt->rows[stmt->cur_row * stmt->col_count + i];
    return v->type == EDB_VAL_TEXT ? (const char*)v->u.bin.p : NULL;
}
const void *edb_column_blob(edb_stmt *stmt, int i, size_t *len) {
    if (!stmt || stmt->cur_row < 0) { if (len) *len=0; return NULL; }
    edb_value *v = &stmt->rows[stmt->cur_row * stmt->col_count + i];
    if (v->type == EDB_VAL_BLOB) { if (len) *len = v->u.bin.len; return v->u.bin.p; }
    if (len) *len = 0;
    return NULL;
}
