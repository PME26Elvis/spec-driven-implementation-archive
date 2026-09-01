/* diff.h - Myers line diff with word/token refinement. */
#ifndef MD_DIFF_H
#define MD_DIFF_H

#include <stddef.h>
#include <stdbool.h>

typedef enum { DIFF_EQUAL = 0, DIFF_DEL = 1, DIFF_ADD = 2 } diff_op;

/* A diff hunk over line ranges. */
typedef struct {
    int op;                    /* DIFF_EQUAL / DIFF_DEL / DIFF_ADD */
    bool modified;             /* true for a paired del+add region */
    long a_start, a_count;     /* old-side line range */
    long b_start, b_count;     /* new-side line range */
    /* word-level refinement segments (for modified hunks) */
    struct {
        int op;
        char *text;            /* owned */
    } *words;
    size_t nwords;
} diff_hunk;

/* Line-level diff with word refinement. Returns number of hunks; *out is
 * malloc'd (caller frees each hunk's words/text via md_diff_free). */
size_t md_diff(const char *a, size_t alen, const char *b, size_t blen, diff_hunk **out);
void md_diff_free(diff_hunk *h, size_t n);

/* Raw line edit script (used by version-history deltas). */
typedef struct { int type; long a_idx; long b_idx; } md_edit;
size_t md_diff_script(const char *a, size_t alen, const char *b, size_t blen, md_edit **out);
/* Split text into lines (returns line count, *out spans). */
size_t md_split_lines(const char *s, size_t len, size_t **starts, size_t **lens);

#endif /* MD_DIFF_H */
