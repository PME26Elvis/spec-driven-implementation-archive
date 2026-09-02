#ifndef TABLETOOL_CODE128_H
#define TABLETOOL_CODE128_H
#include <stddef.h>
int code128_encode(const char *payload, size_t n, int *codes, int *n_codes);
const char *code128_pattern(int v);
#endif
