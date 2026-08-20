/* png.c — dependency-free PNG writer (RGB, 8-bit, stored DEFLATE). */
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t crc_table[256];
static int crc_ready = 0;
static void crc_init(void) {
  for (uint32_t n = 0; n < 256; n++) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
    crc_table[n] = c;
  }
  crc_ready = 1;
}
static uint32_t crc32_buf(const unsigned char *buf, size_t len) {
  if (!crc_ready) crc_init();
  uint32_t c = 0xFFFFFFFFU;
  for (size_t i = 0; i < len; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFU;
}

/* dynamic byte buffer */
typedef struct { unsigned char *d; size_t n, cap; } Buf;
static void buf_push(Buf *b, unsigned char c) {
  if (b->n + 1 > b->cap) { b->cap = b->cap ? b->cap * 2 : 4096; b->d = realloc(b->d, b->cap); }
  b->d[b->n++] = c;
}
static void buf_u32(Buf *b, uint32_t v) { buf_push(b, (v>>24)&0xff); buf_push(b, (v>>16)&0xff); buf_push(b, (v>>8)&0xff); buf_push(b, v&0xff); }
static void buf_u16(Buf *b, uint32_t v) { buf_push(b, v & 0xff); buf_push(b, (v>>8) & 0xff); } /* little-endian: LEN/NLEN in deflate */

static uint32_t adler32_buf(const unsigned char *data, size_t len) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < len; i++) { a = (a + data[i]) % 65521; b = (b + a) % 65521; }
  return (b << 16) | a;
}

static void write_chunk(Buf *out, const char *type, const unsigned char *data, size_t len) {
  buf_u32(out, (uint32_t)len);
  size_t start = out->n;
  for (int i = 0; i < 4; i++) buf_push(out, (unsigned char)type[i]);
  for (size_t i = 0; i < len; i++) buf_push(out, data[i]);
  uint32_t crc = crc32_buf(out->d + start, len + 4);
  buf_u32(out, crc);
}

int png_write_rgb(const char *path, int w, int h, const unsigned char *rgb) {
  /* build raw scanlines with filter byte 0 */
  size_t stride = (size_t)w * 3;
  size_t rawlen = (size_t)h * (stride + 1);
  unsigned char *raw = malloc(rawlen);
  if (!raw) return -1;
  for (int y = 0; y < h; y++) {
    raw[(size_t)y * (stride + 1)] = 0;
    memcpy(raw + (size_t)y * (stride + 1) + 1, rgb + (size_t)y * stride, stride);
  }

  Buf out; out.d = NULL; out.n = 0; out.cap = 0;
  /* signature */
  static const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
  for (int i = 0; i < 8; i++) buf_push(&out, sig[i]);

  /* IHDR */
  unsigned char ihdr[13];
  ihdr[0] = (w>>24)&0xff; ihdr[1] = (w>>16)&0xff; ihdr[2] = (w>>8)&0xff; ihdr[3] = w&0xff;
  ihdr[4] = (h>>24)&0xff; ihdr[5] = (h>>16)&0xff; ihdr[6] = (h>>8)&0xff; ihdr[7] = h&0xff;
  ihdr[8] = 8;   /* bit depth */
  ihdr[9] = 2;   /* color type RGB */
  ihdr[10] = 0;  /* compression */
  ihdr[11] = 0;  /* filter */
  ihdr[12] = 0;  /* interlace */
  write_chunk(&out, "IHDR", ihdr, 13);

  /* IDAT: zlib stored (no compression). A stored DEFLATE block's LEN field is
     16-bit (max 65535), so large images must be split into multiple blocks. */
  uint32_t adler = adler32_buf(raw, rawlen);
  Buf idat; idat.d = NULL; idat.n = 0; idat.cap = 0;
  buf_push(&idat, 0x78); buf_push(&idat, 0x01);            /* zlib header */
  size_t off = 0;
  while (off < rawlen) {
    size_t remain = rawlen - off;
    uint32_t L = (remain > 65535) ? 65535 : (uint32_t)remain;
    int last = (off + L >= rawlen);
    buf_push(&idat, last ? 0x01 : 0x00);                   /* deflate block header: BFINAL, BTYPE=00 */
    buf_u16(&idat, L & 0xffff); buf_u16(&idat, (~L) & 0xffff); /* LEN (LE), NLEN (one's complement) */
    for (uint32_t k = 0; k < L; k++) buf_push(&idat, raw[off + k]);
    off += L;
  }
  buf_u32(&idat, adler);
  write_chunk(&out, "IDAT", idat.d, idat.n);
  free(idat.d);

  /* IEND */
  write_chunk(&out, "IEND", NULL, 0);

  FILE *f = fopen(path, "wb");
  if (!f) { free(raw); free(out.d); return -1; }
  size_t wr = fwrite(out.d, 1, out.n, f);
  fclose(f);
  free(raw); free(out.d);
  return (wr == out.n) ? 0 : -1;
}
