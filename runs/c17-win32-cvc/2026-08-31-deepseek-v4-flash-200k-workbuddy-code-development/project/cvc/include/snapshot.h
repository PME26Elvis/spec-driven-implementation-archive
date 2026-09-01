#ifndef CVC_SNAPSHOT_H
#define CVC_SNAPSHOT_H

#include "objects.h"
#include "repo.h"
#include "util.h"

/* A path->(type,id) map representing a complete snapshot. Sorted by path. */
typedef struct {
    char *path;
    uint8_t type;
    uint8_t id[32];
} SnapLeaf;

typedef struct {
    SnapLeaf *items;
    size_t len;
    size_t cap;
} Snapshot;

void snap_init(Snapshot *s);
void snap_free(Snapshot *s);
int snap_add(Snapshot *s, const char *path, uint8_t type, const uint8_t id[32]);
/* sort by path; returns 0 or -1 on dup/case-collision */
int snap_sort(Snapshot *s);
/* find leaf by path, returns index or -1 */
long snap_find(const Snapshot *s, const char *path);

/* Build tree objects (recursively) from a snapshot's leaves and return the
 * root tree id. Also dedups blobs/symlinks by writing them via obj_write.
 * Returns CVC_OK and sets root_id. */
CvcStatus snap_build_tree(const Repo *repo, const Snapshot *s, uint8_t root_id[32]);

/* Decode a commit's root tree into a Snapshot (path->type,id). */
CvcStatus snap_from_commit(const Repo *repo, const uint8_t commit_id[32], Snapshot *out);

/* Decode a tree object recursively into a snapshot, building canonical paths.
 * Used by snap_from_commit. */
CvcStatus snap_from_tree(const Repo *repo, const uint8_t tree_id[32], const char *prefix, Snapshot *out);

/* Compare two snapshots: produce a status result.
 * status entries: 0 none,1 added,2 modified,3 deleted,4 typechanged */
typedef struct {
    char *path;
    int status;           /* 1 added,2 modified,3 deleted,4 typechanged */
    uint8_t old_type, new_type;
    uint8_t old_id[32], new_id[32];
} DiffResult;

typedef struct {
    DiffResult *items;
    size_t len, cap;
} DiffList;

void diff_list_init(DiffList *d);
void diff_list_free(DiffList *d);

/* Compare old vs new snapshots (both sorted). Produces added/modified/
 * deleted/typechanged entries sorted by path. */
CvcStatus snap_compare(const Snapshot *old_s, const Snapshot *new_s, DiffList *out);

#endif
