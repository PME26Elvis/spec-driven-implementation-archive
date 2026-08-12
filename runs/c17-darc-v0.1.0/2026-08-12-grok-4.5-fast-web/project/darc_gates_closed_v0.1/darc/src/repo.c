#define _POSIX_C_SOURCE 200809L
#include "darc_repo.h"
#include "darc_lzh1.h"
#include "darc_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <limits.h>

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

int darc_repo_init(const char *path) {
    if (mkdir_p(path, 0755) < 0) return -1;
    char buf[PATH_MAX];
    const char *subs[] = {
        "refs/snapshots", "objects/sha256", "index", "parity",
        "state", "journal", "tmp", "locks", NULL
    };
    for (int i = 0; subs[i]; ++i) {
        snprintf(buf, sizeof(buf), "%s/%s", path, subs[i]);
        if (mkdir_p(buf, 0755) < 0) return -1;
    }
    /* FORMAT */
    const char *fmt =
        "DARC\nformat=1\nhash=sha256\nchunking=buzhash64\n"
        "compression=lzh1\nparity=xor8+1\n";
    snprintf(buf, sizeof(buf), "%s/FORMAT", path);
    FILE *f = fopen(buf, "w");
    if (!f) return -1;
    fputs(fmt, f);
    fclose(f);
    /* empty HEAD */
    snprintf(buf, sizeof(buf), "%s/HEAD", path);
    f = fopen(buf, "w");
    if (!f) return -1;
    fclose(f);
    /* empty parity catalog */
    snprintf(buf, sizeof(buf), "%s/parity/CATALOG", path);
    f = fopen(buf, "w");
    if (!f) return -1;
    fclose(f);
    return 0;
}

darc_repo_t *darc_repo_open(const char *path) {
    char fmtpath[PATH_MAX];
    snprintf(fmtpath, sizeof(fmtpath), "%s/FORMAT", path);
    FILE *f = fopen(fmtpath, "r");
    if (!f) return NULL;
    char line[256];
    int ok = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "format=1", 8) == 0) ok = 1;
    }
    fclose(f);
    if (!ok) return NULL;
    darc_repo_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->path = strdup(path);
    if (!r->path) { free(r); return NULL; }
    r->lock_fd = -1;
    return r;
}

void darc_repo_close(darc_repo_t *repo) {
    if (!repo) return;
    darc_repo_unlock(repo);
    free(repo->path);
    free(repo);
}

char *darc_repo_object_path(const darc_repo_t *repo, const darc_cid_t cid) {
    char hex[65];
    darc_cid_hex(cid, hex);
    char *p = malloc(strlen(repo->path) + 128);
    if (!p) return NULL;
    sprintf(p, "%s/objects/sha256/%.2s/%s", repo->path, hex, hex + 2);
    return p;
}

char *darc_repo_ref_path(const darc_repo_t *repo, const char *name) {
    char *p = malloc(strlen(repo->path) + strlen(name) + 32);
    if (!p) return NULL;
    sprintf(p, "%s/refs/snapshots/%s", repo->path, name);
    return p;
}

char *darc_repo_tmp_path(const darc_repo_t *repo, const char *suffix) {
    char *p = malloc(strlen(repo->path) + strlen(suffix) + 32);
    if (!p) return NULL;
    sprintf(p, "%s/tmp/%s.%d", repo->path, suffix, (int)getpid());
    return p;
}

int darc_repo_lock(darc_repo_t *repo) {
    if (repo->locked) return 0;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/locks/repo.lock", repo->path);
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        close(fd);
        return -1;
    }
    repo->lock_fd = fd;
    repo->locked = true;
    return 0;
}

void darc_repo_unlock(darc_repo_t *repo) {
    if (!repo->locked) return;
    flock(repo->lock_fd, LOCK_UN);
    close(repo->lock_fd);
    repo->lock_fd = -1;
    repo->locked = false;
}

int darc_repo_atomic_write(const darc_repo_t *repo, const char *relpath,
                           const void *data, size_t len) {
    char full[PATH_MAX], tmp[PATH_MAX];
    snprintf(full, sizeof(full), "%s/%s", repo->path, relpath);
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", full, (int)getpid());
    /* ensure parent */
    char *slash = strrchr(full, '/');
    if (slash) {
        *slash = 0;
        mkdir_p(full, 0755);
        *slash = '/';
    }
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    size_t w = 0;
    while (w < len) {
        ssize_t n = write(fd, (const char*)data + w, len - w);
        if (n <= 0) { close(fd); unlink(tmp); return -1; }
        w += (size_t)n;
    }
    fsync(fd);
    close(fd);
    if (rename(tmp, full) < 0) { unlink(tmp); return -1; }
    /* fsync directory */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", full);
    slash = strrchr(dir, '/');
    if (slash) *slash = 0;
    int dfd = open(dir, O_RDONLY);
    if (dfd >= 0) { fsync(dfd); close(dfd); }
    return 0;
}

int darc_journal_begin(darc_repo_t *repo, const char *op) {
    char buf[256];
    snprintf(buf, sizeof(buf), "op=%s\npid=%d\n", op, (int)getpid());
    return darc_repo_atomic_write(repo, "journal/current", buf, strlen(buf));
}

int darc_journal_commit(darc_repo_t *repo) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/journal/current", repo->path);
    unlink(path);
    return 0;
}

int darc_journal_abort(darc_repo_t *repo) {
    return darc_journal_commit(repo);
}

int darc_journal_recover(darc_repo_t *repo) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/journal/current", repo->path);
    if (access(path, F_OK) != 0) return 0; /* nothing to recover */
    /* Stale journal: remove incomplete temp objects under tmp/ and clear journal.
       Published refs remain valid. */
    char tmpdir[PATH_MAX];
    snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", repo->path);
    DIR *d = opendir(tmpdir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char fp[PATH_MAX];
            snprintf(fp, sizeof(fp), "%s/%s", tmpdir, e->d_name);
            unlink(fp);
        }
        closedir(d);
    }
    unlink(path);
    return 0;
}

int darc_repo_set_head(darc_repo_t *repo, const darc_cid_t cid) {
    char hex[65];
    darc_cid_hex(cid, hex);
    char line[80];
    snprintf(line, sizeof(line), "%s\n", hex);
    return darc_repo_atomic_write(repo, "HEAD", line, strlen(line));
}

int darc_repo_get_head(darc_repo_t *repo, darc_cid_t out) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/HEAD", repo->path);
    size_t len;
    uint8_t *data = darc_read_file(path, &len);
    if (!data || len < 64) { free(data); return -1; }
    char hex[65];
    memcpy(hex, data, 64); hex[64] = 0;
    free(data);
    return darc_cid_from_hex(hex, out);
}

int darc_repo_write_snapshot_ref(darc_repo_t *repo, const darc_cid_t cid) {
    char hex[65];
    darc_cid_hex(cid, hex);
    char rel[128];
    snprintf(rel, sizeof(rel), "refs/snapshots/%s", hex);
    char line[80];
    snprintf(line, sizeof(line), "%s\n", hex);
    return darc_repo_atomic_write(repo, rel, line, strlen(line));
}

int darc_repo_delete_snapshot_ref(darc_repo_t *repo, const darc_cid_t cid) {
    char *path = darc_repo_ref_path(repo, "");
    /* rebuild: list and delete specific */
    free(path);
    char hex[65];
    darc_cid_hex(cid, hex);
    char full[PATH_MAX];
    snprintf(full, sizeof(full), "%s/refs/snapshots/%s", repo->path, hex);
    return unlink(full);
}

int darc_repo_list_snapshot_refs(darc_repo_t *repo, darc_cid_t **out, size_t *count) {
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/refs/snapshots", repo->path);
    DIR *d = opendir(dir);
    if (!d) { *out = NULL; *count = 0; return 0; }
    size_t cap = 16, n = 0;
    darc_cid_t *arr = malloc(cap * sizeof(darc_cid_t));
    if (!arr) { closedir(d); return -1; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strlen(e->d_name) != 64) continue;
        if (n >= cap) {
            cap *= 2;
            darc_cid_t *na = realloc(arr, cap * sizeof(darc_cid_t));
            if (!na) { free(arr); closedir(d); return -1; }
            arr = na;
        }
        if (darc_cid_from_hex(e->d_name, arr[n]) == 0)
            n++;
    }
    closedir(d);
    *out = arr;
    *count = n;
    return 0;
}

int darc_repo_has_object(const darc_repo_t *repo, const darc_cid_t cid) {
    char *path = darc_repo_object_path(repo, cid);
    if (!path) return 0;
    int ok = access(path, F_OK) == 0;
    free(path);
    return ok;
}

int darc_repo_put_object(darc_repo_t *repo, uint8_t type, uint8_t codec,
                         const void *uncomp, size_t uncomp_len,
                         const void *stored, size_t stored_len,
                         darc_cid_t out_cid) {
    darc_cid_compute(type, 1, uncomp, uncomp_len, out_cid);
    if (darc_repo_has_object(repo, out_cid))
        return 0; /* already present */
    size_t framed_len;
    uint8_t *framed = darc_object_frame(type, codec, uncomp, uncomp_len, stored, stored_len, &framed_len);
    if (!framed) return -1;
    char *path = darc_repo_object_path(repo, out_cid);
    if (!path) { free(framed); return -1; }
    /* ensure parent dir */
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = 0;
        mkdir_p(path, 0755);
        *slash = '/';
    }
    int rc = darc_object_write_file(path, framed, framed_len);
    free(path);
    free(framed);
    return rc;
}

int darc_repo_get_object(const darc_repo_t *repo, const darc_cid_t cid,
                         uint8_t *type, uint8_t **payload, size_t *payload_len) {
    char *path = darc_repo_object_path(repo, cid);
    if (!path) return -1;
    size_t flen;
    uint8_t *framed = darc_read_file(path, &flen);
    free(path);
    if (!framed) return -1;
    uint8_t codec;
    size_t uncomp_len, stored_len;
    const uint8_t *stored_payload;
    int rc = darc_object_unframe(framed, flen, type, &codec, &uncomp_len, &stored_len, &stored_payload);
    if (rc != 0) { free(framed); return rc; }
    uint8_t *raw = NULL;
    size_t raw_len = 0;
    if (codec == DARC_CODEC_RAW) {
        raw = malloc(stored_len);
        if (!raw) { free(framed); return -1; }
        memcpy(raw, stored_payload, stored_len);
        raw_len = stored_len;
    } else if (codec == DARC_CODEC_LZH1) {
        raw = darc_lzh1_decompress(stored_payload, stored_len, uncomp_len, &raw_len);
        if (!raw) { free(framed); return -1; }
    } else {
        free(framed); return -1;
    }
    free(framed);
    /* Verify CID */
    darc_cid_t check;
    darc_cid_compute(*type, 1, raw, raw_len, check);
    if (memcmp(check, cid, 32) != 0) {
        free(raw);
        return -4; /* CID mismatch */
    }
    *payload = raw;
    *payload_len = raw_len;
    return 0;
}
