#ifndef EDB_PAGER_H
#define EDB_PAGER_H

#include "edb/common.h"
#include <stdbool.h>

#ifndef EDB_HEADER_SIZE
#define EDB_HEADER_SIZE 256u
#endif

/* Page types (FMT-007) */
typedef enum edb_page_type {
    EDB_PAGE_META     = 1,
    EDB_PAGE_SCHEMA   = 2,
    EDB_PAGE_BTREE_INTERNAL = 3,
    EDB_PAGE_BTREE_LEAF     = 4,
    EDB_PAGE_OVERFLOW = 5,
    EDB_PAGE_FREELIST = 6,
    EDB_PAGE_WAL_META = 7
} edb_page_type;

typedef struct edb_page {
    uint32_t     page_no;
    edb_page_type type;
    uint32_t     generation;
    uint8_t      data[EDB_PAGE_SIZE];  /* full page image (header + payload) */
    bool         dirty;
    bool         pinned;
    int          refcount;
    /* cache linkage */
    struct edb_page *next;
    struct edb_page *prev;
} edb_page;

typedef struct edb_pager_stats {
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t dirty_writes;
    uint64_t pinned_peak;
} edb_pager_stats;

typedef struct edb_pager edb_pager;

/* Open / create / close */
edb_pager *edb_pager_open(const char *path, bool create, bool read_only,
                          const uint8_t *key32_or_null, edb_error *err);
void       edb_pager_close(edb_pager *p);

/* Page access */
edb_page  *edb_pager_get(edb_pager *p, uint32_t page_no, edb_error *err);
edb_page  *edb_pager_new(edb_pager *p, edb_page_type type, edb_error *err);
void       edb_pager_unpin(edb_pager *p, edb_page *page);
void       edb_pager_mark_dirty(edb_pager *p, edb_page *page);

/* Durability helpers (WAL will coordinate later) */
int        edb_pager_write_page(edb_pager *p, edb_page *page, edb_error *err);
int        edb_pager_sync(edb_pager *p, edb_error *err);

/* Cache control */
void       edb_pager_set_cache_pages(edb_pager *p, size_t n);
const edb_pager_stats *edb_pager_get_stats(const edb_pager *p);

/* Header helpers */
uint32_t   edb_pager_last_page(const edb_pager *p);
uint64_t   edb_pager_db_id(const edb_pager *p);
bool       edb_pager_is_encrypted(const edb_pager *p);
/* Return page to freelist for reuse */
int        edb_pager_free_page(edb_pager *p, uint32_t page_no);
/* Recovery: write full page image (logical 4K) from WAL */
int        edb_pager_overwrite(edb_pager *p, uint32_t page_no, const uint8_t *data, edb_error *err);

#endif
