#ifndef MDEDIT_JSON_H
#define MDEDIT_JSON_H

#include "mdedit/core.h"

typedef enum {
    MD_JSON_NULL,
    MD_JSON_BOOL,
    MD_JSON_NUMBER,
    MD_JSON_STRING,
    MD_JSON_ARRAY,
    MD_JSON_OBJECT
} MdJsonType;

typedef struct MdJson MdJson;

typedef struct {
    char *key;
    MdJson *value;
} MdJsonMember;

struct MdJson {
    MdJsonType type;
    union {
        bool boolean;
        double number;
        char *string;
        struct { MdJson **items; size_t len; size_t cap; } array;
        struct { MdJsonMember *items; size_t len; size_t cap; } object;
    } as;
};

typedef struct {
    size_t offset;
    size_t line;
    size_t column;
    char message[192];
} MdJsonError;

MdJson *md_json_parse(const char *text, size_t len, MdJsonError *error);
void md_json_free(MdJson *value);
const MdJson *md_json_get(const MdJson *object, const char *key);
const char *md_json_string(const MdJson *value);
bool md_json_bool(const MdJson *value, bool *out);
bool md_json_u64(const MdJson *value, uint64_t *out);
bool md_json_write_escaped(MdBuf *out, const char *s, size_t len);

#endif

