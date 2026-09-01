#include "scan.h"
#include "win32.h"
#include "utf8.h"
#include "glob.h"
#include "sha256.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ELIGIBLE_MAX_SIZE 8388608ULL
#define ELIGIBLE_PROBE 8192

void scan_init(ScanResult *r){ memset(r,0,sizeof *r); }

void scan_free(ScanResult *r){
    size_t i; for(i=0;i<r->len;i++) free(r->items[i].path);
    free(r->items);
    memset(r,0,sizeof *r);
}

static int scan_add(ScanResult *r, const char *path, uint8_t type, const uint8_t id[32], uint64_t size){
    if(r->len==r->cap){
        size_t nc=r->cap?r->cap*2:32;
        ScanEntry *ne=(ScanEntry*)realloc(r->items, nc*sizeof(ScanEntry));
        if(!ne) return -1;
        r->items=ne; r->cap=nc;
    }
    ScanEntry *e=&r->items[r->len];
    e->path=strdup(path);
    if(!e->path) return -1;
    e->type=type;
    memcpy(e->id,id,32);
    e->size=size;
    e->is_ignored=0; e->is_binary=0;
    r->len++;
    return 0;
}

/* Built-in exclusions: root .cvc, nested .cvc dirs handled at traversal.
 * Also "." and ".." excluded by scanner. */

static int path_matches_any(const char *path, StrVec *patterns){
    size_t i;
    for(i=0;i<patterns->len;i++){
        if(glob_match(patterns->items[i], path)) return 1;
    }
    return 0;
}

/* tracking selection for a leaf path. include must match, exclude must not. */
int scan_path_selected(Repo *repo, const char *path){
    /* root .cvc excluded always */
    if(strcmp(path,".cvc")==0 || strncmp(path,".cvc/",5)==0) return 0;
    if(getenv("CVC_DBG_SCAN")){
        fprintf(stderr,"[scan] path=%s include_len=%zu", path, repo->cfg.tracking_include.len);
        for(size_t di=0; di<repo->cfg.tracking_include.len; di++) fprintf(stderr," pat[%zu]=%s", di, repo->cfg.tracking_include.items[di]);
        fprintf(stderr,"\n");
    }
    /* include */
    if(!path_matches_any(path, &repo->cfg.tracking_include)) return 0;
    /* exclude */
    if(path_matches_any(path, &repo->cfg.tracking_exclude)) return 0;
    /* built-in: exclude any nested repo .cvc is handled during traversal */
    return 1;
}

/* Check eligibility of a regular file: size<=8MiB and no NUL in first
 * min(size,8192) bytes. Returns 1 eligible, 0 ineligible. */
static int file_eligible(const uint8_t *data, size_t len){
    if(len > ELIGIBLE_MAX_SIZE) return 0;
    size_t probe = len < ELIGIBLE_PROBE ? len : ELIGIBLE_PROBE;
    size_t i;
    for(i=0;i<probe;i++) if(data[i]==0) return 0;
    return 1;
}

typedef struct {
    Repo *repo;
    ScanResult *result;
    char *curpath;      /* growing current canonical path buffer */
    size_t curlen;
    size_t curcap;
    CvcStatus status;
    int fail;
    int for_write;      /* write blob/symlink objects during scan */
} ScanCtx;

static int push_seg(ScanCtx *c, const char *seg){
    size_t n=strlen(seg);
    if(c->curlen>0){
        /* append '/' */
        if(c->curlen+1+n+1>c->curcap){
            size_t nc=c->curcap?c->curcap*2:64;
            while(nc<c->curlen+1+n+1) nc*=2;
            char *np=(char*)realloc(c->curpath,nc);
            if(!np) return -1;
            c->curpath=np; c->curcap=nc;
        }
        c->curpath[c->curlen++]='/';
    }
    if(c->curlen+n+1>c->curcap){
        size_t nc=c->curcap?c->curcap*2:64;
        while(nc<c->curlen+n+1) nc*=2;
        char *np=(char*)realloc(c->curpath,nc);
        if(!np) return -1;
        c->curpath=np; c->curcap=nc;
    }
    memcpy(c->curpath+c->curlen, seg, n);
    c->curlen+=n;
    c->curpath[c->curlen]=0;
    return 0;
}

static void pop_seg(ScanCtx *c, const char *seg){
    size_t n=strlen(seg);
    if(c->curlen>=n+1 && c->curpath[c->curlen-n-1]=='/'){
        c->curlen -= (n+1);
    } else if(c->curlen>=n){
        c->curlen -= n;
    }
    c->curpath[c->curlen]=0;
}

/* Check if a name is a nested repository marker (.cvc real dir). */
static int is_nested_cvc_dir(const char *name8){
    /* name8 is already validated UTF-8. Check ASCII case-insensitive == ".cvc" */
    if(strlen(name8)!=4) return 0;
    for(int i=0;i<4;i++){
        char a=name8[i], b=".cvc"[i];
        if(a>='A'&&a<='Z') a=(char)(a-'A'+'a');
        if(b>='A'&&b<='Z') b=(char)(b-'A'+'a');
        if(a!=b) return 0;
    }
    return 1;
}

static int cb_scan(const uint16_t *dir16, const WDirEntry *e, void *ctx){
    ScanCtx *c=(ScanCtx*)ctx;
    if(c->fail) return 1;
    if(getenv("CVC_DBG_SCAN")){
        fprintf(stderr,"[cb_scan] name8=%s is_dir=%d is_rep=%d is_sym=%d curlen=%zu\n",
            e->name8?e->name8:"(NULL)", e->is_dir, e->is_reparse, e->is_symlink, c->curlen);
    }
    /* Skip the repository's own .cvc metadata directory (root level). */
    if(!e->name8){
        /* unrepresentable name */
        c->result->ignored_unsupported++;
        return 0;
    }
    /* If this is the root .cvc dir (a real non-reparse dir named .cvc),
       skip it entirely. We detect by comparing against repo cvc basename. */
    /* Convert name to utf8 for comparison */
    const char *name=e->name8;
    if(e->is_dir && !e->is_reparse && is_nested_cvc_dir(name) && c->curlen==0){
        /* root .cvc metadata dir: always excluded, never traversed */
        return 0;
    }
    /* Reject Windows-invalid components (forbidden chars, reserved devices,
       trailing dot/space). These are unversionable. */
    if(utf8_validate((const uint8_t*)name, strlen(name)) != 0){
        if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s utf8 invalid\n", name);
        c->result->ignored_unsupported++;
        return 0;
    }
    size_t nlen=strlen(name);
    int win_ok=1;
    for(size_t i=0;i<nlen;i++){
        char ch=name[i];
        if(ch=='<'||ch=='>'||ch==':'||ch=='"'||ch=='\\'||ch=='|'||ch=='?'||ch=='*'||ch==0x7f||((unsigned char)ch)<0x20){ win_ok=0; break; }
    }
    if(win_ok && nlen>0 && (name[nlen-1]==' '||name[nlen-1]=='.')) win_ok=0;
    /* reserved DOS device basename before first dot */
    if(win_ok){
        char base[16]; size_t bi=0;
        for(size_t i=0;i<nlen;i++){
            if(name[i]=='.') break;
            if(bi<15) base[bi++]=name[i];
        }
        base[bi]=0;
        /* case-insensitive compare to devices */
        if(bi>0){
            struct { const char *n; } devs[] = {
                {"CON"},{"PRN"},{"AUX"},{"NUL"},
                {"COM1"},{"COM2"},{"COM3"},{"COM4"},{"COM5"},{"COM6"},{"COM7"},{"COM8"},{"COM9"},
                {"LPT1"},{"LPT2"},{"LPT3"},{"LPT4"},{"LPT5"},{"LPT6"},{"LPT7"},{"LPT8"},{"LPT9"},
                {"COM\u00b9"},{"COM\u00b2"},{"COM\u00b3"},{"LPT\u00b9"},{"LPT\u00b2"},{"LPT\u00b3"}
            };
            for(size_t d=0;d<sizeof(devs)/sizeof(devs[0]);d++){
                if(utf8_ordinal_case_equal(base, devs[d].n)){ win_ok=0; break; }
            }
        }
    }
    if(!win_ok){
        if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s win_ok=0\n", name);
        c->result->ignored_unsupported++;
        return 0;
    }
    /* Nested repo boundary: real non-reparse dir named .cvc => stop. */
    if(e->is_dir && !e->is_reparse && is_nested_cvc_dir(name)){
        c->result->ignored_nested_repo++;
        return 0;
    }
    /* Unsupported reparse point (junction etc.): ignore, don't traverse. */
    if(e->is_reparse && !e->is_symlink){
        c->result->ignored_unsupported++;
        return 0;
    }
    /* Build full path */
    if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s passed validation, push_seg\n", name);
    if(push_seg(c, name)!=0){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); return 1; }
    if(e->is_symlink){
        /* supported symlink: read PrintName without dereference */
        uint16_t *abs=w_join(dir16, name);
        if(!abs){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); pop_seg(c,name); return 1; }
        char *print8=NULL; int is_dir=0;
        if(w_symlink_read(abs, &print8, &is_dir)==0){
            /* build symlink object id */
            uint8_t id[32];
            sha256_one(print8, strlen(print8), id);
            /* envelope is "symlink <len>\0<payload>"; but id is over envelope,
               not payload. Compute correctly. */
            Bytes env; bytes_init(&env);
            {
                char lenbuf[32];
                size_t plen=strlen(print8);
                snprintf(lenbuf,sizeof lenbuf,"%zu",plen);
                bytes_append_cstr(&env,"symlink ");
                bytes_append_cstr(&env,lenbuf);
                bytes_append_byte(&env,0);
                bytes_append(&env,print8,plen);
                sha256_one(env.data,env.len,id);
                bytes_free(&env);
            }
            uint8_t type = is_dir ? OBJ_TREE_DIR_SYMLINK : OBJ_TREE_FILE_SYMLINK;
            if(scan_path_selected(c->repo, c->curpath)){
                if(c->for_write){
                    /* durable symlink object (content-addressed) */
                    if(obj_write_envelope(c->repo,"symlink",(const uint8_t*)print8,strlen(print8),id)!=CVC_OK){
                        c->fail=1; c->status=cvc_fail(CVC_ERR,"failed to write symlink object");
                    }
                }
                if(!c->fail && scan_add(c->result, c->curpath, type, id, 0)!=0){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); }
            } else {
                c->result->ignored_excluded++;
            }
            free(print8);
        } else {
            /* unversionable symlink (empty/invalid printname) */
            c->result->ignored_unsupported++;
        }
        free(abs);
    } else if(e->is_dir){
        /* ordinary directory: if it contains a real non-reparse .cvc child it
           is itself a nested repository root => opaque boundary, skip entirely. */
        uint16_t *abs=w_join(dir16, name);
        if(!abs){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); pop_seg(c,name); return 1; }
        uint16_t *cvcpath=w_join(abs, ".cvc");
        WStat cst;
        int is_nested = (cvcpath && w_stat(cvcpath,&cst)==0 && cst.is_dir && !cst.is_reparse);
        free(cvcpath);
        if(is_nested){
            c->result->ignored_nested_repo++;
            free(abs);
        } else {
            if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] recursing into dir %s\n", c->curpath);
            int rc = wdir_list(abs, cb_scan, c);
            free(abs);
            if(rc!=0 && !c->fail){ c->fail=1; c->status=cvc_fail(CVC_ERR,"scan error"); }
        }
    } else {
        /* ordinary regular file: check eligibility by reading first 8192+ bytes */
        uint16_t *abs=w_join(dir16, name);
        if(!abs){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); pop_seg(c,name); return 1; }
        /* get size */
        WStat st;
        int is_reg=0;
        if(w_stat(abs,&st)==0 && !st.is_dir && !st.is_reparse) is_reg=1;
        if(!is_reg){
            c->result->ignored_unsupported++;
            free(abs);
        } else {
            if(st.size > ELIGIBLE_MAX_SIZE){
                if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s ignored_binary(size)\n", c->curpath);
                c->result->ignored_binary++;
                free(abs);
            } else {
                /* read up to 8192 bytes to check NUL; but need full content to
                   compute blob id. Read whole file (bounded by eligibility). */
                Bytes content; bytes_init(&content);
                CvcStatus st2=w_read_file(abs,&content);
                free(abs);
                if(st2!=CVC_OK){ bytes_free(&content); c->fail=1; c->status=cvc_fail(CVC_ERR,"read error"); }
                else {
                    if(!file_eligible(content.data, content.len)){
                        if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s ignored_binary(nul)\n", c->curpath);
                        c->result->ignored_binary++;
                    } else if(scan_path_selected(c->repo, c->curpath)){
                        uint8_t id[32];
                        /* blob id = SHA-256 of envelope "blob <len>\0<payload>" */
                        Bytes env; bytes_init(&env);
                        char lb[32]; snprintf(lb,sizeof lb,"%zu",content.len);
                        bytes_append_cstr(&env,"blob "); bytes_append_cstr(&env,lb);
                        bytes_append_byte(&env,0); bytes_append(&env,content.data,content.len);
                        sha256_one(env.data,env.len,id);
                        if(c->for_write){
                            if(getenv("CVC_DBG_SCAN")) fprintf(stderr,"[scan] %s writing blob len=%zu\n", c->curpath, content.len);
                            /* durable blob object */
                            if(obj_write_envelope(c->repo,"blob",content.data,content.len,id)!=CVC_OK){
                                bytes_free(&env); bytes_free(&content);
                                c->fail=1; c->status=cvc_fail(CVC_ERR,"failed to write blob object");
                                pop_seg(c,name);
                                return 1;
                            }
                        }
                        bytes_free(&env);
                        if(!c->fail && scan_add(c->result, c->curpath, OBJ_TREE_BLOB, id, content.len)!=0){ c->fail=1; c->status=cvc_fail(CVC_ERR,"oom"); }
                    } else {
                        c->result->ignored_excluded++;
                    }
                    bytes_free(&content);
                }
            }
        }
    }
    pop_seg(c, name);
    return c->fail ? 1 : 0;
}

CvcStatus scan_snapshot(Repo *repo, ScanResult *r, int for_write){
    scan_init(r);
    ScanCtx c;
    memset(&c,0,sizeof c);
    c.repo=repo; c.result=r;
    c.curpath=NULL; c.curlen=0; c.curcap=0;
    c.status=CVC_OK; c.fail=0;
    c.for_write=for_write;
    int rc = wdir_list(repo->root16, cb_scan, &c);
    free(c.curpath);
    (void)rc;
    return c.status;
}
