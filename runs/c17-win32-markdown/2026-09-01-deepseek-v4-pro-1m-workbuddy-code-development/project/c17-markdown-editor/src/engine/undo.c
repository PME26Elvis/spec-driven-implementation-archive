/* undo.c - edit transactions. */
#include "undo.h"
#include "ce_common.h"

void md_txn_init(md_txn *t){ t->ops = NULL; t->nops = t->cap = 0; }

void md_txn_free(md_txn *t){
    for(size_t i = 0; i < t->nops; i++){
        ce_free(t->ops[i].old_text);
        ce_free(t->ops[i].new_text);
    }
    if(t->ops) ce_free(t->ops);
    t->ops = NULL; t->nops = t->cap = 0;
}

void md_txn_add(md_txn *t, size_t pos, const char *old_text, size_t old_len,
                const char *new_text, size_t new_len){
    if(t->nops == t->cap){ t->cap = t->cap ? t->cap * 2 : 8; t->ops = ce_realloc(t->ops, t->cap * sizeof(md_edit_op)); }
    md_edit_op *op = &t->ops[t->nops++];
    op->pos = pos;
    op->old_text = ce_strndup(old_text, old_len);
    op->old_len = old_len;
    op->new_text = ce_strndup(new_text, new_len);
    op->new_len = new_len;
}

/* Forward apply: high-pos first. */
void md_buf_apply(ce_buf *b, const md_txn *t){
    for(size_t i = t->nops; i > 0; i--){
        const md_edit_op *op = &t->ops[i-1];
        ce_buf_erase(b, op->pos, op->old_len);
        ce_buf_insert(b, op->pos, op->new_text, op->new_len);
    }
}

/* Undo apply: low-pos first. */
void md_buf_apply_undo(ce_buf *b, const md_txn *t){
    for(size_t i = 0; i < t->nops; i++){
        const md_edit_op *op = &t->ops[i];
        ce_buf_erase(b, op->pos, op->new_len);
        ce_buf_insert(b, op->pos, op->old_text, op->old_len);
    }
}
