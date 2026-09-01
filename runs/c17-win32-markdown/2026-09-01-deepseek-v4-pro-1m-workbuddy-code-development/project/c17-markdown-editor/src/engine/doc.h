/* doc.h - live document: UTF-8 source buffer, path, dirty state, undo/redo. */
#ifndef MD_DOC_H
#define MD_DOC_H

#include <stddef.h>
#include <stdbool.h>
#include "buf.h"
#include "undo.h"

typedef struct md_document {
    ce_buf source;          /* UTF-8 source */
    char *path;             /* absolute path, or NULL if untitled */
    bool dirty;

    md_txn **undo; size_t nundo, capundo;
    md_txn **redo; size_t nredo, capredo;

    /* in-progress composite transaction */
    md_txn cur;
    bool cur_open;

    /* app-owned attachments */
    void *app;              /* e.g. history handle, editor state */
    char *display_name;     /* cached basename or "Untitled N" */
} md_document;

void md_document_init(md_document *d);
void md_document_free(md_document *d);

const char *md_document_text(const md_document *d);
size_t md_document_len(const md_document *d);

/* Replace the entire source (used on load/restore). Clears undo/redo. */
void md_document_set_source(md_document *d, const char *text, size_t len);

/* ---- editing (transactional) ---- */
void md_document_edit_begin(md_document *d);
/* Record and immediately apply one replacement. pos/old_len relative to current buffer. */
void md_document_edit_op(md_document *d, size_t pos, size_t old_len, const char *new_text, size_t new_len);
/* Commit the open transaction onto the undo stack (clears redo). */
void md_document_edit_end(md_document *d);

/* Single-op convenience. */
void md_document_insert(md_document *d, size_t pos, const char *text, size_t len);
void md_document_delete(md_document *d, size_t pos, size_t len);
void md_document_replace(md_document *d, size_t pos, size_t old_len, const char *text, size_t len);

bool md_document_can_undo(const md_document *d);
bool md_document_can_redo(const md_document *d);
bool md_document_undo(md_document *d);
bool md_document_redo(md_document *d);

/* Drop redo history without changing content (after a new edit this is automatic). */
void md_document_clear_redo(md_document *d);

/* Clear dirty (after successful save) without touching undo history. */
void md_document_set_clean(md_document *d);

#endif /* MD_DOC_H */
