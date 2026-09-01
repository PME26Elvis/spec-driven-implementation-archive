#ifndef CVC_UTIL_H
#define CVC_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Error reporting                                                     */
/* ------------------------------------------------------------------ */

/* Categories used to decide exit semantics. Most are just nonzero.   */
typedef enum {
    CVC_OK = 0,
    CVC_ERR = 1,          /* generic error */
    CVC_ERR_USAGE = 2,    /* bad CLI usage */
    CVC_ERR_NOTREPO = 3,  /* not inside a repository */
    CVC_ERR_INTEGRITY = 4,/* repository corruption */
    CVC_ERR_BUSY = 5,     /* repository locked by writer */
    CVC_ERR_NOMERGE = 6,  /* merge state absent when required */
    CVC_ERR_MERGEDIRTY = 7
} CvcStatus;

/* Last diagnostic text (short reason). */
extern char cvc_errbuf[1024];

/* Set a formatted diagnostic. Returns the status code so callers can
 * `return cvc_fail(CVC_ERR, "msg %s", x);` */
CvcStatus cvc_fail(CvcStatus st, const char *fmt, ...);

/* printf-style to a growable buffer. Returns -1 on OOM. */
int buf_printf(char **buf, size_t *cap, size_t *len, const char *fmt, ...);

/* Dynamic byte buffer (grows; null-terminated whenever buf is nonnull). */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} Bytes;

void bytes_init(Bytes *b);
void bytes_free(Bytes *b);
int bytes_reserve(Bytes *b, size_t extra);       /* ensure len+extra fits */
int bytes_append(Bytes *b, const void *data, size_t n);
int bytes_append_byte(Bytes *b, uint8_t x);
int bytes_append_cstr(Bytes *b, const char *s);
/* Append a big-endian u32 (used by binary serializations). */
int bytes_append_u32(Bytes *b, uint32_t v);
/* Ensure there is a trailing NUL (does not count towards len). */
int bytes_zt(Bytes *b);

/* Growable array of char* strings (caller owns strings). */
typedef struct {
    char **items;
    size_t len;
    size_t cap;
} StrVec;

void strvec_init(StrVec *v);
void strvec_free(StrVec *v);            /* frees array, not the strings */
int strvec_push(StrVec *v, char *owned);/* takes ownership */
int strvec_push_dup(StrVec *v, const char *s);
void strvec_clear(StrVec *v);           /* frees strings too */

/* Growable array of pointers. */
typedef struct {
    void **items;
    size_t len;
    size_t cap;
} PtrVec;
void ptrvec_init(PtrVec *v);
void ptrvec_free(PtrVec *v);
int ptrvec_push(PtrVec *v, void *p);

/* Byte-exact string compare */
int cvc_memcmp(const void *a, const void *b, size_t n);

/* Read a whole file into a Bytes (binary). Returns CVC_OK or error.
 * `limit` = 0 means no limit. Uses wide Win32 open internally (see win32.c). */
CvcStatus file_read_bytes(const wchar_t *abs_path, Bytes *out);

/* In-place UTF-8 helpers live in utf8.h */

#endif
