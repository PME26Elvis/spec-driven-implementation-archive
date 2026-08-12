#ifndef DARC_RESTORE_H
#define DARC_RESTORE_H
#include "darc_repo.h"
#include <stdbool.h>

/* overwrite: 0=never, 1=always, 2=if-newer */
int darc_restore(darc_repo_t *repo, const darc_cid_t snapshot,
                 const char *dest, const char *path_filter,
                 int overwrite);

#endif
