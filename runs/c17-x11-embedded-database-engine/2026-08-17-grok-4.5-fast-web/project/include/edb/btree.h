#ifndef EDB_BTREE_H
#define EDB_BTREE_H

#include "edb/common.h"
#include "edb/pager.h"
#include <stdbool.h>

/* Minimal key: opaque byte sequence + length. Composite keys built on top. */
typedef struct edb_key {
    const uint8_t *data;
    uint16_t       len;
} edb_key;

typedef struct edb_btree {
    edb_pager *pager;
    uint32_t   root_page;
    bool       unique;
    /* for diagnostics */
    int        height;
} edb_btree;

/* Create a new empty B+ tree; returns root page number via out_root. */
int edb_btree_create(edb_pager *pager, bool unique, uint32_t *out_root, edb_error *err);

/* Open existing tree given root page. */
edb_btree *edb_btree_open(edb_pager *pager, uint32_t root, bool unique, edb_error *err);
void       edb_btree_close(edb_btree *t);

/* Insert. For unique trees, returns CONSTRAINT if key exists.
 * value is opaque payload stored in leaf (e.g. rowid / record pointer). */
int edb_btree_insert(edb_btree *t, const edb_key *key,
                     const uint8_t *val, uint16_t val_len, edb_error *err);

/* Point lookup. Returns 0 and fills val_out if found; NOT_FOUND otherwise. */
int edb_btree_get(edb_btree *t, const edb_key *key,
                  uint8_t *val_out, uint16_t *val_len_io, edb_error *err);

/* Delete by key. */
int edb_btree_delete(edb_btree *t, const edb_key *key, edb_error *err);

/* Validation: check ordering, occupancy, no cycles (BTREE-015). */
int edb_btree_validate(edb_btree *t, edb_error *err);

int edb_btree_height(const edb_btree *t);

#endif
