#include "edb/edb_api.h"
#include <stdio.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/utf8.edb");
    edb_db *db = edb_open("/tmp/utf8.edb", true, false, NULL, &err);
    if (!db) return 1;
    if (edb_exec(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT);", &err) != 0) return 1;
    if (edb_exec(db, "INSERT INTO t VALUES (1, '中文測試');", &err) != 0) return 1;
    if (edb_exec(db, "INSERT INTO t VALUES (2, '你好世界');", &err) != 0) return 1;
    if (edb_exec(db, "SELECT * FROM t WHERE name = '中文測試';", &err) != 0) return 1;
    if (edb_exec(db, "SELECT * FROM t ORDER BY name;", &err) != 0) return 1;
    edb_close(db);
    printf("PASS utf8 chinese roundtrip\n");
    return 0;
}
