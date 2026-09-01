#ifndef CVC_JSON_H
#define CVC_JSON_H

#include <stddef.h>
#include <stdint.h>

/* A minimal handwritten JSON parser producing a DOM.
 * Supports RFC 8259: objects, arrays, strings, numbers, true/false/null.
 * String escapes incl. \uXXXX and surrogate pairs.
 * Rejects: comments, trailing commas, trailing garbage, unpaired surrogates,
 * invalid UTF-8, malformed numbers, NaN/Infinity, duplicate keys,
 * length overflow. Returns error with byte offset.
 */

typedef enum {
    J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ
} JType;

typedef struct JVal JVal;
typedef struct JMember { char *key; JVal *val; } JMember;

struct JVal {
    JType type;
    /* BOOL */
    int b;
    /* NUM: canonical raw token text (may be huge). */
    char *num;
    /* STR: decoded bytes (UTF-8), may contain embedded NUL;
       str_len is byte length. */
    char *str;
    size_t str_len;
    /* ARR */
    JVal **arr;
    size_t arr_len, arr_cap;
    /* OBJ */
    JMember *obj;
    size_t obj_len, obj_cap;
};

typedef struct {
    /* location of error */
    size_t err_offset;
    char err_msg[256];
    int has_error;
} JErr;

/* Parse JSON text `text` of length `len` into *out.
 * Returns 0 on success. On failure returns -1 and fills err. */
int json_parse(const char *text, size_t len, JVal **out, JErr *err);

void json_free(JVal *v);

/* Look up member by exact key. Returns member or NULL. */
JMember *json_find(const JVal *obj, const char *key);

#endif
