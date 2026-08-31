#ifndef CVC_SHA256_H
#define CVC_SHA256_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t h[8]; uint64_t bits; unsigned char block[64]; size_t used; } CvcSha256;
void cvc_sha256_init(CvcSha256 *s);
void cvc_sha256_update(CvcSha256 *s, const void *data, size_t n);
void cvc_sha256_final(CvcSha256 *s, uint8_t out[32]);
void cvc_sha256(const void *data, size_t n, uint8_t out[32]);
#endif
