#ifndef CVC_MERGE_H
#define CVC_MERGE_H
#include "cvc_repo.h"

typedef enum { CVC_MERGE_NONE=0, CVC_MERGE_ACTIVE=1, CVC_MERGE_FINALIZING=2, CVC_MERGE_COMPLETED_PENDING=3 } CvcMergePhaseView;

/* Validate persisted state against HEAD/ref.  A mutating command may clean a
 * logically-completed stale finalizing record. */
int cvc_merge_state_view(CvcRepo *r,int mutating,CvcMergePhaseView *view);
int cvc_merge_status(CvcRepo *r);
int cvc_cmd_merge_start(CvcRepo *r,const char *target,const char *message_or_null);
int cvc_cmd_merge_continue(CvcRepo *r,const char *message_or_null);
int cvc_cmd_merge_abort(CvcRepo *r);
int cvc_cmd_resolve(CvcRepo *r,const char *path);
int cvc_merge_forbid_active(CvcRepo *r,const char *op);
#endif
