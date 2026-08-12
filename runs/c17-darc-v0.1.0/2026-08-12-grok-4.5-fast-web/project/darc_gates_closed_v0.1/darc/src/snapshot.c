#define _POSIX_C_SOURCE 200809L
#include "darc_snapshot.h"
#include "darc_buzhash.h"
#include "darc_lzh1.h"
#include "darc_util.h"
#include "darc_config.h"

/* optional runtime config */
darc_config_t *g_darc_config = NULL;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#define MIN_CHUNK 16384u
#define AVG_CHUNK 65536u
#define MAX_CHUNK 262144u
#define MIN_SAVINGS 32u

typedef struct {
    darc_cid_t cid;
    uint64_t length;
} chunk_ref_t;

typedef struct hl_map {
    ino_t ino;
    dev_t dev;
    darc_cid_t file_cid;
    struct hl_map *next;
} hl_map_t;

static hl_map_t *hl_find(hl_map_t *h, ino_t ino, dev_t dev) {
    for (; h; h = h->next)
        if (h->ino == ino && h->dev == dev) return h;
    return NULL;
}

static int process_file(darc_repo_t *repo, darc_index_t *idx, const char *path,
                        uint64_t *logical_out, uint64_t *new_chunks_out,
                        uint64_t *stored_out, darc_cid_t file_cid_out) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    uint64_t table[256];
    darc_buzhash_table_init(table);
    darc_buzhash_ctx bctx;
    darc_buzhash_reset(&bctx, table);

    chunk_ref_t *chunks = NULL;
    size_t nchunks = 0, ccap = 0;
    uint8_t *chunkbuf = malloc(MAX_CHUNK);
    if (!chunkbuf) { close(fd); return -1; }
    size_t cpos = 0;
    darc_sha256_ctx file_hash;
    darc_sha256_init(&file_hash);
    uint64_t logical = 0, new_chunks = 0, stored = 0;

    uint8_t byte;
    while (read(fd, &byte, 1) == 1) {
        darc_sha256_update(&file_hash, &byte, 1);
        chunkbuf[cpos++] = byte;
        logical++;
        uint64_t cmin = g_darc_config ? g_darc_config->chunk_min : MIN_CHUNK;
        uint64_t cavg = g_darc_config ? g_darc_config->chunk_avg : AVG_CHUNK;
        uint64_t cmax = g_darc_config ? g_darc_config->chunk_max : MAX_CHUNK;
        int cut = darc_buzhash_feed(&bctx, byte, (size_t)cmin, (size_t)cavg, (size_t)cmax);
        if (cut) {
            darc_cid_t ccid;
            darc_cid_compute(DARC_TYPE_CHUNK, 1, chunkbuf, cpos, ccid);
            uint8_t codec = DARC_CODEC_RAW;
            const void *sdata = chunkbuf;
            size_t slen = cpos;
            size_t clen = 0;
            uint8_t *comp = darc_lzh1_compress(chunkbuf, cpos, &clen);
            if (comp && clen + MIN_SAVINGS < cpos) {
                codec = DARC_CODEC_LZH1; sdata = comp; slen = clen;
            } else {
                free(comp); comp = NULL;
            }
            if (!darc_repo_has_object(repo, ccid)) {
                if (darc_repo_put_object(repo, DARC_TYPE_CHUNK, codec,
                        chunkbuf, cpos, sdata, slen, ccid) != 0) {
                    free(comp); free(chunkbuf); free(chunks); close(fd); return -1;
                }
                new_chunks++; stored += slen;
                darc_index_put(idx, ccid, DARC_TYPE_CHUNK, cpos);
            }
            free(comp);
            if (nchunks >= ccap) {
                ccap = ccap ? ccap * 2 : 8;
                chunk_ref_t *nc = realloc(chunks, ccap * sizeof(*chunks));
                if (!nc) { free(chunkbuf); free(chunks); close(fd); return -1; }
                chunks = nc;
            }
            memcpy(chunks[nchunks].cid, ccid, 32);
            chunks[nchunks].length = cpos;
            nchunks++;
            cpos = 0;
            darc_buzhash_reset(&bctx, table);
        }
    }
    if (cpos > 0 || logical == 0) {
        if (cpos > 0 || nchunks == 0) {
            darc_cid_t ccid;
            darc_cid_compute(DARC_TYPE_CHUNK, 1, chunkbuf, cpos, ccid);
            if (cpos > 0) {
                uint8_t codec = DARC_CODEC_RAW;
                const void *sdata = chunkbuf;
                size_t slen = cpos;
                size_t clen = 0;
                uint8_t *comp = darc_lzh1_compress(chunkbuf, cpos, &clen);
                if (comp && clen + MIN_SAVINGS < cpos) {
                    codec = DARC_CODEC_LZH1; sdata = comp; slen = clen;
                } else { free(comp); comp = NULL; }
                if (!darc_repo_has_object(repo, ccid)) {
                    darc_repo_put_object(repo, DARC_TYPE_CHUNK, codec,
                        chunkbuf, cpos, sdata, slen, ccid);
                    new_chunks++; stored += slen;
                    darc_index_put(idx, ccid, DARC_TYPE_CHUNK, cpos);
                }
                free(comp);
            }
            if (cpos > 0) {
                if (nchunks >= ccap) {
                    ccap = ccap ? ccap * 2 : 8;
                    chunks = realloc(chunks, ccap * sizeof(*chunks));
                }
                memcpy(chunks[nchunks].cid, ccid, 32);
                chunks[nchunks].length = cpos;
                nchunks++;
            }
        }
    }
    close(fd);
    free(chunkbuf);

    uint8_t file_digest[32];
    darc_sha256_final(&file_hash, file_digest);

    size_t payload_size = 2 + 8 + 8 + nchunks * (32 + 8) + 32;
    uint8_t *payload = malloc(payload_size);
    if (!payload) { free(chunks); return -1; }
    size_t off = 0;
    darc_write_u16_le(payload + off, 1); off += 2;
    darc_write_u64_le(payload + off, logical); off += 8;
    darc_write_u64_le(payload + off, nchunks); off += 8;
    for (size_t i = 0; i < nchunks; ++i) {
        memcpy(payload + off, chunks[i].cid, 32); off += 32;
        darc_write_u64_le(payload + off, chunks[i].length); off += 8;
    }
    memcpy(payload + off, file_digest, 32); off += 32;
    free(chunks);

    darc_cid_t fcid;
    darc_cid_compute(DARC_TYPE_FILE, 1, payload, off, fcid);
    if (!darc_repo_has_object(repo, fcid)) {
        darc_repo_put_object(repo, DARC_TYPE_FILE, DARC_CODEC_RAW, payload, off, payload, off, fcid);
        darc_index_put(idx, fcid, DARC_TYPE_FILE, off);
    }
    free(payload);
    memcpy(file_cid_out, fcid, 32);
    *logical_out = logical;
    *new_chunks_out = new_chunks;
    *stored_out = stored;
    return 0;
}

/* TREE entry serialization helpers */
typedef struct {
    char *name;
    uint8_t type; /* 1=file 2=dir 3=symlink */
    uint32_t mode;
    uint64_t mtime_ns;
    darc_cid_t target;
    char *link_target;
} tent_t;

typedef struct {
    tent_t *e;
    size_t n, cap;
} tbuild_t;

static void tb_init(tbuild_t *t) { memset(t, 0, sizeof(*t)); }
static void tb_free(tbuild_t *t) {
    for (size_t i = 0; i < t->n; ++i) {
        free(t->e[i].name);
        free(t->e[i].link_target);
    }
    free(t->e);
}
static int tb_add(tbuild_t *t, tent_t ent) {
    if (t->n >= t->cap) {
        size_t nc = t->cap ? t->cap * 2 : 16;
        tent_t *ne = realloc(t->e, nc * sizeof(*ne));
        if (!ne) return -1;
        t->e = ne; t->cap = nc;
    }
    t->e[t->n++] = ent;
    return 0;
}
static int tent_cmp(const void *a, const void *b) {
    return strcmp(((const tent_t*)a)->name, ((const tent_t*)b)->name);
}

static int build_tree_obj(darc_repo_t *repo, darc_index_t *idx, tbuild_t *tb,
                          darc_cid_t out_cid) {
    qsort(tb->e, tb->n, sizeof(tent_t), tent_cmp);
    size_t est = 2 + 8;
    for (size_t i = 0; i < tb->n; ++i) {
        est += 8 + strlen(tb->e[i].name) + 1 + 4 + 8;
        if (tb->e[i].type == 1 || tb->e[i].type == 2) est += 32;
        else if (tb->e[i].type == 3)
            est += 8 + (tb->e[i].link_target ? strlen(tb->e[i].link_target) : 0);
    }
    uint8_t *buf = malloc(est + 32);
    if (!buf) return -1;
    size_t off = 0;
    darc_write_u16_le(buf + off, 1); off += 2;
    darc_write_u64_le(buf + off, tb->n); off += 8;
    for (size_t i = 0; i < tb->n; ++i) {
        tent_t *e = &tb->e[i];
        size_t nlen = strlen(e->name);
        darc_write_u64_le(buf + off, nlen); off += 8;
        memcpy(buf + off, e->name, nlen); off += nlen;
        buf[off++] = e->type;
        darc_write_u32_le(buf + off, e->mode); off += 4;
        darc_write_u64_le(buf + off, e->mtime_ns); off += 8;
        if (e->type == 1 || e->type == 2) {
            memcpy(buf + off, e->target, 32); off += 32;
        } else if (e->type == 3) {
            size_t tlen = e->link_target ? strlen(e->link_target) : 0;
            darc_write_u64_le(buf + off, tlen); off += 8;
            if (tlen) { memcpy(buf + off, e->link_target, tlen); off += tlen; }
        }
    }
    darc_cid_compute(DARC_TYPE_TREE, 1, buf, off, out_cid);
    if (!darc_repo_has_object(repo, out_cid)) {
        darc_repo_put_object(repo, DARC_TYPE_TREE, DARC_CODEC_RAW, buf, off, buf, off, out_cid);
        darc_index_put(idx, out_cid, DARC_TYPE_TREE, off);
    }
    free(buf);
    return 0;
}

static int scan_dir_rec(darc_repo_t *repo, darc_index_t *idx, const char *path,
                        hl_map_t **hl,
                        uint64_t *files, uint64_t *dirs, uint64_t *symlinks,
                        uint64_t *hardlinks, uint64_t *logical,
                        uint64_t *new_chunks, uint64_t *stored,
                        darc_cid_t tree_out);

static int scan_dir_rec(darc_repo_t *repo, darc_index_t *idx, const char *path,
                        hl_map_t **hl,
                        uint64_t *files, uint64_t *dirs, uint64_t *symlinks,
                        uint64_t *hardlinks, uint64_t *logical,
                        uint64_t *new_chunks, uint64_t *stored,
                        darc_cid_t tree_out) {
    tbuild_t tb;
    tb_init(&tb);
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
        struct stat st;
        if (lstat(full, &st) < 0) continue;

        tent_t e;
        memset(&e, 0, sizeof(e));
        e.name = strdup(ent->d_name);
        e.mode = (uint32_t)(st.st_mode & 07777);
        e.mtime_ns = (uint64_t)st.st_mtim.tv_sec * 1000000000ULL + (uint64_t)st.st_mtim.tv_nsec;

        if (S_ISREG(st.st_mode)) {
            e.type = 1;
            hl_map_t *existing = hl_find(*hl, st.st_ino, st.st_dev);
            if (existing) {
                memcpy(e.target, existing->file_cid, 32);
                (*hardlinks)++;
                (*files)++;
            } else {
                uint64_t log = 0, nc = 0, stbytes = 0;
                if (process_file(repo, idx, full, &log, &nc, &stbytes, e.target) != 0) {
                    free(e.name); closedir(d); tb_free(&tb); return -1;
                }
                *logical += log; *new_chunks += nc; *stored += stbytes;
                (*files)++;
                hl_map_t *node = malloc(sizeof(*node));
                if (node) {
                    node->ino = st.st_ino; node->dev = st.st_dev;
                    memcpy(node->file_cid, e.target, 32);
                    node->next = *hl; *hl = node;
                }
            }
            if (tb_add(&tb, e) != 0) { free(e.name); closedir(d); tb_free(&tb); return -1; }
        } else if (S_ISDIR(st.st_mode)) {
            e.type = 2;
            if (scan_dir_rec(repo, idx, full, hl, files, dirs, symlinks, hardlinks,
                             logical, new_chunks, stored, e.target) != 0) {
                free(e.name); closedir(d); tb_free(&tb); return -1;
            }
            (*dirs)++;
            if (tb_add(&tb, e) != 0) { free(e.name); closedir(d); tb_free(&tb); return -1; }
        } else if (S_ISLNK(st.st_mode)) {
            e.type = 3;
            char target[PATH_MAX];
            ssize_t n = readlink(full, target, sizeof(target) - 1);
            if (n < 0) { free(e.name); continue; }
            target[n] = 0;
            e.link_target = strdup(target);
            (*symlinks)++;
            if (tb_add(&tb, e) != 0) { free(e.name); free(e.link_target); closedir(d); tb_free(&tb); return -1; }
        } else {
            free(e.name);
        }
    }
    closedir(d);
    int rc = build_tree_obj(repo, idx, &tb, tree_out);
    tb_free(&tb);
    return rc;
}

int darc_snapshot_create(darc_repo_t *repo, darc_index_t *idx,
                         const char **sources, size_t nsrc,
                         const char *name, const darc_cid_t *parent,
                         uint64_t timestamp_ns,
                         darc_cid_t out_cid) {
    if (darc_repo_lock(repo) != 0) return -1;
    darc_journal_begin(repo, "snapshot");
    darc_journal_recover(repo);

    uint64_t files = 0, dirs = 0, symlinks = 0, hardlinks = 0;
    uint64_t logical = 0, new_chunks = 0, stored = 0;
    hl_map_t *hl = NULL;
    tbuild_t root;
    tb_init(&root);

    for (size_t si = 0; si < nsrc; ++si) {
        struct stat st;
        if (lstat(sources[si], &st) < 0) {
            tb_free(&root); while (hl) { hl_map_t *n = hl->next; free(hl); hl = n; }
            darc_journal_abort(repo); darc_repo_unlock(repo); return -1;
        }
        const char *base = strrchr(sources[si], '/');
        base = base ? base + 1 : sources[si];
        tent_t e;
        memset(&e, 0, sizeof(e));
        e.name = strdup(base);
        e.mode = (uint32_t)(st.st_mode & 07777);
        e.mtime_ns = 0; /* deterministic when timestamp override used for snapshot */

        if (S_ISDIR(st.st_mode)) {
            e.type = 2;
            if (scan_dir_rec(repo, idx, sources[si], &hl, &files, &dirs, &symlinks,
                             &hardlinks, &logical, &new_chunks, &stored, e.target) != 0) {
                free(e.name); tb_free(&root);
                while (hl) { hl_map_t *n = hl->next; free(hl); hl = n; }
                darc_journal_abort(repo); darc_repo_unlock(repo); return -1;
            }
            dirs++;
        } else if (S_ISREG(st.st_mode)) {
            e.type = 1;
            uint64_t log = 0, nc = 0, stbytes = 0;
            if (process_file(repo, idx, sources[si], &log, &nc, &stbytes, e.target) != 0) {
                free(e.name); tb_free(&root);
                while (hl) { hl_map_t *n = hl->next; free(hl); hl = n; }
                darc_journal_abort(repo); darc_repo_unlock(repo); return -1;
            }
            logical += log; new_chunks += nc; stored += stbytes; files++;
        } else {
            free(e.name);
            continue;
        }
        if (tb_add(&root, e) != 0) {
            free(e.name); tb_free(&root);
            while (hl) { hl_map_t *n = hl->next; free(hl); hl = n; }
            darc_journal_abort(repo); darc_repo_unlock(repo); return -1;
        }
    }
    while (hl) { hl_map_t *n = hl->next; free(hl); hl = n; }

    darc_cid_t root_cid;
    if (build_tree_obj(repo, idx, &root, root_cid) != 0) {
        tb_free(&root);
        darc_journal_abort(repo); darc_repo_unlock(repo); return -1;
    }
    tb_free(&root);

    /* SNAPSHOT payload */
    size_t nlen = name ? strlen(name) : 0;
    size_t sp_est = 2 + 8 + 1 + 32 + 32 + 8 + nlen + 32 + 8 * 8;
    uint8_t *sp = malloc(sp_est + 64);
    if (!sp) { darc_journal_abort(repo); darc_repo_unlock(repo); return -1; }
    size_t off = 0;
    darc_write_u16_le(sp + off, 1); off += 2;
    darc_write_u64_le(sp + off, timestamp_ns); off += 8;
    if (parent) {
        sp[off++] = 1;
        memcpy(sp + off, parent, 32); off += 32;
    } else {
        sp[off++] = 0;
    }
    memcpy(sp + off, root_cid, 32); off += 32;
    darc_write_u64_le(sp + off, nlen); off += 8;
    if (nlen) { memcpy(sp + off, name, nlen); off += nlen; }
    uint8_t profile[32] = {0};
    memcpy(sp + off, profile, 32); off += 32;
    darc_write_u64_le(sp + off, files); off += 8;
    darc_write_u64_le(sp + off, dirs); off += 8;
    darc_write_u64_le(sp + off, symlinks); off += 8;
    darc_write_u64_le(sp + off, hardlinks); off += 8;
    darc_write_u64_le(sp + off, logical); off += 8;
    darc_write_u64_le(sp + off, new_chunks); off += 8;
    darc_write_u64_le(sp + off, stored); off += 8;
    darc_write_u64_le(sp + off, 0); off += 8;

    darc_cid_t scid;
    darc_cid_compute(DARC_TYPE_SNAPSHOT, 1, sp, off, scid);
    darc_repo_put_object(repo, DARC_TYPE_SNAPSHOT, DARC_CODEC_RAW, sp, off, sp, off, scid);
    darc_index_put(idx, scid, DARC_TYPE_SNAPSHOT, off);
    free(sp);

    darc_repo_write_snapshot_ref(repo, scid);
    darc_repo_set_head(repo, scid);
    darc_index_save(idx, repo);
    darc_journal_commit(repo);
    darc_repo_unlock(repo);
    memcpy(out_cid, scid, 32);
    return 0;
}

int darc_snapshot_load_info(darc_repo_t *repo, const darc_cid_t cid, darc_snapshot_info_t *info) {
    uint8_t type;
    uint8_t *payload = NULL;
    size_t plen = 0;
    if (darc_repo_get_object(repo, cid, &type, &payload, &plen) != 0 || type != DARC_TYPE_SNAPSHOT)
        return -1;
    memset(info, 0, sizeof(*info));
    memcpy(info->cid, cid, 32);
    size_t off = 2;
    if (plen < 11) { free(payload); return -1; }
    info->created_ns = darc_read_u64_le(payload + off); off += 8;
    if (payload[off++]) {
        info->has_parent = 1;
        if (off + 32 > plen) { free(payload); return -1; }
        memcpy(info->parent, payload + off, 32); off += 32;
    }
    if (off + 32 > plen) { free(payload); return -1; }
    memcpy(info->root_tree, payload + off, 32); off += 32;
    if (off + 8 > plen) { free(payload); return -1; }
    uint64_t nl = darc_read_u64_le(payload + off); off += 8;
    if (nl && off + nl <= plen) {
        size_t c = nl < 255 ? (size_t)nl : 255;
        memcpy(info->name, payload + off, c);
        info->name[c] = 0;
        off += (size_t)nl;
    }
    if (off + 32 <= plen) { memcpy(info->profile_hash, payload + off, 32); off += 32; }
    if (off + 64 <= plen) {
        info->file_count = darc_read_u64_le(payload + off); off += 8;
        info->dir_count = darc_read_u64_le(payload + off); off += 8;
        info->symlink_count = darc_read_u64_le(payload + off); off += 8;
        info->hardlink_count = darc_read_u64_le(payload + off); off += 8;
        info->logical_bytes = darc_read_u64_le(payload + off); off += 8;
        info->new_chunks = darc_read_u64_le(payload + off); off += 8;
        info->stored_bytes = darc_read_u64_le(payload + off); off += 8;
    }
    free(payload);
    return 0;
}

int darc_snapshot_list(darc_repo_t *repo, darc_snapshot_info_t **out, size_t *count) {
    darc_cid_t *refs = NULL;
    size_t n = 0;
    if (darc_repo_list_snapshot_refs(repo, &refs, &n) != 0) return -1;
    darc_snapshot_info_t *arr = calloc(n ? n : 1, sizeof(*arr));
    if (!arr) { free(refs); return -1; }
    size_t valid = 0;
    for (size_t i = 0; i < n; ++i) {
        if (darc_snapshot_load_info(repo, refs[i], &arr[valid]) == 0)
            valid++;
    }
    free(refs);
    for (size_t i = 0; i < valid; ++i)
        for (size_t j = i + 1; j < valid; ++j)
            if (arr[j].created_ns > arr[i].created_ns) {
                darc_snapshot_info_t tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
            }
    *out = arr;
    *count = valid;
    return 0;
}
