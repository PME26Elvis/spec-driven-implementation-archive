/* edb-fixture — generate deterministic fixture databases (FIX-001 partial) */
#include "edb/edb_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    fprintf(stderr,
        "Usage: edb-fixture --scenario <name> --seed <n> --out <path>\n"
        "Scenarios: tasks_small, tasks_medium\n");
}

int main(int argc, char **argv) {
    const char *scenario = "tasks_small";
    const char *out = NULL;
    unsigned seed = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        if (!strcmp(argv[i], "--scenario") && i+1 < argc) { scenario = argv[++i]; continue; }
        if (!strcmp(argv[i], "--seed") && i+1 < argc) { seed = (unsigned)atoi(argv[++i]); continue; }
        if (!strcmp(argv[i], "--out") && i+1 < argc) { out = argv[++i]; continue; }
        fprintf(stderr, "unknown arg %s\n", argv[i]); return 1;
    }
    if (!out) { usage(); return 1; }
    unlink(out);

    edb_error err;
    edb_db *db = edb_open(out, true, false, NULL, &err);
    if (!db) { fprintf(stderr, "create failed: %s\n", err.message); return 1; }

    if (edb_exec(db,
        "CREATE TABLE tasks (id INTEGER PRIMARY KEY, title TEXT NOT NULL, "
        "priority INTEGER, category TEXT, deadline TEXT);", &err) != 0) {
        fprintf(stderr, "DDL fail: %s\n", err.message); edb_close(db); return 1;
    }
    if (edb_exec(db, "CREATE INDEX idx_cat_pri ON tasks (category, priority);", &err) != 0) {
        fprintf(stderr, "index fail: %s\n", err.message); edb_close(db); return 1;
    }

    int n = !strcmp(scenario, "tasks_medium") ? 500 : 50;
    /* simple LCG for deterministic "random" */
    unsigned s = seed ? seed : 1;
    for (int i = 1; i <= n; i++) {
        s = s * 1103515245u + 12345u;
        int pri = (int)(s % 5) + 1;
        const char *cats[] = {"work", "home", "学习", "other"};
        const char *cat = cats[s % 4];
        char sql[512];
        if (i % 7 == 0)
            snprintf(sql, sizeof sql,
                "INSERT INTO tasks VALUES (%d, '任务%d 中文', %d, '%s', NULL);", i, i, pri, cat);
        else
            snprintf(sql, sizeof sql,
                "INSERT INTO tasks VALUES (%d, 'task-%d', %d, '%s', '2026-01-%02d');",
                i, i, pri, cat, 1 + (i % 28));
        if (edb_exec(db, sql, &err) != 0) {
            fprintf(stderr, "insert %d fail: %s\n", i, err.message);
            edb_close(db); return 1;
        }
    }

    edb_close(db);
    printf("fixture %s seed=%u rows=%d -> %s\n", scenario, seed, n, out);
    return 0;
}
