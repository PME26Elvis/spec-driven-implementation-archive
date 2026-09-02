#include "utf8.h"
#include <string.h>

static int utf8_decode_one(const unsigned char *s, size_t n, uint32_t *out, size_t *consumed) {
    if (n == 0) return -1;
    unsigned char c0 = s[0];
    if (c0 <= 0x7F) {
        *out = c0;
        *consumed = 1;
        return 0;
    }
    if ((c0 & 0xE0) == 0xC0) {
        if (n < 2 || (s[1] & 0xC0) != 0x80) return -1;
        uint32_t cp = ((c0 & 0x1F) << 6) | (s[1] & 0x3F);
        if (cp < 0x80) return -1; /* overlong */
        *out = cp;
        *consumed = 2;
        return 0;
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (n < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return -1;
        uint32_t cp = ((c0 & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (cp < 0x800) return -1; /* overlong */
        if (cp >= 0xD800 && cp <= 0xDFFF) return -1; /* surrogate */
        *out = cp;
        *consumed = 3;
        return 0;
    }
    if ((c0 & 0xF8) == 0xF0) {
        if (n < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return -1;
        uint32_t cp = ((c0 & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (cp < 0x10000) return -1; /* overlong */
        if (cp > 0x10FFFF) return -1;
        *out = cp;
        *consumed = 4;
        return 0;
    }
    return -1;
}

bool utf8_validate(const unsigned char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint32_t cp;
        size_t cons;
        if (utf8_decode_one(s + i, n - i, &cp, &cons) != 0) return false;
        i += cons;
    }
    return true;
}

size_t utf8_next(const unsigned char *s, size_t n, uint32_t *out) {
    size_t cons;
    if (utf8_decode_one(s, n, out, &cons) != 0) return 0;
    return cons;
}

size_t utf8_scalar_count(const unsigned char *s, size_t n) {
    size_t count = 0, i = 0;
    while (i < n) {
        uint32_t cp;
        size_t cons;
        if (utf8_decode_one(s + i, n - i, &cp, &cons) != 0) return (size_t)-1;
        i += cons;
        count++;
    }
    return count;
}

int utf8_strcmp(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    size_t ia = 0, ib = 0;
    while (ia < an && ib < bn) {
        uint32_t ca, cb;
        size_t ca_n, cb_n;
        if (utf8_decode_one(a + ia, an - ia, &ca, &ca_n) != 0) return -1;
        if (utf8_decode_one(b + ib, bn - ib, &cb, &cb_n) != 0) return 1;
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        ia += ca_n;
        ib += cb_n;
    }
    if (ia < an) return 1;
    if (ib < bn) return -1;
    return 0;
}

static uint32_t fold_ascii(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int utf8_ascii_insensitive_cmp(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    size_t ia = 0, ib = 0;
    while (ia < an && ib < bn) {
        uint32_t ca, cb;
        size_t ca_n, cb_n;
        if (utf8_decode_one(a + ia, an - ia, &ca, &ca_n) != 0) return -1;
        if (utf8_decode_one(b + ib, bn - ib, &cb, &cb_n) != 0) return 1;
        ca = fold_ascii(ca);
        cb = fold_ascii(cb);
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        ia += ca_n;
        ib += cb_n;
    }
    if (ia < an) return 1;
    if (ib < bn) return -1;
    return 0;
}

size_t utf8_find(const unsigned char *hay, size_t hay_n, const unsigned char *needle, size_t needle_n, bool ascii_insensitive) {
    if (needle_n == 0) return 0;
    size_t i = 0;
    while (i < hay_n) {
        size_t j = 0, k = i;
        int match = 1;
        while (j < needle_n) {
            if (k >= hay_n) { match = 0; break; }
            uint32_t hc, nc;
            size_t hn, nn;
            if (utf8_decode_one(hay + k, hay_n - k, &hc, &hn) != 0) { match = 0; break; }
            if (utf8_decode_one(needle + j, needle_n - j, &nc, &nn) != 0) { match = 0; break; }
            if (ascii_insensitive) {
                hc = fold_ascii(hc);
                nc = fold_ascii(nc);
            }
            if (hc != nc) { match = 0; break; }
            k += hn;
            j += nn;
        }
        if (match) return i;
        /* advance one scalar in hay */
        uint32_t dummy;
        size_t adv;
        if (utf8_decode_one(hay + i, hay_n - i, &dummy, &adv) != 0) return (size_t)-1;
        i += adv;
    }
    return (size_t)-1;
}
