/* tinyvcs_core.h - content-addressed snapshot version-control core.
 *
 * Canonical formats: docs/04_DEV_TOOL_TINYVCS.md and
 * docs/19_CANONICAL_FORMATS_AND_LIMITS.md sections 6-15, 29.
 *
 * This header exposes the object store, index, references, ignore rules,
 * status diffing and the atomic checkout machinery used by the CLI
 * (tinyvcs.c) and the scenario test suite (test_tinyvcs.c).  All paths
 * handled here are repository-root-relative canonical UTF-8 with '/'
 * separators unless a function name contains "w" (wide/Win32).
 */
#ifndef TINYVCS_CORE_H
#define TINYVCS_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

/* ------------------------------------------------------------------ */
/* Object identity                                                     */
/* ------------------------------------------------------------------ */

typedef uint8_t tv_oid[32];

typedef enum tv_obj_type {
    TV_OBJ_BLOB   = 1,
    TV_OBJ_TREE   = 2,
    TV_OBJ_COMMIT = 3
} tv_obj_type;

void tv_oid_hex(const tv_oid id, char out_hex[65]);
/* Parses 64 lowercase hex into an oid; returns 1 on success. */
int  tv_oid_from_hex(const char *hex, tv_oid out);
int  tv_oid_equal(const tv_oid a, const tv_oid b);

/* ------------------------------------------------------------------ */
/* Repository handle                                                   */
/* ------------------------------------------------------------------ */

typedef struct tv_repo {
    wchar_t *root;   /* absolute working-tree root (parent of .tinyvcs) */
    wchar_t *meta;   /* absolute path of the .tinyvcs directory          */
} tv_repo;

/* Walks up from the current working directory looking for a .tinyvcs
 * directory.  On success *out_root is malloc'd and must be freed. */
sdk_status tv_find_repo_root(wchar_t **out_root);

/* Opens an existing repository whose working-tree root is `root`. */
sdk_status tv_open_repo(const wchar_t *root, tv_repo *r);
void      tv_close_repo(tv_repo *r);

/* Creates a fresh repository in `root`.  Rejects nested/duplicate init. */
sdk_status tv_init_repo(const wchar_t *root);

/* ------------------------------------------------------------------ */
/* Object store                                                        */
/* ------------------------------------------------------------------ */

/* Hashes "<type> SP <len-decimal> NUL <payload>" into out_id. */
void tv_object_id_for(const char *type, const uint8_t *payload, size_t plen,
                      tv_oid out_id);

sdk_status tv_write_object(tv_repo *r, tv_obj_type type,
                           const uint8_t *payload, size_t plen, tv_oid out_id);
sdk_status tv_read_object(tv_repo *r, const tv_oid id, tv_obj_type *out_type,
                          uint8_t **out_payload, size_t *out_len);
sdk_status tv_object_exists(tv_repo *r, const tv_oid id, int *exists);
sdk_status tv_object_path(tv_repo *r, const tv_oid id, wchar_t **out);

/* Reads a file from disk and computes its blob id (content-addressed). */
sdk_status tv_file_blob_id(const wchar_t *work_path, tv_oid out_id,
                           uint64_t *out_size, int *out_readonly);

/* ------------------------------------------------------------------ */
/* Tree objects                                                        */
/* ------------------------------------------------------------------ */

typedef struct tv_tree_entry {
    char     name[256];      /* single component, canonical UTF-8           */
    uint8_t  entry_type;     /* 1=file, 2=dir                                */
    uint8_t  file_flags;     /* bit0 = read-only                            */
    tv_oid   oid;
} tv_tree_entry;

/* Parses a tree payload into entries (caller frees *out with free()). */
sdk_status tv_tree_parse(const uint8_t *payload, size_t len,
                         tv_tree_entry **out, size_t *out_count);

/* Builds a tree (recursively) from a set of present index paths. */
sdk_status tv_build_tree_from_paths(tv_repo *r,
                                    const char *const *paths,
                                    const tv_oid *blobs,
                                    const uint8_t *flags,
                                    size_t count, tv_oid *out_root);

typedef struct tv_file_entry {
    char    path[SDK_LIMIT_VCS_PATH_BYTES];
    uint8_t file_flags;
    tv_oid  blob;
} tv_file_entry;

/* Recursively expands a tree into a flat list of files.  Caller frees
 * the array with free(). */
sdk_status tv_tree_collect_files(tv_repo *r, const tv_oid tree_id,
                                 tv_file_entry **out, size_t *out_count);

/* ------------------------------------------------------------------ */
/* Commit objects                                                      */
/* ------------------------------------------------------------------ */

typedef struct tv_commit_info {
    uint8_t format_version;
    uint8_t parent_count;
    tv_oid  parent;
    tv_oid  root_tree;
    int64_t timestamp_ms;
    char    author[SDK_LIMIT_VCS_AUTHOR_BYTES + 1];
    char    message[SDK_LIMIT_VCS_MESSAGE_BYTES + 1];
} tv_commit_info;

sdk_status tv_create_commit(tv_repo *r, const tv_oid *parent,
                            const tv_oid *root_tree, const char *author,
                            const char *message, tv_oid out_id);

/* Reads + validates a commit object and returns its fields. */
sdk_status tv_read_commit(tv_repo *r, const tv_oid id, tv_commit_info *out);

/* ------------------------------------------------------------------ */
/* Index                                                               */
/* ------------------------------------------------------------------ */

typedef struct tv_index_entry {
    char    path[SDK_LIMIT_VCS_PATH_BYTES];
    uint8_t file_flags;     /* bit0 = read-only                          */
    uint8_t stage_state;    /* 0=present, 1=deleted                      */
    tv_oid  blob;           /* all zero when deleted                     */
    uint64_t size;
    int64_t  mtime_100ns;
} tv_index_entry;

typedef struct tv_index {
    tv_index_entry *entries;
    size_t          count;
    size_t          cap;
} tv_index;

void tv_index_init(tv_index *idx);
void tv_index_free(tv_index *idx);
/* Finds an entry by exact path; returns NULL if absent. */
tv_index_entry *tv_index_find(tv_index *idx, const char *path);
/* Inserts or replaces an entry, keeping the list canonically sorted.
 * Returns the entry or NULL on allocation failure. */
tv_index_entry *tv_index_upsert(tv_index *idx, const tv_index_entry *e);
/* Removes an entry by exact path. */
void tv_index_remove(tv_index *idx, const char *path);

sdk_status tv_index_load(tv_repo *r, tv_index *idx);
sdk_status tv_index_save(tv_repo *r, const tv_index *idx);

/* ------------------------------------------------------------------ */
/* References and HEAD                                                 */
/* ------------------------------------------------------------------ */

sdk_status tv_head_branch(tv_repo *r, char *out_branch, size_t cap);
/* Reads the commit a branch points to.  *unborn set when the ref is empty. */
sdk_status tv_read_ref(tv_repo *r, const char *branch, tv_oid *out_commit,
                       int *unborn);
/* Atomically writes a branch ref under the ref lock. */
sdk_status tv_write_ref(tv_repo *r, const char *branch, const tv_oid *commit);
sdk_status tv_branch_exists(tv_repo *r, const char *branch, int *exists);
sdk_status tv_create_branch(tv_repo *r, const char *branch,
                            const tv_oid *commit);
sdk_status tv_list_branches(tv_repo *r, char ***out_names, size_t *out_count,
                            char *out_current, size_t current_cap);
void      tv_branch_list_free(char **names, size_t count);

/* Resolves a commit spec: 64-hex id, or a branch/ref name. */
sdk_status tv_resolve_commit(tv_repo *r, const char *spec, tv_oid *out);

/* Validates a branch name per docs/19 section 15.5 / 04 section 9. */
sdk_status tv_branch_name_check(const char *name);

/* ------------------------------------------------------------------ */
/* Ignore rules                                                        */
/* ------------------------------------------------------------------ */

typedef struct tv_ignore {
    char **patterns;
    size_t count;
    size_t cap;
} tv_ignore;

void tv_ignore_init(tv_ignore *ig);
void tv_ignore_free(tv_ignore *ig);
sdk_status tv_ignore_load(tv_repo *r, tv_ignore *ig);
/* Returns 1 when rel_path is excluded. */
int tv_is_ignored(const tv_ignore *ig, const char *rel_path, int is_dir);
/* Always-ignored metadata directory. */
int tv_is_meta_name(const char *component);

/* ------------------------------------------------------------------ */
/* Status                                                              */
/* ------------------------------------------------------------------ */

typedef struct tv_status_info {
    char   branch[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    char **staged_added;       size_t staged_added_n;
    char **staged_modified;    size_t staged_modified_n;
    char **staged_deleted;     size_t staged_deleted_n;
    char **unstaged_modified;  size_t unstaged_modified_n;
    char **unstaged_deleted;   size_t unstaged_deleted_n;
    char **untracked;          size_t untracked_n;
} tv_status_info;

sdk_status tv_status_collect(tv_repo *r, tv_status_info *out);
void tv_status_free(tv_status_info *s);

/* ------------------------------------------------------------------ */
/* Working-tree enumeration                                            */
/* ------------------------------------------------------------------ */

typedef struct tv_workfile {
    char     *rel;     /* canonical UTF-8 rel path                      */
    wchar_t  *wpath;   /* absolute wide path                           */
} tv_workfile;

sdk_status tv_collect_working(tv_repo *r, const tv_ignore *ig,
                              tv_workfile **out, size_t *out_count);
void tv_workfile_list_free(tv_workfile *list, size_t count);

/* ------------------------------------------------------------------ */
/* Checkout apply (switch / reset --hard / restore)                    */
/* ------------------------------------------------------------------ */

/* Updates the working tree so it matches `desired` (path/blobs/flags).
 * Files not in `desired` that are currently tracked are removed; untracked
 * files are left alone.  On any I/O failure the overwritten files are
 * restored from temp backups and an error is returned.  When update_index
 * is set the staging index is rewritten to match `desired`. */
sdk_status tv_apply_checkout(tv_repo *r, const tv_file_entry *desired,
                             size_t desired_n, int update_index);

/* ------------------------------------------------------------------ */
/* Verify                                                               */
/* ------------------------------------------------------------------ */

typedef struct tv_verify_result {
    unsigned scanned;
    unsigned reachable;
    unsigned unreachable;
    unsigned corrupt;
    unsigned missing;
    unsigned malformed;
    unsigned warnings;
    int      ok;   /* 1 when the repository is complete */
} tv_verify_result;

sdk_status tv_verify(tv_repo *r, tv_verify_result *out);

/* ------------------------------------------------------------------ */
/* Error reporting (stable English stage + Win32 code)                 */
/* ------------------------------------------------------------------ */

/* The most recent failing operation stage and Win32 error code, used by the
 * CLI to emit a stable, locale-independent diagnostic on stderr. */
const char *tv_error_stage(void);
uint32_t    tv_error_win32(void);
void        tv_error_clear(void);
void        tv_error_set(const char *stage, uint32_t win32);

/* ------------------------------------------------------------------ */
/* Command dispatch (shared by CLI and tests)                          */
/* ------------------------------------------------------------------ */

/* argc/argv are the same shape as wmain: argv[0] is the program name.
 * Returns a canonical process exit code (SDK_EXIT_*). */
int tv_dispatch(int argc, wchar_t **argv);

#endif /* TINYVCS_CORE_H */
