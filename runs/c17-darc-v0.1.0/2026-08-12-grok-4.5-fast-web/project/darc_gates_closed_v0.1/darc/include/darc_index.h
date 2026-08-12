#ifndef DARC_INDEX_H
#define DARC_INDEX_H

#include "darc_object.h"
#include "darc_repo.h"
#include <stdbool.h>

typedef struct {
    darc_cid_t cid;
    uint64_t size;
    uint8_t type;
    uint8_t used;
} darc_index_entry_t;

typedef struct {
    darc_index_entry_t *slots;
    size_t capacity;
    size_t count;
} darc_index_t;

darc_index_t *darc_index_create(size_t initial_cap);
void darc_index_free(darc_index_t *idx);
int darc_index_put(darc_index_t *idx, const darc_cid_t cid, uint8_t type, uint64_t size);
int darc_index_get(const darc_index_t *idx, const darc_cid_t cid, darc_index_entry_t *out);
int darc_index_save(const darc_index_t *idx, const darc_repo_t *repo);
darc_index_t *darc_index_load(const darc_repo_t *repo);
darc_index_t *darc_index_rebuild(const darc_repo_t *repo);

#endif
