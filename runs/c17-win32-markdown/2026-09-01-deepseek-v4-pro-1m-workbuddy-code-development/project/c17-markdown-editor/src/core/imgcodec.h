/* imgcodec.h - image codec boundary using Windows Imaging Component (WIC).
 * This is the narrow system codec exception permitted by the spec: it only
 * decodes/encodes PNG/JPEG/BMP pixel data; all asset/Base64/layout/resize
 * logic remains application-authored. */
#ifndef CE_IMGCODEC_H
#define CE_IMGCODEC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define IMG_FMT_PNG 1
#define IMG_FMT_JPEG 2
#define IMG_FMT_BMP 3

/* Decode PNG/JPEG/BMP bytes to RGBA8. Returns malloc'd buffer (caller frees),
 * sets width and height via *w and *h. NULL on failure (corrupt/unsupported). */
uint8_t *img_decode(const unsigned char *data, size_t len, int *w, int *h);

/* Encode RGBA8 pixels to the given format. Returns malloc'd buffer (caller
 * frees), sets *out_len. NULL on failure. */
unsigned char *img_encode(const uint8_t *rgba, int w, int h, int fmt, size_t *out_len);

/* Return a MIME subtype string for a format ("png"/"jpeg"/"bmp"). */
const char *img_mime(int fmt);

#endif /* CE_IMGCODEC_H */
