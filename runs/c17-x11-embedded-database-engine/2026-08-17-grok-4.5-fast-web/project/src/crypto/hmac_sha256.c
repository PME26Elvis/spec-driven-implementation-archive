#include "edb/hmac_sha256.h"
#include "edb/sha256.h"
#include <string.h>

void edb_hmac_sha256(const uint8_t *key, size_t key_len,
                     const uint8_t *msg, size_t msg_len,
                     uint8_t out[32]) {
    uint8_t k[64];
    uint8_t ipad[64], opad[64];
    edb_sha256_ctx ctx;
    uint8_t inner[32];
    size_t i;

    memset(k, 0, 64);
    if (key_len > 64) {
        edb_sha256(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }

    for (i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    edb_sha256_init(&ctx);
    edb_sha256_update(&ctx, ipad, 64);
    edb_sha256_update(&ctx, msg, msg_len);
    edb_sha256_final(&ctx, inner);

    edb_sha256_init(&ctx);
    edb_sha256_update(&ctx, opad, 64);
    edb_sha256_update(&ctx, inner, 32);
    edb_sha256_final(&ctx, out);

    /* wipe */
    memset(k, 0, sizeof k);
    memset(ipad, 0, sizeof ipad);
    memset(opad, 0, sizeof opad);
    memset(inner, 0, sizeof inner);
}
