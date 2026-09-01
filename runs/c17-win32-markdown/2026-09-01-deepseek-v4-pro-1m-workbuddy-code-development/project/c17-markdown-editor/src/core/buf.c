/* buf.c - dynamic byte buffer. */
#include "buf.h"
#include "ce_common.h"
#include <stdarg.h>
#include <stdio.h>

void ce_buf_init(ce_buf *b){ b->data = NULL; b->len = 0; b->cap = 0; }

void ce_buf_free(ce_buf *b){ if(b->data) ce_free(b->data); b->data = NULL; b->len = b->cap = 0; }

static void ensure(ce_buf *b, size_t need){
    if(b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 32;
    while(nc < b->len + need + 1) nc *= 2;
    b->data = ce_realloc(b->data, nc);
    b->cap = nc;
}

void ce_buf_reserve(ce_buf *b, size_t extra){ ensure(b, extra); }

void ce_buf_append(ce_buf *b, const void *data, size_t n){
    if(n == 0) return;
    ensure(b, n);
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = 0;
}

void ce_buf_append_c(ce_buf *b, char c){ ce_buf_append(b, &c, 1); }

void ce_buf_append_str(ce_buf *b, const char *s){ ce_buf_append(b, s, strlen(s)); }

void ce_buf_append_fmt(ce_buf *b, const char *fmt, ...){
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if(n < 0){ va_end(ap2); return; }
    ensure(b, (size_t)n);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

void ce_buf_clear(ce_buf *b){ b->len = 0; if(b->data) b->data[0] = 0; }

void ce_buf_set(ce_buf *b, const void *data, size_t n){
    b->len = 0;
    ce_buf_append(b, data, n);
}

void ce_buf_insert(ce_buf *b, size_t pos, const void *data, size_t n){
    if(pos > b->len) pos = b->len;
    if(n == 0) return;
    ensure(b, n);
    memmove(b->data + pos + n, b->data + pos, b->len - pos);
    memcpy(b->data + pos, data, n);
    b->len += n;
    b->data[b->len] = 0;
}

void ce_buf_erase(ce_buf *b, size_t pos, size_t n){
    if(pos > b->len) return;
    if(pos + n > b->len) n = b->len - pos;
    memmove(b->data + pos, b->data + pos + n, b->len - pos - n);
    b->len -= n;
    b->data[b->len] = 0;
}

char *ce_buf_detach(ce_buf *b){
    if(!b->data){ char *p = ce_malloc(1); p[0] = 0; return p; }
    char *p = b->data;
    b->data = NULL; b->len = b->cap = 0;
    return p;
}
