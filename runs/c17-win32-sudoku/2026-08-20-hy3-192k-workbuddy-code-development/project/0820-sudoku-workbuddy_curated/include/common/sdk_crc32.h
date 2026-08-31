/* sdk_crc32.h - self-implemented CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320).
 *
 * Used by the tinyvcs object envelope and index (docs/19 sections 12 and 14)
 * and by the vault payload framing (docs/19 section 17).
 */
#ifndef SDK_CRC32_H
#define SDK_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t sdk_crc32_init(void);
uint32_t sdk_crc32_update(uint32_t crc, const void *data, size_t len);
uint32_t sdk_crc32_final(uint32_t crc);
uint32_t sdk_crc32(const void *data, size_t len);

#endif /* SDK_CRC32_H */
