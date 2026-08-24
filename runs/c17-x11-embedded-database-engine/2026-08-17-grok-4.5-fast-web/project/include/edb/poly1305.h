#ifndef EDB_POLY1305_H
#define EDB_POLY1305_H

#include <stdint.h>
#include <stddef.h>

typedef struct edb_poly1305_ctx {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    uint8_t  buffer[16];
    size_t   leftover;
    uint8_t  final;
} edb_poly1305_ctx;

void edb_poly1305_init(edb_poly1305_ctx *ctx, const uint8_t key[32]);
void edb_poly1305_update(edb_poly1305_ctx *ctx, const uint8_t *m, size_t bytes);
void edb_poly1305_final(edb_poly1305_ctx *ctx, uint8_t tag[16]);

/* One-shot */
void edb_poly1305(const uint8_t key[32], const uint8_t *m, size_t bytes, uint8_t tag[16]);

#endif
