#ifndef EDB_FREELIST_H
#define EDB_FREELIST_H
#include "edb/common.h"
#include "edb/pager.h"
#include <stdbool.h>

typedef struct edb_freelist edb_freelist;

edb_freelist *edb_freelist_create(edb_pager *p);
void          edb_freelist_destroy(edb_freelist *f);
int           edb_freelist_push(edb_freelist *f, uint32_t page_no);
uint32_t      edb_freelist_pop(edb_freelist *f);
int           edb_freelist_load(edb_freelist *f, uint32_t root, edb_error *err);
int           edb_freelist_flush(edb_freelist *f, uint32_t *out_root, edb_error *err);
uint32_t      edb_freelist_root(const edb_freelist *f);
size_t        edb_freelist_count(const edb_freelist *f);

#endif
