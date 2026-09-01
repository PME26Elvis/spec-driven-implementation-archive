/* buf.h - dynamic byte buffer used for document source and string building. */
#ifndef CE_BUF_H
#define CE_BUF_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char  *data;   /* always NUL-terminated (extra byte not counted in len) */
    size_t len;
    size_t cap;
} ce_buf;

void ce_buf_init(ce_buf *b);
void ce_buf_free(ce_buf *b);
void ce_buf_reserve(ce_buf *b, size_t extra);
void ce_buf_append(ce_buf *b, const void *data, size_t n);
void ce_buf_append_c(ce_buf *b, char c);
void ce_buf_append_str(ce_buf *b, const char *s);
void ce_buf_append_fmt(ce_buf *b, const char *fmt, ...);
void ce_buf_clear(ce_buf *b);
void ce_buf_set(ce_buf *b, const void *data, size_t n);
void ce_buf_insert(ce_buf *b, size_t pos, const void *data, size_t n);
void ce_buf_erase(ce_buf *b, size_t pos, size_t n);
/* Growable char* version for convenience. */
char *ce_buf_detach(ce_buf *b);

#endif /* CE_BUF_H */
