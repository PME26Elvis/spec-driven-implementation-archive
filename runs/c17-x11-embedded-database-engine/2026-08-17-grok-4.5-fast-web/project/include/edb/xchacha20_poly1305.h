#ifndef EDB_XCHACHA20_POLY1305_H
#define EDB_XCHACHA20_POLY1305_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Authenticated encryption: XChaCha20-Poly1305 (IETF style).
 * Nonce is 24 bytes. Tag is 16 bytes.
 * Returns true on success / authentication success.
 */

bool edb_xchacha20_poly1305_encrypt(
    const uint8_t key[32],
    const uint8_t nonce[24],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *plaintext, size_t pt_len,
    uint8_t *ciphertext,          /* same length as plaintext */
    uint8_t tag[16]);

bool edb_xchacha20_poly1305_decrypt(
    const uint8_t key[32],
    const uint8_t nonce[24],
    const uint8_t *aad, size_t aad_len,
    const uint8_t *ciphertext, size_t ct_len,
    const uint8_t tag[16],
    uint8_t *plaintext);          /* same length as ciphertext */

#endif
