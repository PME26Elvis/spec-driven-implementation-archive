#ifndef CVC_WORKTREE_H
#define CVC_WORKTREE_H
#include "cvc_repo.h"
#include "cvc_snapshot.h"
int cvc_worktree_selected(CvcRepo *r,int store_objects,CvcSnapshot *out);
int cvc_worktree_is_clean(CvcRepo *r,const CvcSnapshot *head,CvcSnapshot *work,int *clean);
int cvc_worktree_entry(CvcRepo *r,const char *rel,int *exists,uint8_t *type,uint8_t oid[32],int require_eligible);
int cvc_materialize_snapshot(CvcRepo *r,const CvcSnapshot *current_tracked,const CvcSnapshot *target);
int cvc_materialize_snapshot_owned(CvcRepo *r,const CvcSnapshot *current_tracked,const CvcSnapshot *target,const CvcStrVec *extra_owned_roots);
int cvc_snapshot_actual_matches(CvcRepo *r,const CvcSnapshot *expected,const CvcStrVec *ignore_roots,int *matches,char **bad_path);
int cvc_write_working_bytes(CvcRepo *r,const char *rel,const unsigned char *data,size_t len);
#endif
