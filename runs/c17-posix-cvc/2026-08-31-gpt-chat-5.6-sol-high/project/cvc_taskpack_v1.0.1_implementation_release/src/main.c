#define _POSIX_C_SOURCE 200809L
#include "cvc_repo.h"
#include "cvc_commands.h"
#include "cvc_merge.h"
#include "cvc_verify.h"
#include "cvc_glob.h"
#include "cvc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE*f){
    fprintf(f,
"usage: cvc <command> [options] [arguments]\n"
"  cvc init\n"
"  cvc status [--include=PATTERNS] [--exclude=PATTERNS]\n"
"  cvc save -m MESSAGE [--include=PATTERNS] [--exclude=PATTERNS] [--no-diffstat]\n"
"  cvc log [--max-count=N]\n"
"  cvc diff [REVISION] [--include=PATTERNS] [--exclude=PATTERNS]\n"
"  cvc branch\n"
"  cvc branch create NAME\n"
"  cvc branch delete NAME\n"
"  cvc switch BRANCH\n"
"  cvc restore PATH --from REVISION\n"
"  cvc rollback REVISION -m MESSAGE\n"
"  cvc merge BRANCH [-m MESSAGE]\n"
"  cvc merge --continue [-m MESSAGE]\n"
"  cvc merge --abort\n"
"  cvc resolve PATH\n"
"  cvc verify\n"
"  cvc config show|validate\n"
"  cvc help | cvc --help\n");
}
static int message_ok(const char*s){return s&&*s&&cvc_utf8_valid((const unsigned char*)s,strlen(s));}
static int parse_patterns(const char*arg,const char*prefix,CvcStrVec*out){size_t n=strlen(prefix);if(strncmp(arg,prefix,n))return 0;if(arg[n]==0)return cvc_errorf("empty pattern list is invalid");if(cvc_parse_pattern_csv(arg+n,out)<0)return -1;return 1;}
static int open_repo(CvcRepo*r,int wr){return cvc_repo_open(r,wr);}
static int ensure_no_active(CvcRepo*r,const char*op){return cvc_merge_forbid_active(r,op);}
static int do_status(int ac,char**av){
    CvcStrVec in,ex;int hi=0,he=0;cvc_strvec_init(&in);cvc_strvec_init(&ex);
    for(int i=0;i<ac;i++){int z=parse_patterns(av[i],"--include=",&in);if(z){if(z<0||hi){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --include");}hi=1;continue;}z=parse_patterns(av[i],"--exclude=",&ex);if(z){if(z<0||he){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --exclude");}he=1;continue;}cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("unknown status option/argument: %s",av[i]);}
    CvcRepo r;if(open_repo(&r,0)<0){cvc_strvec_free(&in);cvc_strvec_free(&ex);return -1;}CvcMergePhaseView mv;if(cvc_merge_state_view(&r,0,&mv)<0){cvc_repo_close(&r);cvc_strvec_free(&in);cvc_strvec_free(&ex);return -1;}int rc=cvc_cmd_status(&r,hi?&in:NULL,he?&ex:NULL);if(rc==0)rc=cvc_merge_status(&r);cvc_repo_close(&r);cvc_strvec_free(&in);cvc_strvec_free(&ex);return rc;
}
static int do_save(int ac,char**av){
    const char*m=NULL;int hm=0,hi=0,he=0,nd=0;CvcStrVec in,ex;cvc_strvec_init(&in);cvc_strvec_init(&ex);
    for(int i=0;i<ac;i++){if(!strcmp(av[i],"-m")){if(hm||i+1>=ac){cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("missing or duplicate -m");}m=av[++i];hm=1;continue;}int z=parse_patterns(av[i],"--include=",&in);if(z){if(z<0||hi){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --include");}hi=1;continue;}z=parse_patterns(av[i],"--exclude=",&ex);if(z){if(z<0||he){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --exclude");}he=1;continue;}if(!strcmp(av[i],"--no-diffstat")){if(nd){cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("duplicate --no-diffstat");}nd=1;continue;}cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("unknown save option/argument: %s",av[i]);}
    if(!hm||!message_ok(m)){cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("save requires nonempty UTF-8 -m message");}CvcRepo r;if(open_repo(&r,1)<0){cvc_strvec_free(&in);cvc_strvec_free(&ex);return -1;}int rc=ensure_no_active(&r,"save");if(rc==0)rc=cvc_cmd_save(&r,m,hi?&in:NULL,he?&ex:NULL,nd);cvc_repo_close(&r);cvc_strvec_free(&in);cvc_strvec_free(&ex);return rc;
}
static int do_log(int ac,char**av){uint64_t max=0;int lim=0;for(int i=0;i<ac;i++){static const char p[]="--max-count=";if(strncmp(av[i],p,sizeof(p)-1)||lim)return cvc_errorf("unknown/duplicate log option");const char*s=av[i]+sizeof(p)-1;if(!*s||*s=='0'||!cvc_parse_u64_canon(s,&max)||max==0)return cvc_errorf("invalid --max-count");lim=1;}CvcRepo r;if(open_repo(&r,0)<0)return -1;CvcMergePhaseView mv;if(cvc_merge_state_view(&r,0,&mv)<0){cvc_repo_close(&r);return -1;}int rc=cvc_cmd_log(&r,max,lim);cvc_repo_close(&r);return rc;}
static int do_diff(int ac,char**av){const char*rev=NULL;CvcStrVec in,ex;int hi=0,he=0;cvc_strvec_init(&in);cvc_strvec_init(&ex);for(int i=0;i<ac;i++){int z=parse_patterns(av[i],"--include=",&in);if(z){if(z<0||hi){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --include");}hi=1;continue;}z=parse_patterns(av[i],"--exclude=",&ex);if(z){if(z<0||he){cvc_strvec_free(&in);cvc_strvec_free(&ex);return z<0?-1:cvc_errorf("duplicate --exclude");}he=1;continue;}if(av[i][0]=='-'){cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("unknown diff option: %s",av[i]);}if(rev){cvc_strvec_free(&in);cvc_strvec_free(&ex);return cvc_errorf("too many diff revisions");}rev=av[i];}CvcRepo r;if(open_repo(&r,0)<0){cvc_strvec_free(&in);cvc_strvec_free(&ex);return -1;}CvcMergePhaseView mv;if(cvc_merge_state_view(&r,0,&mv)<0){cvc_repo_close(&r);cvc_strvec_free(&in);cvc_strvec_free(&ex);return -1;}int rc=cvc_cmd_diff(&r,rev,hi?&in:NULL,he?&ex:NULL);cvc_repo_close(&r);cvc_strvec_free(&in);cvc_strvec_free(&ex);return rc;}
static int do_branch(int ac,char**av){int wr=ac>0;CvcRepo r;if(open_repo(&r,wr)<0)return -1;int rc=0;if(ac==0){CvcMergePhaseView mv;if(cvc_merge_state_view(&r,0,&mv)<0)rc=-1;else rc=cvc_cmd_branch_list(&r);}else if(ac==2&&!strcmp(av[0],"create")){if(ensure_no_active(&r,"create branch")==0)rc=cvc_cmd_branch_create(&r,av[1]);else rc=-1;}else if(ac==2&&!strcmp(av[0],"delete")){if(ensure_no_active(&r,"delete branch")==0)rc=cvc_cmd_branch_delete(&r,av[1]);else rc=-1;}else rc=cvc_errorf("invalid branch form");cvc_repo_close(&r);return rc;}
static int do_switch(int ac,char**av){if(ac!=1)return cvc_errorf("switch requires exactly one branch");CvcRepo r;if(open_repo(&r,1)<0)return -1;int rc=ensure_no_active(&r,"switch");if(rc==0)rc=cvc_cmd_switch(&r,av[0]);cvc_repo_close(&r);return rc;}
static int do_restore(int ac,char**av){if(ac!=3||strcmp(av[1],"--from"))return cvc_errorf("usage: cvc restore PATH --from REVISION");if(!cvc_repo_path_valid(av[0]))return cvc_errorf("invalid repository path");CvcRepo r;if(open_repo(&r,1)<0)return -1;CvcMergePhaseView mv;if(cvc_merge_state_view(&r,1,&mv)<0){cvc_repo_close(&r);return -1;}if(mv==CVC_MERGE_FINALIZING){cvc_repo_close(&r);return cvc_errorf("restore forbidden while merge is finalizing");}int rc=cvc_cmd_restore(&r,av[0],av[2]);cvc_repo_close(&r);return rc;}
static int do_rollback(int ac,char**av){if(ac!=3||strcmp(av[1],"-m")||!message_ok(av[2]))return cvc_errorf("usage: cvc rollback REVISION -m MESSAGE");if(av[0][0]=='-'&&av[0][1]=='-')return cvc_errorf("unknown rollback option");CvcRepo r;if(open_repo(&r,1)<0)return -1;int rc=ensure_no_active(&r,"rollback");if(rc==0)rc=cvc_cmd_rollback(&r,av[0],av[2]);cvc_repo_close(&r);return rc;}
static int do_merge(int ac,char**av){
    int cont=0,abort=0;const char*branch=NULL,*msg=NULL;int hm=0;
    for(int i=0;i<ac;i++){if(!strcmp(av[i],"--continue")){if(cont||abort||branch)return cvc_errorf("invalid/duplicate merge mode");cont=1;continue;}if(!strcmp(av[i],"--abort")){if(cont||abort||branch)return cvc_errorf("invalid/duplicate merge mode");abort=1;continue;}if(!strcmp(av[i],"-m")){if(hm||i+1>=ac)return cvc_errorf("missing/duplicate merge -m");msg=av[++i];if(!message_ok(msg))return cvc_errorf("merge message must be nonempty UTF-8");hm=1;continue;}if(av[i][0]=='-')return cvc_errorf("unknown merge option: %s",av[i]);if(cont||abort||branch)return cvc_errorf("invalid merge arguments");branch=av[i];}
    if(abort&&hm)return cvc_errorf("merge --abort takes no -m");
    if(!cont&&!abort&&!branch)return cvc_errorf("merge requires branch, --continue, or --abort");
    CvcRepo r;if(open_repo(&r,1)<0)return -1;int rc;if(cont)rc=cvc_cmd_merge_continue(&r,msg);else if(abort)rc=cvc_cmd_merge_abort(&r);else rc=cvc_cmd_merge_start(&r,branch,msg);cvc_repo_close(&r);return rc;
}
static int do_resolve(int ac,char**av){if(ac!=1||!cvc_repo_path_valid(av[0]))return cvc_errorf("resolve requires one canonical repository path");CvcRepo r;if(open_repo(&r,1)<0)return -1;CvcMergePhaseView mv;if(cvc_merge_state_view(&r,1,&mv)<0){cvc_repo_close(&r);return -1;}if(mv!=CVC_MERGE_ACTIVE){cvc_repo_close(&r);return cvc_errorf("no resolution-active merge");}int rc=cvc_cmd_resolve(&r,av[0]);cvc_repo_close(&r);return rc;}
static int do_verify(int ac){if(ac)return cvc_errorf("verify takes no arguments");CvcRepo r;if(open_repo(&r,0)<0)return -1;int rc=cvc_cmd_verify(&r);cvc_repo_close(&r);return rc;}
static int do_config(int ac,char**av){if(ac!=1||(!strcmp(av[0],"show")&&!strcmp(av[0],"validate")))return cvc_errorf("usage: cvc config show|validate");if(strcmp(av[0],"show")&&strcmp(av[0],"validate"))return cvc_errorf("usage: cvc config show|validate");CvcRepo r;if(open_repo(&r,0)<0)return -1;CvcMergePhaseView mv;if(cvc_merge_state_view(&r,0,&mv)<0){cvc_repo_close(&r);return -1;}if(!strcmp(av[0],"show"))cvc_config_show(&r.config);else printf("configuration valid\n");cvc_repo_close(&r);return 0;}
int main(int argc,char**argv){
    if(argc<2){usage(stderr);return 2;}const char*cmd=argv[1];if(!strcmp(cmd,"help")||!strcmp(cmd,"--help")){if(argc!=2){cvc_errorf("help takes no arguments");return 2;}usage(stdout);return 0;}
    if(!strcmp(cmd,"init")){if(argc!=2)return cvc_errorf("init takes no arguments")<0?2:2;return cvc_repo_init_here()==0?0:1;}
    int rc;if(!strcmp(cmd,"status"))rc=do_status(argc-2,argv+2);else if(!strcmp(cmd,"save"))rc=do_save(argc-2,argv+2);else if(!strcmp(cmd,"log"))rc=do_log(argc-2,argv+2);else if(!strcmp(cmd,"diff"))rc=do_diff(argc-2,argv+2);else if(!strcmp(cmd,"branch"))rc=do_branch(argc-2,argv+2);else if(!strcmp(cmd,"switch"))rc=do_switch(argc-2,argv+2);else if(!strcmp(cmd,"restore"))rc=do_restore(argc-2,argv+2);else if(!strcmp(cmd,"rollback"))rc=do_rollback(argc-2,argv+2);else if(!strcmp(cmd,"merge"))rc=do_merge(argc-2,argv+2);else if(!strcmp(cmd,"resolve"))rc=do_resolve(argc-2,argv+2);else if(!strcmp(cmd,"verify"))rc=do_verify(argc-2);else if(!strcmp(cmd,"config"))rc=do_config(argc-2,argv+2);else{cvc_errorf("unknown command: %s",cmd);usage(stderr);return 2;}return rc==0?0:1;
}
