/* stats.h - document statistics. */
#ifndef MD_STATS_H
#define MD_STATS_H

#include <stddef.h>
#include "md.h"

typedef struct {
    size_t raw_chars;        /* Unicode code points; LF counted once */
    size_t rendered_chars;   /* plain-text code points, Markdown syntax removed */
    size_t word_count;       /* deterministic mixed CJK/Latin token count */
    size_t total_lines;
    size_t nonempty_lines;
    size_t paragraphs;
    size_t headings;
    size_t images;
    size_t links;
    size_t code_blocks;
} md_stats;

/* Compute statistics for source text (optionally pre-parsed into doc).
 * If doc is NULL, it is parsed internally. */
void md_stats_compute(const char *src, size_t len, md_doc *doc, md_stats *out);

/* Word/token count on a UTF-8 plain-text buffer (deterministic rule). */
size_t md_count_words(const char *s, size_t len);

/* Unicode code-point count of a UTF-8 buffer. */
size_t md_count_chars(const char *s, size_t len);

#endif /* MD_STATS_H */
