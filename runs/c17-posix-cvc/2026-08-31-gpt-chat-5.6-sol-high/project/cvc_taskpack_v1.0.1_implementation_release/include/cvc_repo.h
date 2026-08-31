#ifndef CVC_REPO_H
#define CVC_REPO_H
#include "cvc_common.h"
#include "cvc_config.h"
#include <stdint.h>
typedef struct {
    char *root;
    char *cvc;
    CvcConfig config;
    char *branch;
    int born;
    uint8_t head[32];
    int lock_fd;
    int write_lock;
} CvcRepo;
int cvc_repo_init_here(void);
int cvc_repo_open(CvcRepo *r,int write_lock);
void cvc_repo_close(CvcRepo *r);
int cvc_repo_refresh_head(CvcRepo *r);
int cvc_branch_read(CvcRepo *r,const char *name,int *born,uint8_t oid[32]);
int cvc_branch_write(CvcRepo *r,const char *name,int born,const uint8_t oid[32]);
int cvc_head_write_branch(CvcRepo *r,const char *name);
int cvc_branch_list(CvcRepo *r,CvcStrVec *out);
int cvc_branch_create(CvcRepo *r,const char *name);
int cvc_branch_delete(CvcRepo *r,const char *name,int *warn_unique);
int cvc_resolve_revision(CvcRepo *r,const char *rev,uint8_t oid[32]);
int cvc_commit_is_ancestor(CvcRepo *r,const uint8_t anc[32],const uint8_t desc[32],int *yes);
int cvc_merge_base(CvcRepo *r,const uint8_t a[32],const uint8_t b[32],uint8_t out[32]);
int cvc_get_timestamp(int64_t *out);
int cvc_repo_atomic_meta_write(CvcRepo *r,const char *dest,const void *data,size_t len,mode_t mode);
#endif
