/* sdk_poly1305.h - Poly1305 one-time authenticator, self-implemented
 * (RFC 8439 section 2.5).
 *
 * docs/08 section 4 requires Poly1305 in-tree; docs/08 section 24 requires that
 * no secret-dependent branch, variable loop bound or early-exit comparison is
 * used.  The 26-bit limb representation below keeps every arithmetic step
 * fixed-length and branch free.
 */
#ifndef SDK_POLY1305_H
#define SDK_POLY1305_H

#include <stddef.h>
#include <stdint.h>

#define SDK_POLY1305_KEY_LEN 32u
#define SDK_POLY1305_TAG_LEN 16u

typedef struct sdk_poly1305_ctx {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    size_t   leftover;
    unsigned char buffer[16];
    unsigned char final_block;
} sdk_poly1305_ctx;

void sdk_poly1305_init(sdk_poly1305_ctx *ctx,
                       const unsigned char key[SDK_POLY1305_KEY_LEN]);
void sdk_poly1305_update(sdk_poly1305_ctx *ctx,
                         const unsigned char *data, size_t len);
void sdk_poly1305_final(sdk_poly1305_ctx *ctx,
                        unsigned char tag[SDK_POLY1305_TAG_LEN]);
void sdk_poly1305(const unsigned char key[SDK_POLY1305_KEY_LEN],
                  const unsigned char *data, size_t len,
                  unsigned char tag[SDK_POLY1305_TAG_LEN]);

#endif /* SDK_POLY1305_H */
