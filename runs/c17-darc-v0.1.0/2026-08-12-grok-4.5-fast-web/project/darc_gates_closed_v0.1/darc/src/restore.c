#define _POSIX_C_SOURCE 200809L
#include "darc_restore.h"
#include "darc_snapshot.h"
#include "darc_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

typedef struct cid_path {
    darc_cid_t cid;
    char *path;
    struct cid_path *next;
} cid_path_t;

static int mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len-1] == '/') tmp[len-1] = 0;
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) < 0 && errno != EEXIST) return -1;
    return 0;
}

static cid_path_t *cid_find(cid_path_t *h, const darc_cid_t cid) {
    for (; h; h = h->next)
        if (memcmp(h->cid, cid, 32) == 0) return h;
    return NULL;
}

static int cid_add(cid_path_t **h, const darc_cid_t cid, const char *path) {
    cid_path_t *n = malloc(sizeof(*n));
    if (!n) return -1;
    memcpy(n->cid, cid, 32);
    n->path = strdup(path);
    n->next = *h;
    *h = n;
    return 0;
}

static void cid_free(cid_path_t *h) {
    while (h) {
        cid_path_t *n = h->next;
        free(h->path);
        free(h);
        h = n;
    }
}

static int restore_file_data(darc_repo_t *repo, const darc_cid_t file_cid,
                             const char *path, mode_t mode) {
    uint8_t type;
    uint8_t *payload = NULL;
    size_t plen = 0;
    if (darc_repo_get_object(repo, file_cid, &type, &payload, &plen) != 0 || type != DARC_TYPE_FILE)
        return -1;
    if (plen < 2 + 8 + 8) { free(payload); return -1; }
    size_t off = 2;
    uint64_t logical = darc_read_u64_le(payload + off); off += 8;
    uint64_t nchunks = darc_read_u64_le(payload + off); off += 8;

    char parent[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = 0; mkdir_p(parent, 0755); }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode ? mode : 0644);
    if (fd < 0) { free(payload); return -1; }

    uint64_t written = 0;
    for (uint64_t i = 0; i < nchunks; ++i) {
        if (off + 40 > plen) { close(fd); free(payload); return -1; }
        darc_cid_t ccid;
        memcpy(ccid, payload + off, 32); off += 32;
        uint64_t clen = darc_read_u64_le(payload + off); off += 8;
        uint8_t ctype;
        uint8_t *cdata = NULL;
        size_t cdlen = 0;
        if (darc_repo_get_object(repo, ccid, &ctype, &cdata, &cdlen) != 0 || ctype != DARC_TYPE_CHUNK) {
            close(fd); free(payload); return -1;
        }
        if (cdlen != clen) { free(cdata); close(fd); free(payload); return -1; }
        size_t w = 0;
        while (w < cdlen) {
            ssize_t n = write(fd, cdata + w, cdlen - w);
            if (n <= 0) { free(cdata); close(fd); free(payload); return -1; }
            w += (size_t)n;
        }
        written += cdlen;
        free(cdata);
    }
    /* optional whole-file hash at end of payload */
    if (off + 32 <= plen) {
        uint8_t expected[32];
        memcpy(expected, payload + off, 32);
        /* could re-hash file for verification */
        (void)expected;
    }
    close(fd);
    free(payload);
    if (written != logical) return -1;
    return 0;
}

static int restore_tree(darc_repo_t *repo, const darc_cid_t tree_cid,
                        const char *dest, const char *path_filter, int overwrite,
                        cid_path_t **file_map);

static int restore_tree(darc_repo_t *repo, const darc_cid_t tree_cid,
                        const char *dest, const char *path_filter, int overwrite,
                        cid_path_t **file_map) {
    (void)path_filter;
    uint8_t type;
    uint8_t *payload = NULL;
    size_t plen = 0;
    if (darc_repo_get_object(repo, tree_cid, &type, &payload, &plen) != 0 || type != DARC_TYPE_TREE)
        return -1;
    if (plen < 10) { free(payload); return -1; }
    size_t off = 2;
    uint64_t nent = darc_read_u64_le(payload + off); off += 8;
    mkdir_p(dest, 0755);

    for (uint64_t i = 0; i < nent; ++i) {
        if (off + 8 > plen) { free(payload); return -1; }
        uint64_t nlen = darc_read_u64_le(payload + off); off += 8;
        if (off + nlen + 1 + 4 + 8 > plen) { free(payload); return -1; }
        char name[PATH_MAX];
        if (nlen >= PATH_MAX) { free(payload); return -1; }
        memcpy(name, payload + off, (size_t)nlen); name[nlen] = 0;
        off += (size_t)nlen;
        uint8_t etype = payload[off++];
        uint32_t mode = darc_read_u32_le(payload + off); off += 4;
        off += 8; /* mtime */

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dest, name);

        /* refuse escape */
        if (strstr(name, "..") != NULL) { free(payload); return -1; }

        if (etype == 1) {
            if (off + 32 > plen) { free(payload); return -1; }
            darc_cid_t fcid;
            memcpy(fcid, payload + off, 32); off += 32;
            if (access(full, F_OK) == 0 && overwrite == 0) continue;
            cid_path_t *existing = cid_find(*file_map, fcid);
            if (existing) {
                unlink(full);
                if (link(existing->path, full) != 0) {
                    /* fallback to copy */
                    if (restore_file_data(repo, fcid, full, (mode_t)mode) != 0) {
                        free(payload); return -1;
                    }
                }
            } else {
                if (restore_file_data(repo, fcid, full, (mode_t)mode) != 0) {
                    free(payload); return -1;
                }
                cid_add(file_map, fcid, full);
            }
        } else if (etype == 2) {
            if (off + 32 > plen) { free(payload); return -1; }
            darc_cid_t tcid;
            memcpy(tcid, payload + off, 32); off += 32;
            if (restore_tree(repo, tcid, full, path_filter, overwrite, file_map) != 0) {
                free(payload); return -1;
            }
            chmod(full, (mode_t)mode);
        } else if (etype == 3) {
            if (off + 8 > plen) { free(payload); return -1; }
            uint64_t tlen = darc_read_u64_le(payload + off); off += 8;
            if (off + tlen > plen) { free(payload); return -1; }
            char target[PATH_MAX];
            if (tlen >= PATH_MAX) { free(payload); return -1; }
            memcpy(target, payload + off, (size_t)tlen); target[tlen] = 0;
            off += (size_t)tlen;
            unlink(full);
            if (symlink(target, full) < 0) { free(payload); return -1; }
        }
    }
    free(payload);
    return 0;
}

int darc_restore(darc_repo_t *repo, const darc_cid_t snapshot,
                 const char *dest, const char *path_filter, int overwrite) {
    if (strstr(dest, "..") != NULL) return -1;
    darc_snapshot_info_t info;
    if (darc_snapshot_load_info(repo, snapshot, &info) != 0) return -1;
    cid_path_t *map = NULL;
    int rc = restore_tree(repo, info.root_tree, dest, path_filter, overwrite, &map);
    cid_free(map);
    return rc;
}
