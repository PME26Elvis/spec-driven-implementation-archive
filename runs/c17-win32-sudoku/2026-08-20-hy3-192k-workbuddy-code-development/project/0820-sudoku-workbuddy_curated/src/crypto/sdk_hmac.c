/* sdk_hmac.c - HMAC-SHA-256 built on the in-tree SHA-256 implementation. */
#include "crypto/sdk_hmac.h"
#include "common/sdk_common.h"

#include <string.h>

/* Expand an arbitrary-length key into the RFC 2104 K0 block: keys longer than
 * the block length are hashed first, shorter keys are zero padded. */
static void hmac_expand_key(const void *key, size_t key_len,
                            unsigned char k0[SDK_HMAC_SHA256_BLOCK_LEN]) {
    memset(k0, 0, SDK_HMAC_SHA256_BLOCK_LEN);
    if (key_len > SDK_HMAC_SHA256_BLOCK_LEN) {
        sdk_sha256(key, key_len, k0);
    } else if (key_len > 0) {
        memcpy(k0, key, key_len);
    }
}

static void hmac_seed(sdk_sha256_ctx *inner, sdk_sha256_ctx *outer,
                      const unsigned char k0[SDK_HMAC_SHA256_BLOCK_LEN]) {
    unsigned char pad[SDK_HMAC_SHA256_BLOCK_LEN];
    size_t i;

    for (i = 0; i < SDK_HMAC_SHA256_BLOCK_LEN; ++i) {
        pad[i] = (unsigned char)(k0[i] ^ 0x36u);
    }
    sdk_sha256_init(inner);
    sdk_sha256_update(inner, pad, sizeof pad);

    for (i = 0; i < SDK_HMAC_SHA256_BLOCK_LEN; ++i) {
        pad[i] = (unsigned char)(k0[i] ^ 0x5cu);
    }
    sdk_sha256_init(outer);
    sdk_sha256_update(outer, pad, sizeof pad);

    sdk_secure_wipe(pad, sizeof pad);
}

void sdk_hmac_sha256_init(sdk_hmac_sha256_ctx *ctx,
                          const void *key, size_t key_len) {
    unsigned char k0[SDK_HMAC_SHA256_BLOCK_LEN];

    hmac_expand_key(key, key_len, k0);
    hmac_seed(&ctx->inner, &ctx->outer, k0);
    sdk_secure_wipe(k0, sizeof k0);
}

void sdk_hmac_sha256_update(sdk_hmac_sha256_ctx *ctx,
                            const void *data, size_t len) {
    sdk_sha256_update(&ctx->inner, data, len);
}

void sdk_hmac_sha256_final(sdk_hmac_sha256_ctx *ctx,
                           unsigned char out[SDK_HMAC_SHA256_MAC_LEN]) {
    unsigned char inner_digest[SDK_SHA256_DIGEST_LEN];

    sdk_sha256_final(&ctx->inner, inner_digest);
    sdk_sha256_update(&ctx->outer, inner_digest, sizeof inner_digest);
    sdk_sha256_final(&ctx->outer, out);

    sdk_secure_wipe(inner_digest, sizeof inner_digest);
    sdk_secure_wipe(ctx, sizeof *ctx);
}

void sdk_hmac_sha256(const void *key, size_t key_len,
                     const void *data, size_t len,
                     unsigned char out[SDK_HMAC_SHA256_MAC_LEN]) {
    sdk_hmac_sha256_ctx ctx;

    sdk_hmac_sha256_init(&ctx, key, key_len);
    sdk_hmac_sha256_update(&ctx, data, len);
    sdk_hmac_sha256_final(&ctx, out);
}

void sdk_hmac_sha256_key_init(sdk_hmac_sha256_key *k,
                              const void *key, size_t key_len) {
    unsigned char k0[SDK_HMAC_SHA256_BLOCK_LEN];

    hmac_expand_key(key, key_len, k0);
    hmac_seed(&k->inner_seed, &k->outer_seed, k0);
    sdk_secure_wipe(k0, sizeof k0);
}

void sdk_hmac_sha256_key_free(sdk_hmac_sha256_key *k) {
    sdk_secure_wipe(k, sizeof *k);
}

void sdk_hmac_sha256_with_key(const sdk_hmac_sha256_key *k,
                              const void *data, size_t len,
                              unsigned char out[SDK_HMAC_SHA256_MAC_LEN]) {
    sdk_sha256_ctx inner = k->inner_seed;
    sdk_sha256_ctx outer = k->outer_seed;
    unsigned char inner_digest[SDK_SHA256_DIGEST_LEN];

    sdk_sha256_update(&inner, data, len);
    sdk_sha256_final(&inner, inner_digest);
    sdk_sha256_update(&outer, inner_digest, sizeof inner_digest);
    sdk_sha256_final(&outer, out);

    sdk_secure_wipe(inner_digest, sizeof inner_digest);
    sdk_secure_wipe(&inner, sizeof inner);
    sdk_secure_wipe(&outer, sizeof outer);
}
