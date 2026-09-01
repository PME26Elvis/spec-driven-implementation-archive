#include "repo.h"
#include "objects.h"
#include "json.h"
#include "sha256.h"
#include "utf8.h"
#include "glob.h"
#include "win32.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>

/* ---------------- path utilities ---------------- */

/* Whether a native directory entry at parent16 with name16 is a real
 * non-reparse directory named ".cvc" (case-insensitively). */
static int is_cvc_dir_entry(const uint16_t *name16, int is_dir, int is_reparse){
    if(!is_dir || is_reparse) return 0;
    if(wcscmp(name16, L".cvc")==0) return 1;
    /* ASCII case-insensitive alias check */
    size_t n=0; while(name16[n]) n++;
    if(n!=4) return 0;
    static const wchar_t *cands[] = { L".cvc", L".CVC", L".Cvc", L".cVc", L".cvC", L".CvC", L".cVC", L".CVc" };
    for(size_t i=0;i<8;i++){
        if(wcscmp(name16,cands[i])==0) return 1;
    }
    /* general ordinal fold */
    int ok=1;
    const char *lo=".cvc";
    for(size_t i=0;i<4;i++){
        wchar_t c=name16[i];
        wchar_t low=(c>='A'&&c<='Z')?(wchar_t)(c+32):c;
        if(low != (wchar_t)(unsigned char)lo[i]){ ok=0; break; }
    }
    return ok;
}

CvcStatus repo_discover(Repo *repo){
    memset(repo,0,sizeof *repo);
    uint16_t *cwd = w_getcwd16();
    if(!cwd) return cvc_fail(CVC_ERR,"cannot get cwd");
    uint16_t *cur = w_extended(cwd);
    free(cwd);
    if(!cur) return cvc_fail(CVC_ERR,"oom");
    while(1){
        /* list directory; look for a real non-reparse dir named .cvc */
        int found=0;
        uint16_t *found_cvc=NULL;
        uint16_t *found_root=NULL;
        /* enumerate cur for .cvc */
        WIN32_FIND_DATAW fd;
        size_t cl=0; while(cur[cl]) cl++;
        int has_sep = cl>0 && (cur[cl-1]==L'\\'||cur[cl-1]==L'/');
        uint16_t *pat=(uint16_t*)malloc((cl+3)*sizeof(uint16_t));
        if(!pat){ free(cur); return cvc_fail(CVC_ERR,"oom"); }
        for(size_t i=0;i<cl;i++) pat[i]=cur[i];
        size_t ppos=cl;
        if(!has_sep) pat[ppos++]=L'\\';
        pat[ppos++]=L'*'; pat[ppos]=0;
        HANDLE h=FindFirstFileW(pat, &fd);
        free(pat);
        if(h!=INVALID_HANDLE_VALUE){
            do {
                if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
                DWORD attrs=fd.dwFileAttributes;
                int is_dir = (attrs&FILE_ATTRIBUTE_DIRECTORY)?1:0;
                int is_rep = (attrs&FILE_ATTRIBUTE_REPARSE_POINT)?1:0;
                if(is_cvc_dir_entry(fd.cFileName, is_dir, is_rep)){
                    found=1;
                    /* build cvc path */
                    found_cvc=w_join(cur, ".cvc");
                    /* build root: strip trailing backslash */
                    found_root=_wcsdup(cur);
                    if(found_root && cl>0 && (found_root[cl-1]==L'\\')) found_root[cl-1]=0;
                    break;
                }
            } while(FindNextFileW(h,&fd));
            FindClose(h);
        }
        if(found){
            /* ensure cvc is a real directory */
            WStat st;
            if(w_stat(found_cvc,&st)==0 && st.is_dir && !st.is_reparse){
                repo->cvc16 = found_cvc;
                /* root16: ensure trailing backslash */
                size_t rl=0; while(found_root[rl]) rl++;
                uint16_t *root=(uint16_t*)malloc((rl+2)*sizeof(uint16_t));
                if(!root){ free(found_root); free(found_cvc); free(cur); return cvc_fail(CVC_ERR,"oom"); }
                memcpy(root,found_root,(rl+1)*sizeof(uint16_t));
                if(root[rl-1]!=L'\\'){ root[rl]=L'\\'; root[rl+1]=0; }
                free(found_root);
                repo->root16=root;
                /* root8: convert root to UTF-8 for display (strip trailing slash) */
                size_t rr=0; while(root[rr]) rr++;
                if(rr>0 && root[rr-1]==L'\\') rr--;
                char *r8=utf8_from_utf16(root, rr, NULL);
                repo->root8 = r8 ? r8 : strdup("");
                free(cur);
                /* load config */
                repo_load_config(repo);
                /* read HEAD branch */
                repo_current_branch(repo, &repo->head_branch);
                return CVC_OK;
            }
            free(found_cvc); free(found_root);
        }
        /* move up one directory */
        size_t nl=0; while(cur[nl]) nl++;
        if(nl<=3) break; /* reached drive root */
        /* strip trailing backslash, then cut at last backslash */
        size_t e=nl;
        if(cur[e-1]==L'\\') e--;
        size_t last=0;
        for(size_t i=0;i<e;i++) if(cur[i]==L'\\') last=i;
        if(last==0) break; /* can't go higher */
        uint16_t *parent=(uint16_t*)malloc((last+2)*sizeof(uint16_t));
        if(!parent){ free(cur); return cvc_fail(CVC_ERR,"oom"); }
        memcpy(parent,cur,last*sizeof(uint16_t)); parent[last]=L'\\'; parent[last+1]=0;
        free(cur); cur=parent;
    }
    free(cur);
    return cvc_fail(CVC_ERR_NOTREPO,"not inside a cvc repository");
}

void repo_free(Repo *repo){
    free(repo->root16);
    free(repo->root8);
    free(repo->cvc16);
    free(repo->head_branch);
    strvec_free(&repo->cfg.tracking_include);
    strvec_free(&repo->cfg.tracking_exclude);
    strvec_free(&repo->cfg.diffstat_include);
    strvec_free(&repo->cfg.diffstat_exclude);
    memset(repo,0,sizeof *repo);
}

/* ---------------- config loading ---------------- */

static int validate_pattern_list(const JVal *arr, const char *where, StrVec *out){
    size_t i;
    for(i=0;i<arr->arr_len;i++){
        JVal *e=arr->arr[i];
        if(e->type != J_STR){
            cvc_fail(CVC_ERR,"%s pattern must be a string", where);
            return -1;
        }
        if(memchr(e->str,0,e->str_len)!=NULL){
            cvc_fail(CVC_ERR,"%s pattern contains NUL", where);
            return -1;
        }
        if(e->str_len==0){
            cvc_fail(CVC_ERR,"%s pattern is empty", where);
            return -1;
        }
        if(glob_validate(e->str)!=0){
            cvc_fail(CVC_ERR,"%s invalid glob pattern", where);
            return -1;
        }
        if(strvec_push_dup(out, e->str)!=0) return -1;
    }
    return 0;
}

static int load_strlist(const JVal *obj, const char *key, const char *where, StrVec *out){
    JMember *m=json_find(obj,key);
    if(!m) return 0; /* use default (empty) */
    if(m->val->type != J_ARR){
        cvc_fail(CVC_ERR,"%s.%s must be an array", where, key);
        return -1;
    }
    return validate_pattern_list(m->val, key, out);
}

CvcStatus repo_load_config(Repo *repo){
    /* defaults */
    memset(&repo->cfg,0,sizeof repo->cfg);
    repo->cfg.format_version=1;
    repo->cfg.save_show_diffstat=1;
    strvec_init(&repo->cfg.tracking_include);
    strvec_init(&repo->cfg.tracking_exclude);
    strvec_init(&repo->cfg.diffstat_include);
    strvec_init(&repo->cfg.diffstat_exclude);
    repo->config_ok=0;

    uint16_t *cfgpath=w_join(repo->cvc16,"config.json");
    if(!cfgpath) return cvc_fail(CVC_ERR,"oom");
    Bytes txt; bytes_init(&txt);
    CvcStatus st=w_read_file(cfgpath,&txt);
    free(cfgpath);
    if(st!=CVC_OK) return cvc_fail(CVC_ERR,"missing config.json");
    if(txt.len==0){ bytes_free(&txt); return cvc_fail(CVC_ERR,"empty config.json"); }
    JVal *root=NULL; JErr err;
    if(json_parse((const char*)txt.data, txt.len, &root, &err)!=0){
        char msg[512];
        snprintf(msg,sizeof msg,"config.json: offset %zu: %s", err.err_offset, err.err_msg);
        bytes_free(&txt);
        return cvc_fail(CVC_ERR,"%s", msg);
    }
    bytes_free(&txt);
    if(root->type != J_OBJ){ json_free(root); return cvc_fail(CVC_ERR,"config.json: top-level must be object"); }
    /* format_version required */
    JMember *fv=json_find(root,"format_version");
    if(!fv || fv->val->type!=J_NUM || !fv->val->num){
        json_free(root); return cvc_fail(CVC_ERR,"config.json: missing/invalid format_version");
    }
    /* must be integer token, no fraction/exponent */
    const char *num=fv->val->num;
    if(strpbrk(num,".eE")!=NULL){
        json_free(root); return cvc_fail(CVC_ERR,"config.json: format_version must be integer");
    }
    errno=0;
    long v=strtol(num,NULL,10);
    if(errno==ERANGE || v!=1){
        json_free(root); return cvc_fail(CVC_ERR,"config.json: unsupported format_version");
    }
    repo->cfg.format_version=(int)v;
    /* unknown top-level keys rejected */
    static const char *known_top[]={"format_version","save","tracking","diffstat"};
    for(size_t i=0;i<root->obj_len;i++){
        const char *k=root->obj[i].key;
        int ok=0;
        for(size_t j=0;j<4;j++) if(strcmp(k,known_top[j])==0) ok=1;
        if(!ok){ json_free(root); return cvc_fail(CVC_ERR,"config.json: unknown key \"%s\"",k); }
    }
    /* save */
    JMember *save=json_find(root,"save");
    if(save){
        if(save->val->type!=J_OBJ){ json_free(root); return cvc_fail(CVC_ERR,"config.json: save must be object"); }
        for(size_t i=0;i<save->val->obj_len;i++){
            const char *k=save->val->obj[i].key;
            if(strcmp(k,"show_diffstat")!=0){ json_free(root); return cvc_fail(CVC_ERR,"config.json: unknown key \"save.%s\"",k); }
        }
        JMember *s=json_find(save->val,"show_diffstat");
        if(s){
            if(s->val->type!=J_BOOL){ json_free(root); return cvc_fail(CVC_ERR,"config.json: save.show_diffstat must be bool"); }
            repo->cfg.save_show_diffstat=s->val->b;
        }
    }
    /* tracking */
    JMember *track=json_find(root,"tracking");
    if(track){
        if(track->val->type!=J_OBJ){ json_free(root); return cvc_fail(CVC_ERR,"config.json: tracking must be object"); }
        for(size_t i=0;i<track->val->obj_len;i++){
            const char *k=track->val->obj[i].key;
            if(strcmp(k,"include")!=0&&strcmp(k,"exclude")!=0){ json_free(root); return cvc_fail(CVC_ERR,"config.json: unknown key \"tracking.%s\"",k); }
        }
        /* default include ["**"] */
        JMember *inc=json_find(track->val,"include");
        if(inc){
            if(inc->val->type!=J_ARR){ json_free(root); return cvc_fail(CVC_ERR,"config.json: tracking.include must be array"); }
            if(validate_pattern_list(inc->val,"tracking.include",&repo->cfg.tracking_include)!=0){ json_free(root); return CVC_ERR; }
        } else {
            strvec_push_dup(&repo->cfg.tracking_include,"**");
        }
        if(load_strlist(track->val,"exclude","tracking",&repo->cfg.tracking_exclude)!=0){ json_free(root); return CVC_ERR; }
    } else {
        strvec_push_dup(&repo->cfg.tracking_include,"**");
    }
    /* diffstat */
    JMember *ds=json_find(root,"diffstat");
    if(ds){
        if(ds->val->type!=J_OBJ){ json_free(root); return cvc_fail(CVC_ERR,"config.json: diffstat must be object"); }
        for(size_t i=0;i<ds->val->obj_len;i++){
            const char *k=ds->val->obj[i].key;
            if(strcmp(k,"include")!=0&&strcmp(k,"exclude")!=0){ json_free(root); return cvc_fail(CVC_ERR,"config.json: unknown key \"diffstat.%s\"",k); }
        }
        JMember *inc=json_find(ds->val,"include");
        if(inc){
            if(inc->val->type!=J_ARR){ json_free(root); return cvc_fail(CVC_ERR,"config.json: diffstat.include must be array"); }
            if(validate_pattern_list(inc->val,"diffstat.include",&repo->cfg.diffstat_include)!=0){ json_free(root); return CVC_ERR; }
        } else {
            strvec_push_dup(&repo->cfg.diffstat_include,"**");
        }
        if(load_strlist(ds->val,"exclude","diffstat",&repo->cfg.diffstat_exclude)!=0){ json_free(root); return CVC_ERR; }
    } else {
        strvec_push_dup(&repo->cfg.diffstat_include,"**");
    }
    json_free(root);
    repo->config_ok=1;
    return CVC_OK;
}

/* ---------------- init ---------------- */

CvcStatus repo_init(Repo *repo){
    memset(repo,0,sizeof *repo);
    uint16_t *cwd = w_getcwd16();
    if(!cwd) return cvc_fail(CVC_ERR,"cannot get cwd");
    uint16_t *cur = w_extended(cwd);
    free(cwd);
    if(!cur) return cvc_fail(CVC_ERR,"oom");
    /* check nothing aliases .cvc already */
    WIN32_FIND_DATAW fd;
    size_t cl=0; while(cur[cl]) cl++;
    int has_sep = cl>0 && (cur[cl-1]==L'\\'||cur[cl-1]==L'/');
    uint16_t *pat=(uint16_t*)malloc((cl+3)*sizeof(uint16_t));
    if(!pat){ free(cur); return cvc_fail(CVC_ERR,"oom"); }
    for(size_t i=0;i<cl;i++) pat[i]=cur[i];
    size_t ppos=cl;
    if(!has_sep) pat[ppos++]=L'\\';
    pat[ppos++]=L'*'; pat[ppos]=0;
    HANDLE h=FindFirstFileW(pat,&fd);
    free(pat);
    int aliased=0;
    if(h!=INVALID_HANDLE_VALUE){
        do {
            if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
            DWORD attrs=fd.dwFileAttributes;
            int is_dir=(attrs&FILE_ATTRIBUTE_DIRECTORY)?1:0;
            int is_rep=(attrs&FILE_ATTRIBUTE_REPARSE_POINT)?1:0;
            if(is_cvc_dir_entry(fd.cFileName,is_dir,is_rep)){ aliased=1; break; }
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
    if(aliased){ free(cur); return cvc_fail(CVC_ERR,".cvc already exists in this directory"); }

    /* create metadata entries */
    uint16_t *cvc = w_join(cur, ".cvc");
    if(!cvc){ free(cur); return cvc_fail(CVC_ERR,"oom"); }
    int ok=1;
    /* remember created paths for cleanup on failure */
    StrVec created; strvec_init(&created);
    /* helper */
    #define MKPATH(rel) w_join(cvc, rel)
    /* .cvc */
    if(CreateDirectoryW(cvc,NULL)==0){ /* may exist */ DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS){ ok=0; } }
    strvec_push_dup(&created,".cvc");
    uint16_t *refs=MKPATH("refs");
    uint16_t *heads=MKPATH("refs/heads");
    uint16_t *objs=MKPATH("objects");
    uint16_t *state=MKPATH("state");
    uint16_t *head=MKPATH("HEAD");
    uint16_t *config=MKPATH("config.json");
    uint16_t *lock=MKPATH("lock");
    uint16_t *main_ref=MKPATH("refs/heads/main");
    if(ok && !CreateDirectoryW(refs,NULL)){ DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS) ok=0; }
    if(ok && !CreateDirectoryW(heads,NULL)){ DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS) ok=0; }
    if(ok && !CreateDirectoryW(objs,NULL)){ DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS) ok=0; }
    if(ok && !CreateDirectoryW(state,NULL)){ DWORD e=GetLastError(); if(e!=ERROR_ALREADY_EXISTS) ok=0; }
    /* HEAD */
    if(ok){
        static const char headtxt[]="ref: refs/heads/main\n";
        HANDLE hf=CreateFileW(head,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf==INVALID_HANDLE_VALUE){ ok=0; }
        else { DWORD wr; WriteFile(hf,headtxt,sizeof headtxt-1,&wr,NULL); FlushFileBuffers(hf); CloseHandle(hf); }
    }
    /* refs/heads/main (zero-length) */
    if(ok){
        HANDLE hf=CreateFileW(main_ref,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf==INVALID_HANDLE_VALUE){ ok=0; } else { CloseHandle(hf); }
    }
    /* config.json */
    if(ok){
        static const char cfg[]="{\"format_version\":1}";
        HANDLE hf=CreateFileW(config,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf==INVALID_HANDLE_VALUE){ ok=0; }
        else { DWORD wr; WriteFile(hf,cfg,sizeof cfg-1,&wr,NULL); FlushFileBuffers(hf); CloseHandle(hf); }
    }
    /* lock (zero-length) */
    if(ok){
        HANDLE hf=CreateFileW(lock,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hf==INVALID_HANDLE_VALUE){ ok=0; } else { CloseHandle(hf); }
    }
    free(refs); free(heads); free(objs); free(state); free(head); free(config); free(lock); free(main_ref);
    if(!ok){
        /* Remove only the metadata entries we created for this attempt.
           Since we verified no .cvc alias pre-existed, all of these are ours.
           Delete files first, then empty directories bottom-up, then .cvc. */
        /* NOTE: path buffers were freed above; rebuild minimal names from cvc.
           Since cvc is the .cvc dir, derive child paths deterministically. */
        {
            size_t cl=0; while(cvc[cl]) cl++;
            int has_slash = cl>0 && cvc[cl-1]==L'\\';
            static const wchar_t *kids[][2]={
                {L"refs\\heads\\main",L"f"},
                {L"lock",L"f"},
                {L"config.json",L"f"},
                {L"HEAD",L"f"},
                {L"refs\\heads",L"d"},
                {L"refs",L"d"},
                {L"objects",L"d"},
                {L"state",L"d"},
            };
            for(int i=0;i<8;i++){
                size_t n=cl + (has_slash?0:1) + wcslen(kids[i][0]) + 1;
                uint16_t *p=(uint16_t*)malloc(n*sizeof(uint16_t));
                if(!p) continue;
                size_t w=0; for(size_t k=0;k<cl;k++) p[w++]=cvc[k];
                if(!has_slash) p[w++]=L'\\';
                for(const wchar_t *s=kids[i][0];*s;s++) p[w++]=*s;
                p[w]=0;
                if(kids[i][1][0]==L'f') DeleteFileW(p); else RemoveDirectoryW(p);
                free(p);
            }
        }
        strvec_free(&created);
        free(cvc); free(cur);
        return cvc_fail(CVC_ERR,"init failed during repository creation");
    }
    /* Populate repo struct */
    size_t rl=0; while(cur[rl]) rl++;
    repo->root16=(uint16_t*)malloc((rl+2)*sizeof(uint16_t));
    if(!repo->root16){ free(cvc); free(cur); return cvc_fail(CVC_ERR,"oom"); }
    memcpy(repo->root16,cur,(rl+1)*sizeof(uint16_t));
    if(repo->root16[rl-1]!=L'\\'){ repo->root16[rl]=L'\\'; repo->root16[rl+1]=0; }
    repo->cvc16=cvc;
    /* root8 */
    size_t rr=rl; if(repo->root16[rr-1]==L'\\') rr--;
    char *r8=utf8_from_utf16(repo->root16, rr, NULL);
    repo->root8 = r8?r8:strdup("");
    repo->head_branch=strdup("main");
    /* config */
    strvec_init(&repo->cfg.tracking_include);
    strvec_init(&repo->cfg.tracking_exclude);
    strvec_init(&repo->cfg.diffstat_include);
    strvec_init(&repo->cfg.diffstat_exclude);
    repo->cfg.format_version=1;
    repo->cfg.save_show_diffstat=1;
    strvec_push_dup(&repo->cfg.tracking_include,"**");
    strvec_push_dup(&repo->cfg.diffstat_include,"**");
    repo->config_ok=1;
    strvec_free(&created);
    free(cur);
    return CVC_OK;
}

/* ---------------- branch refs ---------------- */

/* ---- exact (case-sensitive) branch ref lookup ----
 * Windows NTFS is case-insensitive, but branch lookup MUST match the exact
 * stored canonical spelling (spec 09 §6, edge cases §23). We enumerate the
 * actual refs/heads entries and compare byte-for-byte so that `switch feature`
 * does not find an existing `Feature`.
 * Returns 0 if the exact branch exists (optionally filling id/born), -1 if not.
 */
struct RefWalk { const char *branch; const char *cur; int found; int born; uint8_t id[32]; int file_err; };

static int ref_walk_cb(const uint16_t *dir16, const WDirEntry *e, void *ctx){
    struct RefWalk *w=(struct RefWalk*)ctx;
    if(w->found || w->file_err) return 1;
    if(!e->name8) return 0; /* unrepresentable; cannot be a valid branch name */
    size_t curlen = w->cur?strlen(w->cur):0;
    size_t nlen = strlen(e->name8);
    /* build full rel branch name from cur + name */
    size_t need = curlen + (curlen?1:0) + nlen;
    char *full=(char*)malloc(need+1);
    if(!full){ w->file_err=1; return 1; }
    size_t p=0;
    if(curlen){ memcpy(full, w->cur, curlen); p=curlen; full[p++]='/'; }
    memcpy(full+p, e->name8, nlen); p+=nlen; full[p]=0;
    /* exact byte compare with requested branch */
    if(strcmp(full, w->branch)==0){
        if(e->is_dir){ free(full); return 0; } /* branch name ending in / invalid; not a leaf ref */
        w->found=1;
        uint16_t *abs=w_join(dir16, e->name8);
        if(abs){
            Bytes b; bytes_init(&b);
            if(w_read_file(abs,&b)==CVC_OK){
                if(b.len==0){ w->born=0; }
                else if(b.len==65 && b.data[64]=='\n'){
                    b.data[64]=0;
                    if(sha256_is_hex64((char*)b.data) && sha256_from_hex((char*)b.data,w->id)==0){ w->born=1; }
                    else w->file_err=1;
                } else w->file_err=1;
                bytes_free(&b);
            } else w->file_err=1;
            free(abs);
        } else w->file_err=1;
        free(full);
        return 2; /* 2 = found & stop (distinct from error rc=1) */
    }
    free(full);
    /* recurse into a directory component only if it is a prefix of branch */
    if(e->is_dir && !e->is_reparse){
        /* build child prefix and only descend if branch starts with prefix+"/" */
        char *child=(char*)malloc(need+1);
        if(!child){ w->file_err=1; return 1; }
        size_t q=0;
        if(curlen){ memcpy(child, w->cur, curlen); q=curlen; child[q++]='/'; }
        memcpy(child+q, e->name8, nlen); q+=nlen; child[q]=0;
        size_t bl=strlen(w->branch);
        int descend = (bl > q) && strncmp(child, w->branch, q)==0 && w->branch[q]=='/';
        if(descend){
            uint16_t *abs=w_join(dir16, e->name8);
            int subrc=0;
            if(abs){ w->cur=child; subrc=wdir_list(abs, ref_walk_cb, w); w->cur=NULL; free(abs); }
            free(child);
            if(subrc!=0) return subrc; /* propagate found(2)/error(1) up */
        } else free(child);
    }
    return 0;
}

static int repo_ref_exact(const Repo *repo, const char *branch, uint8_t id[32], int *born){
    struct RefWalk w; memset(&w,0,sizeof w);
    w.branch=branch; w.cur=NULL;
    uint16_t *hd=w_repo_to_abs(repo->cvc16,"refs/heads");
    if(!hd) return -1;
    int rc=wdir_list(hd, ref_walk_cb, &w);
    free(hd);
    if(rc==2 && !w.file_err) rc=0;   /* callback matched branch: stop signal => success */
    if(rc!=0 || w.file_err) return -1;
    if(!w.found) return -1;
    if(id) memcpy(id,w.id,32);
    if(born) *born=w.born;
    return 0;
}

int repo_read_branch(const Repo *repo, const char *branch, uint8_t id[32], int *born){
    return repo_ref_exact(repo, branch, id, born);
}

int repo_branch_exists(const Repo *repo, const char *branch){
    return repo_ref_exact(repo, branch, NULL, NULL)==0;
}

int repo_current_branch(const Repo *repo, char **branch_out){
    uint16_t *head=w_repo_to_abs(repo->cvc16,"HEAD");
    if(!head) return -1;
    Bytes b; bytes_init(&b);
    if(w_read_file(head,&b)!=CVC_OK){ free(head); bytes_free(&b); return -1; }
    free(head);
    /* must be "ref: refs/heads/<name>\n" */
    if(b.len < 16 || memcmp(b.data,"ref: refs/heads/",16)!=0 || b.data[b.len-1]!='\n'){
        bytes_free(&b); return -1;
    }
    size_t namelen=b.len-1-16;
    char *name=(char*)malloc(namelen+1);
    if(!name){ bytes_free(&b); return -1; }
    memcpy(name,b.data+16,namelen); name[namelen]=0;
    bytes_free(&b);
    *branch_out=name;
    return 0;
}

/* ---------------- locking ---------------- */

CvcStatus repo_lock_read(const Repo *repo, RepoLock *lk){
    uint16_t *lockpath=w_join(repo->cvc16,"lock");
    if(!lockpath) return cvc_fail(CVC_ERR,"oom");
    if(w_lock_open(lockpath,lk)!=0){ free(lockpath); return cvc_fail(CVC_ERR,"cannot open lock file"); }
    free(lockpath);
    int r=w_lock_acquire(lk,0);
    if(r==0){ w_lock_close(lk); return cvc_fail(CVC_ERR_BUSY,"repository busy (writer lock held)"); }
    if(r<0){ w_lock_close(lk); return cvc_fail(CVC_ERR,"lock error"); }
    return CVC_OK;
}
CvcStatus repo_lock_write(const Repo *repo, RepoLock *lk){
    uint16_t *lockpath=w_join(repo->cvc16,"lock");
    if(!lockpath) return cvc_fail(CVC_ERR,"oom");
    if(w_lock_open(lockpath,lk)!=0){ free(lockpath); return cvc_fail(CVC_ERR,"cannot open lock file"); }
    free(lockpath);
    int r=w_lock_acquire(lk,1);
    if(r==0){ w_lock_close(lk); return cvc_fail(CVC_ERR_BUSY,"repository busy (writer lock held)"); }
    if(r<0){ w_lock_close(lk); return cvc_fail(CVC_ERR,"lock error"); }
    return CVC_OK;
}
void repo_unlock(RepoLock *lk){ w_lock_release(lk); w_lock_close(lk); }

/* ---------------- revision resolution ---------------- */

CvcStatus repo_resolve_revision(const Repo *repo, const char *spec, uint8_t id[32]){
    /* 1. exact branch name */
    int born=0;
    if(repo_read_branch(repo,spec,id,&born)==0){
        if(born) return CVC_OK;
        /* unborn branch doesn't resolve */
        return cvc_fail(CVC_ERR,"revision does not resolve to a commit");
    }
    /* 2. full 64-hex */
    if(sha256_is_hex64(spec)){
        uint8_t raw[32];
        if(sha256_from_hex(spec,raw)==0){
            /* must be a valid commit physically present */
            ObjectData od;
            if(obj_read(repo,raw,&od)==CVC_OK){
                if(od.type=='c'){ object_free(&od); memcpy(id,raw,32); return CVC_OK; }
                object_free(&od);
            }
        }
    }
    /* 3. unique >=8-hex prefix */
    size_t sl=strlen(spec);
    if(sl>=8){
        int allhex=1;
        for(size_t i=0;i<sl;i++){
            char c=spec[i];
            if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))){ allhex=0; break; }
        }
        if(allhex){
            /* scan all canonical loose object paths for commits */
            /* collect ids by walking objects/ */
            uint8_t match[32]; int found=0;
            uint16_t *objdir=w_repo_to_abs(repo->cvc16,"objects");
            if(objdir){
                /* enumerate fan-out dirs */
                WIN32_FIND_DATAW fd;
                size_t dl=0; while(objdir[dl]) dl++;
                int hs=dl>0 && (objdir[dl-1]==L'\\');
                uint16_t *pat=(uint16_t*)malloc((dl+3)*sizeof(uint16_t));
                if(pat){
                    for(size_t i=0;i<dl;i++) pat[i]=objdir[i];
                    size_t pos=dl; if(!hs) pat[pos++]=L'\\';
                    pat[pos++]=L'*'; pat[pos]=0;
                    HANDLE h=FindFirstFileW(pat,&fd);
                    free(pat);
                    if(h!=INVALID_HANDLE_VALUE){
                        do {
                            if(wcscmp(fd.cFileName,L".")==0||wcscmp(fd.cFileName,L"..")==0) continue;
                            if(wcslen(fd.cFileName)!=2) continue;
                            /* fan-out dir; enumerate its 62-char files */
                            {
                                size_t sl_=0; while(objdir[sl_]) sl_++;
                                int hs2=sl_>0 && (objdir[sl_-1]==L'\\');
                                uint16_t *sub2=(uint16_t*)malloc((sl_+2+1+1)*sizeof(uint16_t));
                                if(sub2){
                                    for(size_t i=0;i<sl_;i++) sub2[i]=objdir[i];
                                    size_t q=sl_; if(!hs2) sub2[q++]=L'\\';
                                    sub2[q++]=fd.cFileName[0]; sub2[q++]=fd.cFileName[1]; sub2[q]=0;
                                    /* list files */
                                    uint16_t *pat2=NULL;
                                    size_t s2l=0; while(sub2[s2l]) s2l++;
                                    pat2=(uint16_t*)malloc((s2l+3)*sizeof(uint16_t));
                                    if(pat2){
                                        for(size_t i=0;i<s2l;i++) pat2[i]=sub2[i];
                                        size_t p2=s2l; if(!(s2l>0&&(sub2[s2l-1]==L'\\'))) pat2[p2++]=L'\\';
                                        pat2[p2++]=L'*'; pat2[p2]=0;
                                        WIN32_FIND_DATAW fd2;
                                        HANDLE h2=FindFirstFileW(pat2,&fd2);
                                        free(pat2);
                                        if(h2!=INVALID_HANDLE_VALUE){
                                            do {
                                                if(wcscmp(fd2.cFileName,L".")==0||wcscmp(fd2.cFileName,L"..")==0) continue;
                                                if(wcslen(fd2.cFileName)!=62) continue;
                                                /* build full id */
                                                wchar_t full[66];
                                                full[0]=fd.cFileName[0]; full[1]=fd.cFileName[1];
                                                wcsncpy(full+2, fd2.cFileName, 62); full[64]=0;
                                                /* convert to utf8 hex */
                                                char hx[65];
                                                /* use wcstombs-safe via custom: each wchar is ascii hex */
                                                for(int k=0;k<64;k++){
                                                    wchar_t wc=full[k];
                                                    hx[k]=(wc<128)?(char)wc:'?';
                                                }
                                                hx[64]=0;
                                                if(sha256_is_hex64(hx)){
                                                    uint8_t rawid[32];
                                                    sha256_from_hex(hx,rawid);
                                                    /* prefix match? compare lowercase */
                                                    int pm=1;
                                                    for(size_t c=0;c<sl;c++){
                                                        char a=spec[c]; char b=hx[c];
                                                        if(a>='A'&&a<='F') a=a-'A'+'a';
                                                        if(a!=b){ pm=0; break; }
                                                    }
                                                    if(pm){
                                                        ObjectData od;
                                                        if(obj_read(repo,rawid,&od)==CVC_OK){
                                                            if(od.type=='c'){
                                                                if(!found){ memcpy(match,rawid,32); found=1; }
                                                                else {
                                                                    object_free(&od);
                                                                    free(sub2);
                                                                    FindClose(h2); FindClose(h);
                                                                    free(objdir);
                                                                    return cvc_fail(CVC_ERR,"ambiguous revision prefix");
                                                                }
                                                            }
                                                            object_free(&od);
                                                        }
                                                    }
                                                }
                                            } while(FindNextFileW(h2,&fd2));
                                            FindClose(h2);
                                        }
                                    }
                                    free(sub2);
                                }
                            }
                        } while(FindNextFileW(h,&fd));
                        FindClose(h);
                    }
                }
                free(objdir);
            }
            if(found){ memcpy(id,match,32); return CVC_OK; }
        }
    }
    return cvc_fail(CVC_ERR,"revision not found");
}

