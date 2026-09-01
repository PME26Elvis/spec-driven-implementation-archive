/* utf8.h - UTF-8 decoding/encoding and user-visible editing-unit (grapheme)
 * helpers. All byte offsets are relative to the start of the buffer. */
#ifndef CE_UTF8_H
#define CE_UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Decode one Unicode scalar starting at s[pos]. On success returns 0, sets
 * *cp and *consumed (1..4). On invalid/truncated input returns -1. */
int ce_utf8_decode(const uint8_t *s, size_t len, size_t pos, uint32_t *cp, size_t *consumed);

/* Encode cp into out (>=4 bytes). Returns byte length. */
int ce_utf8_encode(uint32_t cp, uint8_t *out);

/* 1 if the byte sequence is well-formed UTF-8 (no overlongs, surrogates, >U+10FFFF). */
int ce_utf8_valid(const uint8_t *s, size_t len);

/* Byte offset of the start of the previous code point before pos. */
size_t ce_utf8_prev(const uint8_t *s, size_t pos);

/* Byte offset of the start of the next code point at/after pos. */
size_t ce_utf8_next(const uint8_t *s, size_t len, size_t pos);

/* Number of Unicode code points in [0,len). */
size_t ce_utf8_count(const uint8_t *s, size_t len);

/* ------- user-visible editing units (grapheme clusters) ------- */

/* Byte offset of the start of the grapheme cluster containing pos, and the
 * offset of the next cluster. For a caret that sits at pos, "one unit left"
 * is grapheme_prev, "one unit right" is grapheme_next. */
size_t ce_grapheme_next(const uint8_t *s, size_t len, size_t pos);
size_t ce_grapheme_prev(const uint8_t *s, size_t pos);

/* Number of grapheme clusters in [0,len). */
size_t ce_grapheme_count(const uint8_t *s, size_t len);

/* 1 if cp is a combining mark (Extend) or variation selector. */
bool ce_is_extend(uint32_t cp);
bool ce_is_variation_selector(uint32_t cp);
bool ce_is_emoji(uint32_t cp);
bool ce_is_zwj(uint32_t cp);

#endif /* CE_UTF8_H */
