#ifndef EDB_SHA256_H
#define EDB_SHA256_H

#include <stdint.h>
#include <stddef.h>

typedef struct edb_sha256_ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    size_t   datalen;
} edb_sha256_ctx;

void edb_sha256_init(edb_sha256_ctx *ctx);
void edb_sha256_update(edb_sha256_ctx *ctx, const uint8_t *data, size_t len);
void edb_sha256_final(edb_sha256_ctx *ctx, uint8_t hash[32]);

/* One-shot */
void edb_sha256(const uint8_t *data, size_t len, uint8_t hash[32]);

#endif
