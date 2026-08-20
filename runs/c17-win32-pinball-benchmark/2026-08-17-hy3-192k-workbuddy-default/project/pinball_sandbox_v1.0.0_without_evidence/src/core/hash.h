#ifndef PB_HASH_H
#define PB_HASH_H

#include <stdint.h>
#include <stddef.h>

/* FNV-1a 64-bit (doc 17.15 / doc 07.4). */
uint64_t fnv1a64(const void *data, size_t len);
uint64_t fnv1a64_str(const char *s);
/* Incremental variant. */
void     fnv1a64_init(uint64_t *h);
void     fnv1a64_update(uint64_t *h, const void *data, size_t len);
uint64_t fnv1a64_final(uint64_t h);

/* Hex string (16 chars) for a 64-bit hash. Writes 17 bytes into out. */
void     fnv1a64_to_hex(uint64_t h, char *out);

#endif
