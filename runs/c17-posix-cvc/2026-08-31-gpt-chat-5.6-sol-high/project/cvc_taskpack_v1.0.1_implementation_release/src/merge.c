#define _POSIX_C_SOURCE 200809L
#include "cvc_merge.h"
#include "cvc_object.h"
#include "cvc_snapshot.h"
#include "cvc_worktree.h"
#include "cvc_diff.h"
#include "cvc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#define MERGE_MAGIC "CVCMERGE1"
#define MERGE_MAGIC_LEN 9u

typedef struct {
    char *path;
    uint8_t bkind, okind, tkind;
    uint8_t boid[32], ooid[32], toid[32];
    int textual;
    int resolved;
    uint8_t rkind;
    uint8_t roid[32];
} MergeConflict;

typedef struct {
    int phase; /* 1 resolution active, 2 finalizing */
    char *branch;
    uint8_t ours[32];
    char *target_branch;
    uint8_t target[32];
    uint8_t base[32];
    unsigned char *message;
    size_t message_len;
    uint8_t provisional_tree[32];
    MergeConflict *conflicts;
    size_t nconflicts, capconflicts;
    uint8_t intended[32];
} MergeState;

static void state_init(MergeState *s){ memset(s,0,sizeof(*s)); }
static void state_free(MergeState *s){
    free(s->branch); free(s->target_branch); free(s->message);
    for(size_t i=0;i<s->nconflicts;i++) free(s->conflicts[i].path);
    free(s->conflicts); state_init(s);
}
static char *state_path(CvcRepo *r){ return cvc_path_join(r->cvc,"state/merge"); }

static int rd_u8(const unsigned char *p,size_t n,size_t *o,uint8_t *v){ if(*o>=n)return -1;*v=p[(*o)++];return 0; }
static int rd_u32(const unsigned char *p,size_t n,size_t *o,uint32_t *v){ if(*o>n||n-*o<4)return -1;*v=cvc_get_u32be(p+*o);*o+=4;return 0; }
static int rd_u64(const unsigned char *p,size_t n,size_t *o,uint64_t *v){ if(*o>n||n-*o<8)return -1;*v=cvc_get_u64be(p+*o);*o+=8;return 0; }
static int rd_bytes(const unsigned char *p,size_t n,size_t *o,void *dst,size_t z){ if(*o>n||z>n-*o)return -1;memcpy(dst,p+*o,z);*o+=z;return 0; }
static int rd_string32(const unsigned char*p,size_t n,size_t*o,char **out){
    uint32_t z;if(rd_u32(p,n,o,&z)<0||z==0||z>n-*o)return -1;
    if(!cvc_utf8_valid(p+*o,z)||memchr(p+*o,0,z))return -1;
    *out=cvc_xstrndup((const char*)p+*o,z);*o+=z;return 0;
}
static void put_string32(CvcBuf*b,const char*s){ size_t n=strlen(s);cvc_put_u32be(b,(uint32_t)n);cvc_buf_append(b,s,n); }
static int kind_valid(uint8_t k){return k<=3;}

static int state_serialize(const MergeState*s,CvcBuf*b){
    cvc_buf_init(b); cvc_buf_append(b,MERGE_MAGIC,MERGE_MAGIC_LEN); cvc_buf_putc(b,(unsigned char)s->phase);
    put_string32(b,s->branch); cvc_buf_append(b,s->ours,32); put_string32(b,s->target_branch);
    cvc_buf_append(b,s->target,32); cvc_buf_append(b,s->base,32);
    cvc_put_u64be(b,(uint64_t)s->message_len); cvc_buf_append(b,s->message,s->message_len);
    cvc_buf_append(b,s->provisional_tree,32);
    if(s->nconflicts>UINT32_MAX){cvc_buf_free(b);return cvc_errorf("too many merge conflicts");}
    cvc_put_u32be(b,(uint32_t)s->nconflicts);
    for(size_t i=0;i<s->nconflicts;i++){
        const MergeConflict*c=&s->conflicts[i]; put_string32(b,c->path);
        cvc_buf_putc(b,c->bkind);cvc_buf_append(b,c->boid,32);
        cvc_buf_putc(b,c->okind);cvc_buf_append(b,c->ooid,32);
        cvc_buf_putc(b,c->tkind);cvc_buf_append(b,c->toid,32);
        cvc_buf_putc(b,(unsigned char)(c->textual?1:0));cvc_buf_putc(b,(unsigned char)(c->resolved?1:0));
        cvc_buf_putc(b,c->rkind);cvc_buf_append(b,c->roid,32);
    }
    cvc_buf_append(b,s->intended,32);
    return 0;
}
static int state_write(CvcRepo*r,const MergeState*s){ CvcBuf b;if(state_serialize(s,&b)<0)return -1;char*p=state_path(r);int rc=cvc_repo_atomic_meta_write(r,p,b.data,b.len,0666);free(p);cvc_buf_free(&b);return rc; }
static int state_remove(CvcRepo*r){ char*p=state_path(r);if(unlink(p)<0){int e=errno;free(p);if(e==ENOENT)return 0;return cvc_errorf("cannot remove merge state: %s",strerror(e));}int rc=cvc_fsync_parent(p);free(p);return rc<0?cvc_errorf("cannot sync merge-state removal"):0; }

static int validate_identity_object(CvcRepo*r,uint8_t kind,const uint8_t oid[32]){
    if(kind==0){uint8_t z[32]={0};return memcmp(oid,z,32)==0?0:-1;}
    CvcObjType t;CvcBuf p;if(cvc_object_read(r->cvc,oid,&t,&p)<0)return -1;
    int ok=(kind==1&&t==CVC_OBJ_BLOB)||(kind==2&&t==CVC_OBJ_SYMLINK)||(kind==3&&t==CVC_OBJ_TREE);
    if(ok&&kind==3){CvcTree tr;if(cvc_tree_parse(p.data,p.len,&tr,1)<0)ok=0;else cvc_tree_free(&tr);}cvc_buf_free(&p);
    return ok?0:-1;
}
static int state_load(CvcRepo*r,MergeState*s,int *present){
    state_init(s);*present=0;char*p=state_path(r);struct stat st;
    if(lstat(p,&st)<0){int e=errno;free(p);if(e==ENOENT)return 0;return cvc_errorf("cannot inspect merge state");}
    if(!S_ISREG(st.st_mode)){free(p);return cvc_errorf("repository integrity: merge state must be regular file");}
    CvcBuf b;if(cvc_read_file_nofollow(p,&b)<0){free(p);return cvc_errorf("cannot read merge state");}free(p);*present=1;
    size_t o=0;uint8_t u8;uint32_t nc;uint64_t ml;
    if(b.len<MERGE_MAGIC_LEN+1||memcmp(b.data,MERGE_MAGIC,MERGE_MAGIC_LEN)){goto bad;}o=MERGE_MAGIC_LEN;
    if(rd_u8(b.data,b.len,&o,&u8)<0||(u8!=1&&u8!=2))goto bad;
    s->phase=u8;
    if(rd_string32(b.data,b.len,&o,&s->branch)<0||!cvc_branch_name_valid(s->branch))goto bad;
    if(rd_bytes(b.data,b.len,&o,s->ours,32)<0||rd_string32(b.data,b.len,&o,&s->target_branch)<0||!cvc_branch_name_valid(s->target_branch))goto bad;
    if(rd_bytes(b.data,b.len,&o,s->target,32)<0||rd_bytes(b.data,b.len,&o,s->base,32)<0)goto bad;
    if(rd_u64(b.data,b.len,&o,&ml)<0||ml==0||ml>SIZE_MAX||ml>b.len-o)goto bad;
    if(!cvc_utf8_valid(b.data+o,(size_t)ml))goto bad;
    s->message=cvc_xmalloc((size_t)ml);memcpy(s->message,b.data+o,(size_t)ml);s->message_len=(size_t)ml;o+=(size_t)ml;
    if(rd_bytes(b.data,b.len,&o,s->provisional_tree,32)<0||rd_u32(b.data,b.len,&o,&nc)<0)goto bad;
    if(nc>1000000u)goto bad;
    s->conflicts=cvc_xcalloc(nc?nc:1,sizeof(*s->conflicts));s->nconflicts=s->capconflicts=nc;
    for(uint32_t i=0;i<nc;i++){
        MergeConflict*c=&s->conflicts[i];uint8_t tx,rs;
        if(rd_string32(b.data,b.len,&o,&c->path)<0||!cvc_repo_path_valid(c->path))goto bad;
        if(i&&strcmp(s->conflicts[i-1].path,c->path)>=0)goto bad;
        if(rd_u8(b.data,b.len,&o,&c->bkind)<0||rd_bytes(b.data,b.len,&o,c->boid,32)<0||
           rd_u8(b.data,b.len,&o,&c->okind)<0||rd_bytes(b.data,b.len,&o,c->ooid,32)<0||
           rd_u8(b.data,b.len,&o,&c->tkind)<0||rd_bytes(b.data,b.len,&o,c->toid,32)<0||
           rd_u8(b.data,b.len,&o,&tx)<0||rd_u8(b.data,b.len,&o,&rs)<0||
           rd_u8(b.data,b.len,&o,&c->rkind)<0||rd_bytes(b.data,b.len,&o,c->roid,32)<0)goto bad;
        if(!kind_valid(c->bkind)||!kind_valid(c->okind)||!kind_valid(c->tkind)||!kind_valid(c->rkind)||tx>1||rs>1)goto bad;
        c->textual=tx;c->resolved=rs;if(!c->resolved&&c->rkind!=0)goto bad;
    }
    if(rd_bytes(b.data,b.len,&o,s->intended,32)<0||o!=b.len)goto bad;
    if(s->phase==1){uint8_t z[32]={0};if(memcmp(s->intended,z,32))goto bad;}else{uint8_t z[32]={0};if(!memcmp(s->intended,z,32))goto bad;}
    /* Validate referenced objects eagerly so state cannot redirect later operations. */
    {CvcObjType t;CvcBuf q;if(cvc_object_read(r->cvc,s->ours,&t,&q)<0||t!=CVC_OBJ_COMMIT){if(t)cvc_buf_free(&q);goto bad2;}cvc_buf_free(&q);
     if(cvc_object_read(r->cvc,s->target,&t,&q)<0||t!=CVC_OBJ_COMMIT){if(t)cvc_buf_free(&q);goto bad2;}cvc_buf_free(&q);
     if(cvc_object_read(r->cvc,s->base,&t,&q)<0||t!=CVC_OBJ_COMMIT){if(t)cvc_buf_free(&q);goto bad2;}cvc_buf_free(&q);
     if(cvc_object_read(r->cvc,s->provisional_tree,&t,&q)<0||t!=CVC_OBJ_TREE){if(t)cvc_buf_free(&q);goto bad2;}cvc_buf_free(&q);}
    for(size_t i=0;i<s->nconflicts;i++){
        MergeConflict*c=&s->conflicts[i];
        if(validate_identity_object(r,c->bkind,c->boid)<0||validate_identity_object(r,c->okind,c->ooid)<0||validate_identity_object(r,c->tkind,c->toid)<0||(c->resolved&&validate_identity_object(r,c->rkind,c->roid)<0))goto bad2;
    }
    if(s->phase==2){CvcObjType t;CvcBuf q;if(cvc_object_read(r->cvc,s->intended,&t,&q)<0||t!=CVC_OBJ_COMMIT){if(t)cvc_buf_free(&q);goto bad2;}CvcCommit cc;if(cvc_commit_parse(q.data,q.len,&cc)<0){cvc_buf_free(&q);goto bad2;}cvc_commit_free(&cc);cvc_buf_free(&q);}
    cvc_buf_free(&b);return 0;
bad: cvc_buf_free(&b);state_free(s);return cvc_errorf("repository integrity: malformed merge state");
bad2:cvc_buf_free(&b);state_free(s);return cvc_errorf("repository integrity: merge state references invalid object");
}

static int state_ref_relation(CvcRepo*r,const MergeState*s,int *rel){
    if(!r->branch||!s->branch||!cvc_branch_name_valid(r->branch)||!cvc_branch_name_valid(s->branch))
        return cvc_errorf("repository integrity: invalid HEAD or merge branch");
    if(strcmp(r->branch,s->branch))return cvc_errorf("repository integrity: HEAD branch changed during merge");
    int born;uint8_t id[32];if(cvc_branch_read(r,s->branch,&born,id)<0)return -1;if(!born)return cvc_errorf("repository integrity: merge branch became unborn");
    if(!memcmp(id,s->ours,32)){*rel=0;return 0;}
    if(s->phase==2&&!memcmp(id,s->intended,32)){*rel=1;return 0;}
    return cvc_errorf("repository integrity: current branch ref changed during merge");
}
int cvc_merge_state_view(CvcRepo*r,int mutating,CvcMergePhaseView*view){
    MergeState s;int pr;if(state_load(r,&s,&pr)<0)return -1;if(!pr){*view=CVC_MERGE_NONE;return 0;}int rel;if(state_ref_relation(r,&s,&rel)<0){state_free(&s);return -1;}
    if(s.phase==2&&rel==1){if(mutating){if(state_remove(r)<0){state_free(&s);return -1;}*view=CVC_MERGE_NONE;}else *view=CVC_MERGE_COMPLETED_PENDING;state_free(&s);return 0;}
    *view=s.phase==1?CVC_MERGE_ACTIVE:CVC_MERGE_FINALIZING;state_free(&s);return 0;
}
int cvc_merge_forbid_active(CvcRepo*r,const char*op){CvcMergePhaseView v;if(cvc_merge_state_view(r,1,&v)<0)return -1;if(v!=CVC_MERGE_NONE)return cvc_errorf("cannot %s while merge is active",op);return 0;}

static int snapshot_from_root_tree(CvcRepo*r,const uint8_t oid[32],CvcSnapshot*s){return cvc_snapshot_from_tree(r->cvc,oid,s);}
static int snapshot_commit(CvcRepo*r,const uint8_t id[32],CvcSnapshot*s){return cvc_snapshot_from_commit(r->cvc,id,s);}
static int path_under(const char*p,const char*root){size_t n=strlen(root);return !strcmp(p,root)||(!strncmp(p,root,n)&&p[n]=='/');}
static int strict_desc(const char*p,const char*root){size_t n=strlen(root);return n==0?*p!='\0':(!strncmp(p,root,n)&&p[n]=='/'&&p[n+1]);}
static int snap_node_kind(const CvcSnapshot*s,const char*path){ssize_t i=cvc_snapshot_find(s,path);if(i>=0)return s->v[i].type;for(size_t k=0;k<s->n;k++)if(strict_desc(s->v[k].path,path))return 3;return 0;}
static int add_subtree(const CvcSnapshot*src,const char*root,CvcSnapshot*out){for(size_t i=0;i<src->n;i++)if(path_under(src->v[i].path,root))if(cvc_snapshot_add(out,src->v[i].path,src->v[i].type,src->v[i].oid)<0)return -1;return 0;}
static int relative_subtree(const CvcSnapshot*src,const char*root,CvcSnapshot*out){
    cvc_snapshot_init(out);size_t n=strlen(root);for(size_t i=0;i<src->n;i++)if(strict_desc(src->v[i].path,root)){
        const char*q=src->v[i].path+n+1;if(cvc_snapshot_add(out,q,src->v[i].type,src->v[i].oid)<0){cvc_snapshot_free(out);return -1;}}
    cvc_snapshot_sort(out);return 0;
}
static int node_identity(CvcRepo*r,const CvcSnapshot*s,const char*path,uint8_t*kind,uint8_t oid[32]){
    *kind=(uint8_t)snap_node_kind(s,path);memset(oid,0,32);if(*kind==0)return 0;
    if(*kind==1||*kind==2){ssize_t i=cvc_snapshot_find(s,path);if(i<0)return -1;memcpy(oid,s->v[i].oid,32);return 0;}
    CvcSnapshot sub;if(relative_subtree(s,path,&sub)<0)return -1;int rc=cvc_snapshot_write_tree(r->cvc,&sub,oid);cvc_snapshot_free(&sub);return rc;
}
static int id_equal(uint8_t ak,const uint8_t a[32],uint8_t bk,const uint8_t b[32]){return ak==bk&&(ak==0||!memcmp(a,b,32));}
static int collect_children_one(const CvcSnapshot*s,const char*prefix,CvcStrVec*v){
    size_t pn=strlen(prefix);for(size_t i=0;i<s->n;i++){
        const char*p=s->v[i].path;const char*q;
        if(pn==0)q=p;else {if(strncmp(p,prefix,pn)||p[pn]!='/')continue;q=p+pn+1;}
        const char*slash=strchr(q,'/');size_t n=slash?(size_t)(slash-q):strlen(q);if(n){char*x=cvc_xstrndup(q,n);if(!cvc_strvec_contains(v,x))cvc_strvec_push(v,x);free(x);}
    }return 0;
}
static int add_conflict(MergeState*s,const char*path,uint8_t bk,const uint8_t bo[32],uint8_t ok,const uint8_t oo[32],uint8_t tk,const uint8_t to[32],int textual){
    if(s->nconflicts==s->capconflicts){s->capconflicts=s->capconflicts?s->capconflicts*2:8;s->conflicts=cvc_xrealloc(s->conflicts,s->capconflicts*sizeof(*s->conflicts));}
    MergeConflict*c=&s->conflicts[s->nconflicts++];memset(c,0,sizeof(*c));c->path=cvc_xstrdup(path);c->bkind=bk;c->okind=ok;c->tkind=tk;c->textual=textual;memcpy(c->boid,bo,32);memcpy(c->ooid,oo,32);memcpy(c->toid,to,32);return 0;
}
static int blob_read(CvcRepo*r,const uint8_t id[32],CvcBuf*b){CvcObjType t;if(cvc_object_read(r->cvc,id,&t,b)<0)return -1;if(t!=CVC_OBJ_BLOB){cvc_buf_free(b);return cvc_errorf("merge expected blob");}return 0;}
static int blob_eligible(const CvcBuf*b){size_t n=b->len<CVC_PROBE_MAX?b->len:CVC_PROBE_MAX;return b->len<=CVC_TEXT_MAX&&!memchr(b->data,0,n);}

static int merge_node(CvcRepo*r,const CvcSnapshot*b,const CvcSnapshot*o,const CvcSnapshot*t,const char*path,CvcSnapshot*result,MergeState*st){
    uint8_t bk,ok,tk,bo[32],oo[32],to[32];if(node_identity(r,b,path,&bk,bo)<0||node_identity(r,o,path,&ok,oo)<0||node_identity(r,t,path,&tk,to)<0)return -1;
    if(id_equal(ok,oo,tk,to))return ok?add_subtree(o,path,result):0;
    if(id_equal(ok,oo,bk,bo))return tk?add_subtree(t,path,result):0;
    if(id_equal(tk,to,bk,bo))return ok?add_subtree(o,path,result):0;
    if(ok==3&&tk==3&&(bk==0||bk==3)){
        CvcStrVec kids;cvc_strvec_init(&kids);collect_children_one(b,path,&kids);collect_children_one(o,path,&kids);collect_children_one(t,path,&kids);cvc_strvec_sort(&kids);
        for(size_t i=0;i<kids.n;i++){char*ch=path[0]?cvc_path_join(path,kids.v[i]):cvc_xstrdup(kids.v[i]);if(merge_node(r,b,o,t,ch,result,st)<0){free(ch);cvc_strvec_free(&kids);return -1;}free(ch);}cvc_strvec_free(&kids);return 0;
    }
    if(ok==1&&tk==1&&bk==1){
        CvcBuf bb,ob,tb,res;if(blob_read(r,bo,&bb)<0||blob_read(r,oo,&ob)<0){if(bb.data)cvc_buf_free(&bb);return -1;}if(blob_read(r,to,&tb)<0){cvc_buf_free(&bb);cvc_buf_free(&ob);return -1;}int conf=0;int rc=cvc_three_way_text(bb.data,bb.len,ob.data,ob.len,tb.data,tb.len,&res,&conf);cvc_buf_free(&bb);cvc_buf_free(&ob);cvc_buf_free(&tb);if(rc<0)return -1;
        if(!conf&&blob_eligible(&res)){uint8_t id[32];if(cvc_object_store(r->cvc,CVC_OBJ_BLOB,res.data,res.len,id)<0){cvc_buf_free(&res);return -1;}cvc_snapshot_add(result,path,1,id);cvc_buf_free(&res);return 0;}cvc_buf_free(&res);return add_conflict(st,path,bk,bo,ok,oo,tk,to,conf?1:0);
    }
    /* Different add/add regular files are content conflicts with markers. */
    if(bk==0&&ok==1&&tk==1)return add_conflict(st,path,bk,bo,ok,oo,tk,to,1);
    return add_conflict(st,path,bk,bo,ok,oo,tk,to,0);
}

static int merge_trees(CvcRepo*r,const CvcSnapshot*b,const CvcSnapshot*o,const CvcSnapshot*t,CvcSnapshot*result,MergeState*st){
    cvc_snapshot_init(result);CvcStrVec roots;cvc_strvec_init(&roots);collect_children_one(b,"",&roots);collect_children_one(o,"",&roots);collect_children_one(t,"",&roots);cvc_strvec_sort(&roots);
    for(size_t i=0;i<roots.n;i++)if(merge_node(r,b,o,t,roots.v[i],result,st)<0){cvc_strvec_free(&roots);cvc_snapshot_free(result);return -1;}
    cvc_strvec_free(&roots);cvc_snapshot_sort(result);
    if(st->nconflicts>1){/* recursive traversal is already byte-sorted, but enforce deterministic state ordering */
        for(size_t i=1;i<st->nconflicts;i++)if(strcmp(st->conflicts[i-1].path,st->conflicts[i].path)>=0)return cvc_errorf("internal nondeterministic conflict ordering");
    }return 0;
}

static int identity_to_snapshot(CvcRepo*r,const char*root,uint8_t kind,const uint8_t oid[32],CvcSnapshot*out){
    cvc_snapshot_init(out);if(kind==0)return 0;if(kind==1||kind==2)return cvc_snapshot_add(out,root,kind,oid);
    CvcSnapshot rel;if(cvc_snapshot_from_tree(r->cvc,oid,&rel)<0)return -1;for(size_t i=0;i<rel.n;i++){char*p=cvc_path_join(root,rel.v[i].path);cvc_snapshot_add(out,p,rel.v[i].type,rel.v[i].oid);free(p);}cvc_snapshot_free(&rel);cvc_snapshot_sort(out);return 0;
}
static int work_snapshot_for_conflicts(CvcRepo*r,const CvcSnapshot*prov,const MergeState*st,CvcSnapshot*out){
    cvc_snapshot_init(out);for(size_t i=0;i<prov->n;i++)cvc_snapshot_add(out,prov->v[i].path,prov->v[i].type,prov->v[i].oid);
    for(size_t i=0;i<st->nconflicts;i++){const MergeConflict*c=&st->conflicts[i];if(c->okind){CvcSnapshot q;if(identity_to_snapshot(r,c->path,c->okind,c->ooid,&q)<0){cvc_snapshot_free(out);return -1;}for(size_t j=0;j<q.n;j++)cvc_snapshot_add(out,q.v[j].path,q.v[j].type,q.v[j].oid);cvc_snapshot_free(&q);}}
    cvc_snapshot_sort(out);return 0;
}
static int write_marker(CvcRepo*r,const MergeConflict*c){
    if(!c->textual||c->okind!=1||c->tkind!=1)return 0;
    CvcBuf ob,tb,m;if(blob_read(r,c->ooid,&ob)<0)return -1;if(blob_read(r,c->toid,&tb)<0){cvc_buf_free(&ob);return -1;}cvc_buf_init(&m);cvc_buf_append(&m,"<<<<<<< ours\n",13);cvc_buf_append(&m,ob.data,ob.len);if(ob.len&&ob.data[ob.len-1]!='\n')cvc_buf_putc(&m,'\n');cvc_buf_append(&m,"=======\n",8);cvc_buf_append(&m,tb.data,tb.len);if(tb.len&&tb.data[tb.len-1]!='\n')cvc_buf_putc(&m,'\n');cvc_buf_append(&m,">>>>>>> theirs\n",15);int rc=cvc_write_working_bytes(r,c->path,m.data,m.len);cvc_buf_free(&ob);cvc_buf_free(&tb);cvc_buf_free(&m);return rc;
}

static int valid_commit_id(CvcRepo*r,const uint8_t id[32]){CvcObjType t;CvcBuf p;if(cvc_object_read(r->cvc,id,&t,&p)<0)return -1;if(t!=CVC_OBJ_COMMIT){cvc_buf_free(&p);return -1;}CvcCommit c;int rc=cvc_commit_parse(p.data,p.len,&c);if(!rc)cvc_commit_free(&c);cvc_buf_free(&p);return rc;}
static int ref_move_after_materialize(CvcRepo*r,const uint8_t id[32],const CvcSnapshot*old,const CvcSnapshot*nw){
    if(cvc_branch_write(r,r->branch,1,id)<0){cvc_warnf("ref update failed; restoring original working tree");if(cvc_materialize_snapshot(r,nw,old)<0)cvc_warnf("working-tree rollback also failed");return -1;}r->born=1;memcpy(r->head,id,32);return 0;
}
int cvc_cmd_merge_start(CvcRepo*r,const char*target,const char*msg){
    if(!cvc_branch_name_valid(target))return cvc_errorf("invalid branch name");
    CvcMergePhaseView view;if(cvc_merge_state_view(r,1,&view)<0)return -1;if(view!=CVC_MERGE_NONE)return cvc_errorf("merge already in progress");
    if(!strcmp(target,r->branch)){printf("already up to date (self merge)\n");return 0;}
    int tb;uint8_t tid[32];if(cvc_branch_read(r,target,&tb,tid)<0)return -1;
    if(!tb){printf("no commits to merge\n");return 0;}
    CvcSnapshot ours,work,theirs;if(r->born){if(snapshot_commit(r,r->head,&ours)<0)return -1;int clean;if(cvc_worktree_is_clean(r,&ours,&work,&clean)<0){cvc_snapshot_free(&ours);return -1;}cvc_snapshot_free(&work);if(!clean){cvc_snapshot_free(&ours);return cvc_errorf("working tree has selected changes; merge refused");}}else cvc_snapshot_init(&ours);
    if(snapshot_commit(r,tid,&theirs)<0){cvc_snapshot_free(&ours);return -1;}
    if(!r->born){if(cvc_materialize_snapshot(r,&ours,&theirs)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}if(ref_move_after_materialize(r,tid,&ours,&theirs)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}printf("fast-forward to %s\n",target);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return 0;}
    if(!memcmp(r->head,tid,32)){printf("already up to date\n");cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return 0;}
    int yes;if(cvc_commit_is_ancestor(r,tid,r->head,&yes)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}if(yes){printf("already up to date\n");cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return 0;}
    if(cvc_commit_is_ancestor(r,r->head,tid,&yes)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}if(yes){if(cvc_materialize_snapshot(r,&ours,&theirs)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}if(ref_move_after_materialize(r,tid,&ours,&theirs)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}printf("fast-forward to %s\n",target);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return 0;}
    uint8_t baseid[32];if(cvc_merge_base(r,r->head,tid,baseid)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}CvcSnapshot base;if(snapshot_commit(r,baseid,&base)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}
    MergeState st;state_init(&st);st.phase=1;st.branch=cvc_xstrdup(r->branch);memcpy(st.ours,r->head,32);st.target_branch=cvc_xstrdup(target);memcpy(st.target,tid,32);memcpy(st.base,baseid,32);
    const char*defprefix="Merge branch '";CvcBuf mb;cvc_buf_init(&mb);if(msg){st.message_len=strlen(msg);st.message=cvc_xmalloc(st.message_len);memcpy(st.message,msg,st.message_len);}else{cvc_buf_append(&mb,defprefix,strlen(defprefix));cvc_buf_append(&mb,target,strlen(target));cvc_buf_putc(&mb,'\'');st.message_len=mb.len;st.message=cvc_xmalloc(mb.len);memcpy(st.message,mb.data,mb.len);cvc_buf_free(&mb);}
    CvcSnapshot merged;if(merge_trees(r,&base,&ours,&theirs,&merged,&st)<0){state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);return -1;}
    if(!st.nconflicts){uint8_t tree[32];int64_t ts;if(cvc_snapshot_write_tree(r->cvc,&merged,tree)<0||cvc_get_timestamp(&ts)<0){state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}uint8_t par[64];memcpy(par,st.ours,32);memcpy(par+32,st.target,32);uint8_t cid[32];if(cvc_commit_store(r->cvc,tree,2,par,ts,st.message,st.message_len,cid)<0||cvc_materialize_snapshot(r,&ours,&merged)<0||ref_move_after_materialize(r,cid,&ours,&merged)<0){state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}char h[65];cvc_hex_encode(cid,32,h);printf("merged %s as %s\n",target,h);state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return 0;}
    if(cvc_snapshot_write_tree(r->cvc,&merged,st.provisional_tree)<0){state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}
    CvcSnapshot workmerge;if(work_snapshot_for_conflicts(r,&merged,&st,&workmerge)<0||cvc_materialize_snapshot(r,&ours,&workmerge)<0){cvc_snapshot_free(&workmerge);state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}
    for(size_t i=0;i<st.nconflicts;i++)if(write_marker(r,&st.conflicts[i])<0){/* Best effort rollback before publishing state. */cvc_materialize_snapshot_owned(r,&workmerge,&ours,NULL);cvc_snapshot_free(&workmerge);state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}
    if(state_write(r,&st)<0){cvc_warnf("failed to persist merge state; restoring pre-merge worktree");cvc_materialize_snapshot_owned(r,&workmerge,&ours,NULL);cvc_snapshot_free(&workmerge);state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return -1;}
    printf("merge has %zu conflict(s); resolve each conflict then run cvc merge --continue\n",st.nconflicts);
    for(size_t i=0;i<st.nconflicts;i++)printf("conflict %s\n",st.conflicts[i].path);
    cvc_snapshot_free(&workmerge);state_free(&st);cvc_snapshot_free(&base);cvc_snapshot_free(&ours);cvc_snapshot_free(&theirs);cvc_snapshot_free(&merged);return 1;
}

/* Explicit-resolution scanner. It intentionally ignores tracking globs, but
 * treats an ineligible regular file as a hard error rather than silently
 * dropping it. Nested repositories and special files are excluded. */
static int readlink_buf_local(const char*p,CvcBuf*b){cvc_buf_init(b);size_t cap=128;for(;;){b->data=cvc_xrealloc(b->data,cap);ssize_t n=readlink(p,(char*)b->data,cap);if(n<0){cvc_buf_free(b);return -1;}if((size_t)n<cap){b->len=(size_t)n;b->cap=cap;return 0;}cap*=2;}}
static int has_child_cvc(const char*abs){char*p=cvc_path_join(abs,".cvc");struct stat st;int y=lstat(p,&st)==0&&S_ISDIR(st.st_mode);free(p);return y;}
static int resolve_scan_dir(CvcRepo*r,const char*rootrel,CvcSnapshot*out){
    char*abs=cvc_path_join(r->root,rootrel);DIR*d=opendir(abs);if(!d){free(abs);return cvc_errorf("cannot read resolution directory %s",rootrel);}struct dirent*de;int rc=0;
    while((de=readdir(d))){if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;size_t nl=strlen(de->d_name);if(!cvc_name_component_valid((const unsigned char*)de->d_name,nl)){cvc_warnf("excluding unsafe resolution filename beneath %s",rootrel);continue;}char*rel=cvc_path_join(rootrel,de->d_name);char*ap=cvc_path_join(r->root,rel);struct stat st;if(lstat(ap,&st)<0){free(rel);free(ap);rc=cvc_errorf("cannot inspect resolution path");break;}
        if(S_ISDIR(st.st_mode)){if(!has_child_cvc(ap)&&resolve_scan_dir(r,rel,out)<0)rc=-1;free(rel);free(ap);if(rc<0)break;continue;}
        if(S_ISREG(st.st_mode)){if((uint64_t)st.st_size>CVC_TEXT_MAX){free(rel);free(ap);rc=cvc_errorf("ineligible regular file in resolution");break;}CvcBuf b;if(cvc_read_file_nofollow(ap,&b)<0){free(rel);free(ap);rc=-1;break;}size_t z=b.len<CVC_PROBE_MAX?b.len:CVC_PROBE_MAX;if(memchr(b.data,0,z)){cvc_buf_free(&b);free(rel);free(ap);rc=cvc_errorf("ineligible regular file in resolution");break;}uint8_t id[32];if(cvc_object_store(r->cvc,CVC_OBJ_BLOB,b.data,b.len,id)<0){cvc_buf_free(&b);free(rel);free(ap);rc=-1;break;}cvc_snapshot_add(out,rel,1,id);cvc_buf_free(&b);
        }else if(S_ISLNK(st.st_mode)){CvcBuf b;if(readlink_buf_local(ap,&b)<0){free(rel);free(ap);rc=-1;break;}uint8_t id[32];if(cvc_object_store(r->cvc,CVC_OBJ_SYMLINK,b.data,b.len,id)<0){cvc_buf_free(&b);free(rel);free(ap);rc=-1;break;}cvc_snapshot_add(out,rel,2,id);cvc_buf_free(&b);}
        /* specials excluded */ free(rel);free(ap);
    }
    closedir(d);free(abs);return rc;
}
static int capture_resolution(CvcRepo*r,const char*path,uint8_t*kind,uint8_t oid[32]){
    char*ap=cvc_path_join(r->root,path);struct stat st;if(lstat(ap,&st)<0){int e=errno;free(ap);if(e==ENOENT||e==ENOTDIR){*kind=0;memset(oid,0,32);return 0;}return cvc_errorf("cannot inspect resolution path");}
    if(S_ISREG(st.st_mode)){int ex;uint8_t ty;if(cvc_worktree_entry(r,path,&ex,&ty,oid,1)<0){free(ap);return -1;}if(!ex||ty!=1){free(ap);return -1;}CvcBuf b;if(cvc_read_file_nofollow(ap,&b)<0){free(ap);return -1;}free(ap);int rc=cvc_object_store(r->cvc,CVC_OBJ_BLOB,b.data,b.len,oid);cvc_buf_free(&b);*kind=1;return rc;}
    if(S_ISLNK(st.st_mode)){CvcBuf b;if(readlink_buf_local(ap,&b)<0){free(ap);return -1;}free(ap);int rc=cvc_object_store(r->cvc,CVC_OBJ_SYMLINK,b.data,b.len,oid);cvc_buf_free(&b);*kind=2;return rc;}
    if(S_ISDIR(st.st_mode)){free(ap);CvcSnapshot full;cvc_snapshot_init(&full);if(resolve_scan_dir(r,path,&full)<0){cvc_snapshot_free(&full);return -1;}CvcSnapshot rel;cvc_snapshot_init(&rel);size_t n=strlen(path);for(size_t i=0;i<full.n;i++){const char*q=full.v[i].path+n+1;cvc_snapshot_add(&rel,q,full.v[i].type,full.v[i].oid);}cvc_snapshot_sort(&rel);int rc;if(rel.n==0){*kind=0;memset(oid,0,32);rc=0;}else{*kind=3;rc=cvc_snapshot_write_tree(r->cvc,&rel,oid);}cvc_snapshot_free(&full);cvc_snapshot_free(&rel);return rc;}
    free(ap);return cvc_errorf("conflict root must resolve to absence, file, symlink, or directory");
}
static MergeConflict *find_conflict(MergeState*s,const char*path){for(size_t i=0;i<s->nconflicts;i++)if(!strcmp(s->conflicts[i].path,path))return &s->conflicts[i];return NULL;}
int cvc_cmd_resolve(CvcRepo*r,const char*path){
    MergeState s;int pr;if(state_load(r,&s,&pr)<0)return -1;if(!pr)return cvc_errorf("no merge in progress");int rel;if(state_ref_relation(r,&s,&rel)<0){state_free(&s);return -1;}if(s.phase!=1){state_free(&s);return cvc_errorf("merge is finalizing; resolve is no longer allowed");}
    MergeConflict*c=find_conflict(&s,path);if(!c){state_free(&s);return cvc_errorf("path is not an exact conflict root");}uint8_t kind,id[32];if(capture_resolution(r,path,&kind,id)<0){state_free(&s);return -1;}c->resolved=1;c->rkind=kind;memcpy(c->roid,id,32);if(state_write(r,&s)<0){state_free(&s);return -1;}printf("resolved %s\n",path);state_free(&s);return 0;
}

static int build_final_snapshot(CvcRepo*r,const MergeState*s,CvcSnapshot*out){
    if(snapshot_from_root_tree(r,s->provisional_tree,out)<0)return -1;
    for(size_t i=0;i<s->nconflicts;i++){const MergeConflict*c=&s->conflicts[i];CvcSnapshot q;if(identity_to_snapshot(r,c->path,c->rkind,c->roid,&q)<0){cvc_snapshot_free(out);return -1;}for(size_t j=0;j<q.n;j++)cvc_snapshot_add(out,q.v[j].path,q.v[j].type,q.v[j].oid);cvc_snapshot_free(&q);}cvc_snapshot_sort(out);return 0;
}
static int compare_identity_current(CvcRepo*r,const char*path,uint8_t kind,const uint8_t oid[32],int *same){
    uint8_t ck,co[32];if(capture_resolution(r,path,&ck,co)<0)return -1;*same=id_equal(ck,co,kind,oid);return 0;
}
static int verify_provisional_work(CvcRepo*r,const MergeState*s){
    CvcSnapshot prov;if(snapshot_from_root_tree(r,s->provisional_tree,&prov)<0)return -1;
    /* Verify every provisional leaf exactly; unlike a whole-root all-files scan,
       unrelated untracked files are intentionally irrelevant. */
    for(size_t i=0;i<prov.n;i++){int ex;uint8_t ty,id[32];if(cvc_worktree_entry(r,prov.v[i].path,&ex,&ty,id,1)<0){cvc_snapshot_free(&prov);return -1;}if(!ex||ty!=prov.v[i].type||memcmp(id,prov.v[i].oid,32)){char*p=cvc_xstrdup(prov.v[i].path);cvc_snapshot_free(&prov);int rc=cvc_errorf("provisional merge path changed after merge: %s",p);free(p);return rc;}}
    /* Paths that were intentionally absent from provisional but lie beneath
       a conflict root are checked via resolutions.  Deletions elsewhere must
       also remain absent if they were tracked by ours; compare against ours. */
    CvcSnapshot ours;if(snapshot_commit(r,s->ours,&ours)<0){cvc_snapshot_free(&prov);return -1;}
    for(size_t i=0;i<ours.n;i++)if(cvc_snapshot_find(&prov,ours.v[i].path)<0){int inside=0;for(size_t j=0;j<s->nconflicts;j++)if(path_under(ours.v[i].path,s->conflicts[j].path)){inside=1;break;}if(!inside){int ex;uint8_t ty,id[32];if(cvc_worktree_entry(r,ours.v[i].path,&ex,&ty,id,0)<0){cvc_snapshot_free(&ours);cvc_snapshot_free(&prov);return -1;}if(ex){cvc_snapshot_free(&ours);cvc_snapshot_free(&prov);return cvc_errorf("provisional deletion was modified after merge");}}}
    cvc_snapshot_free(&ours);cvc_snapshot_free(&prov);return 0;
}
int cvc_cmd_merge_continue(CvcRepo*r,const char*override){
    MergeState s;int pr;if(state_load(r,&s,&pr)<0)return -1;if(!pr)return cvc_errorf("no merge in progress");int rel;if(state_ref_relation(r,&s,&rel)<0){state_free(&s);return -1;}
    if(s.phase==2){if(rel==1){if(state_remove(r)<0){state_free(&s);return -1;}memcpy(r->head,s.intended,32);printf("merge was already finalized; cleaned stale state\n");state_free(&s);return 0;}if(override){state_free(&s);return cvc_errorf("-m is not allowed when retrying finalization");}if(valid_commit_id(r,s.intended)<0){state_free(&s);return cvc_errorf("repository integrity: intended merge commit missing or invalid");}if(cvc_branch_write(r,s.branch,1,s.intended)<0){state_free(&s);return -1;}memcpy(r->head,s.intended,32);if(state_remove(r)<0)cvc_warnf("merge committed but stale finalizing state remains");char h[65];cvc_hex_encode(s.intended,32,h);printf("merge finalized %s\n",h);state_free(&s);return 0;}
    for(size_t i=0;i<s.nconflicts;i++)if(!s.conflicts[i].resolved){state_free(&s);return cvc_errorf("not all merge conflicts are resolved");}
    if(verify_provisional_work(r,&s)<0){state_free(&s);return -1;}for(size_t i=0;i<s.nconflicts;i++){int same;if(compare_identity_current(r,s.conflicts[i].path,s.conflicts[i].rkind,s.conflicts[i].roid,&same)<0){state_free(&s);return -1;}if(!same){char*p=cvc_xstrdup(s.conflicts[i].path);state_free(&s);int rc=cvc_errorf("resolved path changed after resolve: %s; run cvc resolve again",p);free(p);return rc;}}
    CvcSnapshot final;if(build_final_snapshot(r,&s,&final)<0){state_free(&s);return -1;}uint8_t tree[32];if(cvc_snapshot_write_tree(r->cvc,&final,tree)<0){cvc_snapshot_free(&final);state_free(&s);return -1;}const unsigned char*message=s.message;size_t ml=s.message_len;if(override){message=(const unsigned char*)override;ml=strlen(override);}int64_t ts;if(cvc_get_timestamp(&ts)<0){cvc_snapshot_free(&final);state_free(&s);return -1;}uint8_t par[64];memcpy(par,s.ours,32);memcpy(par+32,s.target,32);if(cvc_commit_store(r->cvc,tree,2,par,ts,message,ml,s.intended)<0){cvc_snapshot_free(&final);state_free(&s);return -1;}
    s.phase=2;if(override){free(s.message);s.message=cvc_xmalloc(ml);memcpy(s.message,message,ml);s.message_len=ml;}if(state_write(r,&s)<0){cvc_snapshot_free(&final);state_free(&s);return -1;}
    if(cvc_branch_write(r,s.branch,1,s.intended)<0){cvc_snapshot_free(&final);state_free(&s);return -1;}memcpy(r->head,s.intended,32);char h[65];cvc_hex_encode(s.intended,32,h);if(state_remove(r)<0)cvc_warnf("merge committed but stale finalizing state remains");printf("merge finalized %s\n",h);cvc_snapshot_free(&final);state_free(&s);return 0;
}
int cvc_cmd_merge_abort(CvcRepo*r){
    MergeState s;int pr;if(state_load(r,&s,&pr)<0)return -1;if(!pr)return cvc_errorf("no merge in progress");int rel;if(state_ref_relation(r,&s,&rel)<0){state_free(&s);return -1;}
    if(s.phase==2&&rel==1){if(state_remove(r)<0){state_free(&s);return -1;}memcpy(r->head,s.intended,32);state_free(&s);return cvc_errorf("no active merge to abort; merge was already completed");}
    CvcSnapshot ours;if(snapshot_commit(r,s.ours,&ours)<0){state_free(&s);return -1;}CvcSnapshot current;if(s.phase==1){if(snapshot_from_root_tree(r,s.provisional_tree,&current)<0){cvc_snapshot_free(&ours);state_free(&s);return -1;}/* Add conflict-root current expected ownership to allow safe removal/restoration. */for(size_t i=0;i<s.nconflicts;i++){CvcSnapshot q;uint8_t k=s.conflicts[i].resolved?s.conflicts[i].rkind:s.conflicts[i].okind;const uint8_t*id=s.conflicts[i].resolved?s.conflicts[i].roid:s.conflicts[i].ooid;if(identity_to_snapshot(r,s.conflicts[i].path,k,id,&q)<0){cvc_snapshot_free(&current);cvc_snapshot_free(&ours);state_free(&s);return -1;}for(size_t j=0;j<q.n;j++)if(cvc_snapshot_find(&current,q.v[j].path)<0)cvc_snapshot_add(&current,q.v[j].path,q.v[j].type,q.v[j].oid);cvc_snapshot_free(&q);}cvc_snapshot_sort(&current);}else{if(build_final_snapshot(r,&s,&current)<0){cvc_snapshot_free(&ours);state_free(&s);return -1;}}
    CvcStrVec roots;cvc_strvec_init(&roots);for(size_t i=0;i<s.nconflicts;i++)cvc_strvec_push(&roots,s.conflicts[i].path);if(cvc_materialize_snapshot_owned(r,&current,&ours,&roots)<0){cvc_strvec_free(&roots);cvc_snapshot_free(&current);cvc_snapshot_free(&ours);state_free(&s);return -1;}cvc_strvec_free(&roots);if(state_remove(r)<0){cvc_snapshot_free(&current);cvc_snapshot_free(&ours);state_free(&s);return -1;}printf("merge aborted\n");cvc_snapshot_free(&current);cvc_snapshot_free(&ours);state_free(&s);return 0;
}
int cvc_merge_status(CvcRepo*r){
    MergeState s;int pr;if(state_load(r,&s,&pr)<0)return -1;if(!pr)return 0;int rel;if(state_ref_relation(r,&s,&rel)<0){state_free(&s);return -1;}char th[65];cvc_hex_encode(s.target,32,th);
    if(s.phase==2&&rel==1){printf("merge completed; cleanup pending for %s (%s)\n",s.target_branch,th);state_free(&s);return 0;}
    if(s.phase==2){char ih[65];cvc_hex_encode(s.intended,32,ih);printf("merge finalization pending for %s (%s), intended commit %s\n",s.target_branch,th,ih);state_free(&s);return 0;}
    size_t done=0;for(size_t i=0;i<s.nconflicts;i++)if(s.conflicts[i].resolved)done++;printf("merge in progress with %s (%s): %zu resolved, %zu unresolved\n",s.target_branch,th,done,s.nconflicts-done);for(size_t i=0;i<s.nconflicts;i++)printf("conflicted %s%s\n",s.conflicts[i].path,s.conflicts[i].resolved?" (resolved)":"");state_free(&s);return 0;
}
