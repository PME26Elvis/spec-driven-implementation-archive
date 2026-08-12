#ifndef DARC_REPO_H
#define DARC_REPO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "darc_object.h"

#define DARC_FORMAT_VERSION 1

typedef struct darc_repo {
    char *path;          /* absolute repo root */
    int lock_fd;
    bool locked;
} darc_repo_t;

int darc_repo_init(const char *path);
darc_repo_t *darc_repo_open(const char *path);
void darc_repo_close(darc_repo_t *repo);

/* Path helpers (allocated, caller frees) */
char *darc_repo_object_path(const darc_repo_t *repo, const darc_cid_t cid);
char *darc_repo_ref_path(const darc_repo_t *repo, const char *name);
char *darc_repo_tmp_path(const darc_repo_t *repo, const char *suffix);

int darc_repo_lock(darc_repo_t *repo);
void darc_repo_unlock(darc_repo_t *repo);

/* Atomic write of a small text/ref file via tmp + fsync + rename */
int darc_repo_atomic_write(const darc_repo_t *repo, const char *relpath,
                           const void *data, size_t len);

/* Journal for crash-safe publication */
int darc_journal_begin(darc_repo_t *repo, const char *op);
int darc_journal_commit(darc_repo_t *repo);
int darc_journal_abort(darc_repo_t *repo);
int darc_journal_recover(darc_repo_t *repo);

/* HEAD and refs */
int darc_repo_set_head(darc_repo_t *repo, const darc_cid_t cid);
int darc_repo_get_head(darc_repo_t *repo, darc_cid_t out);
int darc_repo_write_snapshot_ref(darc_repo_t *repo, const darc_cid_t cid);
int darc_repo_delete_snapshot_ref(darc_repo_t *repo, const darc_cid_t cid);
int darc_repo_list_snapshot_refs(darc_repo_t *repo, darc_cid_t **out, size_t *count);

/* Object store */
int darc_repo_has_object(const darc_repo_t *repo, const darc_cid_t cid);
int darc_repo_put_object(darc_repo_t *repo, uint8_t type, uint8_t codec,
                         const void *uncomp, size_t uncomp_len,
                         const void *stored, size_t stored_len,
                         darc_cid_t out_cid);
int darc_repo_get_object(const darc_repo_t *repo, const darc_cid_t cid,
                         uint8_t *type, uint8_t **payload, size_t *payload_len);

#endif
