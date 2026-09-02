#ifndef TABLETOOL_UTF8_H
#define TABLETOOL_UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Validate entire buffer is well-formed UTF-8 (no overlong, no surrogates, no >U+10FFFF). */
bool utf8_validate(const unsigned char *s, size_t n);

/* Decode next scalar; returns number of bytes consumed (0 on error). *out receives scalar. */
size_t utf8_next(const unsigned char *s, size_t n, uint32_t *out);

/* Count Unicode scalar values. */
size_t utf8_scalar_count(const unsigned char *s, size_t n);

/* Compare two UTF-8 sequences by scalar value order. Returns <0,0,>0. */
int utf8_strcmp(const unsigned char *a, size_t an, const unsigned char *b, size_t bn);

/* ASCII case-insensitive compare for FIND mode: only A-Z fold to a-z. */
int utf8_ascii_insensitive_cmp(const unsigned char *a, size_t an, const unsigned char *b, size_t bn);

/* Find first occurrence of needle as consecutive scalars in haystack. Returns byte offset or (size_t)-1. */
size_t utf8_find(const unsigned char *hay, size_t hay_n, const unsigned char *needle, size_t needle_n, bool ascii_insensitive);

#endif
