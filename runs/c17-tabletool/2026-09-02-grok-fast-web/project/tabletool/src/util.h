#ifndef TABLETOOL_UTIL_H
#define TABLETOOL_UTIL_H
#include "common.h"

/* Read entire file in binary mode into allocated buffer. NUL-terminated for convenience. */
int read_file_binary(const char *path, unsigned char **out, size_t *out_len);

/* Write buffer to path in binary mode. */
int write_file_binary(const char *path, const void *data, size_t len);

/* Escape for report */
char *report_escape(const char *s, size_t n);

#endif
