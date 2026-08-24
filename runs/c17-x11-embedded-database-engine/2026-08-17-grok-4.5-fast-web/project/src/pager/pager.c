#define _POSIX_C_SOURCE 200809L
#include "edb/pager.h"
#include "edb/byteorder.h"
#include "edb/xchacha20_poly1305.h"
#include "edb/freelist.h"
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define EDB_DEFAULT_CACHE 64
#define EDB_SALT_LEN 16
#define EDB_NONCE_LEN 24
#define EDB_TAG_LEN 16

/* On-disk header layout (little-endian), first 256 bytes of page 0.
 * FMT-H-01 .. FMT-H-15
 */
typedef struct edb_file_header {
    uint8_t  magic[4];
    uint16_t format_major;
    uint16_t format_minor;
    uint32_t page_size;
    uint64_t database_id;
    uint32_t feature_flags;
    uint16_t kdf_id;
    uint32_t kdf_iterations;
    uint8_t  salt[EDB_SALT_LEN];
    uint64_t header_generation;
    uint32_t root_schema_page;
    uint32_t freelist_root;
    uint32_t last_page_number;
    uint64_t checkpoint_sequence;
    uint8_t  header_tag[EDB_TAG_LEN];   /* Poly1305 over authenticated header fields */
    /* remaining bytes reserved / zero */
} edb_file_header;

struct edb_pager {
    int fd;
    bool read_only;
    bool encrypted;
    bool opened_ok;            /* true only after successful open/create */
    uint8_t key[32];
    edb_file_header hdr;
    edb_page **cache;          /* simple array of pointers */
    size_t cache_cap;
    size_t cache_count;
    edb_page *lru_head;
    edb_page *lru_tail;
    edb_pager_stats stats;
    char *path;
    struct edb_freelist *freelist;
};

static void lru_touch(edb_pager *p, edb_page *pg) {
    if (p->lru_head == pg) return;
    /* detach */
    if (pg->prev) pg->prev->next = pg->next;
    if (pg->next) pg->next->prev = pg->prev;
    if (p->lru_tail == pg) p->lru_tail = pg->prev;
    /* push front */
    pg->prev = NULL;
    pg->next = p->lru_head;
    if (p->lru_head) p->lru_head->prev = pg;
    p->lru_head = pg;
    if (!p->lru_tail) p->lru_tail = pg;
}

static void lru_remove(edb_pager *p, edb_page *pg) {
    if (pg->prev) pg->prev->next = pg->next;
    if (pg->next) pg->next->prev = pg->prev;
    if (p->lru_head == pg) p->lru_head = pg->next;
    if (p->lru_tail == pg) p->lru_tail = pg->prev;
    pg->prev = pg->next = NULL;
}

static edb_page *cache_find(edb_pager *p, uint32_t page_no) {
    for (size_t i = 0; i < p->cache_count; i++) {
        if (p->cache[i] && p->cache[i]->page_no == page_no)
            return p->cache[i];
    }
    return NULL;
}

static int cache_insert(edb_pager *p, edb_page *pg) {
    if (p->cache_count >= p->cache_cap) {
        /* evict LRU unpinned */
        edb_page *victim = p->lru_tail;
        while (victim && (victim->pinned || victim->refcount > 0))
            victim = victim->prev;
        if (!victim) return -1; /* all pinned */
        if (victim->dirty) {
            edb_error err;
            if (edb_pager_write_page(p, victim, &err) != 0) return -1;
        }
        lru_remove(p, victim);
        /* remove from array */
        for (size_t i = 0; i < p->cache_count; i++) {
            if (p->cache[i] == victim) {
                p->cache[i] = p->cache[p->cache_count-1];
                p->cache[p->cache_count-1] = NULL;
                p->cache_count--;
                break;
            }
        }
        free(victim);
        p->stats.evictions++;
    }
    p->cache[p->cache_count++] = pg;
    lru_touch(p, pg);
    return 0;
}

static void encode_header(const edb_file_header *h, uint8_t *buf) {
    memset(buf, 0, EDB_HEADER_SIZE);
    buf[0] = h->magic[0]; buf[1] = h->magic[1];
    buf[2] = h->magic[2]; buf[3] = h->magic[3];
    edb_store_u16_le(buf+4, h->format_major);
    edb_store_u16_le(buf+6, h->format_minor);
    edb_store_u32_le(buf+8, h->page_size);
    edb_store_u64_le(buf+12, h->database_id);
    edb_store_u32_le(buf+20, h->feature_flags);
    edb_store_u16_le(buf+24, h->kdf_id);
    edb_store_u32_le(buf+26, h->kdf_iterations);
    memcpy(buf+30, h->salt, EDB_SALT_LEN);
    edb_store_u64_le(buf+46, h->header_generation);
    edb_store_u32_le(buf+54, h->root_schema_page);
    edb_store_u32_le(buf+58, h->freelist_root);
    edb_store_u32_le(buf+62, h->last_page_number);
    edb_store_u64_le(buf+66, h->checkpoint_sequence);
    memcpy(buf+74, h->header_tag, EDB_TAG_LEN);
}

static int decode_header(const uint8_t *buf, edb_file_header *h) {
    if (buf[0]!='E' || buf[1]!='D' || buf[2]!='B' || buf[3]!='1') return -1;
    h->magic[0]=buf[0]; h->magic[1]=buf[1]; h->magic[2]=buf[2]; h->magic[3]=buf[3];
    h->format_major = edb_load_u16_le(buf+4);
    h->format_minor = edb_load_u16_le(buf+6);
    h->page_size    = edb_load_u32_le(buf+8);
    h->database_id  = edb_load_u64_le(buf+12);
    h->feature_flags= edb_load_u32_le(buf+20);
    h->kdf_id       = edb_load_u16_le(buf+24);
    h->kdf_iterations = edb_load_u32_le(buf+26);
    memcpy(h->salt, buf+30, EDB_SALT_LEN);
    h->header_generation = edb_load_u64_le(buf+46);
    h->root_schema_page  = edb_load_u32_le(buf+54);
    h->freelist_root     = edb_load_u32_le(buf+58);
    h->last_page_number  = edb_load_u32_le(buf+62);
    h->checkpoint_sequence = edb_load_u64_le(buf+66);
    memcpy(h->header_tag, buf+74, EDB_TAG_LEN);
    if (h->page_size != EDB_PAGE_SIZE) return -1;
    if (h->format_major != EDB_FORMAT_MAJOR) return -1;
    return 0;
}

static int read_raw_page(edb_pager *p, uint32_t page_no, uint8_t *buf, edb_error *err) {
    off_t off = (off_t)page_no * EDB_PAGE_SIZE;
    if (lseek(p->fd, off, SEEK_SET) != off) {
        edb_error_set(err, EDB_IO, errno, "lseek failed");
        return -1;
    }
    ssize_t n = read(p->fd, buf, EDB_PAGE_SIZE);
    if (n != (ssize_t)EDB_PAGE_SIZE) {
        edb_error_set(err, EDB_IO, errno, "short read or I/O error");
        return -1;
    }
    return 0;
}

static int write_raw_page(edb_pager *p, uint32_t page_no, const uint8_t *buf, edb_error *err) {
    off_t off = (off_t)page_no * EDB_PAGE_SIZE;
    if (lseek(p->fd, off, SEEK_SET) != off) {
        edb_error_set(err, EDB_IO, errno, "lseek failed");
        return -1;
    }
    ssize_t n = write(p->fd, buf, EDB_PAGE_SIZE);
    if (n != (ssize_t)EDB_PAGE_SIZE) {
        edb_error_set(err, EDB_IO, errno, "short write or I/O error");
        return -1;
    }
    return 0;
}

/* For encrypted pages: nonce = page_no || generation || random-ish derived bits
 * We store tag at end of page for simplicity in v1; logical payload is reduced.
 * Full envelope design will be refined in format docs.
 */
static int decrypt_page(edb_pager *p, uint32_t page_no, uint8_t *buf, edb_error *err) {
    if (!p->encrypted) return 0;
    /* nonce: 8 bytes page_no + 8 bytes gen + 8 zeros (simplified unique construction) */
    uint8_t nonce[24] = {0};
    edb_store_u32_le(nonce, page_no);
    /* generation lives in page header after type; for now use zero + page_no uniqueness */
    edb_store_u32_le(nonce+4, page_no ^ 0xA5A5A5A5u);

    uint8_t tag[16];
    memcpy(tag, buf + EDB_PAGE_SIZE - 16, 16);
    uint8_t pt[EDB_PAGE_SIZE - 16];
    bool ok = edb_xchacha20_poly1305_decrypt(p->key, nonce, NULL, 0,
                buf, EDB_PAGE_SIZE - 16, tag, pt);
    if (!ok) {
        edb_error_set(err, EDB_AUTHENTICATION, 0, "page authentication failed");
        return -1;
    }
    memcpy(buf, pt, EDB_PAGE_SIZE - 16);
    memset(buf + EDB_PAGE_SIZE - 16, 0, 16);
    return 0;
}

static int encrypt_page(edb_pager *p, uint32_t page_no, uint8_t *buf, edb_error *err) {
    if (!p->encrypted) return 0;
    uint8_t nonce[24] = {0};
    edb_store_u32_le(nonce, page_no);
    edb_store_u32_le(nonce+4, page_no ^ 0xA5A5A5A5u);

    uint8_t ct[EDB_PAGE_SIZE - 16];
    uint8_t tag[16];
    edb_xchacha20_poly1305_encrypt(p->key, nonce, NULL, 0,
        buf, EDB_PAGE_SIZE - 16, ct, tag);
    memcpy(buf, ct, EDB_PAGE_SIZE - 16);
    memcpy(buf + EDB_PAGE_SIZE - 16, tag, 16);
    (void)err;
    return 0;
}

edb_pager *edb_pager_open(const char *path, bool create, bool read_only,
                          const uint8_t *key32_or_null, edb_error *err) {
    edb_error_clear(err);
    int flags = read_only ? O_RDONLY : O_RDWR;
    if (create) flags |= O_CREAT | O_EXCL;
    int fd = open(path, flags, 0600);
    if (fd < 0) {
        edb_error_set(err, EDB_IO, errno, "open failed");
        return NULL;
    }

    edb_pager *p = calloc(1, sizeof(*p));
    if (!p) {
        close(fd);
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }
    p->fd = fd;
    p->read_only = read_only;
    /* Exclusive lock for writers; shared for readers (second-writer exclusion) */
    if (flock(fd, read_only ? LOCK_SH | LOCK_NB : LOCK_EX | LOCK_NB) != 0) {
        edb_error_set(err, EDB_IO, errno, "database locked by another process");
        free(p->cache); free(p->path); free(p); close(fd);
        return NULL;
    }

    p->path = strdup(path);
    p->cache_cap = EDB_DEFAULT_CACHE;
    p->cache = calloc(p->cache_cap, sizeof(edb_page*));
    if (!p->cache || !p->path) {
        free(p->cache); free(p->path); free(p); close(fd);
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }

    if (key32_or_null) {
        p->encrypted = true;
        memcpy(p->key, key32_or_null, 32);
    }

    if (create) {
        /* initialise header */
        memset(&p->hdr, 0, sizeof p->hdr);
        p->hdr.magic[0]='E'; p->hdr.magic[1]='D'; p->hdr.magic[2]='B'; p->hdr.magic[3]='1';
        p->hdr.format_major = EDB_FORMAT_MAJOR;
        p->hdr.format_minor = EDB_FORMAT_MINOR;
        p->hdr.page_size = EDB_PAGE_SIZE;
        /* simple db id from time + pid (test builds will inject) */
        p->hdr.database_id = ((uint64_t)getpid() << 32) ^ (uint64_t)(uintptr_t)p;
        p->hdr.feature_flags = p->encrypted ? 1u : 0u;
        p->hdr.kdf_id = 1; /* PBKDF2-HMAC-SHA256 */
        p->hdr.kdf_iterations = 100000;
        /* salt left zero for now; real create will fill from CSPRNG */
        p->hdr.header_generation = 1;
        p->hdr.root_schema_page = 0;
        p->hdr.freelist_root = 0;
        p->hdr.last_page_number = 0;
        p->hdr.checkpoint_sequence = 0;

        uint8_t page0[EDB_PAGE_SIZE];
        memset(page0, 0, sizeof page0);
        encode_header(&p->hdr, page0);
        /* page type marker */
        page0[EDB_HEADER_SIZE] = (uint8_t)EDB_PAGE_META;

        if (p->encrypted) {
            if (encrypt_page(p, 0, page0, err) != 0) {
                edb_pager_close(p);
                return NULL;
            }
        }
        if (write_raw_page(p, 0, page0, err) != 0) {
            edb_pager_close(p);
            return NULL;
        }
        fsync(p->fd);
    } else {
        uint8_t page0[EDB_PAGE_SIZE];
        if (read_raw_page(p, 0, page0, err) != 0) {
            edb_pager_close(p);
            return NULL;
        }
        if (p->encrypted) {
            if (decrypt_page(p, 0, page0, err) != 0) {
                edb_pager_close(p);
                return NULL;
            }
        }
        if (decode_header(page0, &p->hdr) != 0) {
            edb_error_set(err, EDB_UNSUPPORTED_FORMAT, 0, "bad magic or version");
            edb_pager_close(p);
            return NULL;
        }
        if ((p->hdr.feature_flags & 1u) && !p->encrypted) {
            edb_error_set(err, EDB_AUTHENTICATION, 0, "encrypted database requires key");
            edb_pager_close(p);
            return NULL;
        }
    }
    p->freelist = edb_freelist_create(p);
    if (p->freelist && p->hdr.freelist_root) {
        edb_error ferr;
        edb_freelist_load(p->freelist, p->hdr.freelist_root, &ferr);
    }
    p->opened_ok = true;
    return p;
}

static int flush_header(edb_pager *p, edb_error *err) {
    uint8_t page0[EDB_PAGE_SIZE];
    memset(page0, 0, sizeof page0);
    encode_header(&p->hdr, page0);
    page0[EDB_HEADER_SIZE] = (uint8_t)EDB_PAGE_META;
    if (p->encrypted) {
        if (encrypt_page(p, 0, page0, err) != 0) return -1;
    }
    return write_raw_page(p, 0, page0, err);
}

void edb_pager_close(edb_pager *p) {
    if (!p) return;
    edb_error err;
    for (size_t i = 0; i < p->cache_count; i++) {
        edb_page *pg = p->cache[i];
        if (pg && pg->dirty && !p->read_only) {
            edb_pager_write_page(p, pg, &err);
        }
        free(pg);
    }
    if (p->freelist && p->opened_ok && !p->read_only) {
        uint32_t fr = 0;
        if (edb_freelist_flush(p->freelist, &fr, &err) == 0)
            p->hdr.freelist_root = fr;
    }
    if (p->opened_ok && !p->read_only) {
        flush_header(p, &err);
        fsync(p->fd);
    }
    free(p->cache);
    if (p->fd >= 0) close(p->fd);
    free(p->path);
    edb_secure_zero(p->key, 32);
    edb_freelist_destroy(p->freelist);
    free(p);
}

edb_page *edb_pager_get(edb_pager *p, uint32_t page_no, edb_error *err) {
    edb_error_clear(err);
    if (page_no > p->hdr.last_page_number) {
        edb_error_set(err, EDB_CORRUPTION, 0, "page number out of range");
        return NULL;
    }
    edb_page *pg = cache_find(p, page_no);
    if (pg) {
        p->stats.hits++;
        pg->refcount++;
        pg->pinned = true;
        lru_touch(p, pg);
        return pg;
    }
    p->stats.misses++;
    pg = calloc(1, sizeof(*pg));
    if (!pg) {
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }
    pg->page_no = page_no;
    if (read_raw_page(p, page_no, pg->data, err) != 0) {
        free(pg);
        return NULL;
    }
    if (decrypt_page(p, page_no, pg->data, err) != 0) {
        free(pg);
        return NULL;
    }
    pg->type = (edb_page_type)pg->data[EDB_HEADER_SIZE]; /* simplistic */
    pg->refcount = 1;
    pg->pinned = true;
    if (cache_insert(p, pg) != 0) {
        free(pg);
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "cache full, all pinned");
        return NULL;
    }
    if (p->stats.pinned_peak < (uint64_t)(p->cache_count)) /* rough */
        p->stats.pinned_peak = p->cache_count;
    return pg;
}

edb_page *edb_pager_new(edb_pager *p, edb_page_type type, edb_error *err) {
    edb_error_clear(err);
    if (p->read_only) {
        edb_error_set(err, EDB_PERMISSION, 0, "read-only");
        return NULL;
    }
    uint32_t new_no = 0;
    if (p->freelist) new_no = edb_freelist_pop(p->freelist);
    if (new_no == 0)
        new_no = p->hdr.last_page_number + 1;
    edb_page *pg = calloc(1, sizeof(*pg));
    if (!pg) {
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }
    pg->page_no = new_no;
    pg->type = type;
    pg->data[EDB_HEADER_SIZE] = (uint8_t)type;
    pg->dirty = true;
    pg->refcount = 1;
    pg->pinned = true;
    if (new_no > p->hdr.last_page_number)
        p->hdr.last_page_number = new_no;
    if (cache_insert(p, pg) != 0) {
        free(pg);
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "cache full");
        return NULL;
    }
    return pg;
}

void edb_pager_unpin(edb_pager *p, edb_page *page) {
    if (!page) return;
    if (page->refcount > 0) page->refcount--;
    if (page->refcount == 0) page->pinned = false;
    (void)p;
}

void edb_pager_mark_dirty(edb_pager *p, edb_page *page) {
    if (page) page->dirty = true;
    (void)p;
}

int edb_pager_write_page(edb_pager *p, edb_page *page, edb_error *err) {
    edb_error_clear(err);
    if (p->read_only) {
        edb_error_set(err, EDB_PERMISSION, 0, "read-only");
        return -1;
    }
    uint8_t buf[EDB_PAGE_SIZE];
    memcpy(buf, page->data, EDB_PAGE_SIZE);
    if (encrypt_page(p, page->page_no, buf, err) != 0) return -1;
    if (write_raw_page(p, page->page_no, buf, err) != 0) return -1;
    page->dirty = false;
    p->stats.dirty_writes++;
    return 0;
}

int edb_pager_sync(edb_pager *p, edb_error *err) {
    edb_error_clear(err);
    if (fsync(p->fd) != 0) {
        edb_error_set(err, EDB_IO, errno, "fsync failed");
        return -1;
    }
    return 0;
}

void edb_pager_set_cache_pages(edb_pager *p, size_t n) {
    if (n < 4) n = 4;
    /* simple: only allow increase for now */
    if (n > p->cache_cap) {
        edb_page **nc = realloc(p->cache, n * sizeof(edb_page*));
        if (nc) {
            for (size_t i = p->cache_cap; i < n; i++) nc[i] = NULL;
            p->cache = nc;
            p->cache_cap = n;
        }
    }
}

const edb_pager_stats *edb_pager_get_stats(const edb_pager *p) {
    return &p->stats;
}

uint32_t edb_pager_last_page(const edb_pager *p) {
    return p->hdr.last_page_number;
}

uint64_t edb_pager_db_id(const edb_pager *p) {
    return p->hdr.database_id;
}

bool edb_pager_is_encrypted(const edb_pager *p) {
    return p->encrypted;
}

int edb_pager_free_page(edb_pager *p, uint32_t page_no) {
    if (!p || !p->freelist || page_no == 0) return -1;
    return edb_freelist_push(p->freelist, page_no);
}



int edb_pager_overwrite(edb_pager *p, uint32_t page_no, const uint8_t *data, edb_error *err) {
    edb_error_clear(err);
    if (!p || !data || page_no == 0) {
        edb_error_set(err, EDB_IO, 0, "bad overwrite args");
        return -1;
    }
    for (size_t i = 0; i < p->cache_count; i++) {
        if (p->cache[i] && p->cache[i]->page_no == page_no) {
            free(p->cache[i]); /* data is embedded in edb_page */
            p->cache[i] = p->cache[p->cache_count - 1];
            p->cache[p->cache_count - 1] = NULL;
            p->cache_count--;
            break;
        }
    }
    uint8_t *buf = malloc(EDB_PAGE_SIZE);
    if (!buf) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
    memcpy(buf, data, EDB_PAGE_SIZE);
    if (encrypt_page(p, page_no, buf, err) != 0) { free(buf); return -1; }
    off_t off = (off_t)page_no * EDB_PAGE_SIZE;
    if (pwrite(p->fd, buf, EDB_PAGE_SIZE, off) != (ssize_t)EDB_PAGE_SIZE) {
        free(buf);
        edb_error_set(err, EDB_IO, errno, "overwrite pwrite failed");
        return -1;
    }
    free(buf);
    if (page_no > p->hdr.last_page_number)
        p->hdr.last_page_number = page_no;
    return 0;
}
