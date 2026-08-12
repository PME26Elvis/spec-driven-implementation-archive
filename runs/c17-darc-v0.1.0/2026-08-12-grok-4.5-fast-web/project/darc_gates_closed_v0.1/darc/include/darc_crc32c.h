#ifndef DARC_CRC32C_H
#define DARC_CRC32C_H

#include <stdint.h>
#include <stddef.h>

uint32_t darc_crc32c(const void *data, size_t len);
uint32_t darc_crc32c_update(uint32_t crc, const void *data, size_t len);

#endif
