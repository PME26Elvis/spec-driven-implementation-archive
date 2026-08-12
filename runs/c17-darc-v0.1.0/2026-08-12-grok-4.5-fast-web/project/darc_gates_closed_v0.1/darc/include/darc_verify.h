#ifndef DARC_VERIFY_H
#define DARC_VERIFY_H

#include "darc_repo.h"
#include "darc_index.h"

typedef enum { DARC_VERIFY_QUICK, DARC_VERIFY_FULL, DARC_VERIFY_SCRUB } darc_verify_level_t;

typedef struct {
    size_t objects_checked;
    size_t objects_ok;
    size_t objects_corrupt;
    size_t objects_missing;
    size_t repaired;
    size_t unrecoverable;
} darc_verify_result_t;

int darc_verify(darc_repo_t *repo, darc_index_t *idx, darc_verify_level_t level,
                bool repair, darc_verify_result_t *result);

int darc_parity_protect_chunk(darc_repo_t *repo, const darc_cid_t *cids, size_t n,
                              darc_cid_t parity_out);
int darc_parity_recover(darc_repo_t *repo, const darc_cid_t missing,
                        const darc_cid_t *stripe, size_t stripe_n,
                        darc_cid_t recovered);

int darc_gc(darc_repo_t *repo, darc_index_t *idx, bool dry_run, size_t *reclaimed);
int darc_parity_protect_all(darc_repo_t *repo, darc_index_t *idx);

#endif
