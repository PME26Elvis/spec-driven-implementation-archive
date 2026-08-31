/* sdk_aead.h - XChaCha20-Poly1305 AEAD, self-implemented.
 *
 * Construction (draft-irtf-cfrg-xchacha, RFC 8439):
 *   subkey       = HChaCha20(key, nonce[0..15])
 *   inner nonce  = 00 00 00 00 || nonce[16..23]
 *   poly key     = ChaCha20(subkey, counter = 0, inner nonce)[0..31]
 *   ciphertext   = plaintext XOR ChaCha20(subkey, counter = 1, inner nonce)
 *   tag          = Poly1305(poly key,
 *                           aad || pad16(aad) ||
 *                           ct  || pad16(ct)  ||
 *                           le64(|aad|) || le64(|ct|))
 *
 * docs/08 section 4: a fresh 24-byte nonce from BCryptGenRandom is required for
 * every write, the tag must be verified before any plaintext is parsed, and the
 * tag comparison must be constant time (docs/08 section 24).
 */
#ifndef SDK_AEAD_H
#define SDK_AEAD_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

#define SDK_XCHACHA20POLY1305_KEY_LEN   32u
#define SDK_XCHACHA20POLY1305_NONCE_LEN 24u
#define SDK_XCHACHA20POLY1305_TAG_LEN   16u

/* Encrypts plaintext in place-capable form: ciphertext may alias plaintext.
 * ciphertext must have room for plaintext_len bytes; tag receives 16 bytes. */
sdk_status sdk_xchacha20poly1305_encrypt(
    const unsigned char key[SDK_XCHACHA20POLY1305_KEY_LEN],
    const unsigned char nonce[SDK_XCHACHA20POLY1305_NONCE_LEN],
    const unsigned char *aad, size_t aad_len,
    const unsigned char *plaintext, size_t plaintext_len,
    unsigned char *ciphertext,
    unsigned char tag[SDK_XCHACHA20POLY1305_TAG_LEN]);

/* Verifies the tag first and only then produces plaintext.  Returns
 * SDK_ERR_AUTH when authentication fails; in that case no plaintext byte is
 * written to the caller buffer. */
sdk_status sdk_xchacha20poly1305_decrypt(
    const unsigned char key[SDK_XCHACHA20POLY1305_KEY_LEN],
    const unsigned char nonce[SDK_XCHACHA20POLY1305_NONCE_LEN],
    const unsigned char *aad, size_t aad_len,
    const unsigned char *ciphertext, size_t ciphertext_len,
    const unsigned char tag[SDK_XCHACHA20POLY1305_TAG_LEN],
    unsigned char *plaintext);

#endif /* SDK_AEAD_H */
