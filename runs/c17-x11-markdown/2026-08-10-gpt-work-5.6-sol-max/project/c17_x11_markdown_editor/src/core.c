#include "mdedit/core.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void md_set_error(char *error, size_t cap, const char *fmt, ...) {
    if (error == NULL || cap == 0U) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(error, cap, fmt, ap);
    va_end(ap);
}

bool md_size_add(size_t a, size_t b, size_t *out) {
    if (b > SIZE_MAX - a) {
        return false;
    }
    *out = a + b;
    return true;
}

bool md_size_mul(size_t a, size_t b, size_t *out) {
    if (a != 0U && b > SIZE_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

void md_buf_init(MdBuf *b) {
    b->data = NULL;
    b->len = 0U;
    b->cap = 0U;
}

void md_buf_free(MdBuf *b) {
    free(b->data);
    md_buf_init(b);
}

bool md_buf_reserve(MdBuf *b, size_t needed) {
    size_t with_nul = 0U;
    if (!md_size_add(needed, 1U, &with_nul)) {
        return false;
    }
    if (with_nul <= b->cap) {
        return true;
    }
    size_t next = b->cap == 0U ? 64U : b->cap;
    while (next < with_nul) {
        if (next > SIZE_MAX / 2U) {
            next = with_nul;
            break;
        }
        next *= 2U;
    }
    char *p = realloc(b->data, next);
    if (p == NULL) {
        return false;
    }
    b->data = p;
    b->cap = next;
    if (b->len == 0U) {
        b->data[0] = '\0';
    }
    return true;
}

bool md_buf_assign(MdBuf *b, const char *data, size_t len) {
    if (!md_buf_reserve(b, len)) {
        return false;
    }
    if (len != 0U) {
        memmove(b->data, data, len);
    }
    b->data[len] = '\0';
    b->len = len;
    return true;
}

bool md_buf_append(MdBuf *b, const char *data, size_t len) {
    size_t total = 0U;
    if (!md_size_add(b->len, len, &total) || !md_buf_reserve(b, total)) {
        return false;
    }
    if (len != 0U) {
        memmove(b->data + b->len, data, len);
    }
    b->len = total;
    b->data[b->len] = '\0';
    return true;
}

bool md_buf_append_cstr(MdBuf *b, const char *s) {
    return md_buf_append(b, s, strlen(s));
}

bool md_buf_append_char(MdBuf *b, char c) {
    return md_buf_append(b, &c, 1U);
}

bool md_buf_appendf(MdBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    int count = vsnprintf(NULL, 0U, fmt, copy);
    va_end(copy);
    if (count < 0) {
        va_end(ap);
        return false;
    }
    size_t total = 0U;
    if (!md_size_add(b->len, (size_t)count, &total) ||
        !md_buf_reserve(b, total)) {
        va_end(ap);
        return false;
    }
    int written = vsnprintf(b->data + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if (written != count) {
        return false;
    }
    b->len = total;
    return true;
}

bool md_buf_replace(MdBuf *b, size_t start, size_t end,
                    const char *replacement, size_t replacement_len) {
    if (start > end || end > b->len) {
        return false;
    }
    size_t tail_len = b->len - end;
    size_t prefix_and_replacement = 0U;
    size_t new_len = 0U;
    if (!md_size_add(start, replacement_len, &prefix_and_replacement) ||
        !md_size_add(prefix_and_replacement, tail_len, &new_len) ||
        !md_buf_reserve(b, new_len)) {
        return false;
    }
    memmove(b->data + prefix_and_replacement, b->data + end, tail_len);
    if (replacement_len != 0U) {
        memmove(b->data + start, replacement, replacement_len);
    }
    b->len = new_len;
    b->data[new_len] = '\0';
    return true;
}

void md_bytes_init(MdBytes *b) {
    b->data = NULL;
    b->len = 0U;
    b->cap = 0U;
}

void md_bytes_free(MdBytes *b) {
    free(b->data);
    md_bytes_init(b);
}

bool md_bytes_reserve(MdBytes *b, size_t needed) {
    if (needed <= b->cap) {
        return true;
    }
    size_t next = b->cap == 0U ? 64U : b->cap;
    while (next < needed) {
        if (next > SIZE_MAX / 2U) {
            next = needed;
            break;
        }
        next *= 2U;
    }
    uint8_t *p = realloc(b->data, next);
    if (p == NULL) {
        return false;
    }
    b->data = p;
    b->cap = next;
    return true;
}

bool md_bytes_append(MdBytes *b, const void *data, size_t len) {
    size_t total = 0U;
    if (!md_size_add(b->len, len, &total) || !md_bytes_reserve(b, total)) {
        return false;
    }
    if (len != 0U) {
        memmove(b->data + b->len, data, len);
    }
    b->len = total;
    return true;
}

uint64_t md_now_millis(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (uint64_t)ts.tv_sec * UINT64_C(1000) + (uint64_t)ts.tv_nsec / UINT64_C(1000000);
}

uint64_t md_now_unix(void) {
    time_t now = time(NULL);
    return now < 0 ? 0U : (uint64_t)now;
}

char *md_strdup(const char *s) {
    return md_strndup(s, strlen(s));
}

char *md_strndup(const char *s, size_t n) {
    size_t cap = 0U;
    if (!md_size_add(n, 1U, &cap)) {
        return NULL;
    }
    char *copy = malloc(cap);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

static bool md_utf8_decode_at(const char *s, size_t len, size_t at,
                              uint32_t *cp, size_t *width) {
    if (at >= len) {
        return false;
    }
    const uint8_t b0 = (uint8_t)s[at];
    if (b0 <= 0x7FU) {
        *cp = b0;
        *width = 1U;
        return true;
    }
    if (b0 >= 0xC2U && b0 <= 0xDFU) {
        if (at + 1U >= len) return false;
        uint8_t b1 = (uint8_t)s[at + 1U];
        if ((b1 & 0xC0U) != 0x80U) return false;
        *cp = ((uint32_t)(b0 & 0x1FU) << 6U) | (uint32_t)(b1 & 0x3FU);
        *width = 2U;
        return true;
    }
    if (b0 >= 0xE0U && b0 <= 0xEFU) {
        if (at + 2U >= len) return false;
        uint8_t b1 = (uint8_t)s[at + 1U];
        uint8_t b2 = (uint8_t)s[at + 2U];
        if ((b1 & 0xC0U) != 0x80U || (b2 & 0xC0U) != 0x80U) return false;
        if ((b0 == 0xE0U && b1 < 0xA0U) || (b0 == 0xEDU && b1 >= 0xA0U)) return false;
        *cp = ((uint32_t)(b0 & 0x0FU) << 12U) |
              ((uint32_t)(b1 & 0x3FU) << 6U) | (uint32_t)(b2 & 0x3FU);
        *width = 3U;
        return true;
    }
    if (b0 >= 0xF0U && b0 <= 0xF4U) {
        if (at + 3U >= len) return false;
        uint8_t b1 = (uint8_t)s[at + 1U];
        uint8_t b2 = (uint8_t)s[at + 2U];
        uint8_t b3 = (uint8_t)s[at + 3U];
        if ((b1 & 0xC0U) != 0x80U || (b2 & 0xC0U) != 0x80U ||
            (b3 & 0xC0U) != 0x80U) return false;
        if ((b0 == 0xF0U && b1 < 0x90U) || (b0 == 0xF4U && b1 > 0x8FU)) return false;
        *cp = ((uint32_t)(b0 & 0x07U) << 18U) |
              ((uint32_t)(b1 & 0x3FU) << 12U) |
              ((uint32_t)(b2 & 0x3FU) << 6U) | (uint32_t)(b3 & 0x3FU);
        *width = 4U;
        return true;
    }
    return false;
}

bool md_utf8_decode(const char *s, size_t len, size_t *at, uint32_t *cp) {
    size_t width = 0U;
    if (!md_utf8_decode_at(s, len, *at, cp, &width)) {
        return false;
    }
    *at += width;
    return true;
}

bool md_utf8_validate(const char *s, size_t len, size_t *bad_offset) {
    size_t at = 0U;
    while (at < len) {
        uint32_t cp = 0U;
        size_t width = 0U;
        if (!md_utf8_decode_at(s, len, at, &cp, &width)) {
            if (bad_offset != NULL) *bad_offset = at;
            return false;
        }
        (void)cp;
        at += width;
    }
    if (bad_offset != NULL) *bad_offset = len;
    return true;
}

bool md_utf8_is_boundary(const char *s, size_t len, size_t at) {
    if (at > len) return false;
    if (at == 0U || at == len) return true;
    return (((uint8_t)s[at] & 0xC0U) != 0x80U);
}

size_t md_utf8_prev(const char *s, size_t at) {
    if (at == 0U) return 0U;
    size_t p = at - 1U;
    while (p > 0U && (((uint8_t)s[p] & 0xC0U) == 0x80U)) --p;
    return p;
}

size_t md_utf8_next(const char *s, size_t len, size_t at) {
    if (at >= len) return len;
    uint32_t cp = 0U;
    size_t width = 0U;
    if (!md_utf8_decode_at(s, len, at, &cp, &width)) return MD_MIN(at + 1U, len);
    (void)cp;
    return at + width;
}

bool md_unicode_is_combining(uint32_t cp) {
    return (cp >= 0x0300U && cp <= 0x036FU) ||
           (cp >= 0x1AB0U && cp <= 0x1AFFU) ||
           (cp >= 0x1DC0U && cp <= 0x1DFFU) ||
           (cp >= 0x20D0U && cp <= 0x20FFU) ||
           (cp >= 0xFE20U && cp <= 0xFE2FU) ||
           (cp >= 0xFE00U && cp <= 0xFE0FU) ||
           (cp >= 0xE0100U && cp <= 0xE01EFU) ||
           (cp >= 0x1F3FBU && cp <= 0x1F3FFU);
}

size_t md_grapheme_next(const char *s, size_t len, size_t at) {
    if (at >= len) return len;
    size_t p = md_utf8_next(s, len, at);
    for (;;) {
        if (p >= len) break;
        size_t q = p;
        uint32_t cp = 0U;
        if (!md_utf8_decode(s, len, &q, &cp)) break;
        if (md_unicode_is_combining(cp)) {
            p = q;
            continue;
        }
        if (cp == 0x200DU) {
            p = q;
            if (p < len) p = md_utf8_next(s, len, p);
            while (p < len) {
                q = p;
                if (!md_utf8_decode(s, len, &q, &cp) || !md_unicode_is_combining(cp)) break;
                p = q;
            }
            continue;
        }
        break;
    }
    return p;
}

size_t md_grapheme_prev(const char *s, size_t len, size_t at) {
    if (at == 0U) return 0U;
    size_t p = 0U;
    size_t last = 0U;
    while (p < at && p < len) {
        last = p;
        size_t next = md_grapheme_next(s, len, p);
        if (next >= at || next <= p) return last;
        p = next;
    }
    return last;
}

size_t md_grapheme_count(const char *s, size_t len) {
    size_t count = 0U;
    size_t at = 0U;
    while (at < len) {
        size_t next = md_grapheme_next(s, len, at);
        if (next <= at) break;
        at = next;
        ++count;
    }
    return count;
}

bool md_unicode_is_space(uint32_t cp) {
    return cp == 0x09U || cp == 0x0AU || cp == 0x0DU || cp == 0x20U ||
           cp == 0x85U || cp == 0xA0U || cp == 0x1680U ||
           (cp >= 0x2000U && cp <= 0x200AU) || cp == 0x2028U ||
           cp == 0x2029U || cp == 0x202FU || cp == 0x205FU || cp == 0x3000U;
}

bool md_unicode_is_cjk(uint32_t cp) {
    return (cp >= 0x3400U && cp <= 0x4DBFU) ||
           (cp >= 0x4E00U && cp <= 0x9FFFU) ||
           (cp >= 0xF900U && cp <= 0xFAFFU) ||
           (cp >= 0x20000U && cp <= 0x2EBEFU) ||
           (cp >= 0x30000U && cp <= 0x323AFU);
}

bool md_unicode_is_word_char(uint32_t cp) {
    return (cp >= (uint32_t)'A' && cp <= (uint32_t)'Z') ||
           (cp >= (uint32_t)'a' && cp <= (uint32_t)'z') ||
           (cp >= (uint32_t)'0' && cp <= (uint32_t)'9') || cp == (uint32_t)'_' ||
           cp >= 0x80U;
}

static uint32_t md_rotr32(uint32_t v, uint32_t n) {
    return (v >> n) | (v << (32U - n));
}

static void md_sha256_transform(MdSha256 *ctx, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t w[64];
    for (size_t i = 0U; i < 16U; ++i) {
        size_t j = i * 4U;
        w[i] = ((uint32_t)block[j] << 24U) | ((uint32_t)block[j+1U] << 16U) |
               ((uint32_t)block[j+2U] << 8U) | (uint32_t)block[j+3U];
    }
    for (size_t i = 16U; i < 64U; ++i) {
        uint32_t s0 = md_rotr32(w[i-15U],7U) ^ md_rotr32(w[i-15U],18U) ^ (w[i-15U] >> 3U);
        uint32_t s1 = md_rotr32(w[i-2U],17U) ^ md_rotr32(w[i-2U],19U) ^ (w[i-2U] >> 10U);
        w[i] = w[i-16U] + s0 + w[i-7U] + s1;
    }
    uint32_t a=ctx->state[0], b=ctx->state[1], c=ctx->state[2], d=ctx->state[3];
    uint32_t e=ctx->state[4], f=ctx->state[5], g=ctx->state[6], h=ctx->state[7];
    for (size_t i = 0U; i < 64U; ++i) {
        uint32_t s1 = md_rotr32(e,6U) ^ md_rotr32(e,11U) ^ md_rotr32(e,25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = md_rotr32(a,2U) ^ md_rotr32(a,13U) ^ md_rotr32(a,22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void md_sha256_init(MdSha256 *ctx) {
    static const uint32_t init[8] = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    memcpy(ctx->state, init, sizeof(init));
    ctx->bit_len = 0U;
    ctx->block_len = 0U;
}

void md_sha256_update(MdSha256 *ctx, const void *data, size_t len) {
    const uint8_t *p = data;
    for (size_t i = 0U; i < len; ++i) {
        ctx->block[ctx->block_len++] = p[i];
        if (ctx->block_len == 64U) {
            md_sha256_transform(ctx, ctx->block);
            ctx->bit_len += UINT64_C(512);
            ctx->block_len = 0U;
        }
    }
}

void md_sha256_final(MdSha256 *ctx, uint8_t digest[32]) {
    ctx->bit_len += (uint64_t)ctx->block_len * UINT64_C(8);
    ctx->block[ctx->block_len++] = 0x80U;
    if (ctx->block_len > 56U) {
        while (ctx->block_len < 64U) ctx->block[ctx->block_len++] = 0U;
        md_sha256_transform(ctx, ctx->block);
        ctx->block_len = 0U;
    }
    while (ctx->block_len < 56U) ctx->block[ctx->block_len++] = 0U;
    for (size_t i = 0U; i < 8U; ++i) {
        ctx->block[63U-i] = (uint8_t)(ctx->bit_len >> (i * 8U));
    }
    md_sha256_transform(ctx, ctx->block);
    for (size_t i = 0U; i < 8U; ++i) {
        digest[i*4U] = (uint8_t)(ctx->state[i] >> 24U);
        digest[i*4U+1U] = (uint8_t)(ctx->state[i] >> 16U);
        digest[i*4U+2U] = (uint8_t)(ctx->state[i] >> 8U);
        digest[i*4U+3U] = (uint8_t)ctx->state[i];
    }
}

void md_sha256(const void *data, size_t len, uint8_t digest[32]) {
    MdSha256 ctx;
    md_sha256_init(&ctx);
    md_sha256_update(&ctx, data, len);
    md_sha256_final(&ctx, digest);
}

void md_hex_encode(const uint8_t *data, size_t len, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0U; i < len; ++i) {
        out[i*2U] = digits[data[i] >> 4U];
        out[i*2U+1U] = digits[data[i] & 0x0FU];
    }
    out[len*2U] = '\0';
}

bool md_hex_decode(const char *hex, size_t hex_len, uint8_t *out, size_t out_len) {
    if (hex_len != out_len * 2U) return false;
    for (size_t i = 0U; i < out_len; ++i) {
        int hi = isdigit((unsigned char)hex[i*2U]) ? hex[i*2U]-'0' :
                 (tolower((unsigned char)hex[i*2U]) >= 'a' && tolower((unsigned char)hex[i*2U]) <= 'f' ?
                  tolower((unsigned char)hex[i*2U])-'a'+10 : -1);
        int lo = isdigit((unsigned char)hex[i*2U+1U]) ? hex[i*2U+1U]-'0' :
                 (tolower((unsigned char)hex[i*2U+1U]) >= 'a' && tolower((unsigned char)hex[i*2U+1U]) <= 'f' ?
                  tolower((unsigned char)hex[i*2U+1U])-'a'+10 : -1);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

uint32_t md_crc32(const void *data, size_t len) {
    const uint8_t *p = data;
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0U; i < len; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

bool md_base64_encode(const uint8_t *data, size_t len, MdBuf *out) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t groups = (len + 2U) / 3U;
    size_t encoded_len = 0U;
    if (!md_size_mul(groups, 4U, &encoded_len) || !md_buf_reserve(out, encoded_len)) return false;
    out->len = 0U;
    out->data[0] = '\0';
    for (size_t i = 0U; i < len; i += 3U) {
        uint32_t value = (uint32_t)data[i] << 16U;
        size_t remain = len - i;
        if (remain > 1U) value |= (uint32_t)data[i+1U] << 8U;
        if (remain > 2U) value |= data[i+2U];
        char block[4];
        block[0] = alphabet[(value >> 18U) & 63U];
        block[1] = alphabet[(value >> 12U) & 63U];
        block[2] = remain > 1U ? alphabet[(value >> 6U) & 63U] : '=';
        block[3] = remain > 2U ? alphabet[value & 63U] : '=';
        if (!md_buf_append(out, block, 4U)) return false;
    }
    return true;
}

static int md_base64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return (int)(c - 'A');
    if (c >= 'a' && c <= 'z') return (int)(c - 'a') + 26;
    if (c >= '0' && c <= '9') return (int)(c - '0') + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

bool md_base64_decode(const char *data, size_t len, MdBytes *out,
                      char *error, size_t error_cap) {
    out->len = 0U;
    if (len == 0U) return true;
    if (len % 4U != 0U) {
        md_set_error(error, error_cap, "Base64 length is not divisible by four");
        return false;
    }
    size_t padding = 0U;
    if (data[len-1U] == '=') ++padding;
    if (len > 1U && data[len-2U] == '=') ++padding;
    for (size_t i = 0U; i < len; i += 4U) {
        bool final = i + 4U == len;
        int a = md_base64_value((unsigned char)data[i]);
        int b = md_base64_value((unsigned char)data[i+1U]);
        int c = data[i+2U] == '=' ? 0 : md_base64_value((unsigned char)data[i+2U]);
        int d = data[i+3U] == '=' ? 0 : md_base64_value((unsigned char)data[i+3U]);
        if (a < 0 || b < 0 || c < 0 || d < 0 ||
            (!final && (data[i+2U] == '=' || data[i+3U] == '=')) ||
            (data[i+2U] == '=' && data[i+3U] != '=') ||
            (data[i+2U] == '=' && (b & 0x0F) != 0) ||
            (data[i+3U] == '=' && data[i+2U] != '=' && (c & 0x03) != 0)) {
            md_set_error(error, error_cap, "Malformed Base64 at byte %zu", i);
            out->len = 0U;
            return false;
        }
        uint32_t value = ((uint32_t)a << 18U) | ((uint32_t)b << 12U) |
                         ((uint32_t)c << 6U) | (uint32_t)d;
        uint8_t bytes[3] = {(uint8_t)(value >> 16U), (uint8_t)(value >> 8U), (uint8_t)value};
        size_t n = final ? 3U - padding : 3U;
        if (!md_bytes_append(out, bytes, n)) {
            md_set_error(error, error_cap, "Out of memory decoding Base64");
            out->len = 0U;
            return false;
        }
    }
    return true;
}

void md_prng_seed(MdPrng *p, uint64_t seed) {
    p->state = seed == 0U ? UINT64_C(0x9e3779b97f4a7c15) : seed;
}

uint64_t md_prng_next(MdPrng *p) {
    uint64_t x = p->state;
    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;
    p->state = x;
    return x * UINT64_C(2685821657736338717);
}

bool md_read_file(const char *path, MdBytes *out, char *error, size_t error_cap) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        md_set_error(error, error_cap, "Cannot open %s: %s", path, strerror(errno));
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 0) {
        md_set_error(error, error_cap, "Cannot stat %s: %s", path, strerror(errno));
        (void)close(fd);
        return false;
    }
    out->len = 0U;
    if ((uintmax_t)st.st_size > SIZE_MAX || !md_bytes_reserve(out, (size_t)st.st_size)) {
        md_set_error(error, error_cap, "File is too large: %s", path);
        (void)close(fd);
        return false;
    }
    uint8_t chunk[65536];
    for (;;) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            md_set_error(error, error_cap, "Cannot read %s: %s", path, strerror(errno));
            (void)close(fd);
            return false;
        }
        if (!md_bytes_append(out, chunk, (size_t)n)) {
            md_set_error(error, error_cap, "Out of memory reading %s", path);
            (void)close(fd);
            return false;
        }
    }
    if (close(fd) != 0) {
        md_set_error(error, error_cap, "Cannot close %s: %s", path, strerror(errno));
        return false;
    }
    return true;
}

static const char *md_find_last_slash(const char *path, size_t len) {
    while (len > 0U) {
        --len;
        if (path[len] == '/') return path + len;
    }
    return NULL;
}

bool md_path_dirname(char out[MD_PATH_MAX], const char *path) {
    size_t len = strlen(path);
    if (len >= MD_PATH_MAX) return false;
    while (len > 1U && path[len-1U] == '/') --len;
    const char *slash = md_find_last_slash(path, len);
    if (slash == NULL) {
        memcpy(out, ".", 2U);
        return true;
    }
    size_t n = slash == path ? 1U : (size_t)(slash - path);
    memcpy(out, path, n);
    out[n] = '\0';
    return true;
}

bool md_path_basename(char out[MD_PATH_MAX], const char *path) {
    size_t len = strlen(path);
    while (len > 1U && path[len-1U] == '/') --len;
    const char *slash = md_find_last_slash(path, len);
    const char *base = slash == NULL ? path : slash + 1;
    size_t n = len - (size_t)(base - path);
    if (n >= MD_PATH_MAX) return false;
    memcpy(out, base, n);
    out[n] = '\0';
    return true;
}

bool md_path_join(char out[MD_PATH_MAX], const char *a, const char *b) {
    if (b[0] == '/') {
        if (strlen(b) >= MD_PATH_MAX) return false;
        strcpy(out, b);
        return true;
    }
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    bool slash = alen != 0U && a[alen-1U] != '/';
    size_t total = alen + (slash ? 1U : 0U) + blen;
    if (total >= MD_PATH_MAX) return false;
    memcpy(out, a, alen);
    size_t at = alen;
    if (slash) out[at++] = '/';
    memcpy(out + at, b, blen);
    out[total] = '\0';
    return true;
}

bool md_mkdirs(const char *path, mode_t mode, char *error, size_t error_cap) {
    size_t len = strlen(path);
    if (len == 0U || len >= MD_PATH_MAX) {
        md_set_error(error, error_cap, "Invalid directory path");
        return false;
    }
    char temp[MD_PATH_MAX];
    memcpy(temp, path, len + 1U);
    for (size_t i = 1U; i <= len; ++i) {
        if (temp[i] == '/' || temp[i] == '\0') {
            char saved = temp[i];
            temp[i] = '\0';
            if (mkdir(temp, mode) != 0 && errno != EEXIST) {
                md_set_error(error, error_cap, "Cannot create %s: %s", temp, strerror(errno));
                return false;
            }
            struct stat st;
            if (stat(temp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                md_set_error(error, error_cap, "%s is not a directory", temp);
                return false;
            }
            temp[i] = saved;
        }
    }
    return true;
}

bool md_write_file_atomic(const char *path, const void *data, size_t len,
                          char *error, size_t error_cap) {
    char dir[MD_PATH_MAX];
    if (!md_path_dirname(dir, path)) {
        md_set_error(error, error_cap, "Destination path is too long");
        return false;
    }
    static uint32_t counter = 0U;
    char temp[MD_PATH_MAX];
    int count = snprintf(temp, sizeof(temp), "%s/.mdedit-tmp-%ld-%u", dir,
                         (long)getpid(), ++counter);
    if (count < 0 || (size_t)count >= sizeof(temp)) {
        md_set_error(error, error_cap, "Temporary path is too long");
        return false;
    }
    mode_t mode = 0666;
    struct stat old;
    if (stat(path, &old) == 0) mode = old.st_mode & 0777;
    int fd = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) {
        md_set_error(error, error_cap, "Cannot create temporary file for %s: %s", path, strerror(errno));
        return false;
    }
    const uint8_t *p = data;
    size_t written = 0U;
    while (written < len) {
        ssize_t n = write(fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            md_set_error(error, error_cap, "Write failed for %s: %s", path, strerror(errno));
            (void)close(fd);
            (void)unlink(temp);
            return false;
        }
        if (n == 0) {
            md_set_error(error, error_cap, "Short write for %s", path);
            (void)close(fd);
            (void)unlink(temp);
            return false;
        }
        written += (size_t)n;
    }
    if (fsync(fd) != 0) {
        md_set_error(error, error_cap, "Flush failed for %s: %s", path, strerror(errno));
        (void)close(fd);
        (void)unlink(temp);
        return false;
    }
    if (close(fd) != 0) {
        md_set_error(error, error_cap, "Close failed for %s: %s", path, strerror(errno));
        (void)unlink(temp);
        return false;
    }
    if (rename(temp, path) != 0) {
        md_set_error(error, error_cap, "Atomic replace failed for %s: %s (temporary: %s)",
                     path, strerror(errno), temp);
        return false;
    }
    int dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0) {
        (void)fsync(dfd);
        (void)close(dfd);
    }
    return true;
}

bool md_file_digest(const char *path, MdFileDigest *out,
                    char *error, size_t error_cap) {
    MdBytes bytes;
    md_bytes_init(&bytes);
    if (!md_read_file(path, &bytes, error, error_cap)) {
        md_bytes_free(&bytes);
        return false;
    }
    if (strlen(path) >= sizeof(out->path)) {
        md_set_error(error, error_cap, "Path is too long: %s", path);
        md_bytes_free(&bytes);
        return false;
    }
    strcpy(out->path, path);
    out->size = (uint64_t)bytes.len;
    md_sha256(bytes.data, bytes.len, out->sha256);
    md_bytes_free(&bytes);
    return true;
}

bool md_path_normalize_relative(const char *path, char out[MD_PATH_MAX]) {
    if (path[0] == '/' || path[0] == '\0') return false;
    char copy[MD_PATH_MAX];
    if (strlen(path) >= sizeof(copy)) return false;
    strcpy(copy, path);
    const char *parts[MD_PATH_MAX / 2U];
    size_t lengths[MD_PATH_MAX / 2U];
    size_t count = 0U;
    char *save = NULL;
    char *token = strtok_r(copy, "/", &save);
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            token = strtok_r(NULL, "/", &save);
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (count == 0U) return false;
            --count;
        } else {
            parts[count] = token;
            lengths[count] = strlen(token);
            ++count;
        }
        token = strtok_r(NULL, "/", &save);
    }
    size_t at = 0U;
    for (size_t i = 0U; i < count; ++i) {
        if (i != 0U) out[at++] = '/';
        if (at + lengths[i] >= MD_PATH_MAX) return false;
        memcpy(out + at, parts[i], lengths[i]);
        at += lengths[i];
    }
    if (at == 0U) {
        out[0] = '.';
        at = 1U;
    }
    out[at] = '\0';
    return true;
}

bool md_path_is_within(const char *root, const char *path) {
    char resolved_root[MD_PATH_MAX];
    char resolved_path[MD_PATH_MAX];
    if (realpath(root, resolved_root) == NULL || realpath(path, resolved_path) == NULL) return false;
    size_t n = strlen(resolved_root);
    return strncmp(resolved_root, resolved_path, n) == 0 &&
           (resolved_path[n] == '\0' || resolved_path[n] == '/');
}

static bool md_wildmatch_impl(const char *p, const char *s) {
    while (*p != '\0') {
        if (*p == '*') {
            while (*p == '*') ++p;
            if (*p == '\0') return true;
            for (const char *q = s;; ++q) {
                if (md_wildmatch_impl(p, q)) return true;
                if (*q == '\0') break;
            }
            return false;
        }
        if (*s == '\0') return false;
        if (*p != '?' && *p != *s) return false;
        ++p;
        ++s;
    }
    return *s == '\0';
}

bool md_wildmatch(const char *pattern, const char *text) {
    return md_wildmatch_impl(pattern, text);
}
