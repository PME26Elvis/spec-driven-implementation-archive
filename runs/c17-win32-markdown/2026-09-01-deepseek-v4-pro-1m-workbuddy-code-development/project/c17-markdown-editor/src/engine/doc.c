/* doc.c - live document with undo/redo. */
#include "doc.h"
#include "ce_common.h"

void md_document_init(md_document *d){
    memset(d, 0, sizeof(*d));
    ce_buf_init(&d->source);
    md_txn_init(&d->cur);
    d->cur_open = false;
}

static void clear_undo_stack(md_document *d){
    for(size_t i = 0; i < d->nundo; i++) md_txn_free(d->undo[i]);
    if(d->undo) ce_free(d->undo);
    d->undo = NULL; d->nundo = d->capundo = 0;
}

static void clear_redo_stack(md_document *d){
    for(size_t i = 0; i < d->nredo; i++) md_txn_free(d->redo[i]);
    if(d->redo) ce_free(d->redo);
    d->redo = NULL; d->nredo = d->capredo = 0;
}

void md_document_free(md_document *d){
    if(d->cur_open) md_txn_free(&d->cur);
    clear_undo_stack(d);
    clear_redo_stack(d);
    ce_buf_free(&d->source);
    if(d->path) ce_free(d->path);
    if(d->display_name) ce_free(d->display_name);
    d->path = NULL; d->display_name = NULL;
}

const char *md_document_text(const md_document *d){ return d->source.data ? d->source.data : ""; }
size_t md_document_len(const md_document *d){ return d->source.len; }

void md_document_set_source(md_document *d, const char *text, size_t len){
    if(d->cur_open){ md_txn_free(&d->cur); d->cur_open = false; }
    clear_undo_stack(d);
    clear_redo_stack(d);
    ce_buf_set(&d->source, text, len);
    d->dirty = false;
}

void md_document_edit_begin(md_document *d){
    if(d->cur_open) md_document_edit_end(d); /* commit previous */
    md_txn_init(&d->cur);
    d->cur_open = true;
}

void md_document_edit_op(md_document *d, size_t pos, size_t old_len, const char *new_text, size_t new_len){
    if(!d->cur_open) md_document_edit_begin(d);
    const char *base = md_document_text(d);
    md_txn_add(&d->cur, pos, base + pos, old_len, new_text, new_len);
    /* apply immediately (ops must be added in ascending pos order) */
    ce_buf_erase(&d->source, pos, old_len);
    ce_buf_insert(&d->source, pos, new_text, new_len);
    d->dirty = true;
}

void md_document_edit_end(md_document *d){
    if(!d->cur_open) return;
    d->cur_open = false;
    if(d->cur.nops == 0){ md_txn_free(&d->cur); return; }
    /* push a heap copy; ownership of the ops array transfers */
    md_txn *heap = ce_malloc(sizeof(md_txn));
    *heap = d->cur;
    if(d->nundo == d->capundo){ d->capundo = d->capundo ? d->capundo * 2 : 16; d->undo = ce_realloc(d->undo, d->capundo * sizeof(md_txn*)); }
    d->undo[d->nundo++] = heap;
    md_document_clear_redo(d);
    md_txn_init(&d->cur);
}

void md_document_insert(md_document *d, size_t pos, const char *text, size_t len){
    md_document_edit_begin(d);
    md_document_edit_op(d, pos, 0, text, len);
    md_document_edit_end(d);
}
void md_document_delete(md_document *d, size_t pos, size_t len){
    md_document_edit_begin(d);
    md_document_edit_op(d, pos, len, "", 0);
    md_document_edit_end(d);
}
void md_document_replace(md_document *d, size_t pos, size_t old_len, const char *text, size_t len){
    md_document_edit_begin(d);
    md_document_edit_op(d, pos, old_len, text, len);
    md_document_edit_end(d);
}

bool md_document_can_undo(const md_document *d){ return d->nundo > 0; }
bool md_document_can_redo(const md_document *d){ return d->nredo > 0; }

bool md_document_undo(md_document *d){
    if(d->nundo == 0) return false;
    md_txn *t = d->undo[--d->nundo];
    md_buf_apply_undo(&d->source, t);
    if(d->nredo == d->capredo){ d->capredo = d->capredo ? d->capredo * 2 : 16; d->redo = ce_realloc(d->redo, d->capredo * sizeof(md_txn*)); }
    d->redo[d->nredo++] = t;
    d->dirty = true;
    return true;
}

bool md_document_redo(md_document *d){
    if(d->nredo == 0) return false;
    md_txn *t = d->redo[--d->nredo];
    md_buf_apply(&d->source, t);
    if(d->nundo == d->capundo){ d->capundo = d->capundo ? d->capundo * 2 : 16; d->undo = ce_realloc(d->undo, d->capundo * sizeof(md_txn*)); }
    d->undo[d->nundo++] = t;
    d->dirty = true;
    return true;
}

void md_document_clear_redo(md_document *d){
    clear_redo_stack(d);
}

void md_document_set_clean(md_document *d){ d->dirty = false; }
