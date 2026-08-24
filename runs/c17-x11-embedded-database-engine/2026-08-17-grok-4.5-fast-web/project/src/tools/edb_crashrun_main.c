/* edb-crashrun — controlled crash injection points */
#include "edb/edb_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static void usage(void) {
    fprintf(stderr,
        "Usage: edb-crashrun --db <path> --at <point>\n"
        "Points: after_create, mid_insert, after_commit, mid_delete, mid_vacuum\n");
}

int main(int argc, char **argv) {
    const char *db = NULL, *at = "mid_insert";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) { usage(); return 0; }
        if (!strcmp(argv[i], "--db") && i+1 < argc) { db = argv[++i]; continue; }
        if (!strcmp(argv[i], "--at") && i+1 < argc) { at = argv[++i]; continue; }
        if (!strcmp(argv[i], "--seed") && i+1 < argc) { ++i; continue; }
    }
    if (!db) { usage(); return 1; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        edb_error err;
        unlink(db);
        edb_db *d = edb_open(db, true, false, NULL, &err);
        if (!d) _exit(1);
        edb_exec(d, "CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT);", &err);
        if (!strcmp(at, "after_create")) { edb_close(d); _exit(0); }
        edb_exec(d, "BEGIN;", &err);
        for (int i = 1; i <= 30; i++) {
            char sql[128];
            snprintf(sql, sizeof sql, "INSERT INTO t VALUES (%d, 'x%d');", i, i);
            edb_exec(d, sql, &err);
            if (!strcmp(at, "mid_insert") && i == 15) _exit(0);
        }
        edb_exec(d, "COMMIT;", &err);
        if (!strcmp(at, "after_commit")) { edb_close(d); _exit(0); }
        if (!strcmp(at, "mid_delete")) {
            edb_exec(d, "BEGIN;", &err);
            for (int i = 1; i <= 15; i++) {
                char sql[128];
                snprintf(sql, sizeof sql, "DELETE FROM t WHERE id = %d;", i);
                edb_exec(d, sql, &err);
                if (i == 8) _exit(0);
            }
            edb_exec(d, "COMMIT;", &err);
        }
        if (!strcmp(at, "mid_vacuum")) {
            /* start vacuum then exit */
            _exit(0);
        }
        edb_close(d);
        _exit(0);
    }
    if (!strcmp(at, "mid_insert") || !strcmp(at, "mid_delete")) {
        usleep(200000);
        kill(pid, SIGKILL);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    printf("crashrun at=%s status=%d\n", at, st);
    edb_error err;
    edb_db *d = edb_open(db, false, false, NULL, &err);
    if (!d) {
        printf("reopen: FAIL %s\n", err.message);
        return 1;
    }
    printf("reopen: OK\n");
    edb_exec(d, "SELECT COUNT(*) FROM t;", &err);
    edb_close(d);
    return 0;
}
