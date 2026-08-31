#define _POSIX_C_SOURCE 200809L
#include "cvc_verify.h"
#include "cvc_object.h"
#include "cvc_snapshot.h"
#include "cvc_merge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct { uint8_t *p; size_t n,cap; } Oids;
static void ids_free(Oids*v){free(v->p);memset(v,0,sizeof(*v));}
static int ids_has(const Oids*v,const uint8_t id[32]){for(size_t i=0;i<v->n;i++)if(!memcmp(v->p+i*32,id,32))return 1;return 0;}
static void ids_add(Oids*v,const uint8_t id[32]){if(ids_has(v,id))return;if(v->n==v->cap){v->cap=v->cap?v->cap*2:64;v->p=cvc_xrealloc(v->p,v->cap*32);}memcpy(v->p+v->n*32,id,32);v->n++;}
static int lower_hex(const char*s,size_t n){if(!cvc_ascii_hex(s,n))return 0;for(size_t i=0;i<n;i++)if(s[i]>='A'&&s[i]<='F')return 0;return 1;}
static int collect_objects(CvcRepo*r,Oids*all){
    memset(all,0,sizeof(*all));char*od=cvc_path_join(r->cvc,"objects");DIR*d=opendir(od);if(!d){free(od);return cvc_errorf("verify: cannot open object store");}struct dirent*de;int rc=0;
    while((de=readdir(d))){if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;char*dp=cvc_path_join(od,de->d_name);struct stat st;if(lstat(dp,&st)<0){free(dp);rc=-1;break;}if(strlen(de->d_name)==2&&lower_hex(de->d_name,2)){
            if(!S_ISDIR(st.st_mode)){free(dp);rc=cvc_errorf("verify: canonical fanout is not a real directory");break;}DIR*e=opendir(dp);if(!e){free(dp);rc=-1;break;}struct dirent*x;while((x=readdir(e))){if(!strcmp(x->d_name,".")||!strcmp(x->d_name,".."))continue;if(strlen(x->d_name)==62&&lower_hex(x->d_name,62)){char*fp=cvc_path_join(dp,x->d_name);struct stat fs;if(lstat(fp,&fs)<0||!S_ISREG(fs.st_mode)){free(fp);rc=cvc_errorf("verify: canonical object path is unsafe");break;}char h[65];memcpy(h,de->d_name,2);memcpy(h+2,x->d_name,62);h[64]=0;uint8_t id[32];if(!cvc_hex_decode_32(h,id)){free(fp);rc=-1;break;}ids_add(all,id);free(fp);}/* noncanonical temp/other names are ignored */}closedir(e);if(rc<0){free(dp);break;}}
        /* noncanonical fanout names are ignored unless they are symlinks that
           could masquerade as a canonical directory (handled above). */free(dp);
    }closedir(d);free(od);return rc;
}
static int object_type_checked(CvcRepo*r,const uint8_t id[32],CvcObjType want,int tree_root_allowed){
    CvcObjType t;CvcBuf p;if(cvc_object_read(r->cvc,id,&t,&p)<0)return -1;if(t!=want){cvc_buf_free(&p);return cvc_errorf("verify: object reference has wrong type");}int rc=0;
    if(t==CVC_OBJ_BLOB){size_t z=p.len<CVC_PROBE_MAX?p.len:CVC_PROBE_MAX;if(p.len>CVC_TEXT_MAX||memchr(p.data,0,z))rc=cvc_errorf("verify: ineligible blob object");}
    else if(t==CVC_OBJ_SYMLINK){if(memchr(p.data,0,p.len))rc=cvc_errorf("verify: symlink payload contains NUL");}
    else if(t==CVC_OBJ_TREE){CvcTree tr;if(cvc_tree_parse(p.data,p.len,&tr,tree_root_allowed)<0)rc=cvc_errorf("verify: malformed/noncanonical tree");else cvc_tree_free(&tr);}
    else if(t==CVC_OBJ_COMMIT){CvcCommit c;if(cvc_commit_parse(p.data,p.len,&c)<0)rc=cvc_errorf("verify: malformed commit");else cvc_commit_free(&c);}
    cvc_buf_free(&p);return rc;
}
static int validate_one(CvcRepo*r,const uint8_t id[32]){
    CvcObjType t;CvcBuf p;if(cvc_object_read(r->cvc,id,&t,&p)<0)return -1;int rc=0;
    if(t==CVC_OBJ_BLOB){size_t z=p.len<CVC_PROBE_MAX?p.len:CVC_PROBE_MAX;if(p.len>CVC_TEXT_MAX||memchr(p.data,0,z))rc=cvc_errorf("verify: ineligible blob");}
    else if(t==CVC_OBJ_SYMLINK){if(memchr(p.data,0,p.len))rc=cvc_errorf("verify: NUL in symlink object");}
    else if(t==CVC_OBJ_TREE){CvcTree tr;if(cvc_tree_parse(p.data,p.len,&tr,1)<0)rc=cvc_errorf("verify: malformed tree");else{for(size_t i=0;i<tr.n&&rc==0;i++){CvcObjType want=tr.v[i].type==1?CVC_OBJ_BLOB:tr.v[i].type==2?CVC_OBJ_SYMLINK:CVC_OBJ_TREE;if(object_type_checked(r,tr.v[i].oid,want,tr.v[i].type==3?0:1)<0)rc=-1;}cvc_tree_free(&tr);}}
    else if(t==CVC_OBJ_COMMIT){CvcCommit c;if(cvc_commit_parse(p.data,p.len,&c)<0)rc=cvc_errorf("verify: malformed commit");else{if(object_type_checked(r,c.root,CVC_OBJ_TREE,1)<0)rc=-1;for(unsigned i=0;i<c.parent_count&&rc==0;i++)if(object_type_checked(r,c.parents[i],CVC_OBJ_COMMIT,1)<0)rc=-1;cvc_commit_free(&c);}}
    else rc=cvc_errorf("verify: unknown object type");
    cvc_buf_free(&p);return rc;
}
static int validate_ref_tree_rec(CvcRepo*r,const char*abs,const char*rel){
    DIR*d=opendir(abs);if(!d)return cvc_errorf("verify: cannot open refs/heads");struct dirent*de;int rc=0;while((de=readdir(d))){if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;char*ap=cvc_path_join(abs,de->d_name);char*rp=rel[0]?cvc_path_join(rel,de->d_name):cvc_xstrdup(de->d_name);struct stat st;if(lstat(ap,&st)<0){free(ap);free(rp);rc=-1;break;}if(S_ISDIR(st.st_mode)){if(validate_ref_tree_rec(r,ap,rp)<0)rc=-1;}else if(S_ISREG(st.st_mode)){if(!cvc_branch_name_valid(rp)){rc=cvc_errorf("verify: invalid branch-ref pathname %s",rp);}else{int born;uint8_t id[32];if(cvc_branch_read(r,rp,&born,id)<0)rc=-1;}}else rc=cvc_errorf("verify: branch ref entry is not a real file/directory: %s",rp);free(ap);free(rp);if(rc<0)break;}closedir(d);return rc;
}
int cvc_cmd_verify(CvcRepo*r){
    /* repo_open already checked/parses config, HEAD, core metadata and lock. */
    char*h=cvc_path_join(r->cvc,"refs/heads");if(validate_ref_tree_rec(r,h,"")<0){free(h);return -1;}free(h);
    Oids all;if(collect_objects(r,&all)<0)return -1;for(size_t i=0;i<all.n;i++)if(validate_one(r,all.p+i*32)<0){ids_free(&all);return -1;}
    CvcMergePhaseView v;if(cvc_merge_state_view(r,0,&v)<0){ids_free(&all);return -1;}
    printf("verify ok: %zu object(s), merge-state %s\n",all.n,v==CVC_MERGE_NONE?"none":v==CVC_MERGE_ACTIVE?"active":v==CVC_MERGE_FINALIZING?"finalizing":"completed-cleanup-pending");ids_free(&all);return 0;
}
