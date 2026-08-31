#ifndef CVC_OBJECT_H
#define CVC_OBJECT_H
#include "cvc_common.h"
#include <stdint.h>
typedef enum { CVC_OBJ_BLOB=1, CVC_OBJ_SYMLINK=2, CVC_OBJ_TREE=3, CVC_OBJ_COMMIT=4 } CvcObjType;
typedef struct { uint8_t type; char *name; size_t name_len; uint8_t oid[32]; } CvcTreeEntry;
typedef struct { CvcTreeEntry *v; size_t n; } CvcTree;
typedef struct { uint8_t root[32]; uint8_t parent_count; uint8_t parents[2][32]; int64_t timestamp; unsigned char *message; size_t message_len; } CvcCommit;
const char *cvc_obj_type_name(CvcObjType t);
void cvc_object_envelope(CvcObjType type, const void *payload, size_t len, CvcBuf *out);
void cvc_object_oid(CvcObjType type, const void *payload, size_t len, uint8_t oid[32]);
int cvc_object_store(const char *cvc_dir, CvcObjType type, const void *payload, size_t len, uint8_t oid[32]);
int cvc_object_read(const char *cvc_dir, const uint8_t oid[32], CvcObjType *type, CvcBuf *payload);
int cvc_object_read_hex(const char *cvc_dir, const char hex[65], CvcObjType *type, CvcBuf *payload);
int cvc_tree_parse(const unsigned char *p, size_t n, CvcTree *out, int allow_empty);
void cvc_tree_free(CvcTree *t);
int cvc_commit_parse(const unsigned char *p, size_t n, CvcCommit *out);
void cvc_commit_free(CvcCommit *c);
int cvc_commit_store(const char *cvc_dir,const uint8_t root[32],uint8_t parent_count,const uint8_t *parents,int64_t ts,const unsigned char *msg,size_t msglen,uint8_t oid[32]);
char *cvc_object_path(const char *cvc_dir, const uint8_t oid[32]);
#endif
