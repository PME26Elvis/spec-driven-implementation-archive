/* sdk_poly1305.c - Poly1305 over 26-bit limbs.
 *
 * The accumulator h and the clamped key r are held as five 26-bit limbs so
 * that a 130-bit value fits in 32-bit words and every multiply-accumulate step
 * fits in a 64-bit product.  Reduction modulo 2^130 - 5 folds the carry out of
 * limb 4 back into limb 0 multiplied by 5.
 */
#include "crypto/sdk_poly1305.h"
#include "common/sdk_common.h"

#include <string.h>

static uint32_t ld32(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void st32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

void sdk_poly1305_init(sdk_poly1305_ctx *ctx,
                       const unsigned char key[SDK_POLY1305_KEY_LEN]) {
    /* r is clamped per RFC 8439: the top four bits of bytes 3, 7, 11, 15 are
     * cleared and the bottom two bits of bytes 4, 8, 12 are cleared. */
    ctx->r[0] = (ld32(key + 0)) & 0x3ffffffu;
    ctx->r[1] = (ld32(key + 3) >> 2) & 0x3ffff03u;
    ctx->r[2] = (ld32(key + 6) >> 4) & 0x3ffc0ffu;
    ctx->r[3] = (ld32(key + 9) >> 6) & 0x3f03fffu;
    ctx->r[4] = (ld32(key + 12) >> 8) & 0x00fffffu;

    ctx->h[0] = 0u;
    ctx->h[1] = 0u;
    ctx->h[2] = 0u;
    ctx->h[3] = 0u;
    ctx->h[4] = 0u;

    ctx->pad[0] = ld32(key + 16);
    ctx->pad[1] = ld32(key + 20);
    ctx->pad[2] = ld32(key + 24);
    ctx->pad[3] = ld32(key + 28);

    ctx->leftover = 0;
    memset(ctx->buffer, 0, sizeof ctx->buffer);
    ctx->final_block = 0;
}

static void poly1305_blocks(sdk_poly1305_ctx *ctx,
                            const unsigned char *m, size_t bytes) {
    const uint32_t hibit = ctx->final_block ? 0u : (1u << 24);
    uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2];
    uint32_t r3 = ctx->r[3], r4 = ctx->r[4];
    uint32_t s1 = r1 * 5u, s2 = r2 * 5u, s3 = r3 * 5u, s4 = r4 * 5u;
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2];
    uint32_t h3 = ctx->h[3], h4 = ctx->h[4];

    while (bytes >= 16) {
        uint64_t d0, d1, d2, d3, d4;
        uint32_t c;

        /* h += m, where m is the little-endian 128-bit block plus the high
         * bit that distinguishes a full block from the padded final block. */
        h0 += (ld32(m + 0)) & 0x3ffffffu;
        h1 += (uint32_t)((((uint64_t)ld32(m + 3)) >> 2) & 0x3ffffffu);
        h2 += (uint32_t)((((uint64_t)ld32(m + 6)) >> 4) & 0x3ffffffu);
        h3 += (uint32_t)((((uint64_t)ld32(m + 9)) >> 6) & 0x3ffffffu);
        h4 += (ld32(m + 12) >> 8) | hibit;

        /* h *= r  (mod 2^130 - 5) */
        d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3
           + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4
           + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0
           + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1
           + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2
           + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffffu;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffffu;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffffu;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffffu;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffffu;
        h0 += c * 5u; c = (h0 >> 26); h0 &= 0x3ffffffu;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    ctx->h[0] = h0;
    ctx->h[1] = h1;
    ctx->h[2] = h2;
    ctx->h[3] = h3;
    ctx->h[4] = h4;
}

void sdk_poly1305_update(sdk_poly1305_ctx *ctx,
                         const unsigned char *data, size_t len) {
    if (len == 0) {
        return;
    }

    if (ctx->leftover > 0) {
        size_t want = 16u - ctx->leftover;
        if (want > len) {
            want = len;
        }
        memcpy(ctx->buffer + ctx->leftover, data, want);
        ctx->leftover += want;
        data += want;
        len -= want;
        if (ctx->leftover < 16u) {
            return;
        }
        poly1305_blocks(ctx, ctx->buffer, 16u);
        ctx->leftover = 0;
    }

    if (len >= 16u) {
        size_t whole = len & ~(size_t)15u;
        poly1305_blocks(ctx, data, whole);
        data += whole;
        len -= whole;
    }

    if (len > 0) {
        memcpy(ctx->buffer + ctx->leftover, data, len);
        ctx->leftover += len;
    }
}

void sdk_poly1305_final(sdk_poly1305_ctx *ctx,
                        unsigned char tag[SDK_POLY1305_TAG_LEN]) {
    uint32_t h0, h1, h2, h3, h4, c;
    uint32_t g0, g1, g2, g3, g4;
    uint32_t mask;
    uint64_t f;

    if (ctx->leftover > 0) {
        size_t i = ctx->leftover;
        ctx->buffer[i++] = 1u;
        while (i < 16u) {
            ctx->buffer[i++] = 0u;
        }
        ctx->final_block = 1u;
        poly1305_blocks(ctx, ctx->buffer, 16u);
    }

    h0 = ctx->h[0]; h1 = ctx->h[1]; h2 = ctx->h[2];
    h3 = ctx->h[3]; h4 = ctx->h[4];

    /* Fully carry h. */
    c = h1 >> 26; h1 &= 0x3ffffffu;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffffu;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffffu;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffffu;
    h0 += c * 5u; c = h0 >> 26; h0 &= 0x3ffffffu;
    h1 += c;

    /* Compute h + -p, i.e. h + 5 in the 130-bit representation. */
    g0 = h0 + 5u; c = g0 >> 26; g0 &= 0x3ffffffu;
    g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffffu;
    g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffffu;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffffu;
    g4 = h4 + c - (1u << 26);

    /* Branch-free select: use g when h >= p, otherwise keep h. */
    mask = (g4 >> 31) - 1u;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    /* h modulo 2^128, packed into four 32-bit words. */
    h0 = (h0 | (h1 << 26));
    h1 = ((h1 >> 6) | (h2 << 20));
    h2 = ((h2 >> 12) | (h3 << 14));
    h3 = ((h3 >> 18) | (h4 << 8));

    /* tag = (h + pad) modulo 2^128 */
    f = (uint64_t)h0 + ctx->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

    st32(tag + 0, h0);
    st32(tag + 4, h1);
    st32(tag + 8, h2);
    st32(tag + 12, h3);

    sdk_secure_wipe(ctx, sizeof *ctx);
}

void sdk_poly1305(const unsigned char key[SDK_POLY1305_KEY_LEN],
                  const unsigned char *data, size_t len,
                  unsigned char tag[SDK_POLY1305_TAG_LEN]) {
    sdk_poly1305_ctx ctx;
    sdk_poly1305_init(&ctx, key);
    sdk_poly1305_update(&ctx, data, len);
    sdk_poly1305_final(&ctx, tag);
}
