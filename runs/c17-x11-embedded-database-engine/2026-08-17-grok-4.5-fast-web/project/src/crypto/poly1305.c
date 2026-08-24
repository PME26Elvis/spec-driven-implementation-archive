#include "edb/poly1305.h"
#include <string.h>

/* Portable 32-bit Poly1305 implementation (project-authored). */

static uint32_t U8TO32_LE(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static void U32TO8_LE(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v>>8);
    p[2] = (uint8_t)(v>>16);
    p[3] = (uint8_t)(v>>24);
}

void edb_poly1305_init(edb_poly1305_ctx *ctx, const uint8_t key[32]) {
    /* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
    ctx->r[0] = (U8TO32_LE(key+0)) & 0x3ffffffu;
    ctx->r[1] = (U8TO32_LE(key+3) >> 2) & 0x3ffff03u;
    ctx->r[2] = (U8TO32_LE(key+6) >> 4) & 0x3ffc0ffu;
    ctx->r[3] = (U8TO32_LE(key+9) >> 6) & 0x3f03fffu;
    ctx->r[4] = (U8TO32_LE(key+12) >> 8) & 0x00fffffu;

    ctx->h[0] = ctx->h[1] = ctx->h[2] = ctx->h[3] = ctx->h[4] = 0;
    ctx->pad[0] = U8TO32_LE(key+16);
    ctx->pad[1] = U8TO32_LE(key+20);
    ctx->pad[2] = U8TO32_LE(key+24);
    ctx->pad[3] = U8TO32_LE(key+28);

    ctx->leftover = 0;
    ctx->final = 0;
}

static void poly1305_blocks(edb_poly1305_ctx *ctx, const uint8_t *m, size_t bytes) {
    const uint32_t hibit = ctx->final ? 0 : (1u << 24);
    uint32_t r0 = ctx->r[0], r1 = ctx->r[1], r2 = ctx->r[2], r3 = ctx->r[3], r4 = ctx->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2], h3 = ctx->h[3], h4 = ctx->h[4];

    while (bytes >= 16) {
        /* h += m[i] */
        h0 += (U8TO32_LE(m+0)) & 0x3ffffffu;
        h1 += (U8TO32_LE(m+3) >> 2) & 0x3ffffffu;
        h2 += (U8TO32_LE(m+6) >> 4) & 0x3ffffffu;
        h3 += (U8TO32_LE(m+9) >> 6) & 0x3ffffffu;
        h4 += (U8TO32_LE(m+12) >> 8) | hibit;

        /* h *= r */
        uint64_t d0 = ((uint64_t)h0*r0) + ((uint64_t)h1*s4) + ((uint64_t)h2*s3) + ((uint64_t)h3*s2) + ((uint64_t)h4*s1);
        uint64_t d1 = ((uint64_t)h0*r1) + ((uint64_t)h1*r0) + ((uint64_t)h2*s4) + ((uint64_t)h3*s3) + ((uint64_t)h4*s2);
        uint64_t d2 = ((uint64_t)h0*r2) + ((uint64_t)h1*r1) + ((uint64_t)h2*r0) + ((uint64_t)h3*s4) + ((uint64_t)h4*s3);
        uint64_t d3 = ((uint64_t)h0*r3) + ((uint64_t)h1*r2) + ((uint64_t)h2*r1) + ((uint64_t)h3*r0) + ((uint64_t)h4*s4);
        uint64_t d4 = ((uint64_t)h0*r4) + ((uint64_t)h1*r3) + ((uint64_t)h2*r2) + ((uint64_t)h3*r1) + ((uint64_t)h4*r0);

        /* (partial) h %= p */
        uint32_t c;
        c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffffu;
        d1 += c;      c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffffu;
        d2 += c;      c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffffu;
        d3 += c;      c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffffu;
        d4 += c;      c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffffu;
        h0 += c * 5;  c = h0 >> 26; h0 = h0 & 0x3ffffffu;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    ctx->h[0] = h0; ctx->h[1] = h1; ctx->h[2] = h2; ctx->h[3] = h3; ctx->h[4] = h4;
}

void edb_poly1305_update(edb_poly1305_ctx *ctx, const uint8_t *m, size_t bytes) {
    if (ctx->leftover) {
        size_t want = 16 - ctx->leftover;
        if (want > bytes) want = bytes;
        memcpy(ctx->buffer + ctx->leftover, m, want);
        bytes -= want;
        m += want;
        ctx->leftover += want;
        if (ctx->leftover < 16) return;
        poly1305_blocks(ctx, ctx->buffer, 16);
        ctx->leftover = 0;
    }

    if (bytes >= 16) {
        size_t want = bytes & ~(size_t)15;
        poly1305_blocks(ctx, m, want);
        m += want;
        bytes -= want;
    }

    if (bytes) {
        memcpy(ctx->buffer + ctx->leftover, m, bytes);
        ctx->leftover += bytes;
    }
}

void edb_poly1305_final(edb_poly1305_ctx *ctx, uint8_t tag[16]) {
    if (ctx->leftover) {
        ctx->buffer[ctx->leftover++] = 1;
        while (ctx->leftover < 16) ctx->buffer[ctx->leftover++] = 0;
        ctx->final = 1;
        poly1305_blocks(ctx, ctx->buffer, 16);
    }

    /* fully carry h */
    uint32_t h0 = ctx->h[0], h1 = ctx->h[1], h2 = ctx->h[2], h3 = ctx->h[3], h4 = ctx->h[4];
    uint32_t c;
    c = h1 >> 26; h1 = h1 & 0x3ffffffu; h2 += c;
    c = h2 >> 26; h2 = h2 & 0x3ffffffu; h3 += c;
    c = h3 >> 26; h3 = h3 & 0x3ffffffu; h4 += c;
    c = h4 >> 26; h4 = h4 & 0x3ffffffu; h0 += c * 5;
    c = h0 >> 26; h0 = h0 & 0x3ffffffu; h1 += c;

    /* compute h + -p */
    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffffu;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffffu;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffffu;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffffu;
    uint32_t g4 = h4 + c - (1u << 26);

    /* select h if h < p, or h + -p if h >= p */
    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    /* h = h % (2^128) */
    h0 = ((h0) | (h1 << 26)) & 0xffffffffu;
    h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffffu;
    h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffffu;
    h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffffu;

    /* mac = (h + pad) % (2^128) */
    uint64_t f;
    f = (uint64_t)h0 + ctx->pad[0]; h0 = (uint32_t)f;
    f = (uint64_t)h1 + ctx->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + ctx->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + ctx->pad[3] + (f >> 32); h3 = (uint32_t)f;

    U32TO8_LE(tag+0, h0);
    U32TO8_LE(tag+4, h1);
    U32TO8_LE(tag+8, h2);
    U32TO8_LE(tag+12, h3);

    /* wipe */
    ctx->h[0]=ctx->h[1]=ctx->h[2]=ctx->h[3]=ctx->h[4]=0;
    ctx->r[0]=ctx->r[1]=ctx->r[2]=ctx->r[3]=ctx->r[4]=0;
    ctx->pad[0]=ctx->pad[1]=ctx->pad[2]=ctx->pad[3]=0;
}

void edb_poly1305(const uint8_t key[32], const uint8_t *m, size_t bytes, uint8_t tag[16]) {
    edb_poly1305_ctx ctx;
    edb_poly1305_init(&ctx, key);
    edb_poly1305_update(&ctx, m, bytes);
    edb_poly1305_final(&ctx, tag);
}
