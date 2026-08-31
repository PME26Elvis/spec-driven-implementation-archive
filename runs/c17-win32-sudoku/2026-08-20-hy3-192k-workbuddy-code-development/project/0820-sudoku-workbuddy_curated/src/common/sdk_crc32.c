/* sdk_crc32.c - CRC-32 with the reflected IEEE polynomial, table built at
 * first use from the polynomial itself (no vendored table blob).
 */

#include "common/sdk_crc32.h"

static uint32_t g_table[256];
static int g_ready = 0;

static void build_table(void) {
    for (uint32_t i = 0; i < 256u; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        g_table[i] = c;
    }
    g_ready = 1;
}

uint32_t sdk_crc32_init(void) {
    return 0xFFFFFFFFu;
}

uint32_t sdk_crc32_update(uint32_t crc, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    if (!g_ready) {
        build_table();
    }
    while (len--) {
        crc = g_table[(crc ^ (uint32_t)(*p++)) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

uint32_t sdk_crc32_final(uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

uint32_t sdk_crc32(const void *data, size_t len) {
    return sdk_crc32_final(sdk_crc32_update(sdk_crc32_init(), data, len));
}
