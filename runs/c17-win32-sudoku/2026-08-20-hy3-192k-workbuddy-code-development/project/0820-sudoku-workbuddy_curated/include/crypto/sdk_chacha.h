/* sdk_chacha.h - ChaCha20 and HChaCha20, self-implemented (RFC 8439 and
 * draft-irtf-cfrg-xchacha).
 *
 * docs/08 section 4 requires the ChaCha20 core and HChaCha20 to be in-tree.
 */
#ifndef SDK_CHACHA_H
#define SDK_CHACHA_H

#include <stddef.h>
#include <stdint.h>

#define SDK_CHACHA20_KEY_LEN    32u
#define SDK_CHACHA20_NONCE_LEN  12u
#define SDK_CHACHA20_BLOCK_LEN  64u
#define SDK_HCHACHA20_NONCE_LEN 16u
#define SDK_HCHACHA20_OUT_LEN   32u

/* Produces the keystream block for (key, counter, nonce) into out. */
void sdk_chacha20_block(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                        uint32_t counter,
                        const unsigned char nonce[SDK_CHACHA20_NONCE_LEN],
                        unsigned char out[SDK_CHACHA20_BLOCK_LEN]);

/* XORs len bytes of keystream, starting at the given block counter, into out.
 * in and out may alias exactly. */
void sdk_chacha20_xor(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                      uint32_t counter,
                      const unsigned char nonce[SDK_CHACHA20_NONCE_LEN],
                      const unsigned char *in, unsigned char *out, size_t len);

/* HChaCha20 subkey derivation: 20 rounds, no feed-forward addition, output is
 * state words 0..3 and 12..15 in little-endian order. */
void sdk_hchacha20(const unsigned char key[SDK_CHACHA20_KEY_LEN],
                   const unsigned char nonce[SDK_HCHACHA20_NONCE_LEN],
                   unsigned char out[SDK_HCHACHA20_OUT_LEN]);

#endif /* SDK_CHACHA_H */
