/* sdk_lzss.h - canonical LZSS codec (docs/19 section 13).
 *
 * Token stream layout:
 *   - one flag byte per group of up to 8 tokens, LSB describes the first token
 *   - flag bit 1 -> literal, followed by exactly one byte
 *   - flag bit 0 -> match, followed by a little-endian u16:
 *        high 12 bits = distance - 1   (0..4095)
 *        low   4 bits = length   - 3   (0..15)
 *   - unused flag bits of the final short group must be zero
 *
 * Encoder is greedy: at each position the longest match in the most recent
 * 4096 bytes wins; equal-length candidates resolve to the nearest one.
 */
#ifndef SDK_LZSS_H
#define SDK_LZSS_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

#define SDK_LZSS_WINDOW      4096
#define SDK_LZSS_MIN_MATCH   3
#define SDK_LZSS_MAX_MATCH   18

/* Compresses `in`/`in_len` into the caller supplied buffer.
 * On success *out_len holds the number of bytes written.
 * Returns SDK_ERR_LIMIT when the output does not fit in out_cap. */
sdk_status sdk_lzss_compress(const uint8_t *in, size_t in_len,
                             uint8_t *out, size_t out_cap, size_t *out_len);

/* Worst case output size for in_len input bytes (all literals). */
size_t sdk_lzss_max_compressed_size(size_t in_len);

/* Decompresses exactly `expected_len` bytes.
 * Rejects: distance beyond produced output, length overrun past
 * expected_len, truncated tokens and trailing garbage. */
sdk_status sdk_lzss_decompress(const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t expected_len);

#endif /* SDK_LZSS_H */
