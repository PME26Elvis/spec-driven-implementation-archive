/* search.h - in-document literal find (UTF-8 safe, case-insensitive ASCII,
 * whole-word, non-overlapping). */
#ifndef MD_SEARCH_H
#define MD_SEARCH_H

#include <stddef.h>
#include <stdbool.h>

typedef struct { size_t pos; size_t len; } md_match;

/* Find all non-overlapping matches. Returns count; *out is malloc'd (caller frees).
 * Matches are always at UTF-8 character boundaries. */
size_t md_find_all(const char *src, size_t len, const char *needle, size_t nlen,
                   bool case_sensitive, bool whole_word, md_match **out);

/* Find the first match at/after `from`. Returns -1 if none. */
long md_find_next(const char *src, size_t len, const char *needle, size_t nlen,
                  bool case_sensitive, bool whole_word, size_t from);

/* Find the last match at/before `from`. Returns -1 if none. */
long md_find_prev(const char *src, size_t len, const char *needle, size_t nlen,
                  bool case_sensitive, bool whole_word, size_t from);

/* 1 if the code point at src[pos] is a word character (ASCII alnum/underscore
 * or any non-ASCII code point). */
bool md_is_word_char(const char *src, size_t len, size_t pos);

#endif /* MD_SEARCH_H */
