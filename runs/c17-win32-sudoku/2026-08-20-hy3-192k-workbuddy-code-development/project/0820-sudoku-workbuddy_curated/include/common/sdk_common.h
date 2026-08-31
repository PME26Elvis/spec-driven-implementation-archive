/* sdk_common.h - shared primitive types, canonical limits and status codes.
 *
 * Normative source: docs/19_CANONICAL_FORMATS_AND_LIMITS.md sections 2-3,
 * docs/18_NORMATIVE_CONVENTIONS_AND_GLOSSARY.md.
 *
 * Every multi-byte integer written to a persistent format is little-endian.
 * Every length is bounds-checked against the canonical limits below before
 * any allocation is attempted.
 */
#ifndef SDK_COMMON_H
#define SDK_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Canonical numeric limits (docs/19 section 2)                        */
/* ------------------------------------------------------------------ */

#define SDK_LIMIT_FILE_BYTES            (64u * 1024u * 1024u)   /* 64 MiB */
#define SDK_LIMIT_LOCSTAT_FILES         200000u
#define SDK_LIMIT_LOCSTAT_PATH_DEPTH    128u
#define SDK_LIMIT_VCS_TRACKED_FILES     100000u
#define SDK_LIMIT_VCS_PATH_BYTES        4096u
#define SDK_LIMIT_WIN32_PATH_UNITS      32767u
#define SDK_LIMIT_VCS_TREE_ENTRIES      100000u
#define SDK_LIMIT_VCS_MESSAGE_BYTES     4096u
#define SDK_LIMIT_VCS_AUTHOR_BYTES      128u
#define SDK_LIMIT_BRANCH_NAME_BYTES     255u
#define SDK_LIMIT_VAULT_CIPHERTEXT      (64u * 1024u * 1024u)
#define SDK_LIMIT_GAMES_IN_PROGRESS     1000u
#define SDK_LIMIT_COMPLETED_RECORDS     100000u
#define SDK_LIMIT_UNDO_TRANSACTIONS     10000u
#define SDK_LIMIT_CHANGES_PER_TXN       512u
#define SDK_LIMIT_UI_RIPPLES            64u
#define SDK_LIMIT_UI_LIST_ITEMS         2000u
#define SDK_LIMIT_JSON_DEPTH            64u
#define SDK_LIMIT_JSON_STRING_BYTES     (1u * 1024u * 1024u)

/* ------------------------------------------------------------------ */
/* CLI exit statuses (docs/19 section 3)                               */
/* ------------------------------------------------------------------ */

#define SDK_EXIT_OK          0
#define SDK_EXIT_USAGE       2
#define SDK_EXIT_DATA        3   /* malformed repository / config / format  */
#define SDK_EXIT_IO          4   /* I/O, permission, resource, system error */
#define SDK_EXIT_VERIFY      5   /* integrity verification found a problem  */
#define SDK_EXIT_INTERNAL    70  /* unexpected internal invariant failure   */

/* ------------------------------------------------------------------ */
/* Result codes used across modules                                    */
/* ------------------------------------------------------------------ */

typedef enum sdk_status {
    SDK_OK = 0,
    SDK_ERR_USAGE,          /* caller supplied invalid arguments           */
    SDK_ERR_DATA,           /* payload/format is malformed                 */
    SDK_ERR_IO,             /* filesystem or OS level failure              */
    SDK_ERR_VERIFY,         /* integrity mismatch                          */
    SDK_ERR_NOMEM,          /* allocation failed or limit exceeded         */
    SDK_ERR_LIMIT,          /* an explicit canonical limit was exceeded    */
    SDK_ERR_NOT_FOUND,      /* requested entity does not exist             */
    SDK_ERR_EXISTS,         /* entity already exists                       */
    SDK_ERR_BUSY,           /* lock held / operation already in flight     */
    SDK_ERR_AUTH,           /* authentication tag / password rejected      */
    SDK_ERR_UNSUPPORTED,    /* known field with unsupported value          */
    SDK_ERR_INTERNAL        /* invariant violation                         */
} sdk_status;

const char *sdk_status_name(sdk_status st);
int         sdk_status_to_exit(sdk_status st);

/* ------------------------------------------------------------------ */
/* Little-endian load/store helpers                                    */
/* ------------------------------------------------------------------ */

static inline void sdk_put_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
static inline void sdk_put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}
static inline void sdk_put_u64le(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}
static inline uint16_t sdk_get_u16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t sdk_get_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint64_t sdk_get_u64le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | (uint64_t)p[i];
    }
    return v;
}
static inline void sdk_put_i64le(uint8_t *p, int64_t v) {
    sdk_put_u64le(p, (uint64_t)v);
}
static inline int64_t sdk_get_i64le(const uint8_t *p) {
    return (int64_t)sdk_get_u64le(p);
}

/* ------------------------------------------------------------------ */
/* Overflow-checked size arithmetic                                    */
/* ------------------------------------------------------------------ */

/* Returns 0 on overflow, 1 on success. */
int sdk_size_add(size_t a, size_t b, size_t *out);
int sdk_size_mul(size_t a, size_t b, size_t *out);

/* Growable byte buffer used by serializers and report writers. */
typedef struct sdk_buf {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    int      failed;   /* sticky: set when an append could not be honoured */
} sdk_buf;

void        sdk_buf_init(sdk_buf *b);
void        sdk_buf_free(sdk_buf *b);
int         sdk_buf_reserve(sdk_buf *b, size_t extra);
int         sdk_buf_append(sdk_buf *b, const void *data, size_t n);
int         sdk_buf_append_u8(sdk_buf *b, uint8_t v);
int         sdk_buf_append_u16le(sdk_buf *b, uint16_t v);
int         sdk_buf_append_u32le(sdk_buf *b, uint32_t v);
int         sdk_buf_append_u64le(sdk_buf *b, uint64_t v);
int         sdk_buf_append_i64le(sdk_buf *b, int64_t v);
int         sdk_buf_append_cstr(sdk_buf *b, const char *s);
int         sdk_buf_appendf(sdk_buf *b, const char *fmt, ...);

/* Sequential reader with strict bounds checking. */
typedef struct sdk_rd {
    const uint8_t *data;
    size_t         len;
    size_t         pos;
    int            failed;
} sdk_rd;

void     sdk_rd_init(sdk_rd *r, const void *data, size_t len);
int      sdk_rd_bytes(sdk_rd *r, void *out, size_t n);
const uint8_t *sdk_rd_take(sdk_rd *r, size_t n);
int      sdk_rd_u8(sdk_rd *r, uint8_t *out);
int      sdk_rd_u16le(sdk_rd *r, uint16_t *out);
int      sdk_rd_u32le(sdk_rd *r, uint32_t *out);
int      sdk_rd_u64le(sdk_rd *r, uint64_t *out);
int      sdk_rd_i64le(sdk_rd *r, int64_t *out);
size_t   sdk_rd_remaining(const sdk_rd *r);
int      sdk_rd_at_end(const sdk_rd *r);

/* Hex helpers: out buffer must hold 2*n + 1 bytes. */
void sdk_hex_encode(const uint8_t *in, size_t n, char *out);
int  sdk_hex_decode(const char *in, size_t n_hex, uint8_t *out);

/* Constant-time comparison for authentication tags. */
int sdk_ct_equal(const void *a, const void *b, size_t n);

/* Wipe memory in a way the optimiser must not remove. */
void sdk_secure_wipe(void *p, size_t n);

#endif /* SDK_COMMON_H */
