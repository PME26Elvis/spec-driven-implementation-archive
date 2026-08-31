#ifndef CVC_SNAPSHOT_H
#define CVC_SNAPSHOT_H
#include "cvc_common.h"
#include "cvc_config.h"
#include <stdint.h>
typedef struct { char *path; uint8_t type; uint8_t oid[32]; } CvcSnapEntry;
typedef struct { CvcSnapEntry *v; size_t n, cap; uint64_t ignored; } CvcSnapshot;
void cvc_snapshot_init(CvcSnapshot *s);
void cvc_snapshot_free(CvcSnapshot *s);
int cvc_snapshot_add(CvcSnapshot *s,const char *path,uint8_t type,const uint8_t oid[32]);
void cvc_snapshot_sort(CvcSnapshot *s);
ssize_t cvc_snapshot_find(const CvcSnapshot *s,const char *path);
int cvc_snapshot_equal(const CvcSnapshot *a,const CvcSnapshot *b);
int cvc_snapshot_scan(const char *repo_root,const char *cvc_dir,const CvcConfig *cfg,int store_objects,CvcSnapshot *out);
int cvc_snapshot_from_tree(const char *cvc_dir,const uint8_t tree_oid[32],CvcSnapshot *out);
int cvc_snapshot_from_commit(const char *cvc_dir,const uint8_t commit_oid[32],CvcSnapshot *out);
int cvc_snapshot_write_tree(const char *cvc_dir,const CvcSnapshot *s,uint8_t root_oid[32]);
int cvc_snapshot_subtree(const CvcSnapshot *src,const char *root,CvcSnapshot *out);
#endif
