#ifndef DARC_LZH1_H
#define DARC_LZH1_H

#include <stdint.h>
#include <stddef.h>

/* Compress raw data to LZH1 payload. Returns allocated buffer, sets *out_len.
   Caller frees. Returns NULL on OOM. */
uint8_t *darc_lzh1_compress(const uint8_t *in, size_t in_len, size_t *out_len);

/* Decompress LZH1 payload to raw. Returns allocated buffer, sets *out_len.
   Validates and returns NULL on error. */
uint8_t *darc_lzh1_decompress(const uint8_t *in, size_t in_len, size_t expected_raw_len, size_t *out_len);

#endif
