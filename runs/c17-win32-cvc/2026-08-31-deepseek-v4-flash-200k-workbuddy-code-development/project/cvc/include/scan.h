#ifndef CVC_SCAN_H
#define CVC_SCAN_H

#include "objects.h"
#include "repo.h"
#include "util.h"

/* A selected leaf in the working-tree snapshot. */
typedef struct {
    char *path;        /* canonical repo-relative path (UTF-8, '/') */
    uint8_t type;      /* OBJ_TREE_BLOB, OBJ_TREE_FILE_SYMLINK, OBJ_TREE_DIR_SYMLINK */
    uint8_t id[32];    /* object id (blob or symlink) */
    uint64_t size;     /* for blobs */
    int is_ignored;    /* ignored (ineligible / excluded / unsupported) */
    int is_binary;     /* ineligible due to NUL/size */
} ScanEntry;

typedef struct {
    ScanEntry *items;
    size_t len;
    size_t cap;
    /* ignored summary counts */
    size_t ignored_binary;
    size_t ignored_unsupported;
    size_t ignored_excluded;
    size_t ignored_nested_repo;
} ScanResult;

void scan_init(ScanResult *r);
void scan_free(ScanResult *r);

/* Build the selected snapshot from the working tree according to repository
 * tracking config + built-in eligibility and safety rules.
 * If `for_write` is set, eligible blobs and symlinks are written to the object
 * store (content-addressed) as they are scanned, so the returned ids are
 * durable and buildable. If not set, only ids are computed in memory.
 * Errors (unrepresentable collisions) return nonzero.
 */
CvcStatus scan_snapshot(Repo *repo, ScanResult *r, int for_write);

/* Is a path selected by the effective tracking filters (including built-in
 * exclusions)? Excludes the root .cvc. Returns 1 selected, 0 not. */
int scan_path_selected(Repo *repo, const char *path);

/* --- status/diff helpers --- */
/* Compare a commit snapshot (tree) with the working-tree snapshot.
 * This is implemented in cli/status using scan_snapshot + tree decode. */

#endif
