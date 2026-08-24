#include "edb/pbkdf2.h"
#include "edb/hmac_sha256.h"
#include <string.h>
#include <stdlib.h>

static void F(const uint8_t *password, size_t password_len,
              const uint8_t *salt, size_t salt_len,
              uint32_t iterations, uint32_t block_index,
              uint8_t out[32]) {
    uint8_t U[32], T[32];
    uint8_t block[4];
    size_t i;
    uint32_t j;

    block[0] = (uint8_t)((block_index >> 24) & 0xff);
    block[1] = (uint8_t)((block_index >> 16) & 0xff);
    block[2] = (uint8_t)((block_index >> 8) & 0xff);
    block[3] = (uint8_t)(block_index & 0xff);

    /* U1 = HMAC(password, salt || INT(i)) */
    {
        uint8_t *tmp = (uint8_t*)malloc(salt_len + 4);
        if (!tmp) return; /* caller must handle OOM higher up; for simplicity */
        memcpy(tmp, salt, salt_len);
        memcpy(tmp + salt_len, block, 4);
        edb_hmac_sha256(password, password_len, tmp, salt_len + 4, U);
        free(tmp);
    }
    memcpy(T, U, 32);

    for (j = 1; j < iterations; j++) {
        edb_hmac_sha256(password, password_len, U, 32, U);
        for (i = 0; i < 32; i++) T[i] ^= U[i];
    }
    memcpy(out, T, 32);
}

int edb_pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
                           const uint8_t *salt, size_t salt_len,
                           uint32_t iterations,
                           uint8_t *dk, size_t dk_len) {
    if (iterations == 0 || dk_len == 0) return -1;
    uint32_t blocks = (uint32_t)((dk_len + 31) / 32);
    uint32_t i;
    size_t offset = 0;

    for (i = 1; i <= blocks; i++) {
        uint8_t block[32];
        F(password, password_len, salt, salt_len, iterations, i, block);
        size_t copy = (dk_len - offset > 32) ? 32 : (dk_len - offset);
        memcpy(dk + offset, block, copy);
        offset += copy;
    }
    return 0;
}
