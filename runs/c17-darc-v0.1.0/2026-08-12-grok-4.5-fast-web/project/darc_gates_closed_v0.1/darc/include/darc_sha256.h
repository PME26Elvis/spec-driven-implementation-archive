#ifndef DARC_SHA256_H
#define DARC_SHA256_H

#include <stdint.h>
#include <stddef.h>

#define DARC_SHA256_DIGEST_SIZE 32
#define DARC_SHA256_BLOCK_SIZE 64

typedef struct {
    uint64_t bitlen;
    uint32_t state[8];
    uint8_t buffer[DARC_SHA256_BLOCK_SIZE];
    size_t buffer_len;
} darc_sha256_ctx;

void darc_sha256_init(darc_sha256_ctx *ctx);
void darc_sha256_update(darc_sha256_ctx *ctx, const void *data, size_t len);
void darc_sha256_final(darc_sha256_ctx *ctx, uint8_t out[DARC_SHA256_DIGEST_SIZE]);
void darc_sha256(const void *data, size_t len, uint8_t out[DARC_SHA256_DIGEST_SIZE]);

/* Hex encode 32-byte digest to 64-char null-terminated string (lowercase) */
void darc_sha256_hex(const uint8_t digest[DARC_SHA256_DIGEST_SIZE], char out[65]);

#endif
