#ifndef CVC_COMMANDS_H
#define CVC_COMMANDS_H
#include "cvc_repo.h"
#include "cvc_snapshot.h"
int cvc_cmd_status(CvcRepo *r,const CvcStrVec *inc,const CvcStrVec *exc);
int cvc_cmd_save(CvcRepo *r,const char *message,const CvcStrVec *inc,const CvcStrVec *exc,int no_diffstat);
int cvc_cmd_log(CvcRepo *r,uint64_t max_count,int limited);
int cvc_cmd_diff(CvcRepo *r,const char *revision,const CvcStrVec *inc,const CvcStrVec *exc);
int cvc_cmd_branch_list(CvcRepo *r);
int cvc_cmd_branch_create(CvcRepo *r,const char *name);
int cvc_cmd_branch_delete(CvcRepo *r,const char *name);
int cvc_cmd_switch(CvcRepo *r,const char *name);
int cvc_cmd_restore(CvcRepo *r,const char *path,const char *revision);
int cvc_cmd_rollback(CvcRepo *r,const char *revision,const char *message);
#endif
