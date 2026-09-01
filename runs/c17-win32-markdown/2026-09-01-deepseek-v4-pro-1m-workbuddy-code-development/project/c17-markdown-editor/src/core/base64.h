/* base64.h - authored Base64 codec. */
#ifndef CE_BASE64_H
#define CE_BASE64_H

#include <stddef.h>

/* Encode n input bytes. Returns malloc'd NUL-terminated string (caller frees). */
char *ce_base64_encode(const unsigned char *data, size_t n);

/* Decode a base64 string (strict: rejects invalid alphabet and padding).
 * On success returns 0 and sets *out and *out_len (malloc'd, caller frees).
 * Returns -1 on malformed input. */
int ce_base64_decode(const char *s, size_t slen, unsigned char **out, size_t *out_len);

#endif /* CE_BASE64_H */
