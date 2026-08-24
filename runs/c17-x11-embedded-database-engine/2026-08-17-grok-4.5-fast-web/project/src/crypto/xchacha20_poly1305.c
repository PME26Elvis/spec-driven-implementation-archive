#include "edb/xchacha20_poly1305.h"
#include "edb/chacha20.h"
#include "edb/poly1305.h"
#include <string.h>
#include <stdbool.h>

/* Construct Poly1305 key from first ChaCha20 block, then encrypt. */

static void poly1305_key_gen(const uint8_t key[32], const uint8_t nonce12[12],
                             uint8_t poly_key[32]) {
    uint8_t zeros[64] = {0};
    edb_chacha20_xor(key, nonce12, 0, zeros, zeros, 64);
    memcpy(poly_key, zeros, 32);
}

static void pad16(edb_poly1305_ctx *ctx, size_t len) {
    static const uint8_t zero[16] = {0};
    size_t rem = len % 16;
    if (rem) edb_poly1305_update(ctx, zero, 16 - rem);
}

bool edb_xchacha20_poly1305_encrypt(
    const uint8_t key[32],
    const uint8_t nonce[24],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plaintext, size_t pt_len,
    uint8_t *ciphertext,
    uint8_t tag[16])
{
    uint8_t subkey[32];
    edb_hchacha20(key, nonce, subkey);          /* first 16 bytes of nonce */

    uint8_t nonce12[12];
    memset(nonce12, 0, 4);
    memcpy(nonce12 + 4, nonce + 16, 8);         /* last 8 bytes of 24-byte nonce */

    uint8_t poly_key[32];
    poly1305_key_gen(subkey, nonce12, poly_key);

    /* encrypt with counter starting at 1 */
    edb_chacha20_xor(subkey, nonce12, 1, plaintext, ciphertext, pt_len);

    /* MAC = Poly1305(AAD || pad || CT || pad || lenAAD || lenCT) */
    edb_poly1305_ctx pctx;
    edb_poly1305_init(&pctx, poly_key);
    if (aad_len) edb_poly1305_update(&pctx, aad, aad_len);
    pad16(&pctx, aad_len);
    if (pt_len) edb_poly1305_update(&pctx, ciphertext, pt_len);
    pad16(&pctx, pt_len);

    uint8_t lens[16];
    memset(lens, 0, 16);
    /* little-endian 64-bit lengths */
    uint64_t al = (uint64_t)aad_len;
    uint64_t cl = (uint64_t)pt_len;
    for (int i = 0; i < 8; i++) {
        lens[i]     = (uint8_t)(al >> (8*i));
        lens[8 + i] = (uint8_t)(cl >> (8*i));
    }
    edb_poly1305_update(&pctx, lens, 16);
    edb_poly1305_final(&pctx, tag);

    /* wipe */
    memset(subkey, 0, 32);
    memset(poly_key, 0, 32);
    return true;
}

bool edb_xchacha20_poly1305_decrypt(
    const uint8_t key[32],
    const uint8_t nonce[24],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *ciphertext, size_t ct_len,
    const uint8_t tag[16],
    uint8_t *plaintext)
{
    uint8_t subkey[32];
    edb_hchacha20(key, nonce, subkey);

    uint8_t nonce12[12];
    memset(nonce12, 0, 4);
    memcpy(nonce12 + 4, nonce + 16, 8);

    uint8_t poly_key[32];
    poly1305_key_gen(subkey, nonce12, poly_key);

    /* compute expected tag */
    edb_poly1305_ctx pctx;
    edb_poly1305_init(&pctx, poly_key);
    if (aad_len) edb_poly1305_update(&pctx, aad, aad_len);
    pad16(&pctx, aad_len);
    if (ct_len) edb_poly1305_update(&pctx, ciphertext, ct_len);
    pad16(&pctx, ct_len);

    uint8_t lens[16];
    memset(lens, 0, 16);
    uint64_t al = (uint64_t)aad_len;
    uint64_t cl = (uint64_t)ct_len;
    for (int i = 0; i < 8; i++) {
        lens[i]     = (uint8_t)(al >> (8*i));
        lens[8 + i] = (uint8_t)(cl >> (8*i));
    }
    edb_poly1305_update(&pctx, lens, 16);

    uint8_t expected[16];
    edb_poly1305_final(&pctx, expected);

    /* constant-time compare */
    uint8_t diff = 0;
    for (int i = 0; i < 16; i++) diff |= expected[i] ^ tag[i];
    if (diff != 0) {
        memset(subkey, 0, 32);
        memset(poly_key, 0, 32);
        return false;
    }

    edb_chacha20_xor(subkey, nonce12, 1, ciphertext, plaintext, ct_len);

    memset(subkey, 0, 32);
    memset(poly_key, 0, 32);
    return true;
}
