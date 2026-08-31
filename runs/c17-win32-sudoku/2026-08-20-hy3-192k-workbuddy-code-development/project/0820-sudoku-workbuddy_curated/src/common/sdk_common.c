/* sdk_common.c - shared buffers, readers, hex, constant-time compare. */

#include "common/sdk_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *sdk_status_name(sdk_status st) {
    switch (st) {
    case SDK_OK:              return "OK";
    case SDK_ERR_USAGE:       return "USAGE";
    case SDK_ERR_DATA:        return "DATA";
    case SDK_ERR_IO:          return "IO";
    case SDK_ERR_VERIFY:      return "VERIFY";
    case SDK_ERR_NOMEM:       return "NOMEM";
    case SDK_ERR_LIMIT:       return "LIMIT";
    case SDK_ERR_NOT_FOUND:   return "NOT_FOUND";
    case SDK_ERR_EXISTS:      return "EXISTS";
    case SDK_ERR_BUSY:        return "BUSY";
    case SDK_ERR_AUTH:        return "AUTH";
    case SDK_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case SDK_ERR_INTERNAL:    return "INTERNAL";
    }
    return "UNKNOWN";
}

int sdk_status_to_exit(sdk_status st) {
    switch (st) {
    case SDK_OK:              return SDK_EXIT_OK;
    case SDK_ERR_USAGE:       return SDK_EXIT_USAGE;
    case SDK_ERR_NOT_FOUND:   return SDK_EXIT_USAGE;
    case SDK_ERR_DATA:        return SDK_EXIT_DATA;
    case SDK_ERR_UNSUPPORTED: return SDK_EXIT_DATA;
    case SDK_ERR_AUTH:        return SDK_EXIT_DATA;
    case SDK_ERR_IO:          return SDK_EXIT_IO;
    case SDK_ERR_NOMEM:       return SDK_EXIT_IO;
    case SDK_ERR_LIMIT:       return SDK_EXIT_IO;
    case SDK_ERR_EXISTS:      return SDK_EXIT_IO;
    case SDK_ERR_BUSY:        return SDK_EXIT_IO;
    case SDK_ERR_VERIFY:      return SDK_EXIT_VERIFY;
    case SDK_ERR_INTERNAL:    return SDK_EXIT_INTERNAL;
    }
    return SDK_EXIT_INTERNAL;
}

int sdk_size_add(size_t a, size_t b, size_t *out) {
    if (a > (size_t)-1 - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

int sdk_size_mul(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > (size_t)-1 / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

void sdk_buf_init(sdk_buf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->failed = 0;
}

void sdk_buf_free(sdk_buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->failed = 0;
}

int sdk_buf_reserve(sdk_buf *b, size_t extra) {
    size_t need;
    if (b->failed) {
        return 0;
    }
    if (!sdk_size_add(b->len, extra, &need)) {
        b->failed = 1;
        return 0;
    }
    if (need <= b->cap) {
        return 1;
    }
    size_t cap = b->cap ? b->cap : 64;
    while (cap < need) {
        if (cap > ((size_t)-1) / 2) {
            b->failed = 1;
            return 0;
        }
        cap *= 2;
    }
    uint8_t *p = (uint8_t *)realloc(b->data, cap);
    if (!p) {
        b->failed = 1;
        return 0;
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

int sdk_buf_append(sdk_buf *b, const void *data, size_t n) {
    if (n == 0) {
        return b->failed ? 0 : 1;
    }
    if (!sdk_buf_reserve(b, n)) {
        return 0;
    }
    memcpy(b->data + b->len, data, n);
    b->len += n;
    return 1;
}

int sdk_buf_append_u8(sdk_buf *b, uint8_t v) {
    return sdk_buf_append(b, &v, 1);
}

int sdk_buf_append_u16le(sdk_buf *b, uint16_t v) {
    uint8_t t[2];
    sdk_put_u16le(t, v);
    return sdk_buf_append(b, t, sizeof t);
}

int sdk_buf_append_u32le(sdk_buf *b, uint32_t v) {
    uint8_t t[4];
    sdk_put_u32le(t, v);
    return sdk_buf_append(b, t, sizeof t);
}

int sdk_buf_append_u64le(sdk_buf *b, uint64_t v) {
    uint8_t t[8];
    sdk_put_u64le(t, v);
    return sdk_buf_append(b, t, sizeof t);
}

int sdk_buf_append_i64le(sdk_buf *b, int64_t v) {
    return sdk_buf_append_u64le(b, (uint64_t)v);
}

int sdk_buf_append_cstr(sdk_buf *b, const char *s) {
    return sdk_buf_append(b, s, strlen(s));
}

int sdk_buf_appendf(sdk_buf *b, const char *fmt, ...) {
    char stackbuf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(stackbuf, sizeof stackbuf, fmt, ap);
    va_end(ap);
    if (n < 0) {
        b->failed = 1;
        return 0;
    }
    if ((size_t)n < sizeof stackbuf) {
        return sdk_buf_append(b, stackbuf, (size_t)n);
    }
    /* Long output: allocate exactly. */
    size_t need = (size_t)n + 1;
    char *heap = (char *)malloc(need);
    if (!heap) {
        b->failed = 1;
        return 0;
    }
    va_start(ap, fmt);
    int n2 = vsnprintf(heap, need, fmt, ap);
    va_end(ap);
    if (n2 < 0) {
        free(heap);
        b->failed = 1;
        return 0;
    }
    int ok = sdk_buf_append(b, heap, (size_t)n2);
    free(heap);
    return ok;
}

void sdk_rd_init(sdk_rd *r, const void *data, size_t len) {
    r->data = (const uint8_t *)data;
    r->len = len;
    r->pos = 0;
    r->failed = 0;
}

const uint8_t *sdk_rd_take(sdk_rd *r, size_t n) {
    if (r->failed) {
        return NULL;
    }
    if (n > r->len - r->pos) {
        r->failed = 1;
        return NULL;
    }
    const uint8_t *p = r->data + r->pos;
    r->pos += n;
    return p;
}

int sdk_rd_bytes(sdk_rd *r, void *out, size_t n) {
    const uint8_t *p = sdk_rd_take(r, n);
    if (!p) {
        return 0;
    }
    if (n) {
        memcpy(out, p, n);
    }
    return 1;
}

int sdk_rd_u8(sdk_rd *r, uint8_t *out) {
    const uint8_t *p = sdk_rd_take(r, 1);
    if (!p) {
        return 0;
    }
    *out = p[0];
    return 1;
}

int sdk_rd_u16le(sdk_rd *r, uint16_t *out) {
    const uint8_t *p = sdk_rd_take(r, 2);
    if (!p) {
        return 0;
    }
    *out = sdk_get_u16le(p);
    return 1;
}

int sdk_rd_u32le(sdk_rd *r, uint32_t *out) {
    const uint8_t *p = sdk_rd_take(r, 4);
    if (!p) {
        return 0;
    }
    *out = sdk_get_u32le(p);
    return 1;
}

int sdk_rd_u64le(sdk_rd *r, uint64_t *out) {
    const uint8_t *p = sdk_rd_take(r, 8);
    if (!p) {
        return 0;
    }
    *out = sdk_get_u64le(p);
    return 1;
}

int sdk_rd_i64le(sdk_rd *r, int64_t *out) {
    uint64_t v;
    if (!sdk_rd_u64le(r, &v)) {
        return 0;
    }
    *out = (int64_t)v;
    return 1;
}

size_t sdk_rd_remaining(const sdk_rd *r) {
    return r->failed ? 0 : (r->len - r->pos);
}

int sdk_rd_at_end(const sdk_rd *r) {
    return !r->failed && r->pos == r->len;
}

void sdk_hex_encode(const uint8_t *in, size_t n, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) {
        out[2 * i] = digits[(in[i] >> 4) & 0x0F];
        out[2 * i + 1] = digits[in[i] & 0x0F];
    }
    out[2 * n] = '\0';
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int sdk_hex_decode(const char *in, size_t n_hex, uint8_t *out) {
    if (n_hex % 2u != 0u) {
        return 0;
    }
    for (size_t i = 0; i < n_hex; i += 2) {
        int hi = hexval(in[i]);
        int lo = hexval(in[i + 1]);
        if (hi < 0 || lo < 0) {
            return 0;
        }
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

int sdk_ct_equal(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    uint8_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc |= (uint8_t)(x[i] ^ y[i]);
    }
    return acc == 0;
}

void sdk_secure_wipe(void *p, size_t n) {
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n--) {
        *v++ = 0;
    }
}
