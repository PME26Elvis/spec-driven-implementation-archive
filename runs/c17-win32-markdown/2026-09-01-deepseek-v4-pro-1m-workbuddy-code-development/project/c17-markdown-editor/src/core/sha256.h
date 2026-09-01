/* sha256.h - authored SHA-256 (FIPS 180-4). */
#ifndef CE_SHA256_H
#define CE_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;      /* total bytes processed */
    uint8_t  buf[64];
    size_t   buflen;
} ce_sha256;

void ce_sha256_init(ce_sha256 *c);
void ce_sha256_update(ce_sha256 *c, const void *data, size_t n);
void ce_sha256_final(ce_sha256 *c, uint8_t out[32]);

/* Convenience: hash a buffer. */
void ce_sha256_hash(const void *data, size_t n, uint8_t out[32]);

/* Convenience: hash a NUL-terminated string, hex output (65 bytes). */
void ce_sha256_hex(const void *data, size_t n, char out[65]);

#endif /* CE_SHA256_H */
