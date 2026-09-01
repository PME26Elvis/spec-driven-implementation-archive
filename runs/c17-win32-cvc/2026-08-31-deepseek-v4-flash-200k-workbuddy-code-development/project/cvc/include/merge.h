#ifndef CVC_MERGE_H
#define CVC_MERGE_H

#include "repo.h"
#include "snapshot.h"
#include "objects.h"
#include "util.h"

/* Merge state phases */
enum { MERGE_PHASE_CONFLICT = 0, MERGE_PHASE_FINALIZING = 1 };

/* A conflict path record. */
typedef struct {
    char *path;             /* exact conflict root */
    int resolved;           /* 1 if resolved */
    /* stored resolution snapshot (from the most recent resolve) */
    Snapshot resolution;    /* leaves beneath this root, or empty for "delete" */
    int has_resolution;     /* whether resolution recorded (0 = still unresolved) */
} ConflictEntry;

typedef struct {
    char *orig_branch;      /* original current branch name */
    uint8_t orig_commit[32];/* original pre-merge HEAD commit */
    char *target_branch;    /* target branch name (descriptive) */
    uint8_t target_commit[32];
    char *message;          /* intended merge message */
    /* provisional nonconflicting merge result (full snapshot leaves) */
    Snapshot provisional;
    /* conflict roots */
    ConflictEntry *conflicts;
    size_t n_conflicts;
    int phase;              /* CONFLICT or FINALIZING */
    int finalizing_has_id;  /* whether intended merge commit id recorded */
    uint8_t finalizing_commit[32];
} MergeState;

void merge_state_init(MergeState *ms);
void merge_state_free(MergeState *ms);

/* Read merge state from .cvc/state/merge. Returns CVC_OK if present,
 * CVC_ERR if absent, or integrity error. */
CvcStatus merge_state_load(const Repo *repo, MergeState *ms);
/* Write merge state. Returns CVC_OK. */
CvcStatus merge_state_save(const Repo *repo, const MergeState *ms);
/* Remove merge state. */
CvcStatus merge_state_remove(const Repo *repo);

/* ---- pure algorithms (called from cli.c; no working-tree side effects) -- */

/* Compute best common ancestor of two commit ids. Returns 0 and sets out if a
 * single best exists (after lexicographic tie-break), 1 if no common ancestor,
 * -1 on repository error. */
int cvc_merge_base(const Repo *repo, const uint8_t a[32], const uint8_t b[32],
                   uint8_t out[32]);

/* A text-conflict detail: the conflict root was a regular file changed on both
 * sides (eligible text), so the working tree should carry conflict markers
 * between ours_id and theirs_id. */
typedef struct {
    char *path;
    uint8_t ours_id[32];
    uint8_t theirs_id[32];
    /* whether the merged outcome must keep "ours" because merged result was
     * ineligible (too large / binary). (Reserved; always 0 for now.) */
} TextConflict;

/* Result of a three-way tree+text merge. `provisional` holds merged leaves
 * for non-conflicting paths. `conflicts` holds the minimal conflict roots
 * (sorted). `textual` lists which of those roots are eligible text conflicts
 * (blob vs blob) with the two sides' blob ids, so the caller can materialize
 * conflict markers. Case collisions set case_collision. */
typedef struct {
    Snapshot provisional;
    StrVec conflicts;       /* sorted, deduped, minimal roots */
    TextConflict *textual;  /* parallel subset info */
    size_t n_textual;
    int case_collision;
    char collision_path_a[512];
    char collision_path_b[512];
} ThreeWayResult;

void threeway_result_init(ThreeWayResult *r);
void threeway_result_free(ThreeWayResult *r);

/* Perform a three-way merge of three commit trees (base/ours/theirs).
 * `repo` is used to read objects. Returns CVC_OK and fills *out.
 * On repository corruption returns an error. No working-tree mutation. */
CvcStatus cvc_merge_threeway(const Repo *repo,
                             const uint8_t base[32],
                             const uint8_t ours[32],
                             const uint8_t theirs[32],
                             ThreeWayResult *out);

/* Determine whether `a` is an ancestor of `b` (a reachable by walking
 * parents from b). Returns 1/0/-1. */
int cvc_is_ancestor(const Repo *repo, const uint8_t a[32], const uint8_t b[32]);

#endif
