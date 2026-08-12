#define _POSIX_C_SOURCE 200809L
#include "darc_index.h"
#include "darc_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

/* Robin Hood open addressing, hash = first 8 bytes of CID as little-endian u64 */
static uint64_t cid_hash(const darc_cid_t cid) {
    return darc_read_u64_le(cid);
}

static size_t probe_dist(size_t ideal, size_t slot, size_t cap) {
    return (slot + cap - ideal) % cap;
}

darc_index_t *darc_index_create(size_t initial_cap) {
    if (initial_cap < 16) initial_cap = 16;
    /* power of two */
    size_t c = 16;
    while (c < initial_cap) c <<= 1;
    darc_index_t *idx = calloc(1, sizeof(*idx));
    if (!idx) return NULL;
    idx->slots = calloc(c, sizeof(darc_index_entry_t));
    if (!idx->slots) { free(idx); return NULL; }
    idx->capacity = c;
    idx->count = 0;
    return idx;
}

void darc_index_free(darc_index_t *idx) {
    if (!idx) return;
    free(idx->slots);
    free(idx);
}

static int grow(darc_index_t *idx) {
    size_t nc = idx->capacity * 2;
    darc_index_entry_t *ns = calloc(nc, sizeof(darc_index_entry_t));
    if (!ns) return -1;
    darc_index_entry_t *old = idx->slots;
    size_t oc = idx->capacity;
    idx->slots = ns;
    idx->capacity = nc;
    idx->count = 0;
    for (size_t i = 0; i < oc; ++i) {
        if (old[i].used)
            darc_index_put(idx, old[i].cid, old[i].type, old[i].size);
    }
    free(old);
    return 0;
}

int darc_index_put(darc_index_t *idx, const darc_cid_t cid, uint8_t type, uint64_t size) {
    if (idx->count * 10 >= idx->capacity * 9) {
        if (grow(idx) < 0) return -1;
    }
    uint64_t h = cid_hash(cid);
    size_t ideal = h % idx->capacity;
    size_t slot = ideal;
    size_t dist = 0;
    darc_index_entry_t entry;
    memcpy(entry.cid, cid, 32);
    entry.size = size;
    entry.type = type;
    entry.used = 1;

    while (1) {
        if (!idx->slots[slot].used) {
            idx->slots[slot] = entry;
            idx->count++;
            return 0;
        }
        if (memcmp(idx->slots[slot].cid, cid, 32) == 0) {
            idx->slots[slot].size = size;
            idx->slots[slot].type = type;
            return 0; /* update */
        }
        size_t existing_ideal = cid_hash(idx->slots[slot].cid) % idx->capacity;
        size_t existing_dist = probe_dist(existing_ideal, slot, idx->capacity);
        if (existing_dist < dist) {
            /* Robin Hood: swap */
            darc_index_entry_t tmp = idx->slots[slot];
            idx->slots[slot] = entry;
            entry = tmp;
            ideal = existing_ideal;
            dist = existing_dist;
        }
        slot = (slot + 1) % idx->capacity;
        dist++;
        if (dist > idx->capacity) return -1; /* full */
    }
}

int darc_index_get(const darc_index_t *idx, const darc_cid_t cid, darc_index_entry_t *out) {
    uint64_t h = cid_hash(cid);
    size_t ideal = h % idx->capacity;
    size_t slot = ideal;
    size_t dist = 0;
    while (idx->slots[slot].used) {
        if (memcmp(idx->slots[slot].cid, cid, 32) == 0) {
            if (out) *out = idx->slots[slot];
            return 0;
        }
        if (probe_dist(cid_hash(idx->slots[slot].cid) % idx->capacity, slot, idx->capacity) < dist)
            break;
        slot = (slot + 1) % idx->capacity;
        dist++;
        if (dist > idx->capacity) break;
    }
    return -1;
}

int darc_index_save(const darc_index_t *idx, const darc_repo_t *repo) {
    /* Simple binary format: magic + count + entries */
    size_t sz = 8 + 8 + idx->count * (32 + 8 + 1);
    uint8_t *buf = malloc(sz);
    if (!buf) return -1;
    memcpy(buf, "DARCIDX1", 8);
    darc_write_u64_le(buf + 8, idx->count);
    size_t off = 16;
    for (size_t i = 0; i < idx->capacity; ++i) {
        if (!idx->slots[i].used) continue;
        memcpy(buf + off, idx->slots[i].cid, 32); off += 32;
        darc_write_u64_le(buf + off, idx->slots[i].size); off += 8;
        buf[off++] = idx->slots[i].type;
    }
    int rc = darc_repo_atomic_write(repo, "index/chunks.idx", buf, off);
    free(buf);
    return rc;
}

darc_index_t *darc_index_load(const darc_repo_t *repo) {
    char path[4096];
    if (!repo || !repo->path) return darc_index_create(64);
    snprintf(path, sizeof(path), "%s/index/chunks.idx", repo->path);
    size_t len;
    uint8_t *data = darc_read_file(path, &len);
    if (!data || len < 16 || memcmp(data, "DARCIDX1", 8) != 0) {
        free(data);
        return darc_index_create(64);
    }
    uint64_t count = darc_read_u64_le(data + 8);
    darc_index_t *idx = darc_index_create((size_t)count * 2 + 16);
    if (!idx) { free(data); return NULL; }
    size_t off = 16;
    for (uint64_t i = 0; i < count; ++i) {
        if (off + 41 > len) break;
        darc_cid_t cid;
        memcpy(cid, data + off, 32); off += 32;
        uint64_t size = darc_read_u64_le(data + off); off += 8;
        uint8_t type = data[off++];
        darc_index_put(idx, cid, type, size);
    }
    free(data);
    return idx;
}

darc_index_t *darc_index_rebuild(const darc_repo_t *repo) {
    darc_index_t *idx = darc_index_create(256);
    if (!idx) return NULL;
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/objects/sha256", repo->path);
    DIR *d1 = opendir(dir);
    if (!d1) return idx;
    struct dirent *e1;
    while ((e1 = readdir(d1)) != NULL) {
        if (strlen(e1->d_name) != 2) continue;
        char sub[4096];
        snprintf(sub, sizeof(sub), "%s/%s", dir, e1->d_name);
        DIR *d2 = opendir(sub);
        if (!d2) continue;
        struct dirent *e2;
        while ((e2 = readdir(d2)) != NULL) {
            if (strlen(e2->d_name) != 62) continue;
            char hex[65];
            snprintf(hex, sizeof(hex), "%s%s", e1->d_name, e2->d_name);
            darc_cid_t cid;
            if (darc_cid_from_hex(hex, cid) != 0) continue;
            uint8_t type;
            uint8_t *payload = NULL;
            size_t plen = 0;
            if (darc_repo_get_object(repo, cid, &type, &payload, &plen) == 0) {
                darc_index_put(idx, cid, type, plen);
                free(payload);
            }
        }
        closedir(d2);
    }
    closedir(d1);
    darc_index_save(idx, repo);
    return idx;
}
