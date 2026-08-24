#include "edb/edb_api.h"
#include "edb/sql_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    fprintf(stderr,
        "Usage: edb [options] [database]\n"
        "  -c, --create     create database\n"
        "  -r, --readonly   open read-only\n"
        "  -p <password>    password\n"
        "  -e <sql>         execute one statement\n"
        "  --dump           SQL dump of all tables to stdout\n"
        "  --import <file>  execute SQL statements from file\n"
        "  -h, --help       show this help\n");
}

/* Minimal dump: for each table SELECT * and emit INSERT statements via COUNT + scan through exec print path is limited.
 * Use public API prepare/step if available. */
static int dump_db(edb_db *db, edb_error *err) {
    return edb_dump_sql(db, stdout, err);
}

static int import_file(edb_db *db, const char *path, edb_error *err) {
    FILE *f = fopen(path, "r");
    if (!f) {
        edb_error_set(err, EDB_IO, errno, "cannot open import file");
        return -1;
    }
    char line[8192];
    char stmt[16384];
    size_t sp = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '-' && line[1] == '-') continue;
        size_t n = strlen(line);
        if (sp + n >= sizeof stmt) {
            fclose(f);
            edb_error_set(err, EDB_LIMIT, 0, "statement too long");
            return -1;
        }
        memcpy(stmt + sp, line, n);
        sp += n;
        stmt[sp] = 0;
        /* execute on semicolon */
        for (size_t i = 0; i < sp; i++) {
            if (stmt[i] == ';') {
                stmt[i] = 0;
                if (edb_exec(db, stmt, err) != 0) {
                    fclose(f);
                    return -1;
                }
                size_t rest = sp - (i + 1);
                memmove(stmt, stmt + i + 1, rest);
                sp = rest;
                stmt[sp] = 0;
                i = (size_t)-1;
            }
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    bool create = false, readonly = false, do_dump = false;
    const char *password = NULL;
    const char *exec_sql = NULL;
    const char *path = NULL;
    const char *import_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(); return 0;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--create") == 0) {
            create = true; continue;
        }
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--readonly") == 0) {
            readonly = true; continue;
        }
        if ((strcmp(argv[i], "-p") == 0) && i+1 < argc) {
            password = argv[++i]; continue;
        }
        if ((strcmp(argv[i], "-e") == 0) && i+1 < argc) {
            exec_sql = argv[++i]; continue;
        }
        if (strcmp(argv[i], "--dump") == 0) {
            do_dump = true; continue;
        }
        if (strcmp(argv[i], "--import") == 0 && i+1 < argc) {
            import_path = argv[++i]; continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            return 1;
        }
        path = argv[i];
    }

    if (!path) { usage(); return 1; }

    edb_error err;
    edb_db *db = edb_open(path, create, readonly, password, &err);
    if (!db) {
        fprintf(stderr, "open failed: [%d] %s\n", err.cat, err.message);
        return 1;
    }
    printf("opened %s (create=%d)\n", path, create);

    if (import_path) {
        if (import_file(db, import_path, &err) != 0) {
            fprintf(stderr, "import failed: [%d] %s\n", err.cat, err.message);
            edb_close(db);
            return 1;
        }
        printf("import ok\n");
    } else if (do_dump) {
        edb_dump_sql(db, stdout, &err);
        printf("dump ok\n");
    } else if (exec_sql) {
        if (edb_exec(db, exec_sql, &err) != 0) {
            fprintf(stderr, "exec failed: [%d] %s\n", err.cat, err.message);
            edb_close(db);
            return 1;
        }
        printf("ok\n");
    } else {
        char line[4096];
        printf("edb> ");
        fflush(stdout);
        while (fgets(line, sizeof line, stdin)) {
            size_t n = strlen(line);
            while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n] = 0;
            if (strcmp(line, ".quit") == 0 || strcmp(line, ".exit") == 0) break;
            if (n == 0) { printf("edb> "); fflush(stdout); continue; }
            if (edb_exec(db, line, &err) != 0)
                fprintf(stderr, "error: [%d] %s\n", err.cat, err.message);
            else
                printf("ok\n");
            printf("edb> ");
            fflush(stdout);
        }
    }

    edb_close(db);
    return 0;
}
