#ifndef CVC_REPO_H
#define CVC_REPO_H

#include <stdint.h>
#include <stdbool.h>
#include "util.h"
#include "win32.h"

/* Repository configuration (effective). */
typedef struct {
    int format_version;
    int save_show_diffstat;       /* default true */
    StrVec tracking_include;
    StrVec tracking_exclude;      /* config list (built-ins applied separately) */
    StrVec diffstat_include;
    StrVec diffstat_exclude;
} RepoConfig;

typedef struct Repo {
    uint16_t *root16;      /* extended absolute repo root with trailing '\\' */
    char *root8;           /* canonical UTF-8 of root for display */
    uint16_t *cvc16;       /* extended .cvc dir */
    RepoConfig cfg;
    /* current branch name (owned) from HEAD */
    char *head_branch;
    /* Is config valid? (for commands that proceed despite config errors) */
    int config_ok;
} Repo;

/* Discover repo by walking up from cwd. Returns CVC_OK + repo populated, or
 * CVC_ERR_NOTREPO if none. Caller must repo_free. */
CvcStatus repo_discover(Repo *repo);

/* Initialize a new repository in cwd. On failure cleans up created entries. */
CvcStatus repo_init(Repo *repo);

/* Load and validate config from .cvc/config.json. Sets repo->cfg.
 * Returns CVC_OK, or error. On error, config_ok=0. */
CvcStatus repo_load_config(Repo *repo);

void repo_free(Repo *repo);

/* Get current branch ref target. Returns 0 if unborn (zero-length), 1 if born.
 * On error returns -1 and sets *err. Outputs id. */
int repo_current_branch(const Repo *repo, char **branch_out);
int repo_read_branch(const Repo *repo, const char *branch, uint8_t id[32], int *born);
int repo_branch_exists(const Repo *repo, const char *branch);

/* --- Locking wrapper --------------------------------------------------- */
CvcStatus repo_lock_read(const Repo *repo, RepoLock *lk);
CvcStatus repo_lock_write(const Repo *repo, RepoLock *lk);
void repo_unlock(RepoLock *lk);

/* Resolve a revision spec to a commit id.
 * Order: exact branch name; full 64-hex; unique >=8-hex prefix.
 * Returns CVC_OK and sets id+is_branch. */
CvcStatus repo_resolve_revision(const Repo *repo, const char *spec, uint8_t id[32]);

/* Default merge message helper is in cli/merge. */

#endif
