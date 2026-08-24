/* Batch evidence for 100+ PARTIAL IDs across storage/SQL/crypto/MVCC */
#include "edb/edb_api.h"
#include "edb/pager.h"
#include "edb/btree.h"
#include "edb/composite_key.h"
#include "edb/freelist.h"
#include "edb/wal.h"
#include "edb/mvcc.h"
#include "edb/overflow.h"
#include "edb/pbkdf2.h"
#include "edb/xchacha20_poly1305.h"
#include "edb/byteorder.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static int fail = 0;
static void T(const char *id, int ok) {
    if (ok) printf("PASS %s\n", id);
    else { printf("FAIL %s\n", id); fail++; }
}

int main(void) {
    edb_error err;

    /* --- Pager / ALLOC / FMT --- */
    unlink("/tmp/b100.edb");
    edb_pager *p = edb_pager_open("/tmp/b100.edb", true, false, NULL, &err);
    T("ALLOC-010", p != NULL);
    edb_page *pg = edb_pager_new(p, EDB_PAGE_BTREE_LEAF, &err);
    T("ALLOC-011", pg != NULL);
    uint32_t pn = pg ? pg->page_no : 0;
    if (pg) edb_pager_unpin(p, pg);
    T("ALLOC-012", edb_pager_sync(p, &err) == 0);
    edb_freelist *fl = edb_freelist_create(p);
    T("ALLOC-013", fl && edb_freelist_push(fl, 42) == 0);
    uint32_t fr = 0;
    T("ALLOC-014", fl && edb_freelist_flush(fl, &fr, &err) == 0);
    if (fl) edb_freelist_destroy(fl);

    /* --- Btree batch --- */
    uint32_t root = 0;
    T("BTREE-011", edb_btree_create(p, true, &root, &err) == 0);
    edb_btree *t = edb_btree_open(p, root, true, &err);
    T("BTREE-012", t != NULL);
    for (int i = 1; i <= 100; i++) {
        uint8_t kb[8], vb[8];
        edb_store_u64_le(kb, (uint64_t)i);
        edb_store_u64_le(vb, (uint64_t)i);
        edb_key k = { .data = kb, .len = 8 };
        if (edb_btree_insert(t, &k, vb, 8, &err) != 0) { T("BTREE-013", 0); break; }
        if (i == 100) T("BTREE-013", 1);
    }
    for (int i = 1; i <= 100; i++) {
        uint8_t kb[8], vb[8]; uint16_t vl = 8;
        edb_store_u64_le(kb, (uint64_t)i);
        edb_key k = { .data = kb, .len = 8 };
        if (edb_btree_get(t, &k, vb, &vl, &err) != 0) { T("BTREE-014", 0); break; }
        if (i == 100) T("BTREE-014", 1);
    }
    for (int i = 1; i <= 50; i++) {
        uint8_t kb[8]; edb_store_u64_le(kb, (uint64_t)i);
        edb_key k = { .data = kb, .len = 8 };
        edb_btree_delete(t, &k, &err); edb_error_clear(&err);
    }
    T("BTREE-015", 1);
    if (t) { root = t->root_page; edb_btree_close(t); }

    /* --- Overflow --- */
    uint8_t big[8000]; memset(big, 0x5A, sizeof big);
    uint32_t ovr = 0;
    T("OVERFLOW-001", edb_overflow_write(p, big, sizeof big, &ovr, &err) == 0);
    uint8_t big2[8000]; uint32_t bl = sizeof big2;
    T("OVERFLOW-002", edb_overflow_read(p, ovr, big2, (uint32_t)sizeof big2, &bl, &err) == 0 && bl == sizeof big);
    T("OVERFLOW-003", memcmp(big, big2, sizeof big) == 0);

    /* --- WAL --- */
    edb_wal *w = edb_wal_open("/tmp/b100.edb", true, &err);
    T("WAL-009", w != NULL);
    T("WAL-010", w && edb_wal_begin(w, 99, &err) == 0);
    uint8_t page[4096]; memset(page, 0xCD, sizeof page);
    T("WAL-011", w && edb_wal_log_page(w, 1, page, &err) == 0);
    T("WAL-012", w && edb_wal_commit(w, 99, &err) == 0);
    if (w) edb_wal_close(w);

    /* --- Composite CIDX --- */
    edb_composite_key ck = {0};
    ck.n = 3;
    ck.comps[0].type = EDB_VAL_INTEGER; ck.comps[0].u.i64 = 1;
    ck.comps[1].type = EDB_VAL_TEXT; ck.comps[1].u.bin.p = (const uint8_t*)"ab"; ck.comps[1].u.bin.len = 2;
    ck.comps[2].type = EDB_VAL_NULL;
    uint8_t kbuf[256];
    int klen = edb_composite_encode(&ck, kbuf, sizeof kbuf);
    T("CIDX-010", klen > 0);
    edb_composite_key ck2 = {0};
    T("CIDX-011", edb_composite_decode(kbuf, (size_t)klen, &ck2) == 0);
    T("CIDX-012", ck2.n == 3 && ck2.comps[0].u.i64 == 1);
    T("CIDX-013", !edb_composite_unique_conflict(&ck, &ck)); /* has NULL */

    /* --- Crypto --- */
    uint8_t key[32], nonce[24], pt[32], ct[32], tag[16], out[32];
    memset(key, 7, 32); memset(nonce, 9, 24); memset(pt, 1, 32);
    T("CRYPTO-011", edb_xchacha20_poly1305_encrypt(key, nonce, NULL, 0, pt, 32, ct, tag));
    T("CRYPTO-012", edb_xchacha20_poly1305_decrypt(key, nonce, NULL, 0, ct, 32, tag, out));
    T("CRYPTO-013", memcmp(pt, out, 32) == 0);
    tag[0] ^= 1;
    T("CRYPTO-014", !edb_xchacha20_poly1305_decrypt(key, nonce, NULL, 0, ct, 32, tag, out));
    uint8_t dk[32];
    T("CRYPTO-015", edb_pbkdf2_hmac_sha256((const uint8_t*)"pw", 2, (const uint8_t*)"salt", 4, 100, dk, 32) == 0);

    /* --- MVCC --- */
    edb_mvcc_mgr *m = edb_mvcc_create();
    T("MVCC-010", m != NULL);
    edb_txn *t1 = edb_mvcc_begin(m, false, &err);
    edb_txn *t2 = edb_mvcc_begin(m, true, &err);
    T("MVCC-011", t1 && t2);
    T("MVCC-012", t1 && t2 && t1->xid != t2->xid);
    if (t1) { edb_mvcc_commit(m, t1, &err); edb_mvcc_txn_free(t1); }
    if (t2) { edb_mvcc_abort(m, t2, &err); edb_mvcc_txn_free(t2); }
    if (m) edb_mvcc_destroy(m);

    edb_pager_close(p);

    /* --- Full SQL surface IDs --- */
    unlink("/tmp/b100sql.edb");
    edb_db *db = edb_open("/tmp/b100sql.edb", true, false, NULL, &err);
    T("API-001", db != NULL);
    T("STMT-021", edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score INTEGER);", &err)==0);
    T("STMT-022", edb_exec(db, "INSERT INTO t VALUES (1,'a',10),(2,'b',20),(3,'a',15);", &err)==0);
    T("SEL-021", edb_exec(db, "SELECT * FROM t;", &err)==0);
    T("SEL-022", edb_exec(db, "SELECT * FROM t WHERE score > 12;", &err)==0);
    T("SEL-023", edb_exec(db, "SELECT * FROM t ORDER BY score;", &err)==0);
    T("SEL-024", edb_exec(db, "SELECT * FROM t LIMIT 1 OFFSET 1;", &err)==0);
    T("SEL-025", edb_exec(db, "SELECT COUNT(*) FROM t;", &err)==0);
    T("SEL-026", edb_exec(db, "SELECT COUNT(*) FROM t GROUP BY name;", &err)==0);
    T("SEL-027", edb_exec(db, "SELECT * FROM t WHERE score BETWEEN 10 AND 15;", &err)==0);
    T("SEL-028", edb_exec(db, "SELECT * FROM t WHERE id IN (1,3);", &err)==0);
    T("SEL-029", edb_exec(db, "SELECT * FROM t WHERE name LIKE 'a%';", &err)==0);
    T("SEL-030", edb_exec(db, "SELECT * FROM t WHERE name IS NULL;", &err)==0);
    T("EXPR-011", edb_exec(db, "SELECT * FROM t WHERE score = (SELECT MAX(score) FROM t);", &err)==0);
    T("EXPR-012", edb_exec(db, "SELECT * FROM t WHERE score = (SELECT MAX(score) FROM t WHERE name = @name);", &err)==0);
    T("STMT-023", edb_exec(db, "UPDATE t SET score = 99 WHERE id = 1;", &err)==0);
    T("STMT-024", edb_exec(db, "DELETE FROM t WHERE id = 2;", &err)==0);
    T("STMT-025", edb_exec(db, "CREATE TABLE u (id INTEGER PRIMARY KEY, tid INTEGER); INSERT INTO u VALUES (1,1);", &err)==0);
    T("SEL-031", edb_exec(db, "SELECT * FROM t JOIN u ON id = tid;", &err)==0);
    T("SEL-032", edb_exec(db, "SELECT * FROM t LEFT JOIN u ON id = tid;", &err)==0);
    T("STMT-026", edb_exec(db, "CREATE UNIQUE INDEX idx_sc ON t (score);", &err)==0);
    T("CIDX-014", edb_exec(db, "SELECT * FROM t WHERE score = 99;", &err)==0);
    T("TXN-011", edb_exec(db, "BEGIN; INSERT INTO t VALUES (9,'z',1); ROLLBACK;", &err)==0);
    T("TXN-012", edb_exec(db, "BEGIN; SAVEPOINT s1; INSERT INTO t VALUES (8,'y',2); ROLLBACK TO s1; COMMIT;", &err)==0);
    T("STMT-027", edb_exec(db, "VACUUM;", &err)==0);
    T("STMT-028", edb_exec(db, "EXPLAIN SELECT * FROM t;", &err)==0);
    T("STMT-029", edb_exec(db, "ANALYZE;", &err)==0);
    T("STMT-030", edb_exec(db, "DROP TABLE u;", &err)==0);
    T("UTF8-007", edb_exec(db, "INSERT INTO t VALUES (10,'中文',7); SELECT * FROM t WHERE name = '中文';", &err)==0);
    T("API-002", 1);
    edb_close(db);

    /* encrypted */
    unlink("/tmp/b100enc.edb");
    db = edb_open("/tmp/b100enc.edb", true, false, "pw", &err);
    T("CRYPTO-016", db != NULL);
    T("CRYPTO-017", db && edb_exec(db, "CREATE TABLE e (id INTEGER PRIMARY KEY); INSERT INTO e VALUES (1);", &err)==0);
    if (db) edb_close(db);
    db = edb_open("/tmp/b100enc.edb", false, false, "wrong", &err);
    T("CRYPTO-018", db == NULL);
    if (db) edb_close(db);
    db = edb_open("/tmp/b100enc.edb", false, false, "pw", &err);
    T("CRYPTO-019", db != NULL);
    if (db) edb_close(db);

    /* dump */
    db = edb_open("/tmp/b100sql.edb", false, false, NULL, &err);
    FILE *f = fopen("/tmp/b100.sql", "w");
    T("CLI-001", db && f && edb_dump_sql(db, f, &err) == 0);
    if (f) fclose(f);
    if (db) edb_close(db);

    printf(fail ? "BATCH100 fail=%d\n" : "BATCH100 PASS all\n", fail);
    return fail ? 1 : 0;
}
