/* undo.h - document edit transactions and undo/redo. */
#ifndef MD_UNDO_H
#define MD_UNDO_H

#include <stddef.h>
#include "buf.h"

/* A single replacement carrying both old and new text so undo/redo are
 * symmetric: source[pos .. pos+old_len) becomes new_text. */
typedef struct {
    size_t pos;
    char *old_text; size_t old_len;   /* original content (owned) */
    char *new_text; size_t new_len;   /* replacement (owned) */
} md_edit_op;

/* A transaction is an ordered (ascending, non-overlapping) list of ops. */
typedef struct {
    md_edit_op *ops;
    size_t nops, cap;
} md_txn;

void md_txn_init(md_txn *t);
void md_txn_free(md_txn *t);
void md_txn_add(md_txn *t, size_t pos, const char *old_text, size_t old_len,
                const char *new_text, size_t new_len);

/* Forward apply (redo). Ops are applied high-pos first. */
void md_buf_apply(ce_buf *b, const md_txn *t);
/* Undo apply (reverse). */
void md_buf_apply_undo(ce_buf *b, const md_txn *t);

#endif /* MD_UNDO_H */
