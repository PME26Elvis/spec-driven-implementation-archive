#include "edb/freelist.h"
#include "edb/byteorder.h"
#include <stdlib.h>
#include <string.h>

/* On-disk freelist page layout (after EDB_HEADER_SIZE):
 * type byte | next_freelist_page u32 | count u16 | page_no u32 * count
 * Max entries per page ~ (PAGE - HDR - 16 - 7) / 4
 */

#define FL_HDR EDB_HEADER_SIZE
#define FL_NEXT_OFF (FL_HDR + 1)
#define FL_COUNT_OFF (FL_HDR + 5)
#define FL_ENTRIES_OFF (FL_HDR + 7)
#define FL_MAX_ENTRIES ((EDB_PAGE_SIZE - FL_ENTRIES_OFF - 16) / 4)

struct edb_freelist {
    edb_pager *pager;
    uint32_t  *pages;   /* in-memory stack mirror */
    size_t     count;
    size_t     cap;
    uint32_t   root_page; /* first on-disk freelist page, 0 if none */
    bool       dirty;
};

edb_freelist *edb_freelist_create(edb_pager *p) {
    edb_freelist *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    f->pager = p;
    f->cap = 128;
    f->pages = calloc(f->cap, sizeof(uint32_t));
    if (!f->pages) { free(f); return NULL; }
    return f;
}

void edb_freelist_destroy(edb_freelist *f) {
    if (!f) return;
    free(f->pages);
    free(f);
}

int edb_freelist_push(edb_freelist *f, uint32_t page_no) {
    if (!f || page_no == 0) return -1;
    if (f->count >= f->cap) {
        size_t nc = f->cap * 2;
        uint32_t *np = realloc(f->pages, nc * sizeof(uint32_t));
        if (!np) return -1;
        f->pages = np; f->cap = nc;
    }
    f->pages[f->count++] = page_no;
    f->dirty = true;
    return 0;
}

uint32_t edb_freelist_pop(edb_freelist *f) {
    if (!f || f->count == 0) return 0;
    f->dirty = true;
    return f->pages[--f->count];
}

/* Load on-disk chain into memory */
int edb_freelist_load(edb_freelist *f, uint32_t root, edb_error *err) {
    if (!f) return -1;
    f->root_page = root;
    f->count = 0;
    uint32_t page = root;
    int hops = 0;
    while (page != 0) {
        if (++hops > 100000) {
            edb_error_set(err, EDB_CORRUPTION, 0, "freelist cycle");
            return -1;
        }
        edb_page *pg = edb_pager_get(f->pager, page, err);
        if (!pg) return -1;
        uint32_t next = edb_load_u32_le(pg->data + FL_NEXT_OFF);
        uint16_t cnt = edb_load_u16_le(pg->data + FL_COUNT_OFF);
        if (cnt > FL_MAX_ENTRIES) {
            edb_pager_unpin(f->pager, pg);
            edb_error_set(err, EDB_CORRUPTION, 0, "freelist count overflow");
            return -1;
        }
        for (uint16_t i = 0; i < cnt; i++) {
            uint32_t pn = edb_load_u32_le(pg->data + FL_ENTRIES_OFF + i * 4);
            if (edb_freelist_push(f, pn) != 0) {
                edb_pager_unpin(f->pager, pg);
                return -1;
            }
        }
        edb_pager_unpin(f->pager, pg);
        page = next;
    }
    f->dirty = false;
    return 0;
}

/* Flush in-memory list to on-disk pages; returns new root */
int edb_freelist_flush(edb_freelist *f, uint32_t *out_root, edb_error *err) {
    if (!f) return -1;
    if (!f->dirty && f->root_page != 0) {
        if (out_root) *out_root = f->root_page;
        return 0;
    }
    /* Free old chain pages onto... themselves is messy; just write new chain */
    uint32_t first = 0, prev = 0;
    size_t idx = 0;
    while (idx < f->count) {
        edb_page *pg = edb_pager_new(f->pager, EDB_PAGE_FREELIST, err);
        if (!pg) return -1;
        memset(pg->data, 0, EDB_PAGE_SIZE);
        pg->data[FL_HDR] = (uint8_t)EDB_PAGE_FREELIST;
        edb_store_u32_le(pg->data + FL_NEXT_OFF, 0);
        uint16_t take = 0;
        while (idx < f->count && take < FL_MAX_ENTRIES) {
            edb_store_u32_le(pg->data + FL_ENTRIES_OFF + take * 4, f->pages[idx]);
            take++; idx++;
        }
        edb_store_u16_le(pg->data + FL_COUNT_OFF, take);
        edb_pager_mark_dirty(f->pager, pg);
        if (edb_pager_write_page(f->pager, pg, err) != 0) {
            edb_pager_unpin(f->pager, pg);
            return -1;
        }
        if (first == 0) first = pg->page_no;
        if (prev != 0) {
            edb_page *pp = edb_pager_get(f->pager, prev, err);
            if (!pp) { edb_pager_unpin(f->pager, pg); return -1; }
            edb_store_u32_le(pp->data + FL_NEXT_OFF, pg->page_no);
            edb_pager_mark_dirty(f->pager, pp);
            edb_pager_write_page(f->pager, pp, err);
            edb_pager_unpin(f->pager, pp);
        }
        prev = pg->page_no;
        edb_pager_unpin(f->pager, pg);
    }
    f->root_page = first;
    f->dirty = false;
    if (out_root) *out_root = first;
    return 0;
}

uint32_t edb_freelist_root(const edb_freelist *f) {
    return f ? f->root_page : 0;
}

size_t edb_freelist_count(const edb_freelist *f) {
    return f ? f->count : 0;
}
