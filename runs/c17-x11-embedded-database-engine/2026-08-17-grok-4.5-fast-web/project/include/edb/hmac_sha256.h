#ifndef EDB_HMAC_SHA256_H
#define EDB_HMAC_SHA256_H

#include <stdint.h>
#include <stddef.h>

void edb_hmac_sha256(const uint8_t *key, size_t key_len,
                     const uint8_t *msg, size_t msg_len,
                     uint8_t out[32]);

#endif
