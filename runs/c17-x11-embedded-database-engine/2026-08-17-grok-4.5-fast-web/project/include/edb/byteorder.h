#ifndef EDB_BYTEORDER_H
#define EDB_BYTEORDER_H

#include <stdint.h>
#include <stddef.h>

/* All multibyte fields on disk use little-endian (documented endian independent of host). */

static inline void edb_store_u16_le(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t)(v & 0xffu);
    dst[1] = (uint8_t)((v >> 8) & 0xffu);
}

static inline uint16_t edb_load_u16_le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline void edb_store_u32_le(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xffu);
    dst[1] = (uint8_t)((v >> 8) & 0xffu);
    dst[2] = (uint8_t)((v >> 16) & 0xffu);
    dst[3] = (uint8_t)((v >> 24) & 0xffu);
}

static inline uint32_t edb_load_u32_le(const uint8_t *src) {
    return (uint32_t)src[0]
         | ((uint32_t)src[1] << 8)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 24);
}

static inline void edb_store_u64_le(uint8_t *dst, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t)(v & 0xffu);
        v >>= 8;
    }
}

static inline uint64_t edb_load_u64_le(const uint8_t *src) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) {
        v = (v << 8) | src[i];
    }
    return v;
}

#endif /* EDB_BYTEORDER_H */
