#ifndef TABLETOOL_URL_H
#define TABLETOOL_URL_H
#include <stddef.h>
int url_canonicalize(const char *s, size_t n, char **out, size_t *out_len);
#endif
