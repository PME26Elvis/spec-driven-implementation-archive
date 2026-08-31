/* sdk_hmac.h - HMAC-SHA-256 (RFC 2104 / FIPS 198-1), self-implemented.
 *
 * docs/08 section 3 requires HMAC-SHA-256 to be implemented in-tree.  No
 * BCrypt hash/HMAC provider is used anywhere in this project; docs/26
 * section 16 restricts the OS crypto surface to BCryptGenRandom.
 */
#ifndef SDK_HMAC_H
#define SDK_HMAC_H

#include "common/sdk_sha256.h"

#define SDK_HMAC_SHA256_BLOCK_LEN 64u
#define SDK_HMAC_SHA256_MAC_LEN   SDK_SHA256_DIGEST_LEN

typedef struct sdk_hmac_sha256_ctx {
    sdk_sha256_ctx inner;
    sdk_sha256_ctx outer;
} sdk_hmac_sha256_ctx;

/* Streaming interface. */
void sdk_hmac_sha256_init(sdk_hmac_sha256_ctx *ctx,
                          const void *key, size_t key_len);
void sdk_hmac_sha256_update(sdk_hmac_sha256_ctx *ctx,
                            const void *data, size_t len);
void sdk_hmac_sha256_final(sdk_hmac_sha256_ctx *ctx,
                           unsigned char out[SDK_HMAC_SHA256_MAC_LEN]);

/* One-shot interface. */
void sdk_hmac_sha256(const void *key, size_t key_len,
                     const void *data, size_t len,
                     unsigned char out[SDK_HMAC_SHA256_MAC_LEN]);

/* Precomputed-key form used by PBKDF2 so that the key padding is expanded
 * once instead of once per iteration.  The context holds derived key
 * material and must be wiped by the caller through sdk_hmac_sha256_key_free. */
typedef struct sdk_hmac_sha256_key {
    sdk_sha256_ctx inner_seed;
    sdk_sha256_ctx outer_seed;
} sdk_hmac_sha256_key;

void sdk_hmac_sha256_key_init(sdk_hmac_sha256_key *k,
                              const void *key, size_t key_len);
void sdk_hmac_sha256_key_free(sdk_hmac_sha256_key *k);
void sdk_hmac_sha256_with_key(const sdk_hmac_sha256_key *k,
                              const void *data, size_t len,
                              unsigned char out[SDK_HMAC_SHA256_MAC_LEN]);

#endif /* SDK_HMAC_H */
