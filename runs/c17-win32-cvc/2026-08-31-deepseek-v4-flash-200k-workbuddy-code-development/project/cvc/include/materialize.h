#ifndef CVC_MATERIALIZE_H
#define CVC_MATERIALIZE_H

#include "repo.h"
#include "snapshot.h"
#include "objects.h"

/* Preflight a materialization to `target` snapshot given the current
 * working tree and the current `current` snapshot (the tracked state that
 * we are allowed to replace). 
 *   replace_tracked: if 1, paths tracked in `current` may be overwritten/
 *     deleted. If 0 (restore), only exact-path replace of tracked is allowed
 *     and anything else that is untracked is a collision.
 * Returns CVC_OK if safe, or error describing the first collision.
 * Also flags whether target contains case collisions.
 */
CvcStatus mat_preflight(Repo *repo, const Snapshot *current, const Snapshot *target,
                        int replace_tracked);

/* Materialize the working tree to exactly equal `target`.
 * Assumes preflight already passed. `current` is the tracked snapshot that
 * exists now (paths in current not in target get removed; paths in target
 * not in current get created; differing paths replaced).
 * On detected runtime failure, restores the pre-command tracked working-tree
 * state and returns error.
 * Note: caller holds the exclusive repo lock and controls ref movement.
 * Returns CVC_OK on success.
 */
CvcStatus mat_materialize(Repo *repo, const Snapshot *current, const Snapshot *target);

#endif
