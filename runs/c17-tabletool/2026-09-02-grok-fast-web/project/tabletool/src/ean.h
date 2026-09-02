#ifndef TABLETOOL_EAN_H
#define TABLETOOL_EAN_H
#include <stddef.h>
int ean13_check_digit(const char *d12);
int ean13_canonicalize(const char *s, size_t n, char *out /*14 bytes*/);
int ean13_encode_modules(const char *ean13, char *out /*96 bytes*/);
#endif
