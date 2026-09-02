#ifndef TABLETOOL_COMMON_H
#define TABLETOOL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

#include "mem.h"
#include "utf8.h"

#define TT_VERSION "1.0.1"

/* Exit codes */
#define EXIT_OK 0
#define EXIT_CLI 2
#define EXIT_SYNTAX 3
#define EXIT_DATA 4
#define EXIT_DOMAIN 5
#define EXIT_IO 6
#define EXIT_RESOURCE 7
#define EXIT_INTERNAL 8

/* Types */
typedef enum {
    TYPE_STRING = 0,
    TYPE_INTEGER,
    TYPE_DECIMAL,
    TYPE_BOOLEAN,
    TYPE_DATE,
    TYPE_URL,
    TYPE_EAN13,
    TYPE_CODE128
} ColumnType;

typedef struct {
    bool is_null;
    bool owns; /* true if v.str.data is owned heap */
    union {
        struct { char *data; size_t len; } str; /* owned */
        int64_t i64;
        struct {
            int8_t sign; /* -1, 0, +1 */
            uint8_t digits[40]; /* up to 38 digits, little-endian style or big */
            int scale; /* canonical after conversion */
            int precision;
        } dec;
        bool boolean;
        struct { int y, m, d; } date;
        /* URL, EAN13, CODE128 stored as canonical string */
    } v;
} Cell;

typedef struct {
    char *name;
    size_t name_len;
    ColumnType type;
} Column;

typedef struct {
    Column *cols;
    size_t ncol;
    size_t col_cap;
    Cell **rows; /* rows[r][c] */
    size_t nrow;
    size_t row_cap;
} Table;

/* Error reporting context */
typedef struct {
    int exit_code;
    int script_line;
    char command[64];
    char category[32];
    char message[512];
    char path[1024];
    int record_line;
    int row;
    char column[256];
} ErrorInfo;

#endif
