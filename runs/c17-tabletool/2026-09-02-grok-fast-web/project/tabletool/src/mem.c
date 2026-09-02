#include "mem.h"
#include <stdio.h>

void *tt_malloc(size_t n) {
    if (n == 0) n = 1;
    void *p = malloc(n);
    return p;
}

void *tt_calloc(size_t n, size_t sz) {
    if (n == 0 || sz == 0) return calloc(1, 1);
    size_t total;
    if (!tt_size_mul(n, sz, &total)) return NULL;
    return calloc(n, sz);
}

void *tt_realloc(void *p, size_t n) {
    if (n == 0) n = 1;
    return realloc(p, n);
}

char *tt_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = tt_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

char *tt_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    char *p = tt_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

void tt_free(void *p) {
    free(p);
}

bool tt_size_mul(size_t a, size_t b, size_t *out) {
    if (a == 0 || b == 0) { *out = 0; return true; }
    if (a > SIZE_MAX / b) return false;
    *out = a * b;
    return true;
}

bool tt_size_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}
