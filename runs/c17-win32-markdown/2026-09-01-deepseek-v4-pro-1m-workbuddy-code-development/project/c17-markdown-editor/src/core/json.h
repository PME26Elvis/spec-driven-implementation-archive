/* json.h - small JSON DOM parser/serializer with arena storage. */
#ifndef CE_JSON_H
#define CE_JSON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "arena.h"

typedef enum {
    CEJ_NULL, CEJ_BOOL, CEJ_INT, CEJ_DOUBLE, CEJ_STR, CEJ_ARR, CEJ_OBJ
} ce_json_type;

typedef struct ce_json ce_json;
struct ce_json {
    ce_json_type type;
    union {
        bool b;
        int64_t i;
        double d;
        struct { char *s; size_t len; } str;
        struct { ce_json **items; size_t count; } arr;
        struct { char **keys; ce_json **vals; size_t count; } obj;
    } u;
};

/* Parse a NUL-terminated JSON document into an arena. Returns NULL on error
 * and sets *errpos (byte offset) if errpos != NULL. */
ce_json *ce_json_parse(ce_arena *a, const char *s, size_t *errpos);

/* Accessors. */
ce_json *ce_json_obj_get(const ce_json *obj, const char *key);
const char *ce_json_str(const ce_json *v);              /* returns "" if not a string */
int64_t ce_json_int(const ce_json *v, int64_t def);     /* int or double-truncated */
bool ce_json_bool(const ce_json *v, bool def);

/* Serializer: append JSON text of v to out. */
void ce_json_write(ce_json *v, void *out_ctx, void (*emit)(void *ctx, const char *s, size_t n));

/* Convenience: write to a dynamically grown string (returns malloc'd). */
char *ce_json_to_string(ce_json *v);

/* Helpers for building JSON. */
ce_json *ce_json_new_null(ce_arena *a);
ce_json *ce_json_new_bool(ce_arena *a, bool b);
ce_json *ce_json_new_int(ce_arena *a, int64_t i);
ce_json *ce_json_new_double(ce_arena *a, double d);
ce_json *ce_json_new_str(ce_arena *a, const char *s);
ce_json *ce_json_new_strn(ce_arena *a, const char *s, size_t n);
ce_json *ce_json_new_arr(ce_arena *a);
ce_json *ce_json_new_obj(ce_arena *a);
void ce_json_arr_push(ce_arena *a, ce_json *arr, ce_json *v);
void ce_json_obj_set(ce_arena *a, ce_json *obj, const char *key, ce_json *v);

#endif /* CE_JSON_H */
