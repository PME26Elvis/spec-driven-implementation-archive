#include "edb/edb_api.h"
#include <stdio.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/sp.edb");
    edb_db *db = edb_open("/tmp/sp.edb", true, false, NULL, &err);
    edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, v INTEGER);", &err);
    edb_exec(db, "BEGIN;", &err);
    edb_exec(db, "INSERT INTO t VALUES (1, 10);", &err);
    edb_exec(db, "SAVEPOINT s1;", &err);
    edb_exec(db, "INSERT INTO t VALUES (2, 20);", &err);
    edb_exec(db, "ROLLBACK TO s1;", &err);
    edb_exec(db, "COMMIT;", &err);
    /* Expect only row 1 */
    edb_exec(db, "SELECT COUNT(*) FROM t;", &err);
    edb_close(db);
    printf("PASS savepoint rollback to\n");
    return 0;
}
