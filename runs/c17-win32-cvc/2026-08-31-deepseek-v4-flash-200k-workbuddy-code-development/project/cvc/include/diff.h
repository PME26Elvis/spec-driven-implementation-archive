#ifndef CVC_DIFF_H
#define CVC_DIFF_H

#include <stddef.h>
#include <stdint.h>

/* Split a byte buffer into lines. A line includes its trailing '\n'
 * except possibly the last. Returns array of (offset,length) into the
 * source buffer. line_offsets/line_lengths are heap arrays; *count set.
 * Caller frees both. Returns 0 on success, -1 on OOM. */
int diff_split_lines(const uint8_t *data, size_t len,
                     size_t **line_offsets, size_t **line_lengths, size_t *count);

/* An edit operation (0 = keep, 1 = delete old, 2 = insert new). */
typedef struct {
    int op;              /* 0 keep, 1 delete, 2 insert */
    size_t old_line;     /* 0-based old line index */
    size_t new_line;     /* 0-based new line index */
    size_t count;        /* number of consecutive lines */
} DiffEdit;

/* Compute Myers shortest edit script between two line sets.
 *   old_lines/old_offsets/old_lens : old line content (each line is
 *     bytes [off, off+len) in old_data)
 *   new_... analogous for new.
 * Produces an ordered list of DiffEdit that reconstructs old->new.
 * Returns 0 on success, -1 on allocation failure. Caller frees *edits. */
int diff_myers(const uint8_t *old_data, const size_t *old_off, const size_t *old_len, size_t old_n,
               const uint8_t *new_data, const size_t *new_off, const size_t *new_len, size_t new_n,
               DiffEdit **edits, size_t *n_edits);

/* Count line insertions and deletions from an edit script. */
void diff_count(const DiffEdit *edits, size_t n, size_t *ins, size_t *del);

/* Byte-safe render a single line payload for human output.
 * Returns a heap NUL-terminated UTF-8 string (never NULL). See spec 05 4.1. */
char *diff_render_line(const uint8_t *line, size_t len);

#endif
