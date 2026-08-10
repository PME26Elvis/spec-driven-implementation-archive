#ifndef MDEDIT_CORE_H
#define MDEDIT_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define MD_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define MD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MD_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MD_CLAMP(v, lo, hi) (MD_MIN(MD_MAX((v), (lo)), (hi)))
#define MD_PATH_MAX 4096

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} MdBuf;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} MdBytes;

typedef struct {
    size_t start;
    size_t end;
} MdRange;

typedef struct {
    char path[MD_PATH_MAX];
    uint64_t size;
    uint8_t sha256[32];
} MdFileDigest;

void md_buf_init(MdBuf *b);
void md_buf_free(MdBuf *b);
bool md_buf_reserve(MdBuf *b, size_t needed);
bool md_buf_assign(MdBuf *b, const char *data, size_t len);
bool md_buf_append(MdBuf *b, const char *data, size_t len);
bool md_buf_append_cstr(MdBuf *b, const char *s);
bool md_buf_append_char(MdBuf *b, char c);
bool md_buf_appendf(MdBuf *b, const char *fmt, ...);
bool md_buf_replace(MdBuf *b, size_t start, size_t end,
                    const char *replacement, size_t replacement_len);

void md_bytes_init(MdBytes *b);
void md_bytes_free(MdBytes *b);
bool md_bytes_reserve(MdBytes *b, size_t needed);
bool md_bytes_append(MdBytes *b, const void *data, size_t len);

bool md_size_add(size_t a, size_t b, size_t *out);
bool md_size_mul(size_t a, size_t b, size_t *out);
uint64_t md_now_millis(void);
uint64_t md_now_unix(void);
char *md_strdup(const char *s);
char *md_strndup(const char *s, size_t n);

bool md_utf8_decode(const char *s, size_t len, size_t *at, uint32_t *cp);
bool md_utf8_validate(const char *s, size_t len, size_t *bad_offset);
size_t md_utf8_prev(const char *s, size_t at);
size_t md_utf8_next(const char *s, size_t len, size_t at);
size_t md_grapheme_next(const char *s, size_t len, size_t at);
size_t md_grapheme_prev(const char *s, size_t len, size_t at);
size_t md_grapheme_count(const char *s, size_t len);
bool md_utf8_is_boundary(const char *s, size_t len, size_t at);
bool md_unicode_is_space(uint32_t cp);
bool md_unicode_is_cjk(uint32_t cp);
bool md_unicode_is_combining(uint32_t cp);
bool md_unicode_is_word_char(uint32_t cp);

typedef struct {
    uint32_t state[8];
    uint64_t bit_len;
    uint8_t block[64];
    size_t block_len;
} MdSha256;

void md_sha256_init(MdSha256 *ctx);
void md_sha256_update(MdSha256 *ctx, const void *data, size_t len);
void md_sha256_final(MdSha256 *ctx, uint8_t digest[32]);
void md_sha256(const void *data, size_t len, uint8_t digest[32]);
void md_hex_encode(const uint8_t *data, size_t len, char *out);
bool md_hex_decode(const char *hex, size_t hex_len, uint8_t *out, size_t out_len);
uint32_t md_crc32(const void *data, size_t len);

bool md_base64_encode(const uint8_t *data, size_t len, MdBuf *out);
bool md_base64_decode(const char *data, size_t len, MdBytes *out,
                      char *error, size_t error_cap);

typedef struct {
    uint64_t state;
} MdPrng;

void md_prng_seed(MdPrng *p, uint64_t seed);
uint64_t md_prng_next(MdPrng *p);

bool md_read_file(const char *path, MdBytes *out, char *error, size_t error_cap);
bool md_write_file_atomic(const char *path, const void *data, size_t len,
                          char *error, size_t error_cap);
bool md_file_digest(const char *path, MdFileDigest *out,
                    char *error, size_t error_cap);
bool md_mkdirs(const char *path, mode_t mode, char *error, size_t error_cap);
bool md_path_join(char out[MD_PATH_MAX], const char *a, const char *b);
bool md_path_dirname(char out[MD_PATH_MAX], const char *path);
bool md_path_basename(char out[MD_PATH_MAX], const char *path);
bool md_path_normalize_relative(const char *path, char out[MD_PATH_MAX]);
bool md_path_is_within(const char *root, const char *path);
bool md_wildmatch(const char *pattern, const char *text);

#endif

