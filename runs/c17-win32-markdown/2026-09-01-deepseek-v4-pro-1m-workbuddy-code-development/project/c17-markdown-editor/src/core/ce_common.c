/* ce_common.c - shared helpers. */
#include "ce_common.h"
#include <stdarg.h>

void *ce_malloc(size_t n){ void *p = malloc(n ? n : 1); if(!p){ fprintf(stderr, "fatal: out of memory (%zu bytes)\n", n); abort(); } return p; }
void *ce_calloc(size_t n, size_t sz){ void *p = calloc(n ? n : 1, sz ? sz : 1); if(!p){ fprintf(stderr, "fatal: out of memory\n"); abort(); } return p; }
void *ce_realloc(void *p, size_t n){ void *q = realloc(p, n ? n : 1); if(!q){ fprintf(stderr, "fatal: out of memory (%zu bytes)\n", n); abort(); } return q; }
void  ce_free(void *p){ free(p); }

char *ce_strdup(const char *s){ size_t n = strlen(s); char *p = ce_malloc(n+1); memcpy(p, s, n+1); return p; }
char *ce_strndup(const char *s, size_t n){ char *p = ce_malloc(n+1); memcpy(p, s, n); p[n] = 0; return p; }

static int lower(int c){ return (c >= 'A' && c <= 'Z') ? c + ('a'-'A') : c; }

int ce_strcasecmp(const char *a, const char *b){
    while(*a && *b){
        int x = lower((unsigned char)*a), y = lower((unsigned char)*b);
        if(x != y) return x - y;
        a++; b++;
    }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}
int ce_strncasecmp(const char *a, const char *b, size_t n){
    while(n && *a && *b){
        int x = lower((unsigned char)*a), y = lower((unsigned char)*b);
        if(x != y) return x - y;
        a++; b++; n--;
    }
    if(n == 0) return 0;
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

int ce_starts_with(const char *s, const char *prefix){ return strncmp(s, prefix, strlen(prefix)) == 0; }
int ce_ends_with(const char *s, const char *suffix){
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && memcmp(s + ls - lf, suffix, lf) == 0;
}

void ce_log_warn(const char *fmt, ...){ va_list ap; va_start(ap, fmt); fprintf(stderr, "warning: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); }
void ce_log_error(const char *fmt, ...){ va_list ap; va_start(ap, fmt); fprintf(stderr, "error: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap); }
