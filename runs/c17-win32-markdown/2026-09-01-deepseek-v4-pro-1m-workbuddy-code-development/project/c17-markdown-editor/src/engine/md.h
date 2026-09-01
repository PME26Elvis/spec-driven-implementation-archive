/* md.h - Markdown document model: parser produces a block/inline tree with
 * source byte ranges, consumed by preview rendering, rendered editing, outline,
 * statistics, and source<->render mapping. */
#ifndef MD_H
#define MD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------- inline ---- */

typedef enum {
    MD_INL_TEXT = 1,
    MD_INL_EMPH,
    MD_INL_STRONG,
    MD_INL_STRIKE,
    MD_INL_CODE,
    MD_INL_LINK,
    MD_INL_IMAGE,
    MD_INL_AUTOLINK,
    MD_INL_SOFTBREAK,
    MD_INL_HARDBREAK,
    MD_INL_HTML
} md_inline_type;

typedef struct md_inline {
    int type;
    size_t start, end;           /* source byte range of whole construct */
    size_t cstart, cend;         /* inner/content source range (text payload) */
    char *text;                  /* decoded text payload (owned) */
    char *url;                   /* link/image destination (owned, may be NULL) */
    char *title;                 /* link/image title (owned, may be NULL) */
    struct md_inline **children; /* nested inlines (emph/strong/link) */
    size_t nchildren, capchildren;
} md_inline;

/* ------------------------------------------------------------- block ---- */

typedef enum {
    MD_BLOCK_PARAGRAPH = 1,
    MD_BLOCK_HEADING,
    MD_BLOCK_THEMATIC_BREAK,
    MD_BLOCK_BLOCKQUOTE,
    MD_BLOCK_LIST,
    MD_BLOCK_LIST_ITEM,
    MD_BLOCK_CODE,
    MD_BLOCK_TABLE,
    MD_BLOCK_HTML
} md_block_type;

typedef struct {
    int align;                  /* -1 none, 0 left, 1 center, 2 right */
} md_col;

typedef struct md_block {
    int type;
    size_t start, end;          /* source byte range of the block */
    int level;                  /* heading level 1..6 */
    int fence_char;             /* '`' or '~' for code blocks */
    int fence_len;              /* number of fence chars */
    char *info;                 /* code info string (owned, may be NULL) */
    int list_ordered;           /* for list / list_item */
    int list_start;             /* ordered list start number */
    char list_marker;           /* '-', '+', '*' or digit for items */
    int task;                   /* -1 none, 0 unchecked, 1 checked (list item) */
    struct md_inline **inlines; /* inline content (paragraph/heading/list item) */
    size_t ninlines, capinlines;

    /* table */
    md_col *cols; size_t ncols;
    char ***cells;              /* [row][col] raw cell text (owned) */
    size_t *row_src;            /* source start of each row line (for mapping) */
    size_t nrows;
    int header_row_present;

    struct md_block **children; /* blockquote / list contain nested blocks */
    size_t nchildren, capchildren;

    /* rendering layout (filled by renderer, not parser) */
    int visible;
} md_block;

/* ------------------------------------------------------------- document -- */

typedef struct {
    char *src;                  /* owned copy of source (UTF-8, NUL-terminated) */
    size_t len;
    md_block **blocks;
    size_t nblocks, capblocks;
} md_doc;

/* Parse a UTF-8 Markdown buffer into a document model. Returns NULL only on
 * allocation failure. Malformed Markdown is tolerated (best-effort blocks). */
md_doc *md_parse(const char *src, size_t len);
void md_free(md_doc *d);

/* ---- helpers ---- */
md_block *md_add_block(md_doc *d, int type, size_t start, size_t end);

/* Create a block without appending it to d->blocks (for container children). */
md_block *md_new_block(int type, size_t start, size_t end);

void md_block_add_inline(md_block *b, md_inline *inl);
void md_block_add_child(md_block *b, md_block *child);

/* Recursively free a block tree (used internally; also useful for callers). */
void md_block_free(md_block *b);

/* Extract the plain text of a block's inlines (Markdown syntax removed).
 * Returns malloc'd NUL-terminated string. */
char *md_block_plaintext(const md_block *b);

/* Find the heading list for outline. Returns count and fills array. */
size_t md_collect_headings(const md_doc *d, md_block ***out);

/* Number of a block's source lines. */
size_t md_block_line_count(const md_doc *d, const md_block *b);

#endif /* MD_H */
