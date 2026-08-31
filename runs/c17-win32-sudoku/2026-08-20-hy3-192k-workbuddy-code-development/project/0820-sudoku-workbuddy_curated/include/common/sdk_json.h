/* sdk_json.h - self-implemented strict JSON reader.
 *
 * docs/03 section 5 requires a hand written parser; no external JSON library
 * is linked. docs/19 section 2 fixes the nesting depth (64) and maximum
 * string length (1 MiB); docs/03 section 10 requires line/column reporting
 * for malformed input.
 *
 * Only standard JSON is accepted: no comments, no trailing commas, no single
 * quotes, no NaN/Infinity, no leading plus, no leading zeros.
 */
#ifndef SDK_JSON_H
#define SDK_JSON_H

#include <stddef.h>
#include <stdint.h>

typedef enum sdk_json_type {
    SDK_JSON_NULL = 0,
    SDK_JSON_BOOL,
    SDK_JSON_NUMBER,
    SDK_JSON_STRING,
    SDK_JSON_ARRAY,
    SDK_JSON_OBJECT
} sdk_json_type;

typedef struct sdk_json_value sdk_json_value;

typedef struct sdk_json_member {
    char           *key;      /* NUL terminated, decoded UTF-8 */
    size_t          key_len;
    sdk_json_value *value;
} sdk_json_member;

struct sdk_json_value {
    sdk_json_type type;
    /* SDK_JSON_BOOL */
    int           boolean;
    /* SDK_JSON_NUMBER: raw text is kept so integers stay exact */
    char         *number_text;
    double        number;
    int           number_is_integer;
    int64_t       number_integer;
    /* SDK_JSON_STRING */
    char         *string;
    size_t        string_len;
    /* SDK_JSON_ARRAY */
    sdk_json_value **items;
    size_t           item_count;
    /* SDK_JSON_OBJECT */
    sdk_json_member *members;
    size_t           member_count;
};

typedef struct sdk_json_error {
    int  line;      /* 1 based */
    int  column;    /* 1 based, counted in bytes */
    char message[192];
} sdk_json_error;

/* Parses `text`/`len`. Returns NULL on failure and fills `err`.
 * The returned tree is owned by the caller; free with sdk_json_free. */
sdk_json_value *sdk_json_parse(const char *text, size_t len, sdk_json_error *err);

void sdk_json_free(sdk_json_value *v);

/* Object lookup; returns NULL when absent or when `v` is not an object. */
const sdk_json_value *sdk_json_object_get(const sdk_json_value *v, const char *key);

#endif /* SDK_JSON_H */
