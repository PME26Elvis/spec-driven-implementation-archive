/* sdk_pbkdf2.h - PBKDF2-HMAC-SHA-256 (RFC 8018 / RFC 2898), self-implemented.
 *
 * docs/08 section 3: the production vault iteration count is fixed at 200000
 * and is persisted in the vault header.  Test mode may use a smaller value for
 * non-performance tests, which is why the count is a parameter here and the
 * production constant lives in the vault module.
 */
#ifndef SDK_PBKDF2_H
#define SDK_PBKDF2_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

#define SDK_PBKDF2_PRODUCTION_ITERATIONS 200000u

/* Derives dk_len bytes from (password, salt).  iterations must be >= 1 and
 * dk_len must be >= 1.  Returns SDK_OK, or SDK_ERR_INVALID on bad arguments,
 * or SDK_ERR_LIMIT when dk_len exceeds the PBKDF2 output limit. */
sdk_status sdk_pbkdf2_hmac_sha256(const void *password, size_t password_len,
                                  const void *salt, size_t salt_len,
                                  uint32_t iterations,
                                  unsigned char *dk, size_t dk_len);

#endif /* SDK_PBKDF2_H */
