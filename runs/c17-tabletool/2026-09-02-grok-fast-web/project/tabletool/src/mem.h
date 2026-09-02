#ifndef TABLETOOL_MEM_H
#define TABLETOOL_MEM_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

void *tt_malloc(size_t n);
void *tt_calloc(size_t n, size_t sz);
void *tt_realloc(void *p, size_t n);
char *tt_strdup(const char *s);
char *tt_strndup(const char *s, size_t n);
void tt_free(void *p);

/* checked size arithmetic */
bool tt_size_mul(size_t a, size_t b, size_t *out);
bool tt_size_add(size_t a, size_t b, size_t *out);

#endif
