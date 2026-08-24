#include "edb/overflow.h"
#include "edb/byteorder.h"
#include <string.h>
#include <stdlib.h>

int edb_overflow_write(edb_pager *p, const uint8_t *data, uint32_t len,
                       uint32_t *out_first_page, edb_error *err) {
    edb_error_clear(err);
    if (!p || !data || !out_first_page) {
        edb_error_set(err, EDB_IO, 0, "bad overflow args");
        return -1;
    }
    uint32_t first = 0, prev = 0;
    uint32_t off = 0;
    while (off < len) {
        edb_page *pg = edb_pager_new(p, EDB_PAGE_OVERFLOW, err);
        if (!pg) return -1;
        uint32_t chunk = len - off;
        if (chunk > EDB_OVERFLOW_PAYLOAD) chunk = EDB_OVERFLOW_PAYLOAD;
        memset(pg->data, 0, EDB_PAGE_SIZE);
        pg->data[EDB_HEADER_SIZE] = (uint8_t)EDB_PAGE_OVERFLOW;
        /* next filled later */
        edb_store_u32_le(pg->data + EDB_HEADER_SIZE + 1, 0);
        edb_store_u16_le(pg->data + EDB_HEADER_SIZE + 5, (uint16_t)chunk);
        memcpy(pg->data + EDB_HEADER_SIZE + 7, data + off, chunk);
        edb_pager_mark_dirty(p, pg);
        if (edb_pager_write_page(p, pg, err) != 0) {
            edb_pager_unpin(p, pg);
            return -1;
        }
        if (first == 0) first = pg->page_no;
        if (prev != 0) {
            edb_page *pp = edb_pager_get(p, prev, err);
            if (!pp) { edb_pager_unpin(p, pg); return -1; }
            edb_store_u32_le(pp->data + EDB_HEADER_SIZE + 1, pg->page_no);
            edb_pager_mark_dirty(p, pp);
            edb_pager_write_page(p, pp, err);
            edb_pager_unpin(p, pp);
        }
        prev = pg->page_no;
        edb_pager_unpin(p, pg);
        off += chunk;
    }
    *out_first_page = first;
    return 0;
}

int edb_overflow_read(edb_pager *p, uint32_t first_page,
                      uint8_t *buf, uint32_t cap, uint32_t *out_len,
                      edb_error *err) {
    edb_error_clear(err);
    uint32_t page = first_page;
    uint32_t written = 0;
    int hops = 0;
    while (page != 0) {
        if (++hops > 100000) {
            edb_error_set(err, EDB_CORRUPTION, 0, "overflow cycle");
            return -1;
        }
        edb_page *pg = edb_pager_get(p, page, err);
        if (!pg) return -1;
        uint32_t next = edb_load_u32_le(pg->data + EDB_HEADER_SIZE + 1);
        uint16_t chunk = edb_load_u16_le(pg->data + EDB_HEADER_SIZE + 5);
        if (chunk > EDB_OVERFLOW_PAYLOAD) {
            edb_pager_unpin(p, pg);
            edb_error_set(err, EDB_CORRUPTION, 0, "overflow chunk too large");
            return -1;
        }
        if (written + chunk > cap) {
            edb_pager_unpin(p, pg);
            edb_error_set(err, EDB_LIMIT, 0, "overflow read buffer too small");
            return -1;
        }
        memcpy(buf + written, pg->data + EDB_HEADER_SIZE + 7, chunk);
        written += chunk;
        edb_pager_unpin(p, pg);
        page = next;
    }
    if (out_len) *out_len = written;
    return 0;
}

int edb_overflow_free(edb_pager *p, uint32_t first_page, edb_error *err) {
    edb_error_clear(err);
    uint32_t page = first_page;
    int hops = 0;
    while (page != 0) {
        if (++hops > 100000) {
            edb_error_set(err, EDB_CORRUPTION, 0, "overflow cycle");
            return -1;
        }
        edb_page *pg = edb_pager_get(p, page, err);
        if (!pg) return -1;
        uint32_t next = edb_load_u32_le(pg->data + EDB_HEADER_SIZE + 1);
        edb_pager_unpin(p, pg);
        edb_pager_free_page(p, page);
        page = next;
    }
    return 0;
}
