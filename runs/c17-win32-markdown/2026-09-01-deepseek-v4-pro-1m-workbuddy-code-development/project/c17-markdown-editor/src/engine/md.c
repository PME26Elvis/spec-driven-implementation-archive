/* md.c - Markdown model helpers and utilities. */
#include "md.h"
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"

md_block *md_new_block(int type, size_t start, size_t end){
    md_block *b = ce_calloc(1, sizeof(md_block));
    b->type = type; b->start = start; b->end = end;
    b->task = -1; b->list_ordered = -1;
    return b;
}

md_block *md_add_block(md_doc *d, int type, size_t start, size_t end){
    if(d->nblocks == d->capblocks){
        d->capblocks = d->capblocks ? d->capblocks * 2 : 32;
        d->blocks = ce_realloc(d->blocks, d->capblocks * sizeof(md_block*));
    }
    md_block *b = md_new_block(type, start, end);
    d->blocks[d->nblocks++] = b;
    return b;
}

void md_block_add_inline(md_block *b, md_inline *inl){
    if(b->ninlines == b->capinlines){
        b->capinlines = b->capinlines ? b->capinlines * 2 : 8;
        b->inlines = ce_realloc(b->inlines, b->capinlines * sizeof(md_inline*));
    }
    b->inlines[b->ninlines++] = inl;
}

void md_block_add_child(md_block *b, md_block *child){
    if(b->nchildren == b->capchildren){
        b->capchildren = b->capchildren ? b->capchildren * 2 : 8;
        b->children = ce_realloc(b->children, b->capchildren * sizeof(md_block*));
    }
    b->children[b->nchildren++] = child;
}

/* ---------------------------------------------------------------- plain text */

static void append_inline_text(ce_buf *out, const md_inline *inl){
    switch(inl->type){
        case MD_INL_TEXT: ce_buf_append(out, inl->text ? inl->text : "", inl->text ? strlen(inl->text) : 0); break;
        case MD_INL_CODE: ce_buf_append(out, inl->text ? inl->text : "", inl->text ? strlen(inl->text) : 0); break;
        case MD_INL_EMPH: case MD_INL_STRONG: case MD_INL_STRIKE:
            for(size_t i = 0; i < inl->nchildren; i++) append_inline_text(out, inl->children[i]);
            break;
        case MD_INL_LINK:
            for(size_t i = 0; i < inl->nchildren; i++) append_inline_text(out, inl->children[i]);
            break;
        case MD_INL_IMAGE:
            for(size_t i = 0; i < inl->nchildren; i++) append_inline_text(out, inl->children[i]);
            break;
        case MD_INL_AUTOLINK: ce_buf_append(out, inl->url ? inl->url : "", inl->url ? strlen(inl->url) : 0); break;
        case MD_INL_SOFTBREAK: ce_buf_append_c(out, ' '); break;
        case MD_INL_HARDBREAK: ce_buf_append_c(out, '\n'); break;
        case MD_INL_HTML: /* raw html excluded from plain text */ break;
    }
}

char *md_block_plaintext(const md_block *b){
    ce_buf out; ce_buf_init(&out);
    for(size_t i = 0; i < b->ninlines; i++) append_inline_text(&out, b->inlines[i]);
    return ce_buf_detach(&out);
}

size_t md_collect_headings(const md_doc *d, md_block ***out){
    size_t count = 0;
    for(size_t i = 0; i < d->nblocks; i++) if(d->blocks[i]->type == MD_BLOCK_HEADING) count++;
    *out = ce_malloc((count ? count : 1) * sizeof(md_block*));
    size_t k = 0;
    for(size_t i = 0; i < d->nblocks; i++) if(d->blocks[i]->type == MD_BLOCK_HEADING) (*out)[k++] = d->blocks[i];
    return count;
}

size_t md_block_line_count(const md_doc *d, const md_block *b){
    size_t n = 0;
    for(size_t i = b->start; i < b->end && i < d->len; i++) if(d->src[i] == '\n') n++;
    /* count trailing line without newline */
    if(b->end > b->start && (b->end == d->len || d->src[b->end-1] != '\n')) n++;
    return n;
}

/* ---------------------------------------------------------------- block free */

static void inl_free_rec(md_inline *inl){
    for(size_t i = 0; i < inl->nchildren; i++) inl_free_rec(inl->children[i]);
    if(inl->children) ce_free(inl->children);
    if(inl->text) ce_free(inl->text);
    if(inl->url) ce_free(inl->url);
    if(inl->title) ce_free(inl->title);
    ce_free(inl);
}

void md_block_free(md_block *b){
    if(!b) return;
    for(size_t i = 0; i < b->ninlines; i++) inl_free_rec(b->inlines[i]);
    if(b->inlines) ce_free(b->inlines);
    if(b->info) ce_free(b->info);
    if(b->cols) ce_free(b->cols);
    if(b->cells){
        for(size_t r = 0; r < b->nrows; r++){
            for(size_t c = 0; c < b->ncols; c++) ce_free(b->cells[r][c]);
            ce_free(b->cells[r]);
        }
        ce_free(b->cells);
    }
    if(b->row_src) ce_free(b->row_src);
    for(size_t i = 0; i < b->nchildren; i++) md_block_free(b->children[i]);
    if(b->children) ce_free(b->children);
    ce_free(b);
}
