#ifndef CVC_UTF8_H
#define CVC_UTF8_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Validate a UTF-8 byte sequence (returns 0 valid, -1 invalid). */
int utf8_validate(const uint8_t *s, size_t len);

/* Decode one UTF-8 scalar starting at s[0..len). Returns the number of
 * bytes consumed (1..4) on success, or 0 on invalid input. If out is
 * nonnull, stores the code point (may be a surrogate code point that the
 * caller must reject at the Windows boundary). */
int utf8_decode(const uint8_t *s, size_t len, uint32_t *out);

/* Encode one scalar (U+0000..U+10FFFF, surrogates rejected) into dst.
 * Returns bytes written (1..4) or 0 if scalar invalid. dst must hold 4. */
int utf8_encode(uint32_t cp, uint8_t dst[4]);

/* UTF-16 -> UTF-8. src in units of UTF-16 code units (u16). Returns a
 * heap-allocated NUL-terminated UTF-8 string, or NULL on invalid
 * input/unpaired surrogate or OOM. `out_len` optional receives byte length. */
char *utf8_from_utf16(const uint16_t *src, size_t src_units, size_t *out_len);

/* UTF-8 -> UTF-16. Returns heap-allocated UTF-16 array (NUL-terminated),
 * or NULL on invalid UTF-8 / unpaired surrogate / OOM.
 * `out_units` optional receives count including the trailing NUL. */
uint16_t *utf8_to_utf16(const char *src, size_t src_bytes, size_t *out_units);

/* Append a single UTF-16 code unit, handling surrogate pairs for non-BMP.
 * Used by JSON \u escape handling. Returns new count or (size_t)-1 on invalid. */
int utf16_push_utf8(uint8_t **buf, size_t *len, size_t *cap, uint32_t cp);

/* Whether s (len bytes) is valid UTF-8 with no embedded NUL. */
int utf8_valid_no_nul(const uint8_t *s, size_t len);

/* Windows ordinal case-insensitive comparison of two UTF-8 strings.
 * Returns 0 if they collide under Windows ordinal case-insensitive rules,
 * nonzero otherwise. Uses ASCII case folding only (ordinal). */
int utf8_ordinal_case_equal(const char *a, const char *b);

#endif
