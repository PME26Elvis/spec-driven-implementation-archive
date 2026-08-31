/* sdk_chacha.c - ChaCha20 / HChaCha20 core.
 *
 * State layout (RFC 8439 section 2.3):
 *   0.. 3  constants "expa" "nd 3" "2-by" "te k"
 *   4..11  key, little-endian words
 *  12      block counter
 *  13..15  nonce, little-endian words
 */
#include "crypto/sdk_chacha.h"
#include "common/sdk_common.h"

#include <string.h>

static const uint32_t CHACHA_SIGMA[4] = {
    0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u
};

static uint32_t rotl32(uint32_t v, int n) {
    return (uint32_t)((v << n) | (v >> (32 - n)));
}

#define QR(a, b, c, d)                       \
    do {                                     \
        (a) += (b); (d) ^= (a); (d) = rotl32((d), 16); \
        (c) += (d); (b) ^= (c); (b) = rotl32((b), 12); \
        (a) += (b); (d) ^= (a); (d) = rotl32((d), 8);  \
        (c) += (d); (b) ^= (c); (b) = rotl32((b), 7);  \
    } while (0)

static void chacha_rounds(uint32_t x[16]) {
    int i;
    for (i = 0; i < 10; ++i) {
        /* Column rounds. */
        QR(x[0], x[4], x[8], x[12]);
        QR(x[1], x[5], x[9], x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        /* Diagonal rounds. */
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8], x[13]);
        QR(x[3], x[4], x[9], x[14]);
    }
}

static void chacha_init_state(uint32_t st[16],
                              const unsigned char *key,
                              uint32_t counter,
                              const unsigned char *nonce,
                              size_t nonce_words) {
    size_t i;

    st[0] = CHACHA_SIGMA[0];
    st[1] = CHACHA_SIGMA[1];
    st[2] = CHACHA_SIGMA[2];
    st[3] = CHACHA_SIGMA[3];
    for (i = 0; i < 8; ++i) {
        st[4 + i] = sdk_get_u32le(key + i * 4);
    }
    if (nonce_words == 3) {
        st[12] = counter;
        st[13] = sdk_get_u32le(nonce + 0);
        st[14] = sdk_get_u32le(nonce + 4);
        st[15] = sdk_get_u32le(nonce + 8);
    } else {
        /* HChaCha20 consumes a 16-byte nonce and has no counter word. */
        st[12] = sdk_get_u32le(nonce + 0);
        st[13] = sdk_get_u32le(nonce + 4);
        st[14] = sdk_get_u32le(nonce + 8);
        st[15] = sdk_get_u32le(nonce + 12);
    }
}

void sdk_chacha20_block(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                        uint32_t counter,
                        const unsigned char nonce[SDK_CHACHA20_NONCE_LEN],
                        unsigned char out[SDK_CHACHA20_BLOCK_LEN]) {
    uint32_t st[16];
    uint32_t work[16];
    size_t i;

    chacha_init_state(st, key, counter, nonce, 3);
    memcpy(work, st, sizeof work);
    chacha_rounds(work);
    for (i = 0; i < 16; ++i) {
        sdk_put_u32le(out + i * 4, work[i] + st[i]);
    }
    sdk_secure_wipe(st, sizeof st);
    sdk_secure_wipe(work, sizeof work);
}

void sdk_chacha20_xor(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                      uint32_t counter,
                      const unsigned char nonce[SDK_CHACHA20_NONCE_LEN],
                      const unsigned char *in, unsigned char *out, size_t len) {
    unsigned char stream[SDK_CHACHA20_BLOCK_LEN];
    size_t offset = 0;

    while (offset < len) {
        size_t chunk = len - offset;
        size_t i;

        if (chunk > SDK_CHACHA20_BLOCK_LEN) {
            chunk = SDK_CHACHA20_BLOCK_LEN;
        }
        sdk_chacha20_block(key, counter, nonce, stream);
        for (i = 0; i < chunk; ++i) {
            out[offset + i] = (unsigned char)(in[offset + i] ^ stream[i]);
        }
        offset += chunk;
        /* The 32-bit counter wraps by definition; the canonical limits keep a
         * single message far below 256 GiB so wrap-around never occurs in
         * practice, and the AEAD layer rejects oversized inputs anyway. */
        ++counter;
    }
    sdk_secure_wipe(stream, sizeof stream);
}

void sdk_hchacha20(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                   const unsigned char nonce[SDK_HCHACHA20_NONCE_LEN],
                   unsigned char out[SDK_HCHACHA20_OUT_LEN]) {
    uint32_t st[16];
    size_t i;

    chacha_init_state(st, key, 0, nonce, 4);
    chacha_rounds(st);
    for (i = 0; i < 4; ++i) {
        sdk_put_u32le(out + i * 4, st[i]);
    }
    for (i = 0; i < 4; ++i) {
        sdk_put_u32le(out + 16 + i * 4, st[12 + i]);
    }
    sdk_secure_wipe(st, sizeof st);
}
