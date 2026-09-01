#include "utf8.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int utf8_decode(const uint8_t *s, size_t len, uint32_t *out){
    uint8_t b0 = s[0];
    uint32_t cp;
    if(b0 < 0x80){
        if(out) *out = b0;
        return 1;
    }
    if(b0 >= 0xC2 && b0 <= 0xDF){
        if(len < 2) return 0;
        if((s[1] & 0xC0) != 0x80) return 0;
        cp = ((uint32_t)(b0 & 0x1F) << 6) | (s[1] & 0x3F);
        if(out) *out = cp;
        return 2;
    }
    if(b0 >= 0xE0 && b0 <= 0xEF){
        if(len < 3) return 0;
        if((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
        cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        /* reject overlong and surrogates */
        if(cp < 0x800) return 0;
        if(cp >= 0xD800 && cp <= 0xDFFF) return 0;
        if(out) *out = cp;
        return 3;
    }
    if(b0 >= 0xF0 && b0 <= 0xF4){
        if(len < 4) return 0;
        if((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return 0;
        cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
             ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if(cp < 0x10000 || cp > 0x10FFFF) return 0;
        if(out) *out = cp;
        return 4;
    }
    return 0; /* continuation byte or 0xF5..0xFF leading */
}

int utf8_validate(const uint8_t *s, size_t len){
    size_t i = 0;
    while(i < len){
        int n = utf8_decode(s + i, len - i, NULL);
        if(n == 0) return -1;
        i += (size_t)n;
    }
    return 0;
}

int utf8_valid_no_nul(const uint8_t *s, size_t len){
    size_t i = 0;
    while(i < len){
        if(s[i] == 0) return 0;
        int n = utf8_decode(s + i, len - i, NULL);
        if(n == 0) return -1;
        i += (size_t)n;
    }
    return 1;
}

int utf8_encode(uint32_t cp, uint8_t dst[4]){
    if(cp <= 0x7F){
        dst[0] = (uint8_t)cp; return 1;
    } else if(cp <= 0x7FF){
        dst[0] = (uint8_t)(0xC0 | (cp >> 6));
        dst[1] = (uint8_t)(0x80 | (cp & 0x3F)); return 2;
    } else if(cp <= 0xFFFF){
        if(cp >= 0xD800 && cp <= 0xDFFF) return 0; /* surrogate */
        dst[0] = (uint8_t)(0xE0 | (cp >> 12));
        dst[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        dst[2] = (uint8_t)(0x80 | (cp & 0x3F)); return 3;
    } else if(cp <= 0x10FFFF){
        dst[0] = (uint8_t)(0xF0 | (cp >> 18));
        dst[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
        dst[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        dst[3] = (uint8_t)(0x80 | (cp & 0x3F)); return 4;
    }
    return 0;
}

int utf16_push_utf8(uint8_t **buf, size_t *len, size_t *cap, uint32_t cp){
    uint8_t enc[4];
    int n = utf8_encode(cp, enc);
    if(n == 0) return -1;
    if(*len + (size_t)n + 1 > *cap){
        size_t ncap = *cap ? *cap * 2 : 64;
        while(ncap < *len + (size_t)n + 1) ncap *= 2;
        uint8_t *nb = (uint8_t*)realloc(*buf, ncap);
        if(!nb) return -1;
        *buf = nb; *cap = ncap;
    }
    memcpy(*buf + *len, enc, (size_t)n);
    *len += (size_t)n;
    (*buf)[*len] = 0;
    return 0;
}

char *utf8_from_utf16(const uint16_t *src, size_t n, size_t *out_len){
    size_t i = 0;
    uint8_t *buf = NULL; size_t len = 0, cap = 0;
    while(i < n){
        uint32_t cp;
        uint16_t u = src[i];
        if(u < 0xD800 || u > 0xDFFF){
            cp = u;
            i++;
        } else if(u >= 0xD800 && u <= 0xDBFF){
            if(i + 1 >= n) goto invalid; /* unpaired high surrogate */
            uint16_t lo = src[i+1];
            if(lo < 0xDC00 || lo > 0xDFFF) goto invalid;
            cp = 0x10000 + ((uint32_t)(u - 0xD800) << 10) + (uint32_t)(lo - 0xDC00);
            i += 2;
        } else {
            goto invalid; /* unpaired low surrogate */
        }
        if(utf16_push_utf8(&buf, &len, &cap, cp) != 0) goto oom;
    }
    if(out_len) *out_len = len;
    return (char*)(buf ? buf : (uint8_t*)calloc(1,1));
invalid:
    free(buf); return NULL;
oom:
    free(buf); return NULL;
}

int utf16_append(uint16_t **out, size_t *n, size_t *cap, uint16_t u){
    if(*n + 1 > *cap){
        size_t ncap = *cap ? *cap * 2 : 64;
        while(ncap < *n + 1) ncap *= 2;
        uint16_t *nb = (uint16_t*)realloc(*out, ncap * sizeof(uint16_t));
        if(!nb) return -1;
        *out = nb; *cap = ncap;
    }
    (*out)[*n] = u; (*n)++;
    return 0;
}

uint16_t *utf8_to_utf16(const char *src, size_t len, size_t *out_units){
    size_t i = 0;
    uint16_t *out = NULL; size_t n = 0, cap = 0;
    while(i < len){
        uint32_t cp;
        int k = utf8_decode((const uint8_t*)src + i, len - i, &cp);
        if(k == 0) goto invalid;
        if(cp <= 0xFFFF){
            if(utf16_append(&out, &n, &cap, (uint16_t)cp) != 0) goto oom;
        } else {
            uint32_t v = cp - 0x10000;
            uint16_t hi = (uint16_t)(0xD800 + (v >> 10));
            uint16_t lo = (uint16_t)(0xDC00 + (v & 0x3FF));
            if(utf16_append(&out, &n, &cap, hi) != 0) goto oom;
            if(utf16_append(&out, &n, &cap, lo) != 0) goto oom;
        }
        i += (size_t)k;
    }
    if(utf16_append(&out, &n, &cap, 0) != 0) goto oom;
    if(out_units) *out_units = n;
    return out;
invalid:
    free(out); return NULL;
oom:
    free(out); return NULL;
}

static int ascii_fold_char(char c){
    if(c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/* Windows ordinal case-insensitive: ASCII case folding only, byte-by-byte,
 * UTF-8 aware only in that multibyte leading bytes are left untouched. */
int utf8_ordinal_case_equal(const char *a, const char *b){
    const unsigned char *pa = (const unsigned char*)a;
    const unsigned char *pb = (const unsigned char*)b;
    while(*pa && *pb){
        if(ascii_fold_char((char)*pa) != ascii_fold_char((char)*pb)) return 0;
        pa++; pb++;
    }
    return *pa == *pb;
}
