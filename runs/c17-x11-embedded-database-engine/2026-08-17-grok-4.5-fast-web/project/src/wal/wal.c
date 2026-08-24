#define _POSIX_C_SOURCE 200809L
#include "edb/wal.h"
#include "edb/byteorder.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WAL_MAGIC "EDBW"
#define WAL_HDR_SIZE 32

struct edb_wal {
    int fd;
    char *path;
    uint64_t next_seq;
    uint64_t last_commit_seq;
};

static uint32_t simple_checksum(const uint8_t *p, size_t n) {
    uint32_t c = 0x811c9dc5u;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        c *= 16777619u;
    }
    return c;
}

edb_wal *edb_wal_open(const char *db_path, bool create, edb_error *err) {
    size_t n = strlen(db_path) + 5;
    char *wpath = malloc(n);
    if (!wpath) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return NULL; }
    snprintf(wpath, n, "%s-wal", db_path);

    int flags = create ? (O_RDWR | O_CREAT) : O_RDWR;
    int fd = open(wpath, flags, 0600);
    if (fd < 0) {
        free(wpath);
        edb_error_set(err, EDB_IO, errno, "wal open failed");
        return NULL;
    }
    edb_wal *w = calloc(1, sizeof(*w));
    if (!w) { close(fd); free(wpath); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return NULL; }
    w->fd = fd;
    w->path = wpath;
    w->next_seq = 1;

    off_t sz = lseek(fd, 0, SEEK_END);
    if (sz < WAL_HDR_SIZE && create) {
        uint8_t hdr[WAL_HDR_SIZE];
        memset(hdr, 0, sizeof hdr);
        memcpy(hdr, WAL_MAGIC, 4);
        edb_store_u64_le(hdr + 4, 1); /* next seq */
        write(fd, hdr, WAL_HDR_SIZE);
        fsync(fd);
    } else if (sz >= WAL_HDR_SIZE) {
        uint8_t hdr[WAL_HDR_SIZE];
        lseek(fd, 0, SEEK_SET);
        if (read(fd, hdr, WAL_HDR_SIZE) == WAL_HDR_SIZE && memcmp(hdr, WAL_MAGIC, 4) == 0) {
            w->next_seq = edb_load_u64_le(hdr + 4);
            if (w->next_seq == 0) w->next_seq = 1;
        }
    }
    return w;
}

void edb_wal_close(edb_wal *w) {
    if (!w) return;
    if (w->fd >= 0) {
        /* update header next_seq */
        uint8_t hdr[WAL_HDR_SIZE];
        memset(hdr, 0, sizeof hdr);
        memcpy(hdr, WAL_MAGIC, 4);
        edb_store_u64_le(hdr + 4, w->next_seq);
        lseek(w->fd, 0, SEEK_SET);
        write(w->fd, hdr, WAL_HDR_SIZE);
        fsync(w->fd);
        close(w->fd);
    }
    free(w->path);
    free(w);
}

uint64_t edb_wal_next_seq(edb_wal *w) { return w->next_seq; }

static int append_record(edb_wal *w, edb_wal_rec_type type, uint64_t txn_id,
                         const uint8_t *payload, uint32_t plen, edb_error *err) {
    /* record: type u8 | flags u8 | seq u64 | txn u64 | plen u32 | payload | checksum u32 */
    uint8_t hdr[1+1+8+8+4];
    hdr[0] = (uint8_t)type;
    hdr[1] = 0;
    edb_store_u64_le(hdr + 2, w->next_seq);
    edb_store_u64_le(hdr + 10, txn_id);
    edb_store_u32_le(hdr + 18, plen);

    uint32_t csum = simple_checksum(hdr, sizeof hdr);
    if (payload && plen)
        csum ^= simple_checksum(payload, plen);

    off_t end = lseek(w->fd, 0, SEEK_END);
    if (end < 0) { edb_error_set(err, EDB_IO, errno, "wal seek"); return -1; }
    if (write(w->fd, hdr, sizeof hdr) != (ssize_t)sizeof hdr) {
        edb_error_set(err, EDB_IO, errno, "wal write hdr"); return -1;
    }
    if (plen && write(w->fd, payload, plen) != (ssize_t)plen) {
        edb_error_set(err, EDB_IO, errno, "wal write payload"); return -1;
    }
    uint8_t cs[4];
    edb_store_u32_le(cs, csum);
    if (write(w->fd, cs, 4) != 4) {
        edb_error_set(err, EDB_IO, errno, "wal write csum"); return -1;
    }
    w->next_seq++;
    return 0;
}

int edb_wal_begin(edb_wal *w, uint64_t txn_id, edb_error *err) {
    return append_record(w, EDB_WAL_BEGIN, txn_id, NULL, 0, err);
}

int edb_wal_log_page(edb_wal *w, uint32_t page_no, const uint8_t *page_data, edb_error *err) {
    uint8_t buf[4 + EDB_PAGE_SIZE];
    edb_store_u32_le(buf, page_no);
    memcpy(buf + 4, page_data, EDB_PAGE_SIZE);
    return append_record(w, EDB_WAL_PAGE_IMAGE, 0, buf, 4 + EDB_PAGE_SIZE, err);
}

int edb_wal_commit(edb_wal *w, uint64_t txn_id, edb_error *err) {
    int rc = append_record(w, EDB_WAL_COMMIT, txn_id, NULL, 0, err);
    if (rc == 0) {
        fsync(w->fd);
        w->last_commit_seq = w->next_seq - 1;
    }
    return rc;
}

int edb_wal_abort(edb_wal *w, uint64_t txn_id, edb_error *err) {
    return append_record(w, EDB_WAL_ABORT, txn_id, NULL, 0, err);
}

int edb_wal_checkpoint(edb_wal *w, edb_pager *pager, edb_error *err) {
    /* simplified: truncate WAL after syncing pager */
    if (edb_pager_sync(pager, err) != 0) return -1;
    int rc = append_record(w, EDB_WAL_CHECKPOINT, 0, NULL, 0, err);
    if (rc != 0) return rc;
    fsync(w->fd);
    /* reset file after header */
    if (ftruncate(w->fd, WAL_HDR_SIZE) != 0) {
        edb_error_set(err, EDB_IO, errno, "wal truncate");
        return -1;
    }
    w->next_seq = 1;
    return 0;
}

int edb_wal_recover(edb_wal *w, edb_pager *pager, edb_error *err) {
    /* Scan records; apply PAGE_IMAGE for txns that have COMMIT. */
    edb_error_clear(err);
    off_t sz = lseek(w->fd, 0, SEEK_END);
    if (sz <= WAL_HDR_SIZE) return 0;

    /* Two-pass: collect committed txn ids, then apply their pages.
       Simplified single-writer: apply all PAGE_IMAGE that appear before a COMMIT
       after the last CHECKPOINT. */
    lseek(w->fd, WAL_HDR_SIZE, SEEK_SET);
    off_t pos = WAL_HDR_SIZE;
    uint64_t committed = 0;
    while (pos + 22 + 4 <= sz) { /* min record size */
        uint8_t hdr[22];
        if (pread(w->fd, hdr, 22, pos) != 22) break;
        uint8_t type = hdr[0];
        uint64_t seq = edb_load_u64_le(hdr + 2);
        uint64_t txn = edb_load_u64_le(hdr + 10);
        uint32_t plen = edb_load_u32_le(hdr + 18);
        off_t rec_end = pos + 22 + plen + 4;
        if (rec_end > sz) break; /* torn tail */

        uint8_t *payload = NULL;
        if (plen) {
            payload = malloc(plen);
            if (!payload) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
            if (pread(w->fd, payload, plen, pos + 22) != (ssize_t)plen) {
                free(payload); break;
            }
        }
        uint8_t csbuf[4];
        if (pread(w->fd, csbuf, 4, pos + 22 + plen) != 4) { free(payload); break; }
        uint32_t stored = edb_load_u32_le(csbuf);
        uint32_t calc = simple_checksum(hdr, 22);
        if (payload) calc ^= simple_checksum(payload, plen);
        if (calc != stored) {
            free(payload);
            /* checksum failure in committed region is corruption; torn tail OK */
            if (type == EDB_WAL_COMMIT) {
                edb_error_set(err, EDB_CORRUPTION, 0, "wal checksum failure");
                return -1;
            }
            break;
        }

        if (type == EDB_WAL_COMMIT)
            committed = txn;
        else if (type == EDB_WAL_PAGE_IMAGE && payload && plen == 4 + EDB_PAGE_SIZE) {
            uint32_t page_no = edb_load_u32_le(payload);
            /* Two-pass needed for correctness; collect for now then apply after scan.
               Single-writer bootstrap: store last image per page in a small map by
               applying only after we finish and know committed txn — simplified:
               apply all images from committed txn in a second pass below. */
            (void)page_no;
        } else if (type == EDB_WAL_CHECKPOINT) {
            committed = 0;
        }
        free(payload);
        pos = rec_end;
        (void)seq;
    }

    /* Second pass: apply PAGE_IMAGE for committed transaction */
    if (committed == 0) return 0;
    lseek(w->fd, WAL_HDR_SIZE, SEEK_SET);
    pos = WAL_HDR_SIZE;
    while (pos + 22 + 4 <= sz) {
        uint8_t hdr[22];
        if (pread(w->fd, hdr, 22, pos) != 22) break;
        uint8_t type = hdr[0];
        uint64_t txn = edb_load_u64_le(hdr + 10);
        uint32_t plen = edb_load_u32_le(hdr + 18);
        off_t rec_end = pos + 22 + plen + 4;
        if (rec_end > sz) break;
        if (type == EDB_WAL_PAGE_IMAGE && txn == committed && plen == 4 + EDB_PAGE_SIZE) {
            uint8_t *payload = malloc(plen);
            if (!payload) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
            if (pread(w->fd, payload, plen, pos + 22) != (ssize_t)plen) {
                free(payload); break;
            }
            uint32_t page_no = edb_load_u32_le(payload);
            if (edb_pager_overwrite(pager, page_no, payload + 4, err) != 0) {
                free(payload); return -1;
            }
            free(payload);
        }
        if (type == EDB_WAL_CHECKPOINT) break;
        pos = rec_end;
    }
    return 0;
}
