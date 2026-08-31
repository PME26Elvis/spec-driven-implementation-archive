#ifndef CVC_JSON_H
#define CVC_JSON_H
#include "cvc_common.h"
typedef enum { J_NULL, J_BOOL, J_NUMBER, J_STRING, J_ARRAY, J_OBJECT } CvcJsonType;
typedef struct CvcJson CvcJson;
typedef struct { unsigned char *p; size_t n; } CvcJString;
typedef struct { CvcJString key; CvcJson *value; } CvcJPair;
struct CvcJson {
    CvcJsonType type;
    union {
        int boolean;
        CvcJString string;
        CvcJString number;
        struct { CvcJson **v; size_t n; } array;
        struct { CvcJPair *v; size_t n; } object;
    } u;
};
int cvc_json_parse(const unsigned char *data, size_t len, CvcJson **out, char **err);
void cvc_json_free(CvcJson *j);
CvcJson *cvc_json_obj_get(const CvcJson *j, const char *ascii_key);
#endif
