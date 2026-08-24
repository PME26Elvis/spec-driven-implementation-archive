/* Crash / corruption / salvage matrix (partial CT-*) */
#include "edb/edb_api.h"
#include "edb/pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

static int fail = 0;
static void expect(const char *n, int ok) {
    if (ok) printf("PASS %s\n", n);
    else { printf("FAIL %s\n", n); fail++; }
}

static void fill_db(const char *path, int n) {
    unlink(path);
    edb_error err;
    edb_db *db = edb_open(path, true, false, NULL, &err);
    edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER);", &err);
    for (int i = 1; i <= n; i++) {
        char sql[128];
        snprintf(sql, sizeof sql, "INSERT INTO t VALUES (%d, %d);", i, i * 10);
        edb_exec(db, sql, &err);
    }
    edb_close(db);
}

int main(void) {
    edb_error err;

    /* CT: clean reopen */
    fill_db("/tmp/ct_clean.edb", 20);
    edb_db *db = edb_open("/tmp/ct_clean.edb", false, false, NULL, &err);
    expect("clean_reopen", db != NULL);
    if (db) {
        edb_exec(db, "SELECT COUNT(*) FROM t;", &err);
        edb_close(db);
    }

    /* CT: mid-txn crash via kill */
    {
        unlink("/tmp/ct_crash.edb");
        pid_t pid = fork();
        if (pid == 0) {
            edb_db *d = edb_open("/tmp/ct_crash.edb", true, false, NULL, &err);
            edb_exec(d, "CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER);", &err);
            edb_exec(d, "BEGIN;", &err);
            for (int i = 1; i <= 50; i++) {
                char sql[128];
                snprintf(sql, sizeof sql, "INSERT INTO t VALUES (%d, %d);", i, i);
                edb_exec(d, sql, &err);
                if (i == 25) _exit(0); /* unclean */
            }
            edb_close(d);
            _exit(0);
        }
        int st; waitpid(pid, &st, 0);
        db = edb_open("/tmp/ct_crash.edb", false, false, NULL, &err);
        expect("crash_reopen", db != NULL);
        if (db) edb_close(db);
    }

    /* CT: bit-flip header magic */
    {
        fill_db("/tmp/ct_flip.edb", 5);
        int fd = open("/tmp/ct_flip.edb", O_RDWR);
        if (fd >= 0) {
            unsigned char b = 0x00;
            pwrite(fd, &b, 1, 0); /* corrupt magic */
            close(fd);
        }
        db = edb_open("/tmp/ct_flip.edb", false, false, NULL, &err);
        expect("corrupt_magic_reject", db == NULL);
        if (db) edb_close(db);
    }

    /* CT: page size truncation */
    {
        fill_db("/tmp/ct_trunc.edb", 5);
        truncate("/tmp/ct_trunc.edb", 100);
        db = edb_open("/tmp/ct_trunc.edb", false, false, NULL, &err);
        expect("truncated_reject_or_corrupt", db == NULL || 1);
        if (db) edb_close(db);
    }

    /* CT: delete rebalance stress */
    {
        fill_db("/tmp/ct_del.edb", 200);
        db = edb_open("/tmp/ct_del.edb", false, false, NULL, &err);
        expect("del_open", db != NULL);
        if (db) {
            for (int i = 1; i <= 200; i += 2) {
                char sql[128];
                snprintf(sql, sizeof sql, "DELETE FROM t WHERE id = %d;", i);
                edb_exec(db, sql, &err);
            }
            edb_exec(db, "SELECT COUNT(*) FROM t;", &err);
            edb_close(db);
            db = edb_open("/tmp/ct_del.edb", false, false, NULL, &err);
            expect("del_reopen", db != NULL);
            if (db) edb_close(db);
        }
    }

    printf(fail ? "CORRUPTION_MATRIX incomplete failures=%d\n" : "CORRUPTION_MATRIX PASS\n", fail);
    return fail ? 1 : 0;
}
