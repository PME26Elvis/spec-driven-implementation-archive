#include "repo.h"
#include "objects.h"
#include "snapshot.h"
#include "scan.h"
#include "merge.h"
#include "diff.h"
#include "sha256.h"
#include "utf8.h"
#include "win32.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================== */
/*  Merge state serialization (binary, deterministic, under state/)    */
/* ================================================================== */
/* Format:
 *   magic      "CVCMS1\n"  (7 bytes)
 *   string     orig_branch
 *   raw32      orig_commit
 *   string     target_branch
 *   raw32      target_commit
 *   string     message
 *   snapshot   provisional
 *   u32        n_conflicts
 *   per conflict:
 *     string   path
 *     u32      resolved
 *     u32      has_resolution
 *     snapshot resolution
 *   u32        phase
 *   u32        finalizing_has_id
 *   raw32      finalizing_commit
 * A "string" is u32 length + bytes.
 * A "snapshot" is u32 leaf_count + per leaf: string path, u8 type, raw32 id.
 */

static int w_str(Bytes *b, const char *s){ return bytes_append_u32(b,(uint32_t)strlen(s)) || bytes_append(b,s,strlen(s)); }
static int w_snap(Bytes *b, const Snapshot *s){
    if(bytes_append_u32(b,(uint32_t)s->len)!=0) return -1;
    size_t i; for(i=0;i<s->len;i++){
        if(w_str(b,s->items[i].path)!=0) return -1;
        if(bytes_append(b,&s->items[i].type,1)!=0) return -1;
        if(bytes_append(b,s->items[i].id,32)!=0) return -1;
    }
    return 0;
}
/* reader helpers */
typedef struct { const uint8_t *p; size_t len; size_t pos; int err; } Rdr;
static const uint8_t *rd_bytes(Rdr *r, size_t n){
    if(r->err) return NULL;
    if(r->pos+n > r->len){ r->err=1; return NULL; }
    const uint8_t *out=r->p+r->pos; r->pos+=n; return out;
}
static uint32_t get_u32(const uint8_t *p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static uint32_t rd_u32(Rdr *r){
    const uint8_t *p=rd_bytes(r,4); if(!p) return 0; return get_u32(p);
}
static char *rd_string(Rdr *r){
    uint32_t n=rd_u32(r); if(r->err) return NULL;
    if(n> (1024*1024)){ r->err=1; return NULL; }
    const uint8_t *p=rd_bytes(r,n); if(!p) return NULL;
    char *s=(char*)malloc(n+1); if(!s){ r->err=1; return NULL; }
    memcpy(s,p,n); s[n]=0; return s;
}
static int rd_snap(Rdr *r, Snapshot *out){
    snap_init(out);
    uint32_t n=rd_u32(r); if(r->err) return -1;
    uint32_t i;
    for(i=0;i<n;i++){
        char *path=rd_string(r); if(r->err){ snap_free(out); return -1; }
        const uint8_t *t=rd_bytes(r,1); if(!t){ free(path); snap_free(out); return -1; }
        const uint8_t *id=rd_bytes(r,32); if(!id){ free(path); snap_free(out); return -1; }
        if(snap_add(out,path,*t,id)!=0){ free(path); snap_free(out); return -1; }
        free(path);
    }
    return 0;
}

void merge_state_init(MergeState *ms){ memset(ms,0,sizeof *ms); snap_init(&ms->provisional); }
void merge_state_free(MergeState *ms){
    free(ms->orig_branch); free(ms->target_branch); free(ms->message);
    snap_free(&ms->provisional);
    size_t i; for(i=0;i<ms->n_conflicts;i++){ free(ms->conflicts[i].path); snap_free(&ms->conflicts[i].resolution); }
    free(ms->conflicts);
    merge_state_init(ms);
}

static uint16_t *merge_state_path(const Repo *repo){
    return w_repo_to_abs(repo->cvc16,"state/merge");
}

static CvcStatus merge_state_write_bytes(const Repo *repo, const Bytes *b){
    uint16_t *p=merge_state_path(repo);
    if(!p) return cvc_fail(CVC_ERR,"oom");
    int rc=w_write_file_atomic(p,b->data,b->len);
    free(p);
    return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot write merge state");
}

CvcStatus merge_state_save(const Repo *repo, const MergeState *ms){
    Bytes b; bytes_init(&b);
    if(bytes_append_cstr(&b,"CVCMS1\n")!=0) goto oom;
    if(w_str(&b,ms->orig_branch)!=0) goto oom;
    if(bytes_append(&b,ms->orig_commit,32)!=0) goto oom;
    if(w_str(&b,ms->target_branch)!=0) goto oom;
    if(bytes_append(&b,ms->target_commit,32)!=0) goto oom;
    if(w_str(&b,ms->message?ms->message:"")!=0) goto oom;
    if(w_snap(&b,&ms->provisional)!=0) goto oom;
    if(bytes_append_u32(&b,(uint32_t)ms->n_conflicts)!=0) goto oom;
    size_t i; for(i=0;i<ms->n_conflicts;i++){
        if(w_str(&b,ms->conflicts[i].path)!=0) goto oom;
        if(bytes_append_u32(&b,(uint32_t)ms->conflicts[i].resolved)!=0) goto oom;
        if(bytes_append_u32(&b,(uint32_t)ms->conflicts[i].has_resolution)!=0) goto oom;
        if(w_snap(&b,&ms->conflicts[i].resolution)!=0) goto oom;
    }
    if(bytes_append_u32(&b,(uint32_t)ms->phase)!=0) goto oom;
    if(bytes_append_u32(&b,(uint32_t)ms->finalizing_has_id)!=0) goto oom;
    if(bytes_append(&b,ms->finalizing_commit,32)!=0) goto oom;
    CvcStatus st=merge_state_write_bytes(repo,&b);
    bytes_free(&b);
    return st;
oom:
    bytes_free(&b);
    return cvc_fail(CVC_ERR,"oom");
}

CvcStatus merge_state_load(const Repo *repo, MergeState *ms){
    merge_state_init(ms);
    uint16_t *p=merge_state_path(repo);
    if(!p) return cvc_fail(CVC_ERR,"oom");
    Bytes raw; bytes_init(&raw);
    CvcStatus st=w_read_file(p,&raw);
    free(p);
    if(st!=CVC_OK) return cvc_fail(CVC_ERR,"no merge state");
    Rdr r; r.p=raw.data; r.len=raw.len; r.pos=0; r.err=0;
    if(raw.len<7 || memcmp(raw.data,"CVCMS1\n",7)!=0){ bytes_free(&raw); return cvc_fail(CVC_ERR,"corrupt merge state"); }
    r.pos=7;
    ms->orig_branch=rd_string(&r);
    if(!ms->orig_branch){ bytes_free(&raw); return cvc_fail(CVC_ERR,"corrupt merge state"); }
    const uint8_t *oc=rd_bytes(&r,32); if(!oc) goto corrupt;
    memcpy(ms->orig_commit,oc,32);
    ms->target_branch=rd_string(&r); if(!ms->target_branch) goto corrupt;
    const uint8_t *tc=rd_bytes(&r,32); if(!tc) goto corrupt;
    memcpy(ms->target_commit,tc,32);
    ms->message=rd_string(&r); if(!ms->message) goto corrupt;
    if(rd_snap(&r,&ms->provisional)!=0) goto corrupt;
    uint32_t nc=rd_u32(&r);
    if(nc>100000){ bytes_free(&raw); merge_state_free(ms); return cvc_fail(CVC_ERR,"corrupt merge state"); }
    ms->conflicts=(ConflictEntry*)calloc(nc?nc:1,sizeof(ConflictEntry));
    if(!ms->conflicts){ bytes_free(&raw); merge_state_free(ms); return cvc_fail(CVC_ERR,"oom"); }
    ms->n_conflicts=nc;
    uint32_t ci; for(ci=0;ci<nc;ci++){
        ms->conflicts[ci].path=rd_string(&r); if(!ms->conflicts[ci].path) goto corrupt;
        ms->conflicts[ci].resolved=(int)rd_u32(&r);
        ms->conflicts[ci].has_resolution=(int)rd_u32(&r);
        if(rd_snap(&r,&ms->conflicts[ci].resolution)!=0) goto corrupt;
    }
    ms->phase=(int)rd_u32(&r);
    ms->finalizing_has_id=(int)rd_u32(&r);
    const uint8_t *fc=rd_bytes(&r,32); if(!fc) goto corrupt;
    memcpy(ms->finalizing_commit,fc,32);
    bytes_free(&raw);
    if(ms->phase!=MERGE_PHASE_CONFLICT && ms->phase!=MERGE_PHASE_FINALIZING) goto corrupt2;
    return CVC_OK;
corrupt:
    bytes_free(&raw);
    merge_state_free(ms);
    return cvc_fail(CVC_ERR,"corrupt merge state");
corrupt2:
    bytes_free(&raw);
    merge_state_free(ms);
    return cvc_fail(CVC_ERR,"corrupt merge state");
}

CvcStatus merge_state_remove(const Repo *repo){
    uint16_t *p=merge_state_path(repo);
    if(!p) return cvc_fail(CVC_ERR,"oom");
    WStat st;
    int ex=0;
    if(w_stat(p,&st)==0 && st.exists) ex=1;
    if(ex){ int rc=w_delete_path(p,0); free(p); return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot remove merge state"); }
    free(p);
    return CVC_OK;
}

/* ================================================================== */
/*  Ancestry                                                           */
/* ================================================================== */
typedef struct { uint8_t *ids; size_t len; size_t cap; } IdSet;
static void idset_init(IdSet *s){ s->ids=NULL; s->len=0; s->cap=0; }
static void idset_free(IdSet *s){ free(s->ids); idset_init(s); }
static int idset_contains(const IdSet *s, const uint8_t id[32]){
    size_t i; for(i=0;i<s->len;i++) if(memcmp(s->ids+i*32,id,32)==0) return 1; return 0;
}
static int idset_add(IdSet *s, const uint8_t id[32]){
    if(idset_contains(s,id)) return 0;
    if(s->len==s->cap){ size_t nc=s->cap?s->cap*2:32; uint8_t*n=(uint8_t*)realloc(s->ids,nc*32); if(!n) return -1; s->ids=n; s->cap=nc; }
    memcpy(s->ids+s->len*32,id,32); s->len++; return 0;
}

static int load_commit(Repo *repo, const uint8_t id[32], Commit *c){
    ObjectData od;
    if(obj_read(repo,id,&od)!=CVC_OK) return -1;
    if(od.type!='c'){ object_free(&od); return -1; }
    int rc=commit_decode(od.payload.data,od.payload.len,c);
    object_free(&od);
    return rc==0?0:-1;
}

/* Collect all commits reachable from tip (including tip) into `set`. */
static CvcStatus collect_ancestors(Repo *repo, const uint8_t tip[32], IdSet *set){
    IdSet q; idset_init(&q);
    if(idset_add(&q,tip)!=0){ idset_free(&q); return cvc_fail(CVC_ERR,"oom"); }
    idset_add(set,tip);
    size_t head=0;
    while(head<q.len){
        Commit c;
        if(load_commit(repo,q.ids+head*32,&c)!=0){ idset_free(&q); return cvc_fail(CVC_ERR,"missing commit"); }
        size_t p; for(p=0;p<c.parent_count;p++){
            if(!idset_contains(&q,c.parents[p]) && idset_add(&q,c.parents[p])!=0){ commit_free(&c); idset_free(&q); return cvc_fail(CVC_ERR,"oom"); }
            idset_add(set,c.parents[p]);
        }
        commit_free(&c);
        head++;
    }
    idset_free(&q);
    return CVC_OK;
}

int cvc_is_ancestor(const Repo *repo_c, const uint8_t a[32], const uint8_t b[32]){
    Repo *repo=(Repo*)repo_c;
    IdSet seen; idset_init(&seen);
    IdSet q; idset_init(&q);
    idset_add(&q,b); idset_add(&seen,b);
    int found=0;
    size_t head=0;
    while(head<q.len){
        if(memcmp(q.ids+head*32,a,32)==0){ found=1; break; }
        Commit c;
        if(load_commit(repo,q.ids+head*32,&c)!=0){ break; }
        size_t p; for(p=0;p<c.parent_count;p++){
            if(!idset_contains(&seen,c.parents[p])){ idset_add(&q,c.parents[p]); idset_add(&seen,c.parents[p]); }
        }
        commit_free(&c);
        head++;
    }
    idset_free(&q); idset_free(&seen);
    return found;
}

int cvc_merge_base(const Repo *repo_c, const uint8_t a[32], const uint8_t b[32], uint8_t out[32]){
    Repo *repo=(Repo*)repo_c;
    IdSet A; idset_init(&A);
    IdSet B; idset_init(&B);
    CvcStatus st=collect_ancestors(repo,a,&A);
    if(st!=CVC_OK){ idset_free(&A); idset_free(&B); return -1; }
    st=collect_ancestors(repo,b,&B);
    if(st!=CVC_OK){ idset_free(&A); idset_free(&B); return -1; }
    IdSet common; idset_init(&common);
    size_t i; for(i=0;i<A.len;i++) if(idset_contains(&B,A.ids+i*32)) idset_add(&common,A.ids+i*32);
    if(common.len==0){ idset_free(&A); idset_free(&B); idset_free(&common); return 1; }
    uint8_t best[32]; int found=0;
    for(i=0;i<common.len;i++){
        int maximal=1;
        size_t j; for(j=0;j<common.len;j++){
            if(i==j) continue;
            if(cvc_is_ancestor(repo, common.ids+i*32, common.ids+j*32)){ maximal=0; break; }
        }
        if(maximal){
            if(!found){ memcpy(best,common.ids+i*32,32); found=1; }
            else {
                char ha[65],hb[65];
                sha256_to_hex(best,ha); sha256_to_hex(common.ids+i*32,hb);
                if(strcmp(hb,ha)<0) memcpy(best,common.ids+i*32,32);
            }
        }
    }
    idset_free(&A); idset_free(&B); idset_free(&common);
    if(!found) return 1;
    memcpy(out,best,32);
    return 0;
}

/* ================================================================== */
/*  Three-way tree merge                                               */
/* ================================================================== */
typedef struct {
    ThreeWayResult pub;     /* exported result */
    int fail;
    CvcStatus status;
} ThreeWay;

void threeway_result_init(ThreeWayResult *r){
    snap_init(&r->provisional);
    strvec_init(&r->conflicts);
    r->textual=NULL; r->n_textual=0;
    r->case_collision=0;
    r->collision_path_a[0]=0; r->collision_path_b[0]=0;
}
void threeway_result_free(ThreeWayResult *r){
    snap_free(&r->provisional);
    strvec_free(&r->conflicts);
    size_t i; for(i=0;i<r->n_textual;i++) free(r->textual[i].path);
    free(r->textual); r->textual=NULL; r->n_textual=0;
}
static void threeway_init(ThreeWay *tw){ threeway_result_init(&tw->pub); tw->fail=0; tw->status=CVC_OK; }
static void threeway_free(ThreeWay *tw){ threeway_result_free(&tw->pub); }

static int textconflict_push(ThreeWay *tw, const char *path,
                             const uint8_t ours_id[32], const uint8_t theirs_id[32]){
    TextConflict *n=(TextConflict*)realloc(tw->pub.textual,(tw->pub.n_textual+1)*sizeof(TextConflict));
    if(!n) return -1;
    tw->pub.textual=n;
    TextConflict *e=&tw->pub.textual[tw->pub.n_textual++];
    e->path=strdup(path);
    if(!e->path) return -1;
    memcpy(e->ours_id,ours_id,32); memcpy(e->theirs_id,theirs_id,32);
    return 0;
}

/* find entry in a decoded Tree by name; returns index or -1 */
static long tree_find(const Tree *t, const char *name){
    size_t i; for(i=0;i<t->count;i++) if(strcmp(t->entries[i].name,name)==0) return (long)i;
    return -1;
}
static int entry_is_subtree(const TreeEntry *e){ return e && e->type==OBJ_TREE_SUBTREE; }
static int entry_equal(const TreeEntry *a, const TreeEntry *b){
    if(!a && !b) return 1;
    if(!a||!b) return 0;
    return a->type==b->type && memcmp(a->id,b->id,32)==0;
}

static char *join_path(const char *prefix, const char *name){
    if(prefix[0]){
        char *path=(char*)malloc(strlen(prefix)+1+strlen(name)+1);
        if(!path) return NULL;
        sprintf(path,"%s/%s",prefix,name);
        return path;
    }
    return strdup(name);
}

static void emit_leaf(ThreeWay *tw, const char *prefix, const char *name, const TreeEntry *e){
    if(!e) return;
    char *path=join_path(prefix,name);
    if(!path){ tw->fail=1; tw->status=cvc_fail(CVC_ERR,"oom"); return; }
    if(snap_add(&tw->pub.provisional,path,e->type,e->id)!=0){ free(path); tw->fail=1; tw->status=cvc_fail(CVC_ERR,"oom"); return; }
    free(path);
}

static void record_conflict(ThreeWay *tw, const char *prefix, const char *name){
    char *path=join_path(prefix,name);
    if(!path){ tw->fail=1; tw->status=cvc_fail(CVC_ERR,"oom"); return; }
    if(strvec_push_dup(&tw->pub.conflicts,path)!=0){ free(path); tw->fail=1; tw->status=cvc_fail(CVC_ERR,"oom"); return; }
    free(path);
}

static CvcStatus decode_tree(Repo *repo, const uint8_t id[32], Tree *t){
    tree_init(t);
    ObjectData od;
    CvcStatus st=obj_read(repo,id,&od);
    if(st!=CVC_OK) return st;
    if(od.type!='t'){ object_free(&od); return cvc_fail(CVC_ERR,"expected tree"); }
    int rc=tree_decode(od.payload.data,od.payload.len,t);
    object_free(&od);
    return rc==0?CVC_OK:cvc_fail(CVC_ERR,"malformed tree");
}

/* Forward: recursively merge trees B/O/T at prefix. */
static CvcStatus merge_entries(Repo *repo, const Tree *B, const Tree *O, const Tree *T,
                               const char *prefix, ThreeWay *tw);

/* Forward: text three-way merge of one changed-on-both-sides blob. */
static CvcStatus merge_text_file(Repo *repo, const char *prefix, const char *name,
                                 const TreeEntry *B, const TreeEntry *O, const TreeEntry *T,
                                 ThreeWay *tw);

/* Recursive merge. B,O,T are trees at `prefix` (absent side = empty tree). */
static CvcStatus merge_entries(Repo *repo, const Tree *B, const Tree *O, const Tree *T,
                               const char *prefix, ThreeWay *tw){
    StrVec names; strvec_init(&names);
    size_t i;
    for(i=0;i<B->count;i++) strvec_push_dup(&names,B->entries[i].name);
    for(i=0;i<O->count;i++) strvec_push_dup(&names,O->entries[i].name);
    for(i=0;i<T->count;i++) strvec_push_dup(&names,T->entries[i].name);
    for(i=0;i<names.len;i++) for(size_t j=i+1;j<names.len;j++) if(strcmp(names.items[j],names.items[i])<0){ char*t=names.items[i];names.items[i]=names.items[j];names.items[j]=t; }
    size_t wi=0; for(i=0;i<names.len;i++){
        if(wi>0 && strcmp(names.items[wi-1],names.items[i])==0){ free(names.items[i]); continue; }
        names.items[wi++]=names.items[i];
    }
    names.len=wi;
    StrVec sib; strvec_init(&sib);

    CvcStatus st=CVC_OK;
    for(i=0;i<names.len;i++){
        const char *nm=names.items[i];
        long bi=tree_find(B,nm), oi=tree_find(O,nm), ti=tree_find(T,nm);
        const TreeEntry *B_e = bi>=0? &B->entries[bi] : NULL;
        const TreeEntry *O_e = oi>=0? &O->entries[oi] : NULL;
        const TreeEntry *T_e = ti>=0? &T->entries[ti] : NULL;
        if(tw->fail){ st=tw->status; break; }

        if(entry_equal(O_e,T_e)){ /* both absent or identical */
            if(O_e && entry_is_subtree(O_e)){
                char *childprefix=join_path(prefix,nm);
                if(!childprefix){ st=cvc_fail(CVC_ERR,"oom"); break; }
                Tree Bc,Tc; tree_init(&Bc); tree_init(&Tc);
                Tree Oc; st=decode_tree(repo,O_e->id,&Oc);
                if(st==CVC_OK) st=merge_entries(repo,&Bc,&Oc,&Tc,childprefix,tw);
                tree_free(&Bc); tree_free(&Oc); tree_free(&Tc);
                free(childprefix);
                if(st!=CVC_OK) break;
            } else if(O_e){
                emit_leaf(tw,prefix,nm,O_e);
                if(tw->fail){ st=tw->status; break; }
            }
            strvec_push_dup(&sib,nm);
            continue;
        }
        if(entry_equal(O_e,B_e)){ /* ours unchanged -> take theirs */
            if(T_e && entry_is_subtree(T_e)){
                char *childprefix=join_path(prefix,nm);
                if(!childprefix){ st=cvc_fail(CVC_ERR,"oom"); break; }
                Tree Bc; tree_init(&Bc);
                Tree Oc; if(B_e && entry_is_subtree(B_e)){ st=decode_tree(repo,B_e->id,&Oc); } else tree_init(&Oc);
                if(st==CVC_OK){ Tree Tc; st=decode_tree(repo,T_e->id,&Tc); if(st==CVC_OK){ st=merge_entries(repo,&Bc,&Oc,&Tc,childprefix,tw); tree_free(&Tc);} tree_free(&Oc); tree_free(&Bc);}
                free(childprefix);
                if(st!=CVC_OK) break;
            } else if(T_e){
                emit_leaf(tw,prefix,nm,T_e);
                if(tw->fail){ st=tw->status; break; }
            }
            strvec_push_dup(&sib,nm);
            continue;
        }
        if(entry_equal(T_e,B_e)){ /* theirs unchanged -> take ours */
            if(O_e && entry_is_subtree(O_e)){
                char *childprefix=join_path(prefix,nm);
                if(!childprefix){ st=cvc_fail(CVC_ERR,"oom"); break; }
                Tree Bc; tree_init(&Bc);
                Tree Tc; if(B_e && entry_is_subtree(B_e)){ st=decode_tree(repo,B_e->id,&Tc); } else tree_init(&Tc);
                if(st==CVC_OK){ Tree Oc; st=decode_tree(repo,O_e->id,&Oc); if(st==CVC_OK){ st=merge_entries(repo,&Bc,&Oc,&Tc,childprefix,tw); tree_free(&Oc);} tree_free(&Tc); tree_free(&Bc);}
                free(childprefix);
                if(st!=CVC_OK) break;
            } else if(O_e){
                emit_leaf(tw,prefix,nm,O_e);
                if(tw->fail){ st=tw->status; break; }
            }
            strvec_push_dup(&sib,nm);
            continue;
        }

        /* Neither side unchanged. */
        int O_sub=entry_is_subtree(O_e), T_sub=entry_is_subtree(T_e);
        int O_blob=O_e&&O_e->type==OBJ_TREE_BLOB, T_blob=T_e&&T_e->type==OBJ_TREE_BLOB;

        if(O_sub && T_sub){
            int B_is_sub = B_e && entry_is_subtree(B_e);
            int B_is_non_dir = B_e && !B_is_sub;
            if(B_is_non_dir){
                record_conflict(tw,prefix,nm);
                strvec_push_dup(&sib,nm);
                continue;
            }
            char *childprefix=join_path(prefix,nm);
            if(!childprefix){ st=cvc_fail(CVC_ERR,"oom"); break; }
            Tree Bc; tree_init(&Bc);
            if(B_is_sub){ st=decode_tree(repo,B_e->id,&Bc); if(st!=CVC_OK){ free(childprefix); break; } }
            Tree Oc; st=decode_tree(repo,O_e->id,&Oc);
            if(st==CVC_OK){ Tree Tc; st=decode_tree(repo,T_e->id,&Tc);
                if(st==CVC_OK){ st=merge_entries(repo,&Bc,&Oc,&Tc,childprefix,tw); tree_free(&Tc);} tree_free(&Oc); tree_free(&Bc); free(childprefix); if(st!=CVC_OK) break; }
            else { tree_free(&Bc); free(childprefix); break; }
            strvec_push_dup(&sib,nm);
            continue;
        }

        /* Regular file three-way text merge.
         * Both sides provide regular-file content => textual merge, even if base
         * is absent (add/add, spec 9) or base is a non-blob (type change to a
         * regular file on both sides). merge_text_file handles absent base. */
        if(O_blob && T_blob){
            st=merge_text_file(repo,prefix,nm,B_e,O_e,T_e,tw);
            strvec_push_dup(&sib,nm);
            if(st!=CVC_OK) break;
            continue;
        }

        /* All remaining cases are structural conflicts. */
        record_conflict(tw,prefix,nm);
        strvec_push_dup(&sib,nm);
    }

    /* Case-collision detection at this directory level (spec 7.4). */
    if(!tw->fail && !tw->pub.case_collision){
        size_t a,b;
        for(a=0;a<sib.len;a++){
            for(b=a+1;b<sib.len;b++){
                if(utf8_ordinal_case_equal(sib.items[a],sib.items[b])){
                    tw->pub.case_collision=1;
                    if(prefix[0]){
                        snprintf(tw->pub.collision_path_a,sizeof tw->pub.collision_path_a,"%s/%s",prefix,sib.items[a]);
                        snprintf(tw->pub.collision_path_b,sizeof tw->pub.collision_path_b,"%s/%s",prefix,sib.items[b]);
                    } else {
                        snprintf(tw->pub.collision_path_a,sizeof tw->pub.collision_path_a,"%s",sib.items[a]);
                        snprintf(tw->pub.collision_path_b,sizeof tw->pub.collision_path_b,"%s",sib.items[b]);
                    }
                    break;
                }
            }
            if(tw->pub.case_collision) break;
        }
    }
    names.len=0; strvec_free(&names);
    sib.len=0; strvec_free(&sib);
    if(tw->fail) return tw->status;
    return st;
}

/* Sort & dedup conflict roots; also remove descendant roots. */
static void normalize_conflicts(ThreeWay *tw){
    size_t i,j;
    for(i=0;i<tw->pub.conflicts.len;i++){
        for(j=i+1;j<tw->pub.conflicts.len;j++){
            if(strcmp(tw->pub.conflicts.items[j],tw->pub.conflicts.items[i])==0){
                free(tw->pub.conflicts.items[j]);
                tw->pub.conflicts.items[j]=tw->pub.conflicts.items[tw->pub.conflicts.len-1];
                tw->pub.conflicts.len--; j--;
            }
        }
    }
    for(i=0;i<tw->pub.conflicts.len;i++){
        size_t il=strlen(tw->pub.conflicts.items[i]);
        for(j=0;j<tw->pub.conflicts.len;){
            if(i==j){ j++; continue; }
            char *p=tw->pub.conflicts.items[j];
            if(strncmp(p,tw->pub.conflicts.items[i],il)==0 && p[il]=='/'){
                free(p); tw->pub.conflicts.items[j]=tw->pub.conflicts.items[tw->pub.conflicts.len-1]; tw->pub.conflicts.len--;
            } else j++;
        }
    }
    for(i=0;i<tw->pub.conflicts.len;i++) for(j=i+1;j<tw->pub.conflicts.len;j++) if(strcmp(tw->pub.conflicts.items[j],tw->pub.conflicts.items[i])<0){ char*t=tw->pub.conflicts.items[i];tw->pub.conflicts.items[i]=tw->pub.conflicts.items[j];tw->pub.conflicts.items[j]=t; }
}

/* ---------------- text three-way merge ---------------- */
typedef struct { int side; int kind; size_t start,end; size_t r_off,r_len; int used; } MEdit;

static int medit_cmp(const void *a,const void*b){
    const MEdit*ea=(const MEdit*)a,*eb=(const MEdit*)b;
    if(ea->start!=eb->start) return ea->start<eb->start?-1:1;
    if(ea->kind!=eb->kind) return ea->kind<eb->kind?-1:1;
    if(ea->end!=eb->end) return ea->end<eb->end?-1:1;
    return 0;
}

static int convert_edits(const DiffEdit *ed, size_t ned, size_t old_n,
                         const uint8_t *new_data, const size_t *new_off, const size_t *new_len, size_t new_n,
                         MEdit **out, size_t *nout){
    (void)old_n; (void)new_n;
    MEdit *m=(MEdit*)malloc((ned+1)*sizeof(MEdit));
    if(!m) return -1;
    size_t mn=0;
    size_t i;
    int have_pend=0; size_t pend_start=0, pend_end=0;
    for(i=0;i<ned;i++){
        if(ed[i].op==0) continue;
        if(ed[i].op==1){
            if(have_pend){
                m[mn].side=0; m[mn].kind=0; m[mn].start=pend_start; m[mn].end=pend_end; m[mn].r_off=0; m[mn].r_len=0; mn++;
            }
            pend_start=ed[i].old_line; pend_end=ed[i].old_line+ed[i].count; have_pend=1;
        } else {
            size_t nl=ed[i].new_line;
            if(have_pend && (ed[i].old_line==pend_start || ed[i].old_line==pend_end)){
                m[mn].side=0; m[mn].kind=0; m[mn].start=pend_start; m[mn].end=pend_end;
                m[mn].r_off=nl; m[mn].r_len=ed[i].count; mn++;
                have_pend=0;
            } else {
                if(have_pend){ m[mn].side=0; m[mn].kind=0; m[mn].start=pend_start; m[mn].end=pend_end; m[mn].r_off=0; m[mn].r_len=0; mn++; have_pend=0; }
                m[mn].side=0; m[mn].kind=1; m[mn].start=ed[i].old_line; m[mn].end=ed[i].old_line;
                m[mn].r_off=nl; m[mn].r_len=ed[i].count; mn++;
            }
        }
    }
    if(have_pend){ m[mn].side=0; m[mn].kind=0; m[mn].start=pend_start; m[mn].end=pend_end; m[mn].r_off=0; m[mn].r_len=0; mn++; }
    for(i=0;i<mn;i++) m[i].used=0;
    *out=m; *nout=mn;
    return 0;
}

typedef struct { const uint8_t *data; const size_t *off; const size_t *len; size_t n; } LineTab;

static int lines_match(const LineTab *a, size_t a_off, size_t a_len, const LineTab *b, size_t b_off, size_t b_len){
    if(a_len!=b_len) return 0;
    size_t i;
    for(i=0;i<a_len;i++){
        size_t ai=a->off[a_off+i], al=a->len[a_off+i];
        size_t bi=b->off[b_off+i], bl=b->len[b_off+i];
        if(al!=bl) return 0;
        if(memcmp(a->data+ai,b->data+bi,al)!=0) return 0;
    }
    return 1;
}

static CvcStatus merge_text_file(Repo *repo, const char *prefix, const char *name,
                                 const TreeEntry *B, const TreeEntry *O, const TreeEntry *T,
                                 ThreeWay *tw){
    Bytes bb,ob,tb; bytes_init(&bb); bytes_init(&ob); bytes_init(&tb);
    CvcStatus st=CVC_OK;
    ObjectData od;
    if(B){
        if(obj_read(repo,B->id,&od)!=CVC_OK || od.type!='b'){ object_free(&od); st=cvc_fail(CVC_ERR,"missing blob"); goto done; }
        bytes_append(&bb,od.payload.data,od.payload.len); object_free(&od);
    } /* B absent (add/add): base is empty */
    if(obj_read(repo,O->id,&od)!=CVC_OK || od.type!='b'){ object_free(&od); st=cvc_fail(CVC_ERR,"missing blob"); goto done; }
    bytes_append(&ob,od.payload.data,od.payload.len); object_free(&od);
    if(obj_read(repo,T->id,&od)!=CVC_OK || od.type!='b'){ object_free(&od); st=cvc_fail(CVC_ERR,"missing blob"); goto done; }
    bytes_append(&tb,od.payload.data,od.payload.len); object_free(&od);

    size_t *bo=NULL,*bl=NULL,*oo=NULL,*ol=NULL,*to=NULL,*tl=NULL; size_t bn=0,on=0,tn=0;
    if(diff_split_lines(bb.data,bb.len,&bo,&bl,&bn)!=0){ st=cvc_fail(CVC_ERR,"oom"); goto done; }
    if(diff_split_lines(ob.data,ob.len,&oo,&ol,&on)!=0){ free(bo);free(bl); st=cvc_fail(CVC_ERR,"oom"); goto done; }
    if(diff_split_lines(tb.data,tb.len,&to,&tl,&tn)!=0){ free(bo);free(bl);free(oo);free(ol); st=cvc_fail(CVC_ERR,"oom"); goto done; }

    DiffEdit *oe=NULL,*te=NULL; size_t oen=0,ten=0;
    if(diff_myers(bb.data,bo,bl,bn, ob.data,oo,ol,on,&oe,&oen)!=0){ st=cvc_fail(CVC_ERR,"oom"); goto done2; }
    if(diff_myers(bb.data,bo,bl,bn, tb.data,to,tl,tn,&te,&ten)!=0){ free(oe); st=cvc_fail(CVC_ERR,"oom"); goto done2; }

    MEdit *om=NULL,*tm=NULL; size_t omn=0,tmn=0;
    if(convert_edits(oe,oen,bn,ob.data,oo,ol,on,&om,&omn)!=0){ free(oe);free(te); st=cvc_fail(CVC_ERR,"oom"); goto done2; }
    if(convert_edits(te,ten,bn,tb.data,to,tl,tn,&tm,&tmn)!=0){ free(oe);free(te);free(om); st=cvc_fail(CVC_ERR,"oom"); goto done2; }
    free(oe); free(te);

    LineTab lours={ob.data,oo,ol,on}, ltheirs={tb.data,to,tl,tn};

    size_t i;
    for(i=0;i<omn;i++) om[i].side=0;
    for(i=0;i<tmn;i++) tm[i].side=1;

    int conflict=0;
    size_t a,b;
    for(a=0;a<omn && !conflict;a++){
        for(b=0;b<tmn && !conflict;b++){
            MEdit *e1=&om[a], *e2=&tm[b];
            if(e1->kind==0 && e2->kind==0){
                int identical = (e1->start==e2->start && e1->end==e2->end &&
                                 lines_match(&lours,e1->r_off,e1->r_len,&ltheirs,e2->r_off,e2->r_len));
                if(identical) continue;
                size_t os = e1->start>e2->start?e1->start:e2->start;
                size_t oe_ = e1->end<e2->end?e1->end:e2->end;
                if(os < oe_) conflict=1;
            } else if(e1->kind==1 && e2->kind==0){
                if(e2->start < e1->start && e1->start < e2->end) conflict=1;
            } else if(e1->kind==0 && e2->kind==1){
                if(e1->start < e2->start && e2->start < e1->end) conflict=1;
            } else {
                if(e1->start==e2->start){
                    if(!lines_match(&lours,e1->r_off,e1->r_len,&ltheirs,e2->r_off,e2->r_len)) conflict=1;
                }
            }
        }
    }

    if(conflict){
        record_conflict(tw,prefix,name);
        char *cpath=join_path(prefix,name);
        if(!cpath){ st=cvc_fail(CVC_ERR,"oom"); goto done2; }
        if(textconflict_push(tw,cpath,O->id,T->id)!=0){ free(cpath); st=cvc_fail(CVC_ERR,"oom"); goto done2; }
        free(cpath);
        st=CVC_OK;
        goto done2;
    }

    /* Clean merge: compose. */
    MEdit *all=(MEdit*)malloc((omn+tmn+1)*sizeof(MEdit));
    if(!all){ st=cvc_fail(CVC_ERR,"oom"); goto done2; }
    size_t an=0;
    for(i=0;i<omn;i++) all[an++]=om[i];
    for(i=0;i<tmn;i++) all[an++]=tm[i];
    qsort(all,an,sizeof(MEdit),medit_cmp);

    Bytes merged; bytes_init(&merged);
    size_t pos=0;
    size_t g;
    int bad_ineligible=0;
    while(pos<=bn){
        for(g=0;g<an;g++){
            MEdit *e=&all[g];
            if(e->used) continue;
            if(e->kind==1 && e->start==pos){
                int dup=0; size_t h;
                for(h=0;h<g;h++){
                    MEdit *e2=&all[h];
                    const LineTab *t1 = e2->side==0? &lours : &ltheirs;
                    const LineTab *t2 = e->side==0? &lours : &ltheirs;
                    if(e2->kind==1 && e2->start==pos && !e2->used &&
                       lines_match(t1,e2->r_off,e2->r_len, t2,e->r_off,e->r_len)){
                        e2->used=1; dup=1; break;
                    }
                }
                if(!dup){
                    const LineTab *lt = e->side==0? &lours : &ltheirs;
                    size_t q; for(q=0;q<e->r_len;q++) bytes_append(&merged, lt->data+lt->off[e->r_off+q], lt->len[e->r_off+q]);
                }
                e->used=1;
            }
        }
        if(pos==bn) break;
        MEdit *rep=NULL;
        for(g=0;g<an;g++){
            MEdit *e=&all[g];
            if(e->used) continue;
            if(e->kind==0 && e->start==pos){ rep=e; break; }
        }
        if(rep){
            const LineTab *lt = rep->side==0? &lours : &ltheirs;
            size_t q; for(q=0;q<rep->r_len;q++) bytes_append(&merged, lt->data+lt->off[rep->r_off+q], lt->len[rep->r_off+q]);
            rep->used=1;
            for(g=0;g<an;g++){
                MEdit *e=&all[g];
                const LineTab *t1 = e->side==0? &lours : &ltheirs;
                const LineTab *t2 = rep->side==0? &lours : &ltheirs;
                if(e!=rep && !e->used && e->kind==0 && e->start==rep->start && e->end==rep->end &&
                   lines_match(t1,e->r_off,e->r_len, t2,rep->r_off,rep->r_len)){
                    e->used=1;
                }
            }
            pos=rep->end;
        } else {
            bytes_append(&merged, bb.data+bo[pos], bl[pos]);
            pos++;
        }
    }
    free(all);
    free(om); free(tm);

    if(merged.len > 8388608ULL){ bad_ineligible=1; }
    else {
        size_t probe = merged.len<8192?merged.len:8192;
        for(i=0;i<probe;i++) if(merged.data[i]==0){ bad_ineligible=1; break; }
    }
    if(bad_ineligible){
        /* Spec 7.5: an otherwise-clean automatic merge whose result becomes
         * ineligible (size > 8 MiB or NUL in first 8192 bytes) is a conflict,
         * and the working tree MUST retain/materialize the OURS representation
         * at that conflict root -- NOT conflict markers. It is therefore a
         * structural conflict (no textual marker pair). resolve <path> then
         * deterministically chooses ours. */
        record_conflict(tw,prefix,name);
        st=CVC_OK;
    } else {
        uint8_t id[32];
        CvcStatus ws=obj_write_envelope(repo,"blob",merged.data,merged.len,id);
        if(ws!=CVC_OK){ st=ws; bytes_free(&merged); goto done2; }
        char *path=join_path(prefix,name);
        if(!path){ bytes_free(&merged); st=cvc_fail(CVC_ERR,"oom"); goto done2; }
        if(snap_add(&tw->pub.provisional,path,OBJ_TREE_BLOB,id)!=0){ free(path); bytes_free(&merged); st=cvc_fail(CVC_ERR,"oom"); goto done2; }
        free(path);
        st=CVC_OK;
    }
    bytes_free(&merged);
done2:
    free(bo); free(bl); free(oo); free(ol); free(to); free(tl);
done:
    bytes_free(&bb); bytes_free(&ob); bytes_free(&tb);
    return st;
}

/* ================================================================== */
/*  Public three-way merge                                             */
/* ================================================================== */
CvcStatus cvc_merge_threeway(const Repo *repo,
                             const uint8_t base[32],
                             const uint8_t ours[32],
                             const uint8_t theirs[32],
                             ThreeWayResult *out){
    threeway_result_init(out);
    Repo *rp=(Repo*)repo;
    Commit bc_,oc_,tc_;
    if(load_commit(rp,base,&bc_)!=0||load_commit(rp,ours,&oc_)!=0||load_commit(rp,theirs,&tc_)!=0){
        commit_free(&bc_); commit_free(&oc_); commit_free(&tc_);
        return cvc_fail(CVC_ERR,"missing commit");
    }
    Tree B,O,T; tree_init(&B); tree_init(&O); tree_init(&T);
    CvcStatus st=decode_tree(rp,bc_.root_tree,&B);
    if(st==CVC_OK) st=decode_tree(rp,oc_.root_tree,&O);
    if(st==CVC_OK) st=decode_tree(rp,tc_.root_tree,&T);
    commit_free(&bc_); commit_free(&oc_); commit_free(&tc_);
    if(st!=CVC_OK){ tree_free(&B);tree_free(&O);tree_free(&T); threeway_result_free(out); return st; }
    ThreeWay tw; threeway_init(&tw);
    st=merge_entries(rp,&B,&O,&T,"",&tw);
    tree_free(&B);tree_free(&O);tree_free(&T);
    if(st!=CVC_OK){ threeway_free(&tw); return st; }
    normalize_conflicts(&tw);
    snap_sort(&tw.pub.provisional);
    /* move results out */
    *out=tw.pub;
    tw.pub.provisional.items=NULL; tw.pub.provisional.len=tw.pub.provisional.cap=0;
    tw.pub.conflicts.items=NULL; tw.pub.conflicts.len=tw.pub.conflicts.cap=0;
    tw.pub.textual=NULL; tw.pub.n_textual=0;
    return CVC_OK;
}
