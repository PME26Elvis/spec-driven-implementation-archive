#ifndef CVC_SHA256_H
#define CVC_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buffer[64];
    size_t buflen;
} Sha256;

void sha256_init(Sha256 *s);
void sha256_update(Sha256 *s, const void *data, size_t len);
void sha256_final(Sha256 *s, uint8_t out[32]);

/* One-shot convenience: hash `len` bytes of `data` into out[32]. */
void sha256_one(const void *data, size_t len, uint8_t out[32]);

/* Format 32 raw bytes as 64 lowercase hex chars into out[65]. */
void sha256_to_hex(const uint8_t digest[32], char out[65]);

/* Parse 64 hex chars into 32 bytes. Returns 0 on success, -1 on invalid. */
int sha256_from_hex(const char *hex64, uint8_t out[32]);

/* Whether a string consists of exactly 64 hex chars [0-9a-fA-F]. */
int sha256_is_hex64(const char *s);

#endif
