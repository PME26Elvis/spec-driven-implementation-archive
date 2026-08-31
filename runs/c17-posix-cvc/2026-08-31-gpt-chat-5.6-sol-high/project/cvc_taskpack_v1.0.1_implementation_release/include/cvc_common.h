#ifndef CVC_COMMON_H
#define CVC_COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#define CVC_TEXT_MAX ((uint64_t)8388608)
#define CVC_PROBE_MAX 8192u
#define CVC_OID_HEX 64

typedef struct { unsigned char *data; size_t len, cap; } CvcBuf;
typedef struct { char **v; size_t n, cap; } CvcStrVec;
typedef struct { char *data; size_t len; } CvcString;

void *cvc_xmalloc(size_t n);
void *cvc_xcalloc(size_t n, size_t s);
void *cvc_xrealloc(void *p, size_t n);
char *cvc_xstrdup(const char *s);
char *cvc_xstrndup(const char *s, size_t n);
int cvc_errorf(const char *fmt, ...);
void cvc_warnf(const char *fmt, ...);

void cvc_buf_init(CvcBuf *b);
void cvc_buf_free(CvcBuf *b);
void cvc_buf_reserve(CvcBuf *b, size_t add);
void cvc_buf_append(CvcBuf *b, const void *p, size_t n);
void cvc_buf_putc(CvcBuf *b, unsigned char c);
void cvc_buf_printf(CvcBuf *b, const char *fmt, ...);

void cvc_strvec_init(CvcStrVec *v);
void cvc_strvec_free(CvcStrVec *v);
void cvc_strvec_push(CvcStrVec *v, const char *s);
void cvc_strvec_pushn(CvcStrVec *v, const char *s, size_t n);
void cvc_strvec_sort(CvcStrVec *v);
int cvc_strvec_contains(const CvcStrVec *v, const char *s);

int cvc_utf8_valid(const unsigned char *s, size_t n);
int cvc_name_component_valid(const unsigned char *s, size_t n);
int cvc_repo_path_valid(const char *s);
int cvc_branch_name_valid(const char *s);
int cvc_ascii_hex(const char *s, size_t n);
int cvc_parse_u64_canon(const char *s, uint64_t *out);
int cvc_parse_i64_timestamp(const char *s, int64_t *out);

char *cvc_path_join(const char *a, const char *b);
char *cvc_path_dirname(const char *p);
char *cvc_path_basename_dup(const char *p);
int cvc_mkdir_p(const char *path, mode_t mode);
int cvc_read_file(const char *path, CvcBuf *out);
int cvc_read_file_nofollow(const char *path, CvcBuf *out);
int cvc_write_all(int fd, const void *p, size_t n);
int cvc_fsync_parent(const char *path);
int cvc_atomic_write(const char *path, const void *data, size_t len, mode_t mode);
int cvc_remove_tree_nofollow(const char *path);
int cvc_is_real_dir(const char *path);
int cvc_is_real_regular(const char *path);
int cvc_lstat_exists(const char *path);
char *cvc_getcwd_alloc(void);
char *cvc_abspath_lexical(const char *path);
int cvc_path_is_prefix(const char *ancestor, const char *path);

void cvc_hex_encode(const uint8_t *in, size_t n, char *out);
int cvc_hex_decode_32(const char *hex, uint8_t out[32]);
int cvc_bytes_cmp(const void *ap, const void *bp, size_t n);

void cvc_put_u32be(CvcBuf *b, uint32_t x);
void cvc_put_u64be(CvcBuf *b, uint64_t x);
uint32_t cvc_get_u32be(const unsigned char *p);
uint64_t cvc_get_u64be(const unsigned char *p);

#endif
