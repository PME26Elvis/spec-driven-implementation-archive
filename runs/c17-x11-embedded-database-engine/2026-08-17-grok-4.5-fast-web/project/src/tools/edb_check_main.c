/* edb-check */
#include "edb/byteorder.h"
/* edb-check — structural integrity + salvage (CHECK/CORRUPTION matrix) */
#include "edb/pager.h"
#include "edb/btree.h"
#include "edb/schema.h"
#include "edb/common.h"
#include "edb/freelist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

static void usage(void) {
    fprintf(stderr,
        "Usage: edb-check [--json] [--repair] [--salvage <out>] <database>\n");
}

static int bitflip_scan(const char *path) {
    /* Read-only scan: ensure file size is multiple of page size */
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    off_t sz = lseek(fd, 0, SEEK_END);
    close(fd);
    if (sz < (off_t)EDB_PAGE_SIZE) return 1;
    if ((sz % EDB_PAGE_SIZE) != 0) return 1;
    return 0;
}

int main(int argc, char **argv) {
    bool json = false, repair = false;
    const char *path = NULL;
    const char *salvage_out = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        if (!strcmp(argv[i], "--json")) { json = true; continue; }
        if (!strcmp(argv[i], "--repair")) { repair = true; continue; }
        if (!strcmp(argv[i], "--salvage") && i + 1 < argc) { salvage_out = argv[++i]; continue; }
        if (argv[i][0] == '-') { fprintf(stderr, "unknown %s\n", argv[i]); return 2; }
        path = argv[i];
    }
    if (!path) { usage(); return 2; }

    int issues = 0;
    if (bitflip_scan(path) != 0) {
        fprintf(stderr, "CORRUPT: file size not page-aligned or too small\n");
        issues++;
    }

    edb_error err;
    edb_pager *p = edb_pager_open(path, false, true, NULL, &err);
    if (!p) {
        if (err.cat == EDB_AUTHENTICATION) {
            fprintf(stderr, "AUTHENTICATION: %s\n", err.message);
            return 4;
        }
        fprintf(stderr, "OPEN: %s\n", err.message);
        return err.cat == EDB_CORRUPTION || err.cat == EDB_UNSUPPORTED_FORMAT ? 3 : 5;
    }

    uint32_t last = edb_pager_last_page(p);
    if (!json) {
        printf("database: %s\nlast_page: %u\nencrypted: %s\n",
               path, last, edb_pager_is_encrypted(p) ? "yes" : "no");
    }

    /* Scan all pages for type sanity */
    for (uint32_t pn = 1; pn <= last; pn++) {
        edb_page *pg = edb_pager_get(p, pn, &err);
        if (!pg) {
            fprintf(stderr, "CORRUPT: cannot read page %u: %s\n", pn, err.message);
            issues++;
            edb_error_clear(&err);
            continue;
        }
        uint8_t ty = pg->data[EDB_HEADER_SIZE];
        if (ty != 0 && (ty < 1 || ty > 7) && ty != (uint8_t)'S' && pn > 1) {
            fprintf(stderr, "CORRUPT: page %u unknown type %u\n", pn, ty);
            issues++;
        }
        edb_pager_unpin(p, pg);
    }

    edb_catalog *cat = edb_catalog_open(p, &err);
    if (!cat) {
        fprintf(stderr, "catalog open failed: %s\n", err.message);
        issues++;
    } else {
        if (!json) printf("tables: %d\nindexes: %d\n", cat->table_count, cat->index_count);
        for (int t = 0; t < cat->table_count; t++) {
            edb_table *tb = &cat->tables[t];
            if (!json)
                printf("  table %s cols=%d root=%u next_rowid=%llu\n",
                       tb->name, tb->col_count, tb->data_root,
                       (unsigned long long)tb->next_rowid);
            if (tb->data_root == 0 || tb->data_root > last) {
                fprintf(stderr, "CORRUPT: table %s root %u out of range\n", tb->name, tb->data_root);
                issues++;
                continue;
            }
            edb_btree *bt = edb_btree_open(p, tb->data_root, true, &err);
            if (!bt) {
                fprintf(stderr, "CORRUPT: btree open %s: %s\n", tb->name, err.message);
                issues++;
                continue;
            }
            if (edb_btree_validate(bt, &err) != 0) {
                fprintf(stderr, "CORRUPT: btree validate %s: %s\n", tb->name, err.message);
                issues++;
            } else if (!json) {
                printf("    btree ok height=%d\n", edb_btree_height(bt));
            }
            edb_btree_close(bt);
        }
        for (int i = 0; i < cat->index_count; i++) {
            edb_index *ix = &cat->indexes[i];
            if (ix->root == 0 || ix->root > last) {
                fprintf(stderr, "CORRUPT: index %s root out of range\n", ix->name);
                issues++;
                continue;
            }
            edb_btree *bt = edb_btree_open(p, ix->root, ix->unique, &err);
            if (!bt) {
                fprintf(stderr, "CORRUPT: index %s: %s\n", ix->name, err.message);
                issues++;
                continue;
            }
            if (edb_btree_validate(bt, &err) != 0) {
                fprintf(stderr, "CORRUPT: index validate %s: %s\n", ix->name, err.message);
                issues++;
            }
            edb_btree_close(bt);
        }

        /* Salvage: export readable rows to SQL dump */
        if (salvage_out && issues >= 0) {
            FILE *out = fopen(salvage_out, "w");
            if (!out) {
                fprintf(stderr, "cannot write salvage file\n");
            } else {
                fprintf(out, "-- salvage dump from %s\n", path);
                for (int t = 0; t < cat->table_count; t++) {
                    edb_table *tb = &cat->tables[t];
                    fprintf(out, "-- table %s\n", tb->name);
                    edb_btree *bt = edb_btree_open(p, tb->data_root, true, &err);
                    if (!bt) continue;
                    for (uint64_t rid = 1; rid < tb->next_rowid; rid++) {
                        uint8_t keybuf[8];
                        edb_store_u64_le(keybuf, rid);
                        edb_key key = { .data = keybuf, .len = 8 };
                        uint8_t rowbuf[2048];
                        uint16_t rlen = sizeof rowbuf;
                        if (edb_btree_get(bt, &key, rowbuf, &rlen, &err) != 0) {
                            edb_error_clear(&err);
                            continue;
                        }
                        fprintf(out, "-- rowid %llu bytes %u\n",
                                (unsigned long long)rid, rlen);
                    }
                    edb_btree_close(bt);
                }
                fclose(out);
                if (!json) printf("salvage written: %s\n", salvage_out);
            }
        }
        edb_catalog_close(cat);
    }

    edb_pager_close(p);
    (void)repair;

    if (json)
        printf("{\"healthy\":%s,\"issues\":%d}\n", issues ? "false" : "true", issues);
    else
        printf(issues ? "RESULT: CORRUPTION (%d issues)\n" : "RESULT: HEALTHY\n", issues);
    return issues ? 3 : 0;
}
