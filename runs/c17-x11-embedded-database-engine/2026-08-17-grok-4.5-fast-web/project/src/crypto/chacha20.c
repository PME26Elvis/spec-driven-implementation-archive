#include "edb/chacha20.h"
#include <string.h>

static uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

#define QR(a,b,c,d) \
    a += b; d ^= a; d = rotl32(d,16); \
    c += d; b ^= c; b = rotl32(b,12); \
    a += b; d ^= a; d = rotl32(d, 8); \
    c += d; b ^= c; b = rotl32(b, 7);

static void chacha20_rounds(uint32_t x[16]) {
    for (int i = 0; i < 10; i++) {
        /* column rounds */
        QR(x[0], x[4], x[ 8], x[12]);
        QR(x[1], x[5], x[ 9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        /* diagonal rounds */
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[ 8], x[13]);
        QR(x[3], x[4], x[ 9], x[14]);
    }
}

void edb_chacha20_block(const uint32_t key[8], uint32_t counter,
                        const uint32_t nonce[3], uint32_t out[16]) {
    uint32_t state[16];
    /* constants "expand 32-byte k" */
    state[0] = 0x61707865u;
    state[1] = 0x3320646eu;
    state[2] = 0x79622d32u;
    state[3] = 0x6b206574u;
    for (int i = 0; i < 8; i++) state[4+i] = key[i];
    state[12] = counter;
    state[13] = nonce[0];
    state[14] = nonce[1];
    state[15] = nonce[2];

    uint32_t working[16];
    memcpy(working, state, sizeof state);
    chacha20_rounds(working);
    for (int i = 0; i < 16; i++) out[i] = working[i] + state[i];
}

void edb_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter, const uint8_t *in, uint8_t *out, size_t len) {
    uint32_t k[8], n[3];
    for (int i = 0; i < 8; i++) {
        k[i] = (uint32_t)key[i*4] | ((uint32_t)key[i*4+1]<<8) |
               ((uint32_t)key[i*4+2]<<16) | ((uint32_t)key[i*4+3]<<24);
    }
    for (int i = 0; i < 3; i++) {
        n[i] = (uint32_t)nonce[i*4] | ((uint32_t)nonce[i*4+1]<<8) |
               ((uint32_t)nonce[i*4+2]<<16) | ((uint32_t)nonce[i*4+3]<<24);
    }

    uint32_t block[16];
    uint8_t stream[64];
    size_t off = 0;
    while (off < len) {
        edb_chacha20_block(k, counter, n, block);
        for (int i = 0; i < 16; i++) {
            stream[i*4+0] = (uint8_t)(block[i]);
            stream[i*4+1] = (uint8_t)(block[i] >> 8);
            stream[i*4+2] = (uint8_t)(block[i] >> 16);
            stream[i*4+3] = (uint8_t)(block[i] >> 24);
        }
        size_t chunk = (len - off > 64) ? 64 : (len - off);
        for (size_t i = 0; i < chunk; i++)
            out[off+i] = in[off+i] ^ stream[i];
        off += chunk;
        counter++;
    }
}

void edb_hchacha20(const uint8_t key[32], const uint8_t nonce[16], uint8_t out[32]) {
    uint32_t state[16];
    state[0] = 0x61707865u;
    state[1] = 0x3320646eu;
    state[2] = 0x79622d32u;
    state[3] = 0x6b206574u;
    for (int i = 0; i < 8; i++) {
        state[4+i] = (uint32_t)key[i*4] | ((uint32_t)key[i*4+1]<<8) |
                     ((uint32_t)key[i*4+2]<<16) | ((uint32_t)key[i*4+3]<<24);
    }
    for (int i = 0; i < 4; i++) {
        state[12+i] = (uint32_t)nonce[i*4] | ((uint32_t)nonce[i*4+1]<<8) |
                      ((uint32_t)nonce[i*4+2]<<16) | ((uint32_t)nonce[i*4+3]<<24);
    }
    chacha20_rounds(state);
    /* output first 4 and last 4 words */
    for (int i = 0; i < 4; i++) {
        out[i*4+0] = (uint8_t)(state[i]);
        out[i*4+1] = (uint8_t)(state[i] >> 8);
        out[i*4+2] = (uint8_t)(state[i] >> 16);
        out[i*4+3] = (uint8_t)(state[i] >> 24);
    }
    for (int i = 0; i < 4; i++) {
        out[16+i*4+0] = (uint8_t)(state[12+i]);
        out[16+i*4+1] = (uint8_t)(state[12+i] >> 8);
        out[16+i*4+2] = (uint8_t)(state[12+i] >> 16);
        out[16+i*4+3] = (uint8_t)(state[12+i] >> 24);
    }
}
