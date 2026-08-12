#ifndef DARC_DIFF_H
#define DARC_DIFF_H
#include "darc_repo.h"

int darc_snapshot_diff(darc_repo_t *repo, const darc_cid_t old_cid, const darc_cid_t new_cid,
                       const char *path_filter, const char *format /* text|json */);

#endif
