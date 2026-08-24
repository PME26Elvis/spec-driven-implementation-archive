#include "edb/edb_api.h"
#include <stdio.h>
#include <unistd.h>
static int fail = 0;
static void ok(const char *n, int c) { if (c) printf("PASS %s\n", n); else { printf("FAIL %s\n", n); fail++; } }
int main(void) {
    edb_error err;
    unlink("/tmp/sqls.edb");
    edb_db *db = edb_open("/tmp/sqls.edb", true, false, NULL, &err);
    ok("open", db != NULL);
    ok("create", edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, score INTEGER);", &err)==0);
    ok("insert", edb_exec(db, "INSERT INTO t VALUES (1,'a',10),(2,'b',20),(3,'a',30);", &err)==0);
    ok("where", edb_exec(db, "SELECT * FROM t WHERE score > 15;", &err)==0);
    ok("order", edb_exec(db, "SELECT * FROM t ORDER BY score DESC;", &err)==0);
    ok("limit", edb_exec(db, "SELECT * FROM t LIMIT 2 OFFSET 1;", &err)==0);
    ok("group", edb_exec(db, "SELECT COUNT(*) FROM t GROUP BY name;", &err)==0);
    ok("between", edb_exec(db, "SELECT * FROM t WHERE score BETWEEN 10 AND 20;", &err)==0);
    ok("in", edb_exec(db, "SELECT * FROM t WHERE id IN (1,3);", &err)==0);
    ok("like", edb_exec(db, "SELECT * FROM t WHERE name LIKE 'a%';", &err)==0);
    ok("isnull", edb_exec(db, "INSERT INTO t VALUES (4,NULL,0); SELECT * FROM t WHERE name IS NULL;", &err)==0);
    ok("subq", edb_exec(db, "SELECT * FROM t WHERE score = (SELECT MAX(score) FROM t WHERE name = @name);", &err)==0);
    ok("join", edb_exec(db, "CREATE TABLE u (id INTEGER PRIMARY KEY, tid INTEGER); INSERT INTO u VALUES (10,1); SELECT * FROM t JOIN u ON id = tid;", &err)==0);
    ok("left", edb_exec(db, "SELECT * FROM t LEFT JOIN u ON id = tid;", &err)==0);
    ok("update", edb_exec(db, "UPDATE t SET score = 99 WHERE id = 1;", &err)==0);
    ok("delete", edb_exec(db, "DELETE FROM t WHERE id = 4;", &err)==0);
    ok("vacuum", edb_exec(db, "VACUUM;", &err)==0);
    ok("explain", edb_exec(db, "EXPLAIN SELECT * FROM t;", &err)==0);
    ok("analyze", edb_exec(db, "ANALYZE;", &err)==0);
    ok("index", edb_exec(db, "CREATE UNIQUE INDEX idx ON t (score); SELECT * FROM t WHERE score = 20;", &err)==0);
    ok("txn", edb_exec(db, "BEGIN; INSERT INTO t VALUES (5,'c',5); ROLLBACK;", &err)==0);
    ok("sp", edb_exec(db, "BEGIN; INSERT INTO t VALUES (6,'d',6); SAVEPOINT s1; INSERT INTO t VALUES (7,'e',7); ROLLBACK TO s1; COMMIT;", &err)==0);
    ok("drop", edb_exec(db, "CREATE TABLE z (id INTEGER PRIMARY KEY); DROP TABLE z;", &err)==0);
    edb_close(db);
    printf(fail ? "SQL_SURFACE fail=%d\n" : "SQL_SURFACE PASS\n", fail);
    return fail ? 1 : 0;
}
