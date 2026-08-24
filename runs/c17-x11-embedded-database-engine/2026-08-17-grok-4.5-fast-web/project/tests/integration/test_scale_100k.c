#include "edb/edb_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/scale100k.edb");
    edb_db *db = edb_open("/tmp/scale100k.edb", true, false, NULL, &err);
    if (!db) { printf("FAIL open %s\n", err.message); return 1; }
    if (edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER);", &err) != 0) {
        printf("FAIL create %s\n", err.message); return 1;
    }
    const int N = 100000;
    for (int i = 1; i <= N; i++) {
        char sql[128];
        snprintf(sql, sizeof sql, "INSERT INTO t VALUES (%d, %d);", i, i % 1000);
        if (edb_exec(db, sql, &err) != 0) {
            printf("FAIL insert %d %s\n", i, err.message); return 1;
        }
        if (i % 20000 == 0) fprintf(stderr, "inserted %d\n", i);
    }
    if (edb_exec(db, "SELECT COUNT(*) FROM t;", &err) != 0) {
        printf("FAIL count\n"); return 1;
    }
    edb_close(db);
    db = edb_open("/tmp/scale100k.edb", false, false, NULL, &err);
    if (!db) { printf("FAIL reopen\n"); return 1; }
    if (edb_exec(db, "SELECT COUNT(*) FROM t;", &err) != 0) {
        printf("FAIL count2\n"); return 1;
    }
    edb_close(db);
    printf("PASS scale 100k rows\n");
    return 0;
}
