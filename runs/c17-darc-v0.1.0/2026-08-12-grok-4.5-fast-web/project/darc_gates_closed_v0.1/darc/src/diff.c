#define _POSIX_C_SOURCE 200809L
#include "darc_diff.h"
#include "darc_snapshot.h"
#include "darc_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *path;
    uint8_t type;
    darc_cid_t cid;
    uint64_t size;
} path_entry_t;

typedef struct {
    path_entry_t *e;
    size_t n, cap;
} path_list_t;

static void pl_init(path_list_t *p) { memset(p, 0, sizeof(*p)); }
static void pl_free(path_list_t *p) {
    for (size_t i = 0; i < p->n; ++i) free(p->e[i].path);
    free(p->e);
}
static int pl_add(path_list_t *p, const char *path, uint8_t type, const darc_cid_t cid, uint64_t size) {
    if (p->n >= p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 32;
        path_entry_t *ne = realloc(p->e, nc * sizeof(*ne));
        if (!ne) return -1;
        p->e = ne; p->cap = nc;
    }
    p->e[p->n].path = strdup(path);
    p->e[p->n].type = type;
    memcpy(p->e[p->n].cid, cid, 32);
    p->e[p->n].size = size;
    p->n++;
    return 0;
}
static int pl_cmp(const void *a, const void *b) {
    return strcmp(((const path_entry_t*)a)->path, ((const path_entry_t*)b)->path);
}

static int walk_tree(darc_repo_t *repo, const darc_cid_t tree, const char *prefix, path_list_t *pl) {
    uint8_t type;
    uint8_t *payload = NULL;
    size_t plen = 0;
    if (darc_repo_get_object(repo, tree, &type, &payload, &plen) != 0 || type != DARC_TYPE_TREE)
        return -1;
    size_t off = 2;
    if (plen < 10) { free(payload); return -1; }
    uint64_t nent = darc_read_u64_le(payload + off); off += 8;
    for (uint64_t i = 0; i < nent; ++i) {
        if (off + 8 > plen) break;
        uint64_t nlen = darc_read_u64_le(payload + off); off += 8;
        if (off + nlen + 1 + 4 + 8 > plen) break;
        char name[1024];
        if (nlen >= sizeof(name)) { free(payload); return -1; }
        memcpy(name, payload + off, (size_t)nlen); name[nlen] = 0;
        off += (size_t)nlen;
        uint8_t et = payload[off++];
        off += 4 + 8; /* mode + mtime */
        char full[2048];
        if (prefix[0])
            snprintf(full, sizeof(full), "%s/%s", prefix, name);
        else
            snprintf(full, sizeof(full), "%s", name);
        if (et == 1 || et == 2) {
            if (off + 32 > plen) break;
            darc_cid_t cid;
            memcpy(cid, payload + off, 32); off += 32;
            if (et == 1) {
                pl_add(pl, full, 1, cid, 0);
            } else {
                pl_add(pl, full, 2, cid, 0);
                walk_tree(repo, cid, full, pl);
            }
        } else if (et == 3) {
            if (off + 8 > plen) break;
            uint64_t tlen = darc_read_u64_le(payload + off); off += 8;
            darc_cid_t zero = {0};
            pl_add(pl, full, 3, zero, 0);
            off += (size_t)tlen;
        }
    }
    free(payload);
    return 0;
}

int darc_snapshot_diff(darc_repo_t *repo, const darc_cid_t old_cid, const darc_cid_t new_cid,
                       const char *path_filter, const char *format) {
    (void)path_filter;
    darc_snapshot_info_t oi, ni;
    if (darc_snapshot_load_info(repo, old_cid, &oi) != 0) return -1;
    if (darc_snapshot_load_info(repo, new_cid, &ni) != 0) return -1;

    path_list_t oldpl, newpl;
    pl_init(&oldpl); pl_init(&newpl);
    walk_tree(repo, oi.root_tree, "", &oldpl);
    walk_tree(repo, ni.root_tree, "", &newpl);
    qsort(oldpl.e, oldpl.n, sizeof(path_entry_t), pl_cmp);
    qsort(newpl.e, newpl.n, sizeof(path_entry_t), pl_cmp);

    size_t added = 0, removed = 0, modified = 0;
    size_t i = 0, j = 0;
    /* merge walk */
    typedef struct { char *path; char kind; } change_t;
    change_t *ch = NULL;
    size_t nc = 0, ccap = 0;

    while (i < oldpl.n || j < newpl.n) {
        int cmp = 0;
        if (i >= oldpl.n) cmp = 1;
        else if (j >= newpl.n) cmp = -1;
        else cmp = strcmp(oldpl.e[i].path, newpl.e[j].path);

        if (cmp < 0) {
            /* removed */
            removed++;
            if (nc >= ccap) { ccap = ccap ? ccap*2 : 16; ch = realloc(ch, ccap*sizeof(*ch)); }
            ch[nc].path = strdup(oldpl.e[i].path);
            ch[nc].kind = 'D';
            nc++; i++;
        } else if (cmp > 0) {
            added++;
            if (nc >= ccap) { ccap = ccap ? ccap*2 : 16; ch = realloc(ch, ccap*sizeof(*ch)); }
            ch[nc].path = strdup(newpl.e[j].path);
            ch[nc].kind = 'A';
            nc++; j++;
        } else {
            if (oldpl.e[i].type != newpl.e[j].type ||
                memcmp(oldpl.e[i].cid, newpl.e[j].cid, 32) != 0) {
                modified++;
                if (nc >= ccap) { ccap = ccap ? ccap*2 : 16; ch = realloc(ch, ccap*sizeof(*ch)); }
                ch[nc].path = strdup(newpl.e[j].path);
                ch[nc].kind = 'M';
                nc++;
            }
            i++; j++;
        }
    }

    if (format && strcmp(format, "json") == 0) {
        printf("{\"added\":%zu,\"removed\":%zu,\"modified\":%zu,\"changes\":[",
               added, removed, modified);
        for (size_t k = 0; k < nc; ++k) {
            if (k) printf(",");
            printf("{\"path\":\"%s\",\"kind\":\"%c\"}", ch[k].path, ch[k].kind);
        }
        printf("]}\n");
    } else {
        printf("Diff summary\n");
        printf("  Added:    %zu\n", added);
        printf("  Removed:  %zu\n", removed);
        printf("  Modified: %zu\n", modified);
        printf("\n");
        for (size_t k = 0; k < nc; ++k)
            printf("%c  %s\n", ch[k].kind, ch[k].path);
    }

    for (size_t k = 0; k < nc; ++k) free(ch[k].path);
    free(ch);
    pl_free(&oldpl);
    pl_free(&newpl);
    return 0;
}
