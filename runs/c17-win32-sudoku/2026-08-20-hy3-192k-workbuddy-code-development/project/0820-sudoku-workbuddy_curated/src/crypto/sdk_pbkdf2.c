/* sdk_pbkdf2.c - PBKDF2-HMAC-SHA-256.
 *
 * DK = T_1 || T_2 || ... || T_l
 * T_i = F(P, S, c, i)
 * F   = U_1 xor U_2 xor ... xor U_c
 * U_1 = HMAC(P, S || INT_BE32(i))
 * U_j = HMAC(P, U_{j-1})
 */
#include "crypto/sdk_pbkdf2.h"
#include "crypto/sdk_hmac.h"

#include <string.h>

sdk_status sdk_pbkdf2_hmac_sha256(const void *password, size_t password_len,
                                  const void *salt, size_t salt_len,
                                  uint32_t iterations,
                                  unsigned char *dk, size_t dk_len) {
    sdk_hmac_sha256_key key;
    unsigned char u[SDK_HMAC_SHA256_MAC_LEN];
    unsigned char t[SDK_HMAC_SHA256_MAC_LEN];
    unsigned char *salt_block = NULL;
    size_t salt_block_len;
    uint32_t block_index;
    uint32_t blocks_needed;
    size_t produced = 0;
    sdk_status st = SDK_OK;

    if (dk == NULL || dk_len == 0 || iterations == 0) {
        return SDK_ERR_USAGE;
    }
    if (salt_len > 0 && salt == NULL) {
        return SDK_ERR_USAGE;
    }
    if (password_len > 0 && password == NULL) {
        return SDK_ERR_USAGE;
    }

    /* The PBKDF2 output limit is (2^32 - 1) * hLen; the canonical limits in
     * docs/19 keep every real request far below that, but the check is kept
     * so that no silent truncation is possible. */
    blocks_needed = (uint32_t)((dk_len + SDK_HMAC_SHA256_MAC_LEN - 1u)
                               / SDK_HMAC_SHA256_MAC_LEN);
    if (blocks_needed == 0u) {
        return SDK_ERR_USAGE;
    }

    if (!sdk_size_add(salt_len, 4u, &salt_block_len)) {
        return SDK_ERR_LIMIT;
    }
    salt_block = (unsigned char *)malloc(salt_block_len);
    if (salt_block == NULL) {
        return SDK_ERR_NOMEM;
    }
    if (salt_len > 0) {
        memcpy(salt_block, salt, salt_len);
    }

    sdk_hmac_sha256_key_init(&key, password, password_len);

    for (block_index = 1u; block_index <= blocks_needed; ++block_index) {
        uint32_t iter;
        size_t chunk;
        size_t i;

        /* INT_BE32(block_index) appended to the salt for U_1. */
        salt_block[salt_len + 0] = (unsigned char)((block_index >> 24) & 0xffu);
        salt_block[salt_len + 1] = (unsigned char)((block_index >> 16) & 0xffu);
        salt_block[salt_len + 2] = (unsigned char)((block_index >> 8) & 0xffu);
        salt_block[salt_len + 3] = (unsigned char)(block_index & 0xffu);

        sdk_hmac_sha256_with_key(&key, salt_block, salt_block_len, u);
        memcpy(t, u, sizeof t);

        for (iter = 1u; iter < iterations; ++iter) {
            sdk_hmac_sha256_with_key(&key, u, sizeof u, u);
            for (i = 0; i < sizeof t; ++i) {
                t[i] ^= u[i];
            }
        }

        chunk = dk_len - produced;
        if (chunk > sizeof t) {
            chunk = sizeof t;
        }
        memcpy(dk + produced, t, chunk);
        produced += chunk;
    }

    sdk_hmac_sha256_key_free(&key);
    sdk_secure_wipe(u, sizeof u);
    sdk_secure_wipe(t, sizeof t);
    sdk_secure_wipe(salt_block, salt_block_len);
    free(salt_block);
    return st;
}
