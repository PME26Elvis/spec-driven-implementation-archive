/* sdk_sha256.c - SHA-256 per FIPS 180-4, implemented from the specification. */

#include "common/sdk_sha256.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotr32(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}

static void sha256_compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[4 * i] << 24) |
               ((uint32_t)block[4 * i + 1] << 16) |
               ((uint32_t)block[4 * i + 2] << 8) |
               ((uint32_t)block[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sdk_sha256_init(sdk_sha256_ctx *c) {
    c->state[0] = 0x6a09e667u;
    c->state[1] = 0xbb67ae85u;
    c->state[2] = 0x3c6ef372u;
    c->state[3] = 0xa54ff53au;
    c->state[4] = 0x510e527fu;
    c->state[5] = 0x9b05688cu;
    c->state[6] = 0x1f83d9abu;
    c->state[7] = 0x5be0cd19u;
    c->bitlen = 0;
    c->blocklen = 0;
    memset(c->block, 0, sizeof c->block);
}

void sdk_sha256_update(sdk_sha256_ctx *c, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    while (len > 0) {
        size_t space = SDK_SHA256_BLOCK_LEN - c->blocklen;
        size_t take = (len < space) ? len : space;
        memcpy(c->block + c->blocklen, p, take);
        c->blocklen += take;
        p += take;
        len -= take;
        if (c->blocklen == SDK_SHA256_BLOCK_LEN) {
            sha256_compress(c->state, c->block);
            c->bitlen += (uint64_t)SDK_SHA256_BLOCK_LEN * 8u;
            c->blocklen = 0;
        }
    }
}

void sdk_sha256_final(sdk_sha256_ctx *c, uint8_t out[SDK_SHA256_DIGEST_LEN]) {
    uint64_t total_bits = c->bitlen + (uint64_t)c->blocklen * 8u;
    size_t i = c->blocklen;

    c->block[i++] = 0x80u;
    if (i > 56) {
        while (i < SDK_SHA256_BLOCK_LEN) {
            c->block[i++] = 0;
        }
        sha256_compress(c->state, c->block);
        i = 0;
    }
    while (i < 56) {
        c->block[i++] = 0;
    }
    for (int k = 7; k >= 0; --k) {
        c->block[i++] = (uint8_t)((total_bits >> (8 * k)) & 0xFFu);
    }
    sha256_compress(c->state, c->block);

    for (int k = 0; k < 8; ++k) {
        out[4 * k] = (uint8_t)((c->state[k] >> 24) & 0xFFu);
        out[4 * k + 1] = (uint8_t)((c->state[k] >> 16) & 0xFFu);
        out[4 * k + 2] = (uint8_t)((c->state[k] >> 8) & 0xFFu);
        out[4 * k + 3] = (uint8_t)(c->state[k] & 0xFFu);
    }
}

void sdk_sha256(const void *data, size_t len, uint8_t out[SDK_SHA256_DIGEST_LEN]) {
    sdk_sha256_ctx c;
    sdk_sha256_init(&c);
    sdk_sha256_update(&c, data, len);
    sdk_sha256_final(&c, out);
}
