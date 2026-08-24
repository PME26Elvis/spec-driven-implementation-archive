/* Common types, error categories, and portability helpers for edb.
 * ISO C17 only. */
#ifndef EDB_COMMON_H
#define EDB_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

/* Error categories required by Section 29 */
typedef enum edb_err_cat {
    EDB_OK = 0,
    EDB_USAGE,
    EDB_IO,
    EDB_NOT_FOUND,
    EDB_PERMISSION,
    EDB_BUSY_LOCKED,
    EDB_AUTHENTICATION,
    EDB_CORRUPTION,
    EDB_UNSUPPORTED_FORMAT,
    EDB_SQL_LEX,
    EDB_SQL_PARSE,
    EDB_SQL_BIND,
    EDB_CONSTRAINT,
    EDB_TYPE,
    EDB_LIMIT,
    EDB_OUT_OF_MEMORY,
    EDB_CANCELLED,
    EDB_TRANSACTION_STATE,
    EDB_RECOVERY_REQUIRED,
    EDB_RECOVERY_FAILED,
    EDB_INTERNAL_INVARIANT
} edb_err_cat;

typedef struct edb_error {
    edb_err_cat cat;
    int         code;          /* more granular numeric code */
    char        message[512];  /* human-readable, no secrets */
} edb_error;

static inline void edb_error_clear(edb_error *e) {
    if (e) {
        e->cat = EDB_OK;
        e->code = 0;
        e->message[0] = '\0';
    }
}

static inline void edb_error_set(edb_error *e, edb_err_cat cat, int code, const char *msg) {
    if (!e) return;
    e->cat = cat;
    e->code = code;
    if (msg) {
        size_t n = strlen(msg);
        if (n >= sizeof(e->message)) n = sizeof(e->message) - 1;
        memcpy(e->message, msg, n);
        e->message[n] = '\0';
    } else {
        e->message[0] = '\0';
    }
}

/* Checked size arithmetic helpers (MEM-002) */
static inline bool edb_size_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}

static inline bool edb_size_mul(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > SIZE_MAX / a) return false;
    *out = a * b;
    return true;
}

/* Page size fixed at 4096 (FMT-002) */
#define EDB_PAGE_SIZE 4096u

/* Database magic */
#define EDB_MAGIC_0 0x45u /* 'E' */
#define EDB_MAGIC_1 0x44u /* 'D' */
#define EDB_MAGIC_2 0x42u /* 'B' */
#define EDB_MAGIC_3 0x31u /* '1' */

#define EDB_FORMAT_MAJOR 1u
#define EDB_FORMAT_MINOR 0u

/* Secure zero (CRYPTO-014) */
static inline void edb_secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = (volatile unsigned char *)p;
    while (n--) *vp++ = 0;
}

#endif /* EDB_COMMON_H */
