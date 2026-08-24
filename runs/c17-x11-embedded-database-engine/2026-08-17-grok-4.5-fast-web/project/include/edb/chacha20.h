#ifndef EDB_CHACHA20_H
#define EDB_CHACHA20_H

#include <stdint.h>
#include <stddef.h>

/* ChaCha20 block function and stream (project-authored).
 * HChaCha20 for XChaCha20 key derivation.
 */

void edb_chacha20_block(const uint32_t key[8], uint32_t counter,
                        const uint32_t nonce[3], uint32_t out[16]);

void edb_chacha20_xor(const uint8_t key[32], const uint8_t nonce[12],
                      uint32_t counter, const uint8_t *in, uint8_t *out, size_t len);

/* HChaCha20: 32-byte key + 16-byte nonce -> 32-byte subkey */
void edb_hchacha20(const uint8_t key[32], const uint8_t nonce[16], uint8_t out[32]);

#endif
