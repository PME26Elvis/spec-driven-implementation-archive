#include "darc_sha256.h"
#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t sig0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static uint32_t sig1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

static void transform(darc_sha256_ctx *ctx, const uint8_t data[64]) {
    uint32_t m[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i, j;

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) |
               ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
    for (; i < 64; ++i)
        m[i] = gamma1(m[i-2]) + m[i-7] + gamma0(m[i-15]) + m[i-16];

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + sig1(e) + ch(e, f, g) + K[i] + m[i];
        t2 = sig0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void darc_sha256_init(darc_sha256_ctx *ctx) {
    ctx->bitlen = 0;
    ctx->buffer_len = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void darc_sha256_update(darc_sha256_ctx *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t i;

    for (i = 0; i < len; ++i) {
        ctx->buffer[ctx->buffer_len++] = p[i];
        if (ctx->buffer_len == 64) {
            transform(ctx, ctx->buffer);
            ctx->bitlen += 512;
            ctx->buffer_len = 0;
        }
    }
}

void darc_sha256_final(darc_sha256_ctx *ctx, uint8_t out[DARC_SHA256_DIGEST_SIZE]) {
    size_t i = ctx->buffer_len;

    if (ctx->buffer_len < 56) {
        ctx->buffer[i++] = 0x80;
        while (i < 56)
            ctx->buffer[i++] = 0x00;
    } else {
        ctx->buffer[i++] = 0x80;
        while (i < 64)
            ctx->buffer[i++] = 0x00;
        transform(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
        i = 56;
    }

    ctx->bitlen += ctx->buffer_len * 8;
    ctx->buffer[63] = (uint8_t)(ctx->bitlen);
    ctx->buffer[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->buffer[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->buffer[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->buffer[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->buffer[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->buffer[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->buffer[56] = (uint8_t)(ctx->bitlen >> 56);
    transform(ctx, ctx->buffer);

    for (i = 0; i < 4; ++i) {
        out[i]      = (ctx->state[0] >> (24 - i * 8)) & 0xff;
        out[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0xff;
        out[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0xff;
        out[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xff;
        out[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xff;
        out[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xff;
        out[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xff;
        out[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xff;
    }
}

void darc_sha256(const void *data, size_t len, uint8_t out[DARC_SHA256_DIGEST_SIZE]) {
    darc_sha256_ctx ctx;
    darc_sha256_init(&ctx);
    darc_sha256_update(&ctx, data, len);
    darc_sha256_final(&ctx, out);
}

void darc_sha256_hex(const uint8_t digest[DARC_SHA256_DIGEST_SIZE], char out[65]) {
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < 32; ++i) {
        out[i*2]   = hex[(digest[i] >> 4) & 0xf];
        out[i*2+1] = hex[digest[i] & 0xf];
    }
    out[64] = '\0';
}
