/* json.h - minimal JSON value tree + parser (no third-party deps).
 * Supports objects, arrays, strings (with escapes), integers, doubles,
 * booleans, null. Sufficient for locscan/releasecheck configs and outputs. */
#ifndef PB_JSON_H
#define PB_JSON_H

typedef enum { J_NULL, J_BOOL, J_INT, J_DBL, J_STR, J_ARR, J_OBJ } JType;

typedef struct JValue {
    JType type;
    union {
        int        b;
        long long  i;
        double     d;
        char      *s;                 /* owned, for J_STR */
        struct { struct JValue **items; int count, cap; } arr;
        struct { char **keys; struct JValue **vals; int count, cap; } obj;
    } u;
} JValue;

/* Parse JSON text. On error, returns NULL and writes a message into err. */
JValue *json_parse(const char *text, char *err, int errcap);

void json_free(JValue *v);

/* Accessors (NULL-safe). */
JValue *json_obj_get(const JValue *o, const char *key);
JValue *json_arr_get(const JValue *a, int idx);
int        json_is_obj(const JValue *v);
int        json_is_arr(const JValue *v);
int        json_is_str(const JValue *v);
int        json_is_int(const JValue *v);
int        json_is_bool(const JValue *v);
const char *json_str(const JValue *v);          /* "" if not string */
long long   json_int(const JValue *v);
int        json_bool(const JValue *v);
double      json_dbl(const JValue *v);

/* Pretty-print to buffer (caller provides buf). Returns bytes written (excl NUL). */
int json_print(const JValue *v, char *buf, int cap);

#endif /* PB_JSON_H */
