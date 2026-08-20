#ifndef PB_PNG_H
#define PB_PNG_H

/* Minimal dependency-free PNG writer (colortype 2 = RGB, 8-bit, no alpha).
   Uses zlib "stored" (uncompressed) DEFLATE blocks — no external zlib needed. */
int png_write_rgb(const char *path, int w, int h, const unsigned char *rgb /* w*h*3, top-left origin */);

#endif /* PB_PNG_H */
