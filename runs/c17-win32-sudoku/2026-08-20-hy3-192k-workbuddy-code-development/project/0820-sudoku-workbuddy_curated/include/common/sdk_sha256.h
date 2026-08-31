/* sdk_sha256.h - self-implemented SHA-256 (FIPS 180-4).
 *
 * No OS or third-party crypto provider is used. docs/02 section 3 forbids
 * CNG hash providers, OpenSSL and libsodium; Bcrypt is limited to RNG.
 */
#ifndef SDK_SHA256_H
#define SDK_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SDK_SHA256_DIGEST_LEN 32
#define SDK_SHA256_BLOCK_LEN  64

typedef struct sdk_sha256_ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  block[SDK_SHA256_BLOCK_LEN];
    size_t   blocklen;
} sdk_sha256_ctx;

void sdk_sha256_init(sdk_sha256_ctx *c);
void sdk_sha256_update(sdk_sha256_ctx *c, const void *data, size_t len);
void sdk_sha256_final(sdk_sha256_ctx *c, uint8_t out[SDK_SHA256_DIGEST_LEN]);
void sdk_sha256(const void *data, size_t len, uint8_t out[SDK_SHA256_DIGEST_LEN]);

#endif /* SDK_SHA256_H */
