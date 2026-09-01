/* lzss.h - LZSS compressor/decompressor with the task-pack profile:
 *   4096-byte sliding window, minimum match length 3, maximum match length 18.
 * A bit flag distinguishes literal bytes (1 byte following) from
 * (offset,length) matches. */
#ifndef CE_LZSS_H
#define CE_LZSS_H

#include <stddef.h>

/* Compress input. Returns malloc'd output buffer and sets *out_len.
 * Returns NULL on allocation failure. */
unsigned char *ce_lzss_compress(const unsigned char *in, size_t in_len, size_t *out_len);

/* Decompress. Returns malloc'd output buffer and sets *out_len.
 * Returns NULL if the stream is malformed (caller treats as corrupt). */
unsigned char *ce_lzss_decompress(const unsigned char *in, size_t in_len, size_t *out_len);

#endif /* CE_LZSS_H */
