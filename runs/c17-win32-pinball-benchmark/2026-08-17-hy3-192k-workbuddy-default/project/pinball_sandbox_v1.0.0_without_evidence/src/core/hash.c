#include "hash.h"
#include <string.h>

#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME  1099511628211ULL

void fnv1a64_init(uint64_t *h) { *h = FNV_OFFSET; }
void fnv1a64_update(uint64_t *h, const void *data, size_t len) {
  const unsigned char *p = (const unsigned char*)data;
  for (size_t i = 0; i < len; i++) {
    *h ^= (uint64_t)p[i];
    *h *= FNV_PRIME;
  }
}
uint64_t fnv1a64_final(uint64_t h) { return h; }

uint64_t fnv1a64(const void *data, size_t len) {
  uint64_t h; fnv1a64_init(&h); fnv1a64_update(&h, data, len); return fnv1a64_final(h);
}
uint64_t fnv1a64_str(const char *s) { return fnv1a64(s, s ? strlen(s) : 0); }

void fnv1a64_to_hex(uint64_t h, char *out) {
  static const char *hexd = "0123456789abcdef";
  for (int i = 15; i >= 0; i--) {
    out[i] = hexd[h & 0xF];
    h >>= 4;
  }
  out[16] = 0;
}
