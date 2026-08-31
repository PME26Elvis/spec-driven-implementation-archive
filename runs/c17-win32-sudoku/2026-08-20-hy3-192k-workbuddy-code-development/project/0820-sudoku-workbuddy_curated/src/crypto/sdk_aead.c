/* sdk_aead.c - XChaCha20-Poly1305 AEAD. */
#include "crypto/sdk_aead.h"
#include "crypto/sdk_chacha.h"
#include "crypto/sdk_poly1305.h"

#include <string.h>

static const unsigned char AEAD_ZEROS[16] = { 0 };

/* Poly1305 is fed aad || pad16 || ct || pad16 || le64(aad_len) || le64(ct_len).
 * The padding brings each variable-length section to a 16-byte boundary. */
static void aead_mac(const unsigned char poly_key[SDK_POLY1305_KEY_LEN],
                     const unsigned char *aad, size_t aad_len,
                     const unsigned char *ct, size_t ct_len,
                     unsigned char tag[SDK_XCHACHA20POLY1305_TAG_LEN]) {
    sdk_poly1305_ctx mac;
    unsigned char lengths[16];
    size_t pad;

    sdk_poly1305_init(&mac, poly_key);

    if (aad_len > 0) {
        sdk_poly1305_update(&mac, aad, aad_len);
    }
    pad = (16u - (aad_len & 15u)) & 15u;
    if (pad > 0) {
        sdk_poly1305_update(&mac, AEAD_ZEROS, pad);
    }

    if (ct_len > 0) {
        sdk_poly1305_update(&mac, ct, ct_len);
    }
    pad = (16u - (ct_len & 15u)) & 15u;
    if (pad > 0) {
        sdk_poly1305_update(&mac, AEAD_ZEROS, pad);
    }

    sdk_put_u64le(lengths + 0, (uint64_t)aad_len);
    sdk_put_u64le(lengths + 8, (uint64_t)ct_len);
    sdk_poly1305_update(&mac, lengths, sizeof lengths);

    sdk_poly1305_final(&mac, tag);
}

/* Derives the XChaCha20 subkey and the 12-byte inner nonce. */
static void aead_setup(const unsigned char *key, const unsigned char *nonce,
                       unsigned char subkey[SDK_CHACHA20_KEY_LEN],
                       unsigned char inner_nonce[SDK_CHACHA20_NONCE_LEN]) {
    sdk_hchacha20(key, nonce, subkey);
    memset(inner_nonce, 0, 4);
    memcpy(inner_nonce + 4, nonce + 16, 8);
}

sdk_status sdk_xchacha20poly1305_encrypt(
    const unsigned char key[SDK_XCHACHA20POLY1305_KEY_LEN],
    const unsigned char nonce[SDK_XCHACHA20POLY1305_NONCE_LEN],
    const unsigned char *aad, size_t aad_len,
    const unsigned char *plaintext, size_t plaintext_len,
    unsigned char *ciphertext,
    unsigned char tag[SDK_XCHACHA20POLY1305_TAG_LEN]) {
    unsigned char subkey[SDK_CHACHA20_KEY_LEN];
    unsigned char inner_nonce[SDK_CHACHA20_NONCE_LEN];
    unsigned char poly_block[SDK_CHACHA20_BLOCK_LEN];

    if (key == NULL || nonce == NULL || tag == NULL) {
        return SDK_ERR_USAGE;
    }
    if (plaintext_len > 0 && (plaintext == NULL || ciphertext == NULL)) {
        return SDK_ERR_USAGE;
    }
    if (aad_len > 0 && aad == NULL) {
        return SDK_ERR_USAGE;
    }
    if (plaintext_len > SDK_LIMIT_VAULT_CIPHERTEXT) {
        return SDK_ERR_LIMIT;
    }

    aead_setup(key, nonce, subkey, inner_nonce);

    /* Block counter 0 yields the one-time Poly1305 key. */
    sdk_chacha20_block(subkey, 0u, inner_nonce, poly_block);

    if (plaintext_len > 0) {
        sdk_chacha20_xor(subkey, 1u, inner_nonce,
                         plaintext, ciphertext, plaintext_len);
    }

    aead_mac(poly_block, aad, aad_len, ciphertext, plaintext_len, tag);

    sdk_secure_wipe(subkey, sizeof subkey);
    sdk_secure_wipe(inner_nonce, sizeof inner_nonce);
    sdk_secure_wipe(poly_block, sizeof poly_block);
    return SDK_OK;
}

sdk_status sdk_xchacha20poly1305_decrypt(
    const unsigned char key[SDK_XCHACHA20POLY1305_KEY_LEN],
    const unsigned char nonce[SDK_XCHACHA20POLY1305_NONCE_LEN],
    const unsigned char *aad, size_t aad_len,
    const unsigned char *ciphertext, size_t ciphertext_len,
    const unsigned char tag[SDK_XCHACHA20POLY1305_TAG_LEN],
    unsigned char *plaintext) {
    unsigned char subkey[SDK_CHACHA20_KEY_LEN];
    unsigned char inner_nonce[SDK_CHACHA20_NONCE_LEN];
    unsigned char poly_block[SDK_CHACHA20_BLOCK_LEN];
    unsigned char expected[SDK_XCHACHA20POLY1305_TAG_LEN];
    int ok;

    if (key == NULL || nonce == NULL || tag == NULL) {
        return SDK_ERR_USAGE;
    }
    if (ciphertext_len > 0 && (ciphertext == NULL || plaintext == NULL)) {
        return SDK_ERR_USAGE;
    }
    if (aad_len > 0 && aad == NULL) {
        return SDK_ERR_USAGE;
    }
    if (ciphertext_len > SDK_LIMIT_VAULT_CIPHERTEXT) {
        return SDK_ERR_LIMIT;
    }

    aead_setup(key, nonce, subkey, inner_nonce);
    sdk_chacha20_block(subkey, 0u, inner_nonce, poly_block);

    aead_mac(poly_block, aad, aad_len, ciphertext, ciphertext_len, expected);

    /* Constant-time comparison over the full tag length (docs/08 section 24):
     * no early return on the first differing byte. */
    ok = sdk_ct_equal(expected, tag, sizeof expected);

    sdk_secure_wipe(expected, sizeof expected);
    sdk_secure_wipe(poly_block, sizeof poly_block);

    if (!ok) {
        /* No plaintext is produced when authentication fails. */
        sdk_secure_wipe(subkey, sizeof subkey);
        sdk_secure_wipe(inner_nonce, sizeof inner_nonce);
        return SDK_ERR_AUTH;
    }

    if (ciphertext_len > 0) {
        sdk_chacha20_xor(subkey, 1u, inner_nonce,
                         ciphertext, plaintext, ciphertext_len);
    }

    sdk_secure_wipe(subkey, sizeof subkey);
    sdk_secure_wipe(inner_nonce, sizeof inner_nonce);
    return SDK_OK;
}
