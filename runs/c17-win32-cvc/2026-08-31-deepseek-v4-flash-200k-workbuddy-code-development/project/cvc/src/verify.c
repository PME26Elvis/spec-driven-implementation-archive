#include "repo.h"
#include "objects.h"
#include "sha256.h"
#include "utf8.h"
#include "glob.h"
#include "merge.h"
#include "win32.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================================================================== */
/*  cvc verify - read-only repository integrity verification          */
/*  (spec 07 §7). Acquires a shared read lock and never mutates state. */
/* ================================================================== */

static int g_errors = 0;

static const char *id_hex_buf(const uint8_t id[32]);

static void vreport(const char *fmt, ...){
    char buf[1024];
    va_list ap; va_start(ap,fmt);
    vsnprintf(buf,sizeof buf,fmt,ap);
    va_end(ap);
    w_out_stderr(buf, strlen(buf));
    w_out_stderr("\n",1);
    g_errors++;
}

/* ---- object id set ---- */
typedef struct { uint8_t *ids; size_t len, cap; } OidSet;
static void oidset_init(OidSet *s){ s->ids=NULL; s->len=0; s->cap=0; }
static void oidset_free(OidSet *s){ free(s->ids); oidset_init(s); }
static int oidset_has(const OidSet *s, const uint8_t id[32]){
    for(size_t i=0;i<s->len;i++) if(memcmp(s->ids+i*32,id,32)==0) return 1;
    return 0;
}
static int oidset_add(OidSet *s, const uint8_t id[32]){
    if(oidset_has(s,id)) return 0;
    if(s->len==s->cap){ size_t nc=s->cap?s->cap*2:256; uint8_t*n=(uint8_t*)realloc(s->ids,nc*32); if(!n) return -1; s->ids=n; s->cap=nc; }
    memcpy(s->ids+s->len*32,id,32); s->len++; return 0;
}

/* ---- enumerate all loose objects and verify hash+structure ---- */
static void verify_loose_object(const Repo *repo, const uint8_t id[32], OidSet *all){
    uint16_t *p=obj_path(repo,id);
    if(!p){ vreport("verify: oom"); return; }
    Bytes raw; bytes_init(&raw);
    if(w_read_file(p,&raw)!=CVC_OK){ free(p); vreport("verify: cannot read object file"); bytes_free(&raw); return; }
    free(p);
    if(raw.len==0){ bytes_free(&raw); return; }
    /* rehash */
    uint8_t check[32]; sha256_one(raw.data,raw.len,check);
    if(memcmp(check,id,32)!=0){
        char hx[65]; sha256_to_hex(id,hx);
        vreport("verify: object %s hash mismatch", hx);
        bytes_free(&raw); return;
    }
    /* structure */
    ObjectData od; memset(&od,0,sizeof od);
    /* parse envelope via obj_read-like; reuse obj_read for parsing */
    /* We already have raw; parse manually. */
    size_t i=0; size_t ts=i;
    while(i<raw.len && raw.data[i]!=' ' && raw.data[i]!=0) i++;
    if(i>=raw.len || raw.data[i]!=' '){ bytes_free(&raw); return; }
    size_t tlen=i-ts; char type[8]; if(tlen>=sizeof type){ bytes_free(&raw); return; }
    memcpy(type,raw.data+ts,tlen); type[tlen]=0;
    i++; size_t ls=i;
    while(i<raw.len && raw.data[i]!=0) i++;
    if(i>=raw.len){ bytes_free(&raw); return; }
    char lenbuf[32]; size_t llen=i-ls; if(llen==0||llen>=sizeof lenbuf){ bytes_free(&raw); return; }
    memcpy(lenbuf,raw.data+ls,llen); lenbuf[llen]=0;
    i++;
    for(size_t k=0;k<llen;k++) if(lenbuf[k]<'0'||lenbuf[k]>'9'){ bytes_free(&raw); return; }
    if(llen>1&&lenbuf[0]=='0'){ bytes_free(&raw); return; }
    unsigned long long plen=strtoull(lenbuf,NULL,10);
    if(i+plen!=raw.len){ bytes_free(&raw); return; }
    char otype=0;
    if(strcmp(type,"blob")==0) otype='b';
    else if(strcmp(type,"symlink")==0) otype='s';
    else if(strcmp(type,"tree")==0) otype='t';
    else if(strcmp(type,"commit")==0) otype='c';
    else { char hx[65]; sha256_to_hex(id,hx); vreport("verify: object %s unknown type '%s'", hx,type); bytes_free(&raw); return; }
    const uint8_t *payload=raw.data+i;
    size_t plen_s=(size_t)plen;

    if(otype=='b'){
        if(plen_s>8388608ULL){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: blob %s exceeds 8 MiB", hx); }
        else {
            size_t probe=plen_s<8192?plen_s:8192;
            for(size_t k=0;k<probe;k++) if(payload[k]==0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: blob %s has NUL in prefix", hx); break; }
        }
    } else if(otype=='s'){
        if(plen_s==0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: symlink %s empty", hx); }
        else {
            for(size_t k=0;k<plen_s;k++) if(payload[k]==0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: symlink %s contains NUL", hx); break; }
            if(utf8_validate(payload,plen_s)!=0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: symlink %s invalid UTF-8", hx); }
        }
    } else if(otype=='t'){
        Tree t; tree_init(&t);
        if(tree_decode(payload,plen_s,&t)!=0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: tree %s malformed", hx); }
        else {
            /* component UTF-16 length must fit the repository volume's max
             * component length (acceptance N17) */
            uint32_t lim=w_volume_component_limit(repo->cvc16);
            for(size_t k=0;k<t.count;k++){
                size_t u16len=0;
                uint16_t *u= utf8_to_utf16(t.entries[k].name, strlen(t.entries[k].name), &u16len);
                free(u);
                if(u16len>(size_t)lim){
                    vreport("verify: tree %s has a component exceeding volume component limit", id_hex_buf(id));
                    break;
                }
            }
            tree_free(&t);
        }
    } else if(otype=='c'){
        Commit c;
        if(commit_decode(payload,plen_s,&c)!=0){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: commit %s malformed", hx); }
        else {
            if(c.parent_count==2 && memcmp(c.parents[0],c.parents[1],32)==0){
                char hx[65]; sha256_to_hex(id,hx); vreport("verify: commit %s has duplicate parents", hx);
            }
            commit_free(&c);
        }
    }
    bytes_free(&raw);
    oidset_add(all,id);
}

/* ---- simplified enumerator: recursively find objects via findfirst ----
 * We implement a two-level walk: objects/<2>/<62>. */
static int enumerate_objects(const Repo *repo, OidSet *all){
    uint16_t *objdir=w_repo_to_abs(repo->cvc16,"objects");
    if(!objdir) return -1;
    size_t dl=0; while(objdir[dl]) dl++;
    int hs=dl>0&&(objdir[dl-1]==L'\\');
    uint16_t *pat=(uint16_t*)malloc((dl+3)*sizeof(uint16_t));
    if(!pat){ free(objdir); return -1; }
    for(size_t i=0;i<dl;i++) pat[i]=objdir[i];
    size_t pp=dl; if(!hs) pat[pp++]=L'\\';
    pat[pp++]=L'*'; pat[pp]=0;
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW(pat,&fd);
    free(pat);
    if(h==INVALID_HANDLE_VALUE){ free(objdir); return 0; }
    do {
        if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
        if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) continue;
        if(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT){ vreport("verify: objects fan-out is a reparse point"); continue; }
        size_t nn=0; while(fd.cFileName[nn]) nn++;
        if(nn!=2){ vreport("verify: invalid objects fan-out directory name"); continue; }
        if(!isxdigit((unsigned char)fd.cFileName[0])||!isxdigit((unsigned char)fd.cFileName[1])){
            vreport("verify: objects fan-out name not hex"); continue;
        }
        /* lowercase check: fan-out must be lowercase (spec: exact spelling) */
        if(!((fd.cFileName[0]>='0'&&fd.cFileName[0]<='9')||(fd.cFileName[0]>='a'&&fd.cFileName[0]<='f')) ||
           !((fd.cFileName[1]>='0'&&fd.cFileName[1]<='9')||(fd.cFileName[1]>='a'&&fd.cFileName[1]<='f'))){
            vreport("verify: objects fan-out name not lowercase hex");
        }
        char fanout[3]; fanout[0]=(char)fd.cFileName[0]; fanout[1]=(char)fd.cFileName[1]; fanout[2]=0;
        char rel[96]; snprintf(rel,sizeof rel,"objects/%s",fanout);
        uint16_t *fodir=w_repo_to_abs(repo->cvc16,rel);
        if(!fodir) continue;
        size_t fdl=0; while(fodir[fdl]) fdl++;
        int fhs=fdl>0&&(fodir[fdl-1]==L'\\');
        uint16_t *fpat=(uint16_t*)malloc((fdl+3)*sizeof(uint16_t));
        if(!fpat){ free(fodir); continue; }
        for(size_t i=0;i<fdl;i++) fpat[i]=fodir[i];
        size_t fp=fdl; if(!fhs) fpat[fp++]=L'\\';
        fpat[fp++]=L'*'; fpat[fp]=0;
        HANDLE h2=FindFirstFileW(fpat,&fd);
        free(fpat);
        if(h2!=INVALID_HANDLE_VALUE){
            do {
                if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
                if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
                if(fd.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT){ vreport("verify: loose object is a reparse point"); continue; }
                size_t nn2=0; while(fd.cFileName[nn2]) nn2++;
                if(nn2!=62){ vreport("verify: loose object filename length != 62"); continue; }
                if(!isxdigit((unsigned char)fd.cFileName[0])||!isxdigit((unsigned char)fd.cFileName[1])){ vreport("verify: loose object filename not hex"); continue; }
                char hex[65];
                hex[0]=fanout[0]; hex[1]=fanout[1];
                for(size_t k=0;k<62;k++) hex[2+k]=(char)fd.cFileName[k];
                hex[64]=0;
                uint8_t id[32];
                if(sha256_from_hex(hex,id)!=0){ vreport("verify: invalid object id %s", hex); continue; }
                verify_loose_object(repo,id,all);
            } while(FindNextFileW(h2,&fd));
            FindClose(h2);
        }
        free(fodir);
    } while(FindNextFileW(h,&fd));
    FindClose(h);
    free(objdir);
    return 0;
}

/* ---- graph walk from branch tips ---- */
typedef struct { const Repo *repo; OidSet *all; OidSet visited; } GraphCtx;

static int graph_verify_tree(const Repo *repo, const uint8_t id[32], GraphCtx *g);
static int graph_verify_commit(const Repo *repo, const uint8_t id[32], GraphCtx *g);

static int graph_verify_commit(const Repo *repo, const uint8_t id[32], GraphCtx *g){
    if(oidset_has(&g->visited,id)) return 0;
    if(!oidset_has(g->all,id)){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: commit %s missing", hx); return -1; }
    oidset_add(&g->visited,id);
    ObjectData od;
    if(obj_read(repo,id,&od)!=CVC_OK || od.type!='c'){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: commit %s not a commit", hx); return -1; }
    Commit c;
    if(commit_decode(od.payload.data,od.payload.len,&c)!=0){ object_free(&od); return -1; }
    object_free(&od);
    /* root tree */
    graph_verify_tree(repo,c.root_tree,g);
    /* parents */
    for(size_t p=0;p<c.parent_count;p++) graph_verify_commit(repo,c.parents[p],g);
    commit_free(&c);
    return 0;
}

static int graph_verify_tree(const Repo *repo, const uint8_t id[32], GraphCtx *g){
    if(oidset_has(&g->visited,id)) return 0;
    if(!oidset_has(g->all,id)){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: tree %s missing", hx); return -1; }
    oidset_add(&g->visited,id);
    ObjectData od;
    if(obj_read(repo,id,&od)!=CVC_OK || od.type!='t'){ char hx[65]; sha256_to_hex(id,hx); vreport("verify: tree %s not a tree", hx); return -1; }
    Tree t; tree_init(&t);
    if(tree_decode(od.payload.data,od.payload.len,&t)!=0){ object_free(&od); char hx[65]; sha256_to_hex(id,hx); vreport("verify: tree %s malformed", hx); tree_free(&t); return -1; }
    object_free(&od);
    for(size_t i=0;i<t.count;i++){
        TreeEntry *e=&t.entries[i];
        if(e->type==OBJ_TREE_SUBTREE){
            graph_verify_tree(repo,e->id,g);
        } else if(e->type==OBJ_TREE_BLOB){
            if(!oidset_has(g->all,e->id)){ vreport("verify: tree references missing blob"); }
        } else if(e->type==OBJ_TREE_FILE_SYMLINK || e->type==OBJ_TREE_DIR_SYMLINK){
            /* must reference a symlink object */
            if(!oidset_has(g->all,e->id)){ vreport("verify: tree references missing symlink"); }
            else {
                ObjectData so;
                if(obj_read(repo,e->id,&so)==CVC_OK && so.type=='s'){ object_free(&so); }
                else { char hx[65]; sha256_to_hex(e->id,hx); vreport("verify: symlink entry does not reference a symlink object (%s)", hx); }
            }
        }
    }
    tree_free(&t);
    return 0;
}

/* ---- metadata helper: ensure a required plain dir/file ---- */
static int check_plain_dir(const Repo *repo, const char *rel){
    uint16_t *p=w_repo_to_abs(repo->cvc16,rel);
    if(!p){ vreport("verify: oom"); return -1; }
    WStat st; int rc=w_stat(p,&st); free(p);
    if(rc!=0||!st.exists){ vreport("verify: missing required directory %s", rel); return -1; }
    if(!st.is_dir||st.is_reparse){ vreport("verify: %s is not an ordinary non-reparse directory", rel); return -1; }
    return 0;
}
static int check_plain_file(const Repo *repo, const char *rel){
    uint16_t *p=w_repo_to_abs(repo->cvc16,rel);
    if(!p){ vreport("verify: oom"); return -1; }
    WStat st; int rc=w_stat(p,&st); free(p);
    if(rc!=0||!st.exists){ vreport("verify: missing required file %s", rel); return -1; }
    if(st.is_dir||st.is_reparse){ vreport("verify: %s is not an ordinary non-reparse file", rel); return -1; }
    return 0;
}

/* ---- full reference-integrity pass over ALL objects ----
 * Spec 07: every committed tree must reference existing objects of the correct
 * type, and every commit must reference an existing tree/parents. This covers
 * unreachable objects too (acceptance N14/N15): an unreachable commit/tree with
 * a missing or wrong-type referenced object still fails verify.
 */
static const char *id_hex_buf(const uint8_t id[32]);
static void verify_references_all(const Repo *repo, const OidSet *all){
    for(size_t i=0;i<all->len;i++){
        const uint8_t *id=all->ids+i*32;
        ObjectData od;
        if(obj_read(repo,id,&od)!=CVC_OK) continue;
        if(od.type=='t'){
            Tree t; tree_init(&t);
            if(tree_decode(od.payload.data,od.payload.len,&t)==0){
                for(size_t k=0;k<t.count;k++){
                    TreeEntry *e=&t.entries[k];
                    int expect=0;
                    if(e->type==OBJ_TREE_BLOB) expect='b';
                    else if(e->type==OBJ_TREE_SUBTREE) expect='t';
                    else if(e->type==OBJ_TREE_FILE_SYMLINK||e->type==OBJ_TREE_DIR_SYMLINK) expect='s';
                    if(expect){
                        if(!oidset_has(all,e->id)){
                            char hx[65]; sha256_to_hex(e->id,hx);
                            vreport("verify: tree %s references missing object %s", id_hex_buf(id), hx);
                        } else {
                            ObjectData so; int so_st=obj_read(repo,e->id,&so);
                            if(so_st!=CVC_OK || so.type!=expect){
                                char hx[65]; sha256_to_hex(e->id,hx);
                                vreport("verify: tree %s references wrong-type object %s (want %c)", id_hex_buf(id), hx, expect);
                            }
                            if(so_st==CVC_OK) object_free(&so);
                        }
                    }
                }
            }
            tree_free(&t);
        } else if(od.type=='c'){
            Commit c;
            if(commit_decode(od.payload.data,od.payload.len,&c)==0){
                if(!oidset_has(all,c.root_tree)){
                    vreport("verify: commit %s references missing root tree", id_hex_buf(id));
                } else {
                    ObjectData so; int so_st=obj_read(repo,c.root_tree,&so);
                    if(so_st!=CVC_OK || so.type!='t'){
                        vreport("verify: commit %s root tree is wrong type", id_hex_buf(id));
                    }
                    if(so_st==CVC_OK) object_free(&so);
                }
                for(size_t p=0;p<c.parent_count;p++){
                    if(!oidset_has(all,c.parents[p])){
                        char hx[65]; sha256_to_hex(c.parents[p],hx);
                        vreport("verify: commit %s references missing parent %s", id_hex_buf(id), hx);
                    }
                }
            }
            commit_free(&c);
        }
        object_free(&od);
    }
}
/* small helper to print an object id hex */
static const char *id_hex_buf(const uint8_t id[32]){
    static char buf[3][65]; static int idx=0; idx=(idx+1)%3;
    sha256_to_hex(id,buf[idx]); return buf[idx];
}

/* ---- main ---- */
static CvcStatus verify_repo(Repo *repo){
    /* 7.1 metadata */
    if(check_plain_dir(repo,"refs")!=0) return CVC_ERR;
    if(check_plain_dir(repo,"refs/heads")!=0) return CVC_ERR;
    if(check_plain_dir(repo,"objects")!=0) return CVC_ERR;
    if(check_plain_dir(repo,"state")!=0) return CVC_ERR;
    if(check_plain_file(repo,"HEAD")!=0) return CVC_ERR;
    if(check_plain_file(repo,"lock")!=0) return CVC_ERR;
    if(check_plain_file(repo,"config.json")!=0) return CVC_ERR;
    /* lock zero length */
    {
        uint16_t *p=w_repo_to_abs(repo->cvc16,"lock");
        if(p){ WStat st; if(w_stat(p,&st)==0 && st.exists && st.size!=0) vreport("verify: lock file is not zero length"); free(p); }
    }
    if(repo->cfg.format_version!=1) vreport("verify: unsupported format_version %d", repo->cfg.format_version);
    if(!repo->config_ok) vreport("verify: configuration is invalid");
    /* HEAD symbolic-ref */
    {
        uint16_t *p=w_repo_to_abs(repo->cvc16,"HEAD");
        if(p){ Bytes raw; bytes_init(&raw);
            if(w_read_file(p,&raw)==CVC_OK){
                size_t n=raw.len;
                if(n<6||memcmp(raw.data,"ref: ",5)!=0){ vreport("verify: HEAD does not use symbolic-ref format"); }
                else {
                    const uint8_t *body=raw.data+5; size_t bn=n-5;
                    if(bn==0||body[bn-1]!='\n'){ vreport("verify: HEAD missing newline"); }
                    else {
                        char b[300]; size_t bl=bn-1; if(bl>=sizeof b) bl=sizeof b-1;
                        memcpy(b,body,bl); b[bl]=0;
                        if(strncmp(b,"refs/heads/",11)!=0) vreport("verify: HEAD ref target invalid");
                        else {
                            const char *br=b+11;
                            if(br[0]=='\0') vreport("verify: HEAD names an empty branch");
                            else if(!repo_branch_exists(repo,br)) vreport("verify: current branch %s does not exist", br);
                        }
                    }
                }
            }
            bytes_free(&raw); free(p);
        }
    }
    /* object enumeration + hashing + structure */
    OidSet all; oidset_init(&all);
    enumerate_objects(repo,&all);
    /* full reference-integrity pass over all objects (covers unreachable) */
    verify_references_all(repo,&all);
    /* graph walk from every born branch tip */
    GraphCtx g; g.repo=repo; g.all=&all; oidset_init(&g.visited);
    /* enumerate branches via refs/heads */
    {
        uint16_t *dir=w_repo_to_abs(repo->cvc16,"refs/heads");
        if(dir){
            size_t dl=0; while(dir[dl]) dl++;
            int hs=dl>0&&(dir[dl-1]==L'\\');
            uint16_t *pat=(uint16_t*)malloc((dl+3)*sizeof(uint16_t));
            if(pat){
                for(size_t i=0;i<dl;i++) pat[i]=dir[i];
                size_t pp=dl; if(!hs) pat[pp++]=L'\\';
                pat[pp++]=L'*'; pat[pp]=0;
                WIN32_FIND_DATAW fd;
                HANDLE h=FindFirstFileW(pat,&fd);
                if(h!=INVALID_HANDLE_VALUE){
                    do {
                        if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
                        if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
                        size_t nn=0; while(fd.cFileName[nn]) nn++;
                        char *n8=utf8_from_utf16(fd.cFileName,nn,NULL);
                        if(n8){
                            uint8_t id[32]; int born=0;
                            if(repo_read_branch(repo,n8,id,&born)!=0){ vreport("verify: cannot read branch %s", n8); }
                            else if(born){
                                if(!oidset_has(&all,id)){ vreport("verify: branch %s names a missing commit", n8); }
                                else graph_verify_commit(repo,id,&g);
                            }
                            free(n8);
                        }
                    } while(FindNextFileW(h,&fd));
                    FindClose(h);
                }
                free(pat);
            }
            free(dir);
        }
    }
    oidset_free(&g.visited);
    oidset_free(&all);
    return g_errors?CVC_ERR:CVC_OK;
}

CvcStatus cmd_verify_dispatch(char **av, int n){
    (void)av; (void)n;
    Repo repo; RepoLock lk;
    memset(&repo,0,sizeof repo);
    CvcStatus st=repo_discover(&repo);
    if(st!=CVC_OK){
        if(st==CVC_ERR_NOTREPO) return cvc_fail(CVC_ERR_NOTREPO,"not inside a CVC repository");
        return st;
    }
    st=repo_load_config(&repo);
    if(st!=CVC_OK){ /* config_ok flag set; continue to report */ }
    st=repo_lock_read(&repo,&lk);
    if(st!=CVC_OK){ repo_free(&repo); return st; }
    g_errors=0;
    CvcStatus vr=verify_repo(&repo);
    repo_unlock(&lk); repo_free(&repo);
    if(vr==CVC_OK && g_errors==0){
        w_out_stdout("repository verified OK\n", strlen("repository verified OK\n"));
        return CVC_OK;
    }
    return cvc_fail(CVC_ERR,"repository verification failed");
}
