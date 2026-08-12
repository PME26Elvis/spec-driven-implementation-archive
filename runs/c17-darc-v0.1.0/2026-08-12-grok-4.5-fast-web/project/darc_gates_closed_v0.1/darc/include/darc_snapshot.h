#ifndef DARC_SNAPSHOT_H
#define DARC_SNAPSHOT_H

#include "darc_repo.h"
#include "darc_index.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    darc_cid_t cid;
    char name[256];
    uint64_t created_ns;
    darc_cid_t parent;
    int has_parent;
    darc_cid_t root_tree;
    uint64_t file_count, dir_count, symlink_count, hardlink_count;
    uint64_t logical_bytes;
    uint64_t unique_chunks;
    uint64_t new_chunks;
    uint64_t stored_bytes;
    uint8_t profile_hash[32];
} darc_snapshot_info_t;

int darc_snapshot_create(darc_repo_t *repo, darc_index_t *idx,
                         const char **sources, size_t nsrc,
                         const char *name, const darc_cid_t *parent,
                         uint64_t timestamp_ns,
                         darc_cid_t out_cid);

int darc_snapshot_load_info(darc_repo_t *repo, const darc_cid_t cid, darc_snapshot_info_t *info);
int darc_snapshot_list(darc_repo_t *repo, darc_snapshot_info_t **out, size_t *count);

#endif
