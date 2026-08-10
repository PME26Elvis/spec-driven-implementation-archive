#ifndef MDEDIT_IMAGE_H
#define MDEDIT_IMAGE_H

#include "mdedit/core.h"

typedef enum {
    MD_IMAGE_UNKNOWN,
    MD_IMAGE_PNG,
    MD_IMAGE_JPEG,
    MD_IMAGE_BMP
} MdImageFormat;

typedef struct {
    uint8_t *rgba;
    uint32_t width;
    uint32_t height;
    MdImageFormat format;
} MdImage;

void md_image_init(MdImage *image);
void md_image_free(MdImage *image);
MdImageFormat md_image_detect(const uint8_t *data, size_t len);
const char *md_image_mime(MdImageFormat format);
const char *md_image_extension(MdImageFormat format);
bool md_image_decode(const uint8_t *data, size_t len, MdImage *image,
                     char *error, size_t error_cap);
bool md_image_load(const char *path, MdImage *image, MdBytes *original,
                   char *error, size_t error_cap);
bool md_image_write_png(const char *path, const uint8_t *rgba,
                        uint32_t width, uint32_t height,
                        char *error, size_t error_cap);
bool md_image_write_jpeg(const char *path, const uint8_t *rgba,
                         uint32_t width, uint32_t height, int quality,
                         char *error, size_t error_cap);
bool md_image_parse_data_uri(const char *uri, size_t len,
                             MdImageFormat *format, MdBytes *bytes,
                             char *error, size_t error_cap);
bool md_image_make_data_uri(MdImageFormat format, const uint8_t *data,
                            size_t len, MdBuf *out);

#endif
