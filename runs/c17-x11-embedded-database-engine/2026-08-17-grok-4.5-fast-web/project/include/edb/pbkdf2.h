#ifndef EDB_PBKDF2_H
#define EDB_PBKDF2_H

#include <stdint.h>
#include <stddef.h>

/* PBKDF2-HMAC-SHA-256 as required by CRYPTO-002 */
int edb_pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
                           const uint8_t *salt, size_t salt_len,
                           uint32_t iterations,
                           uint8_t *dk, size_t dk_len);

#endif
