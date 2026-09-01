#include "cli.h"
#include "repo.h"
#include "objects.h"
#include "scan.h"
#include "snapshot.h"
#include "materialize.h"
#include "merge.h"
#include "sha256.h"
#include "utf8.h"
#include "glob.h"
#include "diff.h"
#include "win32.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include <errno.h>

/* ================================================================== */
/*  Wide-argument conversion                                           */
/* ================================================================== */
static int convert_argv(int argc, wchar_t **wargv, char ***out_argv){
    char **av=(char**)malloc((size_t)(argc+1)*sizeof(char*));
    if(!av) return -1;
    for(int i=0;i<argc;i++){
        size_t n=0; while(wargv[i][n]) n++;
        char *u8=utf8_from_utf16(wargv[i], n, NULL);
        if(!u8){ for(int k=0;k<i;k++) free(av[k]); free(av); return -1; }
        av[i]=u8;
    }
    av[argc]=NULL;
    *out_argv=av;
    return 0;
}
static void free_argv(char **av){
    if(!av) return;
    for(size_t i=0;av[i];i++) free(av[i]);
    free(av);
}

static void out_fmt(const char *fmt, ...){
    char buf[4096];
    va_list ap; va_start(ap,fmt);
    vsnprintf(buf,sizeof buf,fmt,ap);
    va_end(ap);
    w_out_stdout(buf, strlen(buf));
}
static void print_utf8(const char *s){
    if(s) w_out_stdout(s, strlen(s));
}
static void err_fmt(const char *fmt, ...){
    char buf[4096];
    va_list ap; va_start(ap,fmt);
    vsnprintf(buf,sizeof buf,fmt,ap);
    va_end(ap);
    w_out_stderr(buf, strlen(buf));
}

/* ================================================================== */
/*  Refs / commits                                                     */
/* ================================================================== */
static CvcStatus write_ref_file(const Repo *repo, const char *relpath, const uint8_t *bytes, size_t n){
    uint16_t *abs=w_repo_to_abs(repo->cvc16, relpath);
    if(!abs) return cvc_fail(CVC_ERR,"oom");
    const char *slash=strrchr(relpath,'/');
    if(slash){
        char *dir=(char*)malloc((size_t)(slash-relpath)+1);
        if(!dir){ free(abs); return cvc_fail(CVC_ERR,"oom"); }
        memcpy(dir,relpath,(size_t)(slash-relpath)); dir[slash-relpath]=0;
        char *buf=(char*)malloc(strlen(dir)+1);
        if(!buf){ free(dir); free(abs); return cvc_fail(CVC_ERR,"oom"); }
        size_t bl=0; const char *start=dir;
        while(1){
            const char *sl=strchr(start,'/');
            size_t seglen=sl?(size_t)(sl-start):strlen(start);
            if(bl>0) buf[bl++]='/';
            memcpy(buf+bl,start,seglen); bl+=seglen; buf[bl]=0;
            uint16_t *d=w_repo_to_abs(repo->cvc16,buf);
            if(d){ WStat st; if(w_stat(d,&st)!=0||!st.exists) w_mkdir(d); free(d); }
            if(!sl) break;
            start=sl+1;
        }
        free(buf); free(dir);
    }
    int rc=w_write_file_durable(abs, bytes, n);
    free(abs);
    return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot write %s",relpath);
}
static CvcStatus set_ref_born(const Repo *repo, const char *branch, const uint8_t id[32]){
    char rel[300]; snprintf(rel,sizeof rel,"refs/heads/%s",branch);
    char hex[65]; id_hex(id,hex);
    char buf[66]; memcpy(buf,hex,64); buf[64]='\n'; buf[65]=0;
    return write_ref_file(repo, rel, (const uint8_t*)buf, 65);
}
static CvcStatus write_head(const Repo *repo, const char *branch){
    char buf[300];
    int n=snprintf(buf,sizeof buf,"ref: refs/heads/%s\n",branch);
    return write_ref_file(repo,"HEAD",(const uint8_t*)buf,(size_t)n);
}
/* returns 1 born (id set), 0 unborn, -1 error */
static int head_commit(const Repo *repo, uint8_t id[32]){
    char *branch=NULL;
    if(repo_current_branch(repo,&branch)!=0) return -1;
    int born=0;
    int rc=repo_read_branch(repo,branch,id,&born);
    free(branch);
    if(rc!=0) return -1;
    return born?1:0;
}
static CvcStatus make_commit(const Repo *repo, const uint8_t root_tree[32],
                             const uint8_t *parents, int nparents,
                             const char *message, size_t message_len,
                             uint8_t commit_id[32]){
    int64_t ts;
    if(!w_timestamp_valid(&ts))
        return cvc_fail(CVC_ERR, "malformed CVC_TEST_TIMESTAMP");
    if(getenv("CVC_TEST_TIMESTAMP")){ /* use ts */ } else { ts=w_wall_clock(); }
    Commit c; memset(&c,0,sizeof c);
    memcpy(c.root_tree,root_tree,32);
    if(nparents>2) return cvc_fail(CVC_ERR,"too many parents");
    c.parent_count=(uint8_t)nparents;
    for(int i=0;i<nparents;i++) memcpy(c.parents[i], parents+i*32, 32);
    c.timestamp=ts;
    c.message=(char*)message;
    c.message_len=message_len;
    Bytes pl;
    if(commit_encode(&c,&pl)!=0) return cvc_fail(CVC_ERR,"oom");
    CvcStatus st=obj_write_envelope(repo,"commit",pl.data,pl.len,commit_id);
    bytes_free(&pl);
    return st;
}

/* ================================================================== */
/*  Option helpers                                                     */
/* ================================================================== */
static int parse_long_opt(const char *arg, const char *opt, char **value){
    size_t l=strlen(opt);
    if(strncmp(arg,opt,l)!=0) return 0;
    if(arg[l]=='\0'){ *value=NULL; return 1; }
    if(arg[l]=='='){ *value=(char*)(arg+l+1); return 1; }
    return 0;
}
static CvcStatus parse_pattern_list(const char *csv, StrVec *out){
    strvec_init(out);
    if(!csv || csv[0]=='\0') return cvc_fail(CVC_ERR_USAGE,"empty pattern list");
    const char *p=csv;
    while(1){
        const char *comma=strchr(p,',');
        size_t n=comma?(size_t)(comma-p):strlen(p);
        if(n==0){ strvec_free(out); return cvc_fail(CVC_ERR_USAGE,"empty element in pattern list"); }
        char *elem=(char*)malloc(n+1);
        if(!elem){ strvec_free(out); return cvc_fail(CVC_ERR,"oom"); }
        memcpy(elem,p,n); elem[n]=0;
        if(!utf8_valid_no_nul((const uint8_t*)elem,n)){
            free(elem); strvec_free(out);
            return cvc_fail(CVC_ERR_USAGE,"pattern not valid UTF-8");
        }
        if(glob_validate(elem)!=0){
            char *msg=(char*)malloc(strlen(elem)+64);
            if(msg){ snprintf(msg,strlen(elem)+64,"invalid glob pattern \"%s\"",elem); }
            free(elem); strvec_free(out);
            CvcStatus bad=msg?cvc_fail(CVC_ERR_USAGE,"%s",msg):cvc_fail(CVC_ERR_USAGE,"invalid glob pattern");
            free(msg);
            return bad;
        }
        strvec_push(out,elem);
        if(!comma) break;
        p=comma+1;
    }
    return CVC_OK;
}

/* ================================================================== */
/*  Open repo with lock                                                */
/* ================================================================== */
static CvcStatus open_repo(Repo *repo, RepoLock *lk, int need_write){
    memset(repo,0,sizeof *repo);
    CvcStatus st=repo_discover(repo);
    if(st!=CVC_OK){
        if(st==CVC_ERR_NOTREPO) return cvc_fail(CVC_ERR_NOTREPO,"not inside a CVC repository");
        return st;
    }
    st=repo_load_config(repo);
    if(st!=CVC_OK){ repo_free(repo); return st; }
    st=need_write?repo_lock_write(repo,lk):repo_lock_read(repo,lk);
    if(st!=CVC_OK){ repo_free(repo); return st; }
    return CVC_OK;
}

/* ================================================================== */
/*  Dirty-check                                                        */
/* ================================================================== */
/* Compares selected working tree to `cur` snapshot. Returns 1 dirty, 0 clean, -1 err. */
static int wt_differs(Repo *repo, const Snapshot *cur){
    ScanResult scan; scan_init(&scan);
    CvcStatus st=scan_snapshot(repo,&scan,0);
    if(st!=CVC_OK){ scan_free(&scan); return -1; }
    Snapshot wt; snap_init(&wt);
    for(size_t i=0;i<scan.len;i++){
        if(snap_add(&wt,scan.items[i].path,scan.items[i].type,scan.items[i].id)!=0){
            scan_free(&scan); snap_free(&wt); return -1;
        }
    }
    scan_free(&scan);
    DiffList dl; diff_list_init(&dl);
    st=snap_compare(cur,&wt,&dl);
    int dirty=(dl.len>0);
    diff_list_free(&dl); snap_free(&wt);
    return dirty;
}

/* ================================================================== */
/*  Branch name validation                                             */
/* ================================================================== */
static int is_win_component_ok(const char *comp){
    size_t n=strlen(comp);
    if(n==0) return 0;
    for(size_t i=0;i<n;i++){
        unsigned char c=(unsigned char)comp[i];
        if(c=='<'||c=='>'||c==':'||c=='"'||c=='\\'||c=='|'||c=='?'||c=='*'||c==0x7f||c<0x20) return 0;
    }
    if(comp[n-1]==' '||comp[n-1]=='.') return 0;
    if(utf8_validate((const uint8_t*)comp,n)!=0) return 0;
    /* reserved DOS device */
    char base[16]; size_t bi=0;
    for(size_t i=0;i<n;i++){ if(comp[i]=='.') break; if(bi<15) base[bi++]=comp[i]; }
    base[bi]=0;
    if(bi>0){
        static const char *devs[]={"CON","PRN","AUX","NUL",
            "COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
            "LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9",
            "COM\xc2\xb9","COM\xc2\xb2","COM\xc2\xb3","LPT\xc2\xb9","LPT\xc2\xb2","LPT\xc2\xb3"};
        for(size_t d=0;d<sizeof(devs)/sizeof(devs[0]);d++)
            if(utf8_ordinal_case_equal(base,devs[d])) return 0;
    }
    return 1;
}

/* Validate a branch name. Returns 0 valid, nonzero invalid (diagnostic set). */
static int branch_name_valid(const char *name){
    size_t n=strlen(name);
    if(n<1||n>128) return 0;
    if(!utf8_valid_no_nul((const uint8_t*)name,n)) return 0;
    if(name[0]=='/'||name[n-1]=='/') return 0;
    if(name[0]=='-') return 0;
    if(strcmp(name,"HEAD")==0) return 0;
    if(strstr(name,"..")) return 0;
    if(strchr(name,'\\')) return 0;
    /* split on '/' */
    const char *p=name;
    while(*p){
        const char *sl=strchr(p,'/');
        size_t seglen=sl?(size_t)(sl-p):strlen(p);
        if(seglen==0) return 0;
        char *seg=(char*)malloc(seglen+1);
        if(!seg) return 0;
        memcpy(seg,p,seglen); seg[seglen]=0;
        int ok=is_win_component_ok(seg);
        if(!ok || strcmp(seg,".")==0 || strcmp(seg,"..")==0){ free(seg); return 0; }
        free(seg);
        if(!sl) break;
        p=sl+1;
    }
    return 1;
}

/* forward decl: used by ref_case_collision / branch_has_unreachable below */
static int br_list_cb(const uint16_t *dir16, const WDirEntry *e, void *ctx);
/* context for br_list_cb: gathers all leaf branch names recursively */
struct BrListCtx { StrVec *names; const char *cur; const char *prefix; int err; };

/* ---- ordinal case-insensitive ref-namespace collision ----
 * Spec 09 §6: branch creation MUST reject a branch whose ref pathname would
 * case-insensitively collide with an existing branch/ref namespace component.
 */
/* Returns 1 if any existing branch leaf or namespace prefix collides
 * case-insensitively with `name`. */
static int ref_case_collision(const Repo *repo, const char *name){
    uint16_t *hd=w_repo_to_abs(repo->cvc16,"refs/heads");
    if(!hd) return 0;
    /* We need full-path comparison, not per-entry. Reuse a gather of leaves. */
    StrVec leaves; strvec_init(&leaves);
    struct BrListCtx lc; lc.names=&leaves; lc.cur=NULL; lc.prefix=NULL; lc.err=0;
    wdir_list(hd, br_list_cb, &lc);
    free(hd);
    int hit=0;
    for(size_t i=0;i<leaves.len && !hit;i++){
        const char *ex=leaves.items[i];
        if(utf8_ordinal_case_equal(ex,name)){ hit=1; break; }
        /* namespace prefix collision: one is a proper prefix + '/' of the other */
        size_t el=strlen(ex), nl=strlen(name);
        size_t m = el<nl?el:nl;
        if(m && strncmp(ex,name,m)==0){
            if((el<nl && name[m]=='/') || (nl<el && ex[m]=='/')){ hit=1; break; }
        }
    }
    strvec_free(&leaves);
    return hit;
}

/* ---- reachability warning for branch delete ----
 * Spec 02 §178: deletion of a branch containing commits not reachable from
 * another branch MUST print a warning before/with successful deletion.
 */

/* Does `id` already appear in set `set` (list of lowercase hex ids)? */
static int reach_set_has(const StrVec *set, const uint8_t id[32]){
    char hex[65]; sha256_to_hex(id,hex);
    for(size_t i=0;i<set->len;i++) if(strcmp(set->items[i],hex)==0) return 1;
    return 0;
}
/* Add `id` to set unless present. */
static int reach_set_add(StrVec *set, const uint8_t id[32]){
    if(reach_set_has(set,id)) return 0;
    char hex[65]; sha256_to_hex(id,hex);
    return strvec_push_dup(set, hex);
}
/* DFS walk of commit ancestry; adds every commit id to `set`. */
static int reach_walk_commits(const Repo *repo, StrVec *set, const uint8_t id[32]){
    if(reach_set_add(set,id)!=0) return -1;
    ObjectData od; if(obj_read(repo,id,&od)!=CVC_OK) return -1;
    if(od.type!='c'){ object_free(&od); return 0; }
    Commit cm; if(commit_decode(od.payload.data,od.payload.len,&cm)!=0){ object_free(&od); return -1; }
    for(size_t i=0;i<cm.parent_count;i++){
        if(reach_walk_commits(repo,set,cm.parents[i])!=0){ commit_free(&cm); object_free(&od); return -1; }
    }
    commit_free(&cm); object_free(&od);
    return 0;
}
/* DFS walk of commit ancestry; returns 1 if any commit in the history is NOT
 * in `base`. Used to detect commits that would become unreachable. */
struct UnreachWalkCtx { const Repo *repo; const StrVec *base; StrVec *vis; int found; int err; };
static int unreach_walk_rec(struct UnreachWalkCtx *d, const uint8_t id[32]){
    if(d->found || d->err) return 0;
    if(reach_set_add(d->vis,id)!=0){ d->err=1; return 0; }
    if(!reach_set_has(d->base,id)){ d->found=1; return 0; }
    ObjectData od; if(obj_read(d->repo,id,&od)!=CVC_OK){ d->err=1; return 0; }
    if(od.type!='c'){ object_free(&od); return 0; }
    Commit cm; if(commit_decode(od.payload.data,od.payload.len,&cm)!=0){ object_free(&od); d->err=1; return 0; }
    for(size_t i=0;i<cm.parent_count && !d->found && !d->err;i++){
        unreach_walk_rec(d, cm.parents[i]);
    }
    commit_free(&cm); object_free(&od);
    return 0;
}
/* Returns 1 if the branch `name` history contains a commit not reachable from
 * the union of all other branches (i.e. deleting it would orphan that commit). */
static int branch_has_unreachable(const Repo *repo, const char *name){
    uint8_t del_id[32]; int del_born=0;
    if(repo_read_branch(repo,name,del_id,&del_born)!=0 || !del_born) return 0;
    /* 1) reachable set from all branches except `name` */
    StrVec base; strvec_init(&base);
    uint16_t *hd=w_repo_to_abs(repo->cvc16,"refs/heads");
    if(!hd){ strvec_free(&base); return 0; }
    StrVec all; strvec_init(&all);
    struct BrListCtx lc; lc.names=&all; lc.cur=NULL; lc.prefix=NULL; lc.err=0;
    wdir_list(hd, br_list_cb, &lc);
    free(hd);
    for(size_t i=0;i<all.len;i++){
        if(strcmp(all.items[i],name)==0) continue;
        uint8_t id[32]; int born=0;
        if(repo_read_branch(repo,all.items[i],id,&born)==0 && born){
            if(reach_walk_commits(repo,&base,id)!=0){ strvec_free(&all); strvec_free(&base); return 0; }
        }
    }
    strvec_free(&all);
    /* 2) walk deleted branch; if any commit missing from base -> unreachable */
    StrVec vis; strvec_init(&vis);
    struct UnreachWalkCtx d; d.repo=repo; d.base=&base; d.vis=&vis; d.found=0; d.err=0;
    unreach_walk_rec(&d, del_id);
    int found = d.found && !d.err;
    strvec_free(&vis);
    strvec_free(&base);
    return found;
}

/* Validate a canonical repository-root-relative path operand (restore/resolve).
 * Must be nonempty, valid UTF-8, no NUL/control, use '/', no leading/trailing
 * '/', no empty segment, no '.'/'..' segment, no backslash, and each component
 * must be Windows-safe (reuses is_win_component_ok). Returns 0 valid, else -1. */
static int path_operand_valid(const char *p){
    if(!p || p[0]=='\0') return -1;
    size_t n=strlen(p);
    if(memchr(p,0,n)) return -1;
    if(!utf8_valid_no_nul((const uint8_t*)p,n)) return -1;
    if(p[0]=='/' || p[n-1]=='/') return -1;
    if(strchr(p,'\\')) return -1;
    const char *seg=p;
    while(*seg){
        const char *sl=strchr(seg,'/');
        size_t seglen=sl?(size_t)(sl-seg):strlen(seg);
        if(seglen==0) return -1;
        char *comp=(char*)malloc(seglen+1);
        if(!comp) return -1;
        memcpy(comp,seg,seglen); comp[seglen]=0;
        int ok=is_win_component_ok(comp);
        int bad = !ok || strcmp(comp,".")==0 || strcmp(comp,"..")==0;
        free(comp);
        if(bad) return -1;
        if(!sl) break;
        seg=sl+1;
    }
    return 0;
}

/* ================================================================== */
/*  Commands                                                           */
/* ================================================================== */

static const char *usage_text(void);

static CvcStatus cmd_help(void){
    print_utf8(usage_text());
    return CVC_OK;
}

static const char *usage_text(void){
    return
"Usage: cvc <command> [options] [arguments]\n"
"\n"
"Commands:\n"
"  init                       Initialize a new repository in the current directory\n"
"  status                     Show working-tree changes relative to HEAD\n"
"  save -m <message>          Record a new snapshot of tracked files\n"
"  log [--max-count=<N>]      Show commit history (first-parent)\n"
"  diff [<revision>]          Show working-tree changes\n"
"  branch                     List branches\n"
"  branch create <name>       Create a branch at current commit\n"
"  branch delete <name>       Delete a branch\n"
"  switch <branch>            Switch to a branch\n"
"  restore <path> --from <rev> Restore a tracked path from a revision\n"
"  rollback <rev> -m <msg>    Create a new commit reverting to a revision's tree\n"
"  merge <branch> [-m <msg>]  Merge a branch\n"
"  merge --continue [-m <msg>] Continue a merge after resolving conflicts\n"
"  merge --abort              Abort an in-progress merge\n"
"  resolve <path>             Record the resolution of a conflict root\n"
"  verify                     Verify repository integrity\n"
"  config show                Show effective configuration\n"
"  config validate            Validate configuration\n"
"  help                       Show this help\n"
"\n"
"Options:\n"
"  -m <msg>, --include=<pats>, --exclude=<pats>, --no-diffstat,\n"
"  --max-count=<N>, --from <rev>\n";
}

static CvcStatus cmd_init(char **av, int n){
    if(n!=0) return cvc_fail(CVC_ERR_USAGE,"init takes no arguments");
    Repo repo;
    CvcStatus st=repo_init(&repo);
    if(st!=CVC_OK) return st;
    out_fmt("Initialized empty CVC repository in %s\n", repo.root8);
    repo_free(&repo);
    return CVC_OK;
}

/* --- config --- */
static void print_patterns(const char *label, StrVec *v){
    out_fmt("  %s = [",label);
    for(size_t i=0;i<v->len;i++){
        if(i) out_fmt(", ");
        out_fmt("\"%s\"",v->items[i]);
    }
    out_fmt("]\n");
}
static CvcStatus cmd_config_show(Repo *repo){
    out_fmt("format_version = %d\n", repo->cfg.format_version);
    out_fmt("save.show_diffstat = %s\n", repo->cfg.save_show_diffstat?"true":"false");
    print_patterns("tracking.include",&repo->cfg.tracking_include);
    print_patterns("tracking.exclude",&repo->cfg.tracking_exclude);
    print_patterns("diffstat.include",&repo->cfg.diffstat_include);
    print_patterns("diffstat.exclude",&repo->cfg.diffstat_exclude);
    return CVC_OK;
}
static CvcStatus cmd_config(char **av, int n){
    if(n!=1) return cvc_fail(CVC_ERR_USAGE,"usage: cvc config <show|validate>");
    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,0);
    if(st!=CVC_OK) return st;
    if(strcmp(av[0],"show")==0){
        st=cmd_config_show(&repo);
    } else if(strcmp(av[0],"validate")==0){
        out_fmt("configuration valid\n");
        st=CVC_OK;
    } else {
        st=cvc_fail(CVC_ERR_USAGE,"usage: cvc config <show|validate>");
    }
    repo_unlock(&lk); repo_free(&repo);
    return st;
}

/* --- status --- */
static int path_display_selected(const char *path, StrVec *inc, StrVec *exc){
    size_t i; int m=0;
    for(i=0;i<inc->len;i++) if(glob_match(inc->items[i],path)){ m=1; break; }
    if(!m) return 0;
    for(i=0;i<exc->len;i++) if(glob_match(exc->items[i],path)) return 0;
    return 1;
}
static CvcStatus cmd_status(char **av, int n){
    StrVec inc; strvec_init(&inc); strvec_push_dup(&inc,"**");
    StrVec exc; strvec_init(&exc);
    int seen_inc=0, seen_exc=0;
    for(int i=0;i<n;i++){
        char *v=NULL;
        if(parse_long_opt(av[i],"--include",&v)){
            if(seen_inc){ strvec_free(&inc); strvec_free(&exc); return cvc_fail(CVC_ERR_USAGE,"duplicate --include"); }
            seen_inc=1;
            StrVec ni; CvcStatus st=parse_pattern_list(v,&ni);
            if(st!=CVC_OK){ strvec_free(&inc); strvec_free(&exc); return st; }
            strvec_free(&inc); inc=ni;
            continue;
        }
        if(parse_long_opt(av[i],"--exclude",&v)){
            if(seen_exc){ strvec_free(&inc); strvec_free(&exc); return cvc_fail(CVC_ERR_USAGE,"duplicate --exclude"); }
            seen_exc=1;
            StrVec ne; CvcStatus st=parse_pattern_list(v,&ne);
            if(st!=CVC_OK){ strvec_free(&inc); strvec_free(&exc); return st; }
            strvec_free(&exc); exc=ne;
            continue;
        }
        strvec_free(&inc); strvec_free(&exc);
        return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
    }
    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,0);
    if(st!=CVC_OK){ strvec_free(&inc); strvec_free(&exc); return st; }
    uint8_t head[32]; int born=head_commit(&repo,head);
    Snapshot cur; snap_init(&cur);
    if(born==1){
        st=snap_from_commit(&repo,head,&cur);
        if(st!=CVC_OK){ repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    }
    ScanResult scan; scan_init(&scan);
    st=scan_snapshot(&repo,&scan,0);
    if(st!=CVC_OK){ snap_free(&cur); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    Snapshot wt; snap_init(&wt);
    for(size_t i=0;i<scan.len;i++){
        if(snap_add(&wt,scan.items[i].path,scan.items[i].type,scan.items[i].id)!=0){
            st=cvc_fail(CVC_ERR,"oom"); break;
        }
    }
    scan_free(&scan);
    if(st!=CVC_OK){ snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    DiffList dl; diff_list_init(&dl);
    st=snap_compare(&cur,&wt,&dl);
    if(st!=CVC_OK){ snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    int shown=0;
    for(size_t i=0;i<dl.len;i++){
        DiffResult *r=&dl.items[i];
        if(!path_display_selected(r->path,&inc,&exc)) continue;
        const char *label;
        switch(r->status){
            case 1: label="added"; break;
            case 2: label="modified"; break;
            case 3: label="deleted"; break;
            case 4: label="type-changed"; break;
            default: label="changed"; break;
        }
        out_fmt("%-12s %s\n",label,r->path);
        shown=1;
    }
    /* merge state */
    MergeState ms; merge_state_init(&ms);
    int has_merge=(merge_state_load(&repo,&ms)==CVC_OK);
    if(has_merge){
        if(ms.phase==MERGE_PHASE_CONFLICT){
            out_fmt("merge in progress: target %s\n", ms.target_branch?ms.target_branch:"");
            int unres=0;
            for(size_t i=0;i<ms.n_conflicts;i++) if(!ms.conflicts[i].resolved) unres++;
            out_fmt("  %zu conflict(s), %d unresolved\n", ms.n_conflicts, unres);
            for(size_t i=0;i<ms.n_conflicts;i++){
                if(!ms.conflicts[i].resolved) out_fmt("  conflicted    %s\n", ms.conflicts[i].path);
            }
        } else {
            out_fmt("merge finalizing in progress\n");
        }
        merge_state_free(&ms);
    }
    if(!shown && !has_merge) print_utf8("clean\n");
    diff_list_free(&dl); snap_free(&cur); snap_free(&wt);
    repo_unlock(&lk); repo_free(&repo);
    strvec_free(&inc); strvec_free(&exc);
    return CVC_OK;
}

/* --- log --- */
static CvcStatus cmd_log(char **av, int n){
    char *mc=NULL;
    for(int i=0;i<n;i++){
        char *v=NULL;
        if(parse_long_opt(av[i],"--max-count",&v)){ if(mc){ return cvc_fail(CVC_ERR_USAGE,"duplicate --max-count"); } mc=v; continue; }
        return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
    }
    unsigned long long limit=~0ULL;
    if(mc){
        /* [1-9][0-9]* fitting uint64 */
        if(mc[0]<'1'||mc[0]>'9') return cvc_fail(CVC_ERR_USAGE,"invalid --max-count");
        for(size_t i=0;mc[i];i++) if(mc[i]<'0'||mc[i]>'9') return cvc_fail(CVC_ERR_USAGE,"invalid --max-count");
        errno=0; char *end=NULL;
        unsigned long long v=strtoull(mc,&end,10);
        if(errno==ERANGE || end==NULL || *end!='\0') return cvc_fail(CVC_ERR_USAGE,"invalid --max-count");
        limit=v;
    }
    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,0);
    if(st!=CVC_OK) return st;
    uint8_t head[32]; int born=head_commit(&repo,head);
    if(!born){ print_utf8("no commits\n"); repo_unlock(&lk); repo_free(&repo); return CVC_OK; }
    uint8_t cur[32]; memcpy(cur,head,32);
    unsigned long long count=0;
    while(1){
        if(count>=limit) break;
        ObjectData od;
        st=obj_read(&repo,cur,&od);
        if(st!=CVC_OK){ repo_unlock(&lk); repo_free(&repo); return st; }
        if(od.type!='c'){ object_free(&od); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"corrupt commit"); }
        Commit c;
        if(commit_decode(od.payload.data,od.payload.len,&c)!=0){ object_free(&od); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"corrupt commit"); }
        object_free(&od);
        char hex[65]; id_hex(cur,hex);
        out_fmt("commit %s\n",hex);
        out_fmt("time %lld\n",(long long)c.timestamp);
        if(c.parent_count==0) out_fmt("root commit\n");
        else {
            out_fmt("parents:");
            for(int p=0;p<c.parent_count;p++){
                char ph[65]; id_hex(c.parents[p],ph);
                out_fmt(" %s",ph);
            }
            out_fmt("\n");
        }
        out_fmt("%s\n", c.message);
        out_fmt("\n");
        if(c.parent_count==0){ commit_free(&c); break; }
        memcpy(cur,c.parents[0],32);
        commit_free(&c);
        count++;
    }
    repo_unlock(&lk); repo_free(&repo);
    return CVC_OK;
}

/* ================================================================== */
/*  Shared: read blob/symlink content for a snapshot leaf              */
/* ================================================================== */
static CvcStatus read_leaf_bytes(Repo *repo, const uint8_t id[32], uint8_t type, Bytes *out){
    bytes_init(out);
    ObjectData od;
    CvcStatus st=obj_read(repo,id,&od);
    if(st!=CVC_OK) return st;
    bytes_append(out,od.payload.data,od.payload.len);
    object_free(&od);
    return CVC_OK;
}

/* ================================================================== */
/*  Shared: render a byte-safe diff of two text blobs                  */
/* ================================================================== */
static void render_line_diff(const uint8_t *oldd, size_t oldn, const uint8_t *newd, size_t newn,
                             const char *path){
    size_t *oo=NULL,*ol=NULL,*no=NULL,*nl=NULL; size_t onc=0,nnc=0;
    if(diff_split_lines(oldd,oldn,&oo,&ol,&onc)!=0) return;
    if(diff_split_lines(newd,newn,&no,&nl,&nnc)!=0){ free(oo);free(ol); return; }
    DiffEdit *edits=NULL; size_t ne=0;
    if(diff_myers(oldd,oo,ol,onc, newd,no,nl,nnc, &edits,&ne)!=0){ free(oo);free(ol);free(no);free(nl); return; }
    out_fmt("diff %s\n",path);
    size_t i;
    for(i=0;i<ne;i++){
        DiffEdit *e=&edits[i];
        if(e->op==0) continue;
        if(e->op==1){
            /* deletion */
            for(size_t k=0;k<e->count;k++){
                size_t li=e->old_line+k;
                if(li<onc){
                    size_t cl=ol[li];
                    int has_nl = cl>0 && oldd[oo[li]+cl-1]=='\n';
                    if(has_nl) cl--;
                    char *r=diff_render_line(oldd+oo[li], cl);
                    out_fmt("-%s\n",r);
                    free(r);
                    if(!has_nl) out_fmt("\\ No newline at end of file\n");
                }
            }
        } else {
            /* insertion */
            for(size_t k=0;k<e->count;k++){
                size_t li=e->new_line+k;
                if(li<nnc){
                    size_t cl=nl[li];
                    int has_nl = cl>0 && newd[no[li]+cl-1]=='\n';
                    if(has_nl) cl--;
                    char *r=diff_render_line(newd+no[li], cl);
                    out_fmt("+%s\n",r);
                    free(r);
                    if(!has_nl) out_fmt("\\ No newline at end of file\n");
                }
            }
        }
    }
    free(oo);free(ol);free(no);free(nl);free(edits);
}

/* Render a symlink entry change. */
static void render_symlink_change(Repo *repo, const char *path, const uint8_t *oldid, uint8_t oldtype,
                                  const uint8_t *newid, uint8_t newtype){
    out_fmt("symlink %s\n",path);
    Bytes ob; 
    if(oldid && read_leaf_bytes(repo,oldid,oldtype,&ob)==CVC_OK){
        out_fmt("  old: kind=%s target=%s\n", (oldtype==OBJ_TREE_DIR_SYMLINK)?"directory":"file", (const char*)ob.data);
        bytes_free(&ob);
    } else {
        out_fmt("  old: (none)\n");
    }
    if(newid && read_leaf_bytes(repo,newid,newtype,&ob)==CVC_OK){
        out_fmt("  new: kind=%s target=%s\n", (newtype==OBJ_TREE_DIR_SYMLINK)?"directory":"file", (const char*)ob.data);
        bytes_free(&ob);
    } else {
        out_fmt("  new: (none)\n");
    }
}

/* ================================================================== */
/*  Command: save                                                      */
/* ================================================================== */
static CvcStatus cmd_save_dispatch(char **av, int n){
    char *msg=NULL; int have_msg=0;
    StrVec dsinc; strvec_init(&dsinc); strvec_push_dup(&dsinc,"**");
    StrVec dsexc; strvec_init(&dsexc);
    int seen_inc=0,seen_exc=0, seen_m=0, no_diffstat=0, seen_nd=0;
    for(int i=0;i<n;i++){
        if(strcmp(av[i],"-m")==0){
            if(seen_m) return cvc_fail(CVC_ERR_USAGE,"duplicate -m");
            if(i+1>=n) return cvc_fail(CVC_ERR_USAGE,"-m requires an argument");
            seen_m=1; msg=av[++i]; have_msg=1;
            if(msg[0]=='\0') return cvc_fail(CVC_ERR_USAGE,"message must be nonempty");
            if(!utf8_valid_no_nul((const uint8_t*)msg,strlen(msg))) return cvc_fail(CVC_ERR_USAGE,"message not valid UTF-8");
            continue;
        }
        char *v=NULL;
        if(parse_long_opt(av[i],"--include",&v)){
            if(seen_inc) return cvc_fail(CVC_ERR_USAGE,"duplicate --include");
            seen_inc=1;
            StrVec ni; CvcStatus st=parse_pattern_list(v,&ni);
            if(st!=CVC_OK) return st;
            strvec_free(&dsinc); dsinc=ni;
            continue;
        }
        if(parse_long_opt(av[i],"--exclude",&v)){
            if(seen_exc) return cvc_fail(CVC_ERR_USAGE,"duplicate --exclude");
            seen_exc=1;
            StrVec ne; CvcStatus st=parse_pattern_list(v,&ne);
            if(st!=CVC_OK) return st;
            strvec_free(&dsexc); dsexc=ne;
            continue;
        }
        if(parse_long_opt(av[i],"--no-diffstat",&v)){
            if(seen_nd) return cvc_fail(CVC_ERR_USAGE,"duplicate --no-diffstat");
            seen_nd=1; no_diffstat=1;
            continue;
        }
        return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
    }
    if(!have_msg) return cvc_fail(CVC_ERR_USAGE,"save requires -m <message>");

    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;
    /* diffstat filters default from config unless overridden on CLI (spec 05 §7) */
    if(!seen_inc){ strvec_free(&dsinc); for(size_t z=0;z<repo.cfg.diffstat_include.len;z++) strvec_push_dup(&dsinc,repo.cfg.diffstat_include.items[z]); }
    if(!seen_exc){ strvec_free(&dsexc); for(size_t z=0;z<repo.cfg.diffstat_exclude.len;z++) strvec_push_dup(&dsexc,repo.cfg.diffstat_exclude.items[z]); }
    /* reject active merge */
    MergeState ms; merge_state_init(&ms);
    int has_merge=(merge_state_load(&repo,&ms)==CVC_OK);
    if(has_merge){
        merge_state_free(&ms);
        repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc);
        return cvc_fail(CVC_ERR_MERGEDIRTY,"a merge is in progress; resolve or abort it before saving");
    }
    uint8_t head[32]; int born=head_commit(&repo,head);
    Snapshot cur; snap_init(&cur);
    if(born==1){
        st=snap_from_commit(&repo,head,&cur);
        if(st!=CVC_OK){ snap_free(&cur); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    }
    /* scan with object writes */
    ScanResult scan; scan_init(&scan);
    st=scan_snapshot(&repo,&scan,1);
    if(st!=CVC_OK){ snap_free(&cur); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    Snapshot wt; snap_init(&wt);
    for(size_t i=0;i<scan.len;i++){
        if(snap_add(&wt,scan.items[i].path,scan.items[i].type,scan.items[i].id)!=0){
            st=cvc_fail(CVC_ERR,"oom"); break;
        }
    }
    /* ignored summary */
    size_t ign=scan.ignored_binary+scan.ignored_unsupported+scan.ignored_excluded+scan.ignored_nested_repo;
    scan_free(&scan);
    if(st!=CVC_OK){ snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    /* compute diff */
    DiffList dl; diff_list_init(&dl);
    st=snap_compare(&cur,&wt,&dl);
    if(st!=CVC_OK){ diff_list_free(&dl); snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    if(dl.len==0){
        diff_list_free(&dl); snap_free(&cur); snap_free(&wt);
        if(ign>0) out_fmt("nothing to save (%zu ignored)\n",ign);
        else print_utf8("nothing to save\n");
        repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc);
        return CVC_OK;
    }
    /* diffstat (if enabled) */
    int show_ds = repo.cfg.save_show_diffstat && !no_diffstat;
    if(show_ds){
        size_t tot_ins=0,tot_del=0;
        for(size_t i=0;i<dl.len;i++){
            DiffResult *r=&dl.items[i];
            if(!path_display_selected(r->path,&dsinc,&dsexc)) continue;
            if(r->old_type==OBJ_TREE_BLOB && r->new_type==OBJ_TREE_BLOB && r->status==2){
                Bytes ob,nb;
                if(read_leaf_bytes(&repo,r->old_id,OBJ_TREE_BLOB,&ob)==CVC_OK && read_leaf_bytes(&repo,r->new_id,OBJ_TREE_BLOB,&nb)==CVC_OK){
                    size_t *oo=NULL,*ol=NULL,*no=NULL,*nl=NULL; size_t onc=0,nnc=0;
                    if(diff_split_lines(ob.data,ob.len,&oo,&ol,&onc)==0 && diff_split_lines(nb.data,nb.len,&no,&nl,&nnc)==0){
                        DiffEdit *ed=NULL; size_t nen=0;
                        if(diff_myers(ob.data,oo,ol,onc,nb.data,no,nl,nnc,&ed,&nen)==0){
                            size_t ins=0,del=0; diff_count(ed,nen,&ins,&del);
                            out_fmt("%s: +%zu -%zu\n",r->path,ins,del);
                            tot_ins+=ins; tot_del+=del;
                            free(ed);
                        }
                        free(oo);free(ol);free(no);free(nl);
                    }
                }
                bytes_free(&ob); bytes_free(&nb);
            } else if(r->status==1 && r->new_type==OBJ_TREE_BLOB){
                Bytes nb;
                if(read_leaf_bytes(&repo,r->new_id,OBJ_TREE_BLOB,&nb)==CVC_OK){
                    size_t *no=NULL,*nl=NULL; size_t nnc=0;
                    if(diff_split_lines(nb.data,nb.len,&no,&nl,&nnc)==0){
                        out_fmt("%s: +%zu -0\n",r->path,nnc);
                        tot_ins+=nnc; free(no);free(nl);
                    }
                }
                bytes_free(&nb);
            } else if(r->status==3 && r->old_type==OBJ_TREE_BLOB){
                Bytes ob;
                if(read_leaf_bytes(&repo,r->old_id,OBJ_TREE_BLOB,&ob)==CVC_OK){
                    size_t *oo=NULL,*ol=NULL; size_t onc=0;
                    if(diff_split_lines(ob.data,ob.len,&oo,&ol,&onc)==0){
                        out_fmt("%s: +0 -%zu\n",r->path,onc);
                        tot_del+=onc; free(oo);free(ol);
                    }
                }
                bytes_free(&ob);
            }
        }
        out_fmt("total: %zu insertions, %zu deletions\n",tot_ins,tot_del);
    }
    /* build tree + commit */
    uint8_t root_tree[32];
    st=snap_build_tree(&repo,&wt,root_tree);
    if(st!=CVC_OK){ diff_list_free(&dl); snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    uint8_t commit_id[32];
    uint8_t parents[64];
    int np=0;
    if(born==1){ memcpy(parents,head,32); np=1; }
    st=make_commit(&repo,root_tree,parents,np,msg,strlen(msg),commit_id);
    if(st!=CVC_OK){ diff_list_free(&dl); snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    /* move branch ref */
    char *branch=NULL;
    if(repo_current_branch(&repo,&branch)!=0){ diff_list_free(&dl); snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return cvc_fail(CVC_ERR,"cannot read HEAD"); }
    st=set_ref_born(&repo,branch,commit_id);
    free(branch);
    if(st!=CVC_OK){ diff_list_free(&dl); snap_free(&cur); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc); return st; }
    char hex[65]; id_hex(commit_id,hex);
    out_fmt("saved %s\n",hex);
    diff_list_free(&dl); snap_free(&cur); snap_free(&wt);
    repo_unlock(&lk); repo_free(&repo); strvec_free(&dsinc); strvec_free(&dsexc);
    return CVC_OK;
}

/* ================================================================== */
/*  Command: diff                                                      */
/* ================================================================== */
static CvcStatus cmd_diff_dispatch(char **av, int n){
    char *rev=NULL;
    StrVec inc; strvec_init(&inc); strvec_push_dup(&inc,"**");
    StrVec exc; strvec_init(&exc);
    int seen_inc=0,seen_exc=0;
    /* first positional may be a revision */
    int pos=0;
    for(int i=0;i<n;i++){
        char *v=NULL;
        if(parse_long_opt(av[i],"--include",&v)){
            if(seen_inc) return cvc_fail(CVC_ERR_USAGE,"duplicate --include");
            seen_inc=1; StrVec ni; CvcStatus st=parse_pattern_list(v,&ni);
            if(st!=CVC_OK) return st;
            strvec_free(&inc); inc=ni; continue;
        }
        if(parse_long_opt(av[i],"--exclude",&v)){
            if(seen_exc) return cvc_fail(CVC_ERR_USAGE,"duplicate --exclude");
            seen_exc=1; StrVec ne; CvcStatus st=parse_pattern_list(v,&ne);
            if(st!=CVC_OK) return st;
            strvec_free(&exc); exc=ne; continue;
        }
        if(av[i][0]!='-'){
            if(pos==0){ rev=av[i]; pos++; continue; }
            return cvc_fail(CVC_ERR_USAGE,"too many arguments");
        }
        return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
    }
    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,0);
    if(st!=CVC_OK) return st;
    uint8_t base[32];
    Snapshot base_snap; snap_init(&base_snap);
    if(rev){
        st=repo_resolve_revision(&repo,rev,base);
        if(st!=CVC_OK){ snap_free(&base_snap); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
        st=snap_from_commit(&repo,base,&base_snap);
        if(st!=CVC_OK){ snap_free(&base_snap); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    } else {
        int born=head_commit(&repo,base);
        if(born==1){
            st=snap_from_commit(&repo,base,&base_snap);
            if(st!=CVC_OK){ snap_free(&base_snap); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
        }
    }
    /* working tree. Write content-addressed blobs so read_leaf_bytes can
       render their bytes; unreachable objects are explicitly permitted to
       remain and never alter branch history. */
    ScanResult scan; scan_init(&scan);
    st=scan_snapshot(&repo,&scan,1);
    if(st!=CVC_OK){ snap_free(&base_snap); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    Snapshot wt; snap_init(&wt);
    for(size_t i=0;i<scan.len;i++){
        if(snap_add(&wt,scan.items[i].path,scan.items[i].type,scan.items[i].id)!=0){ st=cvc_fail(CVC_ERR,"oom"); break; }
    }
    scan_free(&scan);
    if(st!=CVC_OK){ snap_free(&base_snap); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    DiffList dl; diff_list_init(&dl);
    st=snap_compare(&base_snap,&wt,&dl);
    if(st!=CVC_OK){ diff_list_free(&dl); snap_free(&base_snap); snap_free(&wt); repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc); return st; }
    for(size_t i=0;i<dl.len;i++){
        DiffResult *r=&dl.items[i];
        if(!path_display_selected(r->path,&inc,&exc)) continue;
        if(r->status==4 || r->old_type!=r->new_type){
            /* type change */
            out_fmt("%-12s %s (type %d -> %d)\n","type-changed",r->path,r->old_type,r->new_type);
        } else if(r->old_type==OBJ_TREE_BLOB && r->new_type==OBJ_TREE_BLOB){
            Bytes ob,nb;
            CvcStatus so=read_leaf_bytes(&repo,r->old_id,OBJ_TREE_BLOB,&ob);
            CvcStatus sn=read_leaf_bytes(&repo,r->new_id,OBJ_TREE_BLOB,&nb);
            if(so==CVC_OK && sn==CVC_OK){
                render_line_diff(ob.data,ob.len,nb.data,nb.len,r->path);
            }
            bytes_free(&ob); bytes_free(&nb);
        } else if((r->old_type==OBJ_TREE_FILE_SYMLINK||r->old_type==OBJ_TREE_DIR_SYMLINK)||
                  (r->new_type==OBJ_TREE_FILE_SYMLINK||r->new_type==OBJ_TREE_DIR_SYMLINK)){
            render_symlink_change(&repo,r->path,
                r->status!=1?r->old_id:NULL, r->old_type,
                r->status!=3?r->new_id:NULL, r->new_type);
        } else {
            out_fmt("%s\n",r->path);
        }
    }
    diff_list_free(&dl); snap_free(&base_snap); snap_free(&wt);
    repo_unlock(&lk); repo_free(&repo); strvec_free(&inc); strvec_free(&exc);
    return CVC_OK;
}

/* ================================================================== */
/*  Command: branch                                                    */
/* ================================================================== */
/* collect all leaf branch names under refs/heads recursively */
static int br_list_cb(const uint16_t *dir16, const WDirEntry *e, void *ctx){
    struct BrListCtx *c=(struct BrListCtx*)ctx;
    if(c->err) return 1;
    if(!e->name8) return 0;
    size_t plen = c->prefix?strlen(c->prefix):0;
    size_t nlen = strlen(e->name8);
    /* full branch name = prefix + name */
    char *full=(char*)malloc(plen+nlen+1);
    if(!full){ c->err=1; return 1; }
    size_t q=0;
    if(plen){ memcpy(full,c->prefix,plen); q=plen; full[q++]='/'; }
    memcpy(full+q,e->name8,nlen); q+=nlen; full[q]=0;
    if(e->is_dir && !e->is_reparse){
        /* recurse */
        struct BrListCtx sub; sub.names=c->names; sub.cur=c->cur; sub.prefix=full; sub.err=0;
        uint16_t *abs=w_join(dir16,e->name8);
        int subrc=0;
        if(abs){ subrc=wdir_list(abs, br_list_cb, &sub); free(abs); }
        free(full);
        if(sub.err){ c->err=1; return 1; }
        return subrc;
    } else {
        strvec_push_dup(c->names,full);
        free(full);
        return 0;
    }
}
static int list_branches(Repo *repo){
    /* enumerate refs/heads recursively */
    uint16_t *dir=w_repo_to_abs(repo->cvc16,"refs/heads");
    if(!dir) return -1;
    char *cur=NULL; repo_current_branch(repo,&cur);
    StrVec names; strvec_init(&names);
    struct BrListCtx ctx; ctx.names=&names; ctx.cur=cur; ctx.prefix=NULL; ctx.err=0;
    wdir_list(dir, br_list_cb, &ctx);
    free(dir);
    /* sort by unsigned byte (use qsort on strcmp) */
    for(size_t i=0;i<names.len;i++){
        for(size_t j=i+1;j<names.len;j++){
            if(strcmp(names.items[j],names.items[i])<0){
                char *t=names.items[i]; names.items[i]=names.items[j]; names.items[j]=t;
            }
        }
    }
    for(size_t i=0;i<names.len;i++){
        if(cur && strcmp(names.items[i],cur)==0) out_fmt("* %s\n",names.items[i]);
        else out_fmt("  %s\n",names.items[i]);
    }
    free(cur);
    strvec_free(&names);
    return ctx.err? -1 : 0;
}

static CvcStatus cmd_branch_dispatch(char **av, int n){
    /* branch list is read-only during merge; create/delete blocked */
    if(n==0){
        Repo repo; RepoLock lk;
        CvcStatus st=open_repo(&repo,&lk,0);
        if(st!=CVC_OK) return st;
        int rc=list_branches(&repo);
        repo_unlock(&lk); repo_free(&repo);
        return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot list branches");
    }
    if(n>=1 && strcmp(av[0],"create")==0){
        if(n!=2) return cvc_fail(CVC_ERR_USAGE,"usage: cvc branch create <name>");
        if(!branch_name_valid(av[1])) return cvc_fail(CVC_ERR_USAGE,"invalid branch name");
        Repo repo; RepoLock lk;
        CvcStatus st=open_repo(&repo,&lk,1);
        if(st!=CVC_OK) return st;
        MergeState ms; merge_state_init(&ms);
        if(merge_state_load(&repo,&ms)==CVC_OK){ merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR_MERGEDIRTY,"cannot create branch during a merge"); }
        if(repo_branch_exists(&repo,av[1])){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"branch already exists"); }
        /* spec 09 §6: reject ordinal case-insensitive ref-namespace collision */
        if(ref_case_collision(&repo,av[1])){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"branch name collides case-insensitively with an existing branch"); }
        uint8_t head[32]; int born=head_commit(&repo,head);
        if(born==1){
            st=set_ref_born(&repo,av[1],head);
        } else {
            /* unborn: create zero-length ref */
            char rel[300]; snprintf(rel,sizeof rel,"refs/heads/%s",av[1]);
            st=write_ref_file(&repo,rel,NULL,0);
        }
        repo_unlock(&lk); repo_free(&repo);
        return st;
    }
    if(n>=1 && strcmp(av[0],"delete")==0){
        if(n!=2) return cvc_fail(CVC_ERR_USAGE,"usage: cvc branch delete <name>");
        Repo repo; RepoLock lk;
        CvcStatus st=open_repo(&repo,&lk,1);
        if(st!=CVC_OK) return st;
        MergeState ms; merge_state_init(&ms);
        if(merge_state_load(&repo,&ms)==CVC_OK){ merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR_MERGEDIRTY,"cannot delete branch during a merge"); }
        char *cur=NULL; repo_current_branch(&repo,&cur);
        if(cur && strcmp(cur,av[1])==0){ free(cur); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot delete the current branch"); }
        free(cur);
        if(!repo_branch_exists(&repo,av[1])){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"branch does not exist"); }
        /* spec 02 §178: warn if branch contains commits unreachable from other branches */
        if(branch_has_unreachable(&repo,av[1])){
            err_fmt("warning: branch '%s' contains commits unreachable from any other branch; deleting it will lose them\n", av[1]);
        }
        /* check reachability warning: is av[1] tip reachable from another branch? */
        char rel[300]; snprintf(rel,sizeof rel,"refs/heads/%s",av[1]);
        uint16_t *refpath=w_repo_to_abs(repo.cvc16,rel);
        int del_ok = (refpath && w_delete_path(refpath,0)==0);
        free(refpath);
        if(!del_ok){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot delete branch"); }
        repo_unlock(&lk); repo_free(&repo);
        return CVC_OK;
    }
    return cvc_fail(CVC_ERR_USAGE,"usage: cvc branch [create <name>|delete <name>]");
}

/* ================================================================== */
/*  Command: switch                                                    */
/* ================================================================== */
static CvcStatus cmd_switch_dispatch(char **av, int n){
    if(n!=1) return cvc_fail(CVC_ERR_USAGE,"usage: cvc switch <branch>");
    if(!branch_name_valid(av[0])) return cvc_fail(CVC_ERR_USAGE,"invalid branch name");
    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;
    MergeState ms; merge_state_init(&ms);
    if(merge_state_load(&repo,&ms)==CVC_OK){ merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR_MERGEDIRTY,"cannot switch during a merge"); }
    char *cur=NULL;
    if(repo_current_branch(&repo,&cur)!=0){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot read HEAD"); }
    if(strcmp(cur,av[0])==0){
        free(cur); repo_unlock(&lk); repo_free(&repo);
        out_fmt("already on branch %s\n",av[0]);
        return CVC_OK;
    }
    /* target branch exists? */
    if(!repo_branch_exists(&repo,av[0])){ free(cur); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"branch does not exist"); }
    uint8_t target[32]; int tborn=0;
    repo_read_branch(&repo,av[0],target,&tborn);
    uint8_t head[32]; int hborn=head_commit(&repo,head);
    /* dirty check against current HEAD */
    Snapshot cur_snap; snap_init(&cur_snap);
    if(hborn==1){
        st=snap_from_commit(&repo,head,&cur_snap);
        if(st!=CVC_OK){ free(cur); snap_free(&cur_snap); repo_unlock(&lk); repo_free(&repo); return st; }
    }
    int dirty=wt_differs(&repo,&cur_snap);
    if(dirty>0){ free(cur); snap_free(&cur_snap); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"working tree has uncommitted changes; cannot switch"); }
    if(dirty<0){ free(cur); snap_free(&cur_snap); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot scan working tree"); }
    /* target snapshot (empty if unborn) */
    Snapshot target_snap; snap_init(&target_snap);
    if(tborn){
        st=snap_from_commit(&repo,target,&target_snap);
        if(st!=CVC_OK){ free(cur); snap_free(&cur_snap); snap_free(&target_snap); repo_unlock(&lk); repo_free(&repo); return st; }
    }
    /* preflight collisions */
    st=mat_preflight(&repo,&cur_snap,&target_snap,1);
    if(st!=CVC_OK){ free(cur); snap_free(&cur_snap); snap_free(&target_snap); repo_unlock(&lk); repo_free(&repo); return st; }
    /* materialize */
    st=mat_materialize(&repo,&cur_snap,&target_snap);
    if(st!=CVC_OK){ free(cur); snap_free(&cur_snap); snap_free(&target_snap); repo_unlock(&lk); repo_free(&repo); return st; }
    /* update HEAD */
    st=write_head(&repo,av[0]);
    if(st!=CVC_OK){
        /* HEAD update failed: roll the working tree back to the original snapshot */
        CvcStatus rback=mat_materialize(&repo,&target_snap,&cur_snap);
        free(cur); snap_free(&cur_snap); snap_free(&target_snap);
        repo_unlock(&lk); repo_free(&repo);
        if(rback!=CVC_OK)
            return cvc_fail(CVC_ERR,"failed to update HEAD; and failed to restore old tree");
        return cvc_fail(CVC_ERR,"failed to update HEAD; restored old tree");
    }
    out_fmt("switched to branch %s\n",av[0]);
    free(cur); snap_free(&cur_snap); snap_free(&target_snap);
    repo_unlock(&lk); repo_free(&repo);
    return CVC_OK;
}

/* ================================================================== */
/*  Command: restore                                                    */
/* ================================================================== */
/* Build a "current" snapshot (HEAD-tracked) and a "target" snapshot
 * (source-revision subtree) restricted to paths under `root` (or exact
 * leaf for a leaf restore), then materialize with restore collision rules.
 */
static CvcStatus restore_apply(Repo *repo, const char *root, const Snapshot *src_snap){
    /* current snapshot = HEAD tracked state */
    uint8_t head[32]; int hborn=head_commit(repo,head);
    Snapshot cur; snap_init(&cur);
    if(hborn==1){
        CvcStatus st=snap_from_commit(repo,head,&cur);
        if(st!=CVC_OK){ snap_free(&cur); return st; }
    }
    /* Build "current" restricted to the restore target namespace:
       paths in `cur` under root, plus any tracked path that is an ancestor
       container (structural). For materialization we only need the leaves.
       We pass full cur to preflight/materialize but restrict target to the
       source subtree. Since materialize compares current vs target leaves,
       passing full cur is safe: it only deletes tracked leaves not in target.
       However that would delete tracked leaves OUTSIDE root too. So we must
       restrict current to paths under root (leaf restore) or under root. */
    Snapshot cur_restricted; snap_init(&cur_restricted);
    size_t rl=strlen(root);
    size_t i;
    for(i=0;i<cur.len;i++){
        const char *p=cur.items[i].path;
        /* is p == root (exact leaf) or under root/ */
        if(strcmp(p,root)==0 || (rl>0 && strncmp(p,root,rl)==0 && p[rl]=='/')){
            if(snap_add(&cur_restricted,p,cur.items[i].type,cur.items[i].id)!=0){
                snap_free(&cur); snap_free(&cur_restricted); return cvc_fail(CVC_ERR,"oom");
            }
        }
    }
    /* Build target snapshot from src_snap under root (exact leaf or subtree) */
    Snapshot tgt; snap_init(&tgt);
    for(i=0;i<src_snap->len;i++){
        const char *p=src_snap->items[i].path;
        if(strcmp(p,root)==0 || (rl>0 && strncmp(p,root,rl)==0 && p[rl]=='/')){
            if(snap_add(&tgt,p,src_snap->items[i].type,src_snap->items[i].id)!=0){
                snap_free(&cur); snap_free(&cur_restricted); snap_free(&tgt);
                return cvc_fail(CVC_ERR,"oom");
            }
        }
    }
    /* If source path absent from revision, fail. */
    long idx=snap_find(src_snap,root);
    if(idx<0){
        /* if root is a directory in target but no exact leaf, check subtree */
        int has_sub=0;
        for(i=0;i<src_snap->len;i++){
            if(rl>0 && strncmp(src_snap->items[i].path,root,rl)==0 && src_snap->items[i].path[rl]=='/'){ has_sub=1; break; }
        }
        if(!has_sub){
            snap_free(&cur); snap_free(&cur_restricted); snap_free(&tgt);
            return cvc_fail(CVC_ERR,"path not present in revision");
        }
    }
    /* Preflight: restore allows replacing tracked, forbids overwriting untracked. */
    CvcStatus st=mat_preflight(repo,&cur_restricted,&tgt,0);
    if(st!=CVC_OK){ snap_free(&cur); snap_free(&cur_restricted); snap_free(&tgt); return st; }
    /* Materialize with rollback on failure. */
    st=mat_materialize(repo,&cur_restricted,&tgt);
    snap_free(&cur); snap_free(&cur_restricted); snap_free(&tgt);
    return st;
}

CvcStatus cmd_restore_dispatch(char **av, int n){
    /* restore <path> --from <revision> */
    char *path=NULL, *rev=NULL;
    for(int i=0;i<n;i++){
        if(strcmp(av[i],"--from")==0){
            if(rev) return cvc_fail(CVC_ERR_USAGE,"duplicate --from");
            if(i+1>=n) return cvc_fail(CVC_ERR_USAGE,"--from requires a revision");
            rev=av[i+1]; i++;
            continue;
        }
        if(parse_long_opt(av[i],"--from",&path)){
            /* --from=<rev> form */
            if(rev) return cvc_fail(CVC_ERR_USAGE,"duplicate --from");
            if(!path || path[0]=='\0') return cvc_fail(CVC_ERR_USAGE,"--from requires a revision");
            rev=path;
            continue;
        }
        /* positional: the path operand (may begin with '-', e.g. -notes.txt) */
        if(!path){ path=av[i]; continue; }
        return cvc_fail(CVC_ERR_USAGE,"too many arguments");
    }
    if(!rev || !path) return cvc_fail(CVC_ERR_USAGE,"usage: cvc restore <path> --from <revision>");
    if(path_operand_valid(path)!=0) return cvc_fail(CVC_ERR_USAGE,"invalid path operand");

    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;
    /* Forbidden during finalizing phase (retryable or completed). During
       conflict phase it's allowed to edit working tree before resolve. */
    MergeState ms; merge_state_init(&ms);
    if(merge_state_load(&repo,&ms)==CVC_OK){
        if(ms.phase==MERGE_PHASE_FINALIZING){
            merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
            return cvc_fail(CVC_ERR,"restore is forbidden during merge finalizing");
        }
        merge_state_free(&ms);
    }
    /* Resolve revision to commit and get its snapshot. */
    uint8_t cid[32];
    st=repo_resolve_revision(&repo,rev,cid);
    if(st!=CVC_OK){ repo_unlock(&lk); repo_free(&repo); return st; }
    Snapshot src; snap_init(&src);
    st=snap_from_commit(&repo,cid,&src);
    if(st!=CVC_OK){ snap_free(&src); repo_unlock(&lk); repo_free(&repo); return st; }
    st=restore_apply(&repo,path,&src);
    snap_free(&src);
    repo_unlock(&lk); repo_free(&repo);
    if(st!=CVC_OK) return st;
    out_fmt("restored %s\n",path);
    return CVC_OK;
}

/* ================================================================== */
/*  Command: rollback                                                   */
/* ================================================================== */
CvcStatus cmd_rollback_dispatch(char **av, int n){
    /* rollback <revision> -m <message> */
    char *rev=NULL, *msg=NULL;
    int have_msg=0, seen_m=0;
    for(int i=0;i<n;i++){
        if(strcmp(av[i],"-m")==0){
            if(seen_m) return cvc_fail(CVC_ERR_USAGE,"duplicate -m");
            if(i+1>=n) return cvc_fail(CVC_ERR_USAGE,"-m requires an argument");
            seen_m=1; msg=av[++i]; have_msg=1;
            if(msg[0]=='\0') return cvc_fail(CVC_ERR_USAGE,"message must be nonempty");
            if(!utf8_valid_no_nul((const uint8_t*)msg,strlen(msg))) return cvc_fail(CVC_ERR_USAGE,"message not valid UTF-8");
            continue;
        }
        if(av[i][0]!='-' && !rev){ rev=av[i]; continue; }
        return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
    }
    if(!rev) return cvc_fail(CVC_ERR_USAGE,"usage: cvc rollback <revision> -m <message>");
    if(!have_msg) return cvc_fail(CVC_ERR_USAGE,"rollback requires -m <message>");
    if(strchr(rev,'/')==NULL && path_operand_valid(rev)!=0 && !branch_name_valid(rev)){
        /* rev may be a branch name (with / allowed) or hex. Only gate obvious garbage. */
    }

    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;
    /* merge state must be absent */
    MergeState ms; merge_state_init(&ms);
    if(merge_state_load(&repo,&ms)==CVC_OK){
        merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
        return cvc_fail(CVC_ERR_MERGEDIRTY,"cannot rollback during a merge");
    }
    /* current branch must be born */
    uint8_t head[32]; int hborn=head_commit(&repo,head);
    if(hborn!=1){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot rollback from an unborn branch"); }
    /* resolve target revision */
    uint8_t tid[32];
    st=repo_resolve_revision(&repo,rev,tid);
    if(st!=CVC_OK){ repo_unlock(&lk); repo_free(&repo); return st; }
    /* target snapshot */
    Snapshot tgt; snap_init(&tgt);
    st=snap_from_commit(&repo,tid,&tgt);
    if(st!=CVC_OK){ snap_free(&tgt); repo_unlock(&lk); repo_free(&repo); return st; }
    /* current snapshot */
    Snapshot cur; snap_init(&cur);
    st=snap_from_commit(&repo,head,&cur);
    if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return st; }
    /* dirty check: selected working snapshot must equal current HEAD */
    int dirty=wt_differs(&repo,&cur);
    if(dirty>0){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"working tree has uncommitted changes; cannot rollback"); }
    if(dirty<0){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot scan working tree"); }
    /* preflight collisions (rollback may replace tracked state) */
    st=mat_preflight(&repo,&cur,&tgt,1);
    if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return st; }
    /* materialize target tree */
    st=mat_materialize(&repo,&cur,&tgt);
    if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return st; }
    /* build tree + commit with old HEAD as single parent */
    uint8_t root_tree[32];
    st=snap_build_tree(&repo,&tgt,root_tree);
    if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return st; }
    uint8_t parents[32]; memcpy(parents,head,32);
    uint8_t commit_id[32];
    st=make_commit(&repo,root_tree,parents,1,msg,strlen(msg),commit_id);
    if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return st; }
    /* move current branch ref */
    char *branch=NULL;
    if(repo_current_branch(&repo,&branch)!=0){ snap_free(&tgt); snap_free(&cur); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"cannot read HEAD"); }
    st=set_ref_born(&repo,branch,commit_id);
    free(branch);
    snap_free(&tgt); snap_free(&cur);
    repo_unlock(&lk); repo_free(&repo);
    if(st!=CVC_OK) return st;
    char hex[65]; id_hex(commit_id,hex);
    out_fmt("rollback %s\n",hex);
    return CVC_OK;
}

/* ================================================================== */
/*  Command: merge (orchestration)                                     */
/* ================================================================== */
/* Write conflict markers for a textual-content conflict between two blobs. */
static CvcStatus conflict_markers_write(Repo *repo, const char *path,
                                        const uint8_t ours_id[32], const uint8_t theirs_id[32],
                                        const char *ours_label, const char *theirs_label){
    Bytes ob,tb,out; bytes_init(&ob); bytes_init(&tb); bytes_init(&out);
    ObjectData od;
    if(obj_read(repo,ours_id,&od)!=CVC_OK||od.type!='b'){ object_free(&od); bytes_free(&ob);bytes_free(&tb);bytes_free(&out); return cvc_fail(CVC_ERR,"missing ours blob"); }
    bytes_append(&ob,od.payload.data,od.payload.len); object_free(&od);
    if(obj_read(repo,theirs_id,&od)!=CVC_OK||od.type!='b'){ object_free(&od); bytes_free(&ob);bytes_free(&tb);bytes_free(&out); return cvc_fail(CVC_ERR,"missing theirs blob"); }
    bytes_append(&tb,od.payload.data,od.payload.len); object_free(&od);
    bytes_append_cstr(&out,"<<<<<<< "); bytes_append_cstr(&out,ours_label?ours_label:"ours"); bytes_append_byte(&out,'\n');
    bytes_append(&out,ob.data,ob.len);
    if(ob.len>0 && ob.data[ob.len-1]!='\n') bytes_append_byte(&out,'\n');
    bytes_append_cstr(&out,"=======\n");
    bytes_append(&out,tb.data,tb.len);
    if(tb.len>0 && tb.data[tb.len-1]!='\n') bytes_append_byte(&out,'\n');
    bytes_append_cstr(&out,">>>>>>> "); bytes_append_cstr(&out,theirs_label?theirs_label:"theirs"); bytes_append_byte(&out,'\n');
    uint16_t *abs=w_repo_to_abs(repo->root16,path);
    if(!abs){ bytes_free(&ob);bytes_free(&tb);bytes_free(&out); return cvc_fail(CVC_ERR,"oom"); }
    int rc=w_write_file_atomic(abs,out.data,out.len);
    bytes_free(&ob);bytes_free(&tb);bytes_free(&out); free(abs);
    return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot write conflict file");
}

/* Build the working-tree target snapshot for a merge in the conflict phase:
 * the provisional merged leaves, with the "ours" representation at each
 * conflict root (ours leaves under the root; provisional leaves under the
 * root removed). This is what the merge working tree must equal. */
static CvcStatus build_conflict_wt(const Snapshot *provisional,
                                   const ConflictEntry *conflicts, size_t n_conflicts,
                                   const Snapshot *ours, Snapshot *out){
    snap_init(out);
    size_t i;
    for(i=0;i<provisional->len;i++){
        if(snap_add(out,provisional->items[i].path,provisional->items[i].type,provisional->items[i].id)!=0)
            return cvc_fail(CVC_ERR,"oom");
    }
    for(i=0;i<n_conflicts;i++){
        const char *root=conflicts[i].path;
        size_t rl=strlen(root);
        size_t j;
        for(j=0;j<out->len;){
            const char *p=out->items[j].path;
            if(strcmp(p,root)==0 || (rl>0 && strncmp(p,root,rl)==0 && p[rl]=='/')){
                free(out->items[j].path);
                out->items[j]=out->items[out->len-1];
                out->len--;
            } else j++;
        }
        for(j=0;j<ours->len;j++){
            const char *p=ours->items[j].path;
            if(strcmp(p,root)==0 || (rl>0 && strncmp(p,root,rl)==0 && p[rl]=='/')){
                if(snap_add(out,p,ours->items[j].type,ours->items[j].id)!=0) return cvc_fail(CVC_ERR,"oom");
            }
        }
    }
    snap_sort(out);
    return CVC_OK;
}

/* Does branch ref (by name) equal the given id? Returns 1 equal, 0 not, -1 err. */
static int ref_equals(const Repo *repo, const char *branch, const uint8_t id[32]){
    uint8_t cur[32]; int born=0;
    if(repo_read_branch(repo,branch,cur,&born)!=0) return -1;
    if(!born) return 0;
    return memcmp(cur,id,32)==0;
}

/* Build the final merge tree snapshot: provisional leaves + each resolved
 * conflict root's recorded resolution leaves. Paths under a resolved root are
 * replaced by the resolution. */
static CvcStatus merge_tree_snapshot(const MergeState *ms, Snapshot *out){
    snap_init(out);
    size_t i,j;
    for(i=0;i<ms->provisional.len;i++){
        if(snap_add(out,ms->provisional.items[i].path,ms->provisional.items[i].type,ms->provisional.items[i].id)!=0)
            return cvc_fail(CVC_ERR,"oom");
    }
    for(i=0;i<ms->n_conflicts;i++){
        const ConflictEntry *c=&ms->conflicts[i];
        size_t rl=strlen(c->path);
        for(j=0;j<out->len;){
            const char *p=out->items[j].path;
            if(strcmp(p,c->path)==0 || (rl>0 && strncmp(p,c->path,rl)==0 && p[c->path[rl]?rl:0]=='/' && (rl>0&&strncmp(p,c->path,rl)==0&&p[rl]=='/'))){
                free(out->items[j].path);
                out->items[j]=out->items[out->len-1];
                out->len--;
            } else j++;
        }
        if(!c->has_resolution) continue; /* still unresolved (shouldn't happen here) */
        for(j=0;j<c->resolution.len;j++){
            if(snap_add(out,c->resolution.items[j].path,c->resolution.items[j].type,c->resolution.items[j].id)!=0)
                return cvc_fail(CVC_ERR,"oom");
        }
    }
    snap_sort(out);
    return CVC_OK;
}

/* Scan the working tree ignoring the repository tracking filters (select all
 * eligible paths), used for resolve/continue verification of merge-controlled
 * paths. Writes objects (for_write=1). */
static CvcStatus scan_all(Repo *repo, ScanResult *r){
    StrVec save_inc; strvec_init(&save_inc); size_t i;
    for(i=0;i<repo->cfg.tracking_include.len;i++) strvec_push_dup(&save_inc,repo->cfg.tracking_include.items[i]);
    StrVec save_exc; strvec_init(&save_exc);
    for(i=0;i<repo->cfg.tracking_exclude.len;i++) strvec_push_dup(&save_exc,repo->cfg.tracking_exclude.items[i]);
    strvec_free(&repo->cfg.tracking_include); strvec_init(&repo->cfg.tracking_include); strvec_push_dup(&repo->cfg.tracking_include,"**");
    strvec_free(&repo->cfg.tracking_exclude); strvec_init(&repo->cfg.tracking_exclude);
    CvcStatus st=scan_snapshot(repo,r,1);
    strvec_free(&repo->cfg.tracking_include); strvec_free(&repo->cfg.tracking_exclude);
    repo->cfg.tracking_include=save_inc; repo->cfg.tracking_exclude=save_exc;
    return st;
}

/* Does the on-disk working tree at `root` (exact or subtree) match the given
 * snapshot? Used to verify a resolution or a frozen projection. Returns 1
 * match, 0 mismatch, -1 error. */
static int wt_matches_snapshot(Repo *repo, const Snapshot *snap){
    ScanResult scan; scan_init(&scan);
    CvcStatus st=scan_all(repo,&scan);
    if(st!=CVC_OK){ scan_free(&scan); return -1; }
    Snapshot wt; snap_init(&wt);
    for(size_t i=0;i<scan.len;i++){
        if(snap_add(&wt,scan.items[i].path,scan.items[i].type,scan.items[i].id)!=0){ scan_free(&scan); snap_free(&wt); return -1; }
    }
    scan_free(&scan);
    DiffList dl; diff_list_init(&dl);
    st=snap_compare(snap,&wt,&dl);
    int same=(st==CVC_OK && dl.len==0);
    diff_list_free(&dl); snap_free(&wt);
    if(st!=CVC_OK) return -1;
    return same;
}

/* Resolve the current working-tree state at a conflict root into a Snapshot
 * of eligible leaves. Returns CVC_OK and sets *out (empty snapshot = the user
 * deleted the path). On ineligible captured file, returns an error. */
static CvcStatus resolve_capture(Repo *repo, const char *root, Snapshot *out){
    /* If the path does not exist at all on disk, resolution is "delete" (empty). */
    uint16_t *abs=w_repo_to_abs(repo->root16,root);
    if(!abs) return cvc_fail(CVC_ERR,"oom");
    WStat s;
    int present=(w_stat(abs,&s)==0 && s.exists);
    free(abs);
    if(!present){ snap_init(out); return CVC_OK; }
    /* Scan all eligible paths under root (ignoring repo filters) and capture. */
    ScanResult scan; scan_init(&scan);
    CvcStatus st=scan_all(repo,&scan);
    if(st!=CVC_OK){ scan_free(&scan); return st; }
    /* The scan ignores ineligible files silently (counts them). If any
       ineligible regular file lies under root, reject the resolution. We
       detect by re-walking root for an ineligible file: if scan ignored any
       binary and the root is present, we conservatively reject unless the
       root is an exact leaf. For a single-file root, compare directly. */
    snap_init(out);
    size_t rl=strlen(root);
    int captured=0;
    for(size_t i=0;i<scan.len;i++){
        const char *p=scan.items[i].path;
        if(strcmp(p,root)==0 || (rl>0 && strncmp(p,root,rl)==0 && p[rl]=='/')){
            if(snap_add(out,p,scan.items[i].type,scan.items[i].id)!=0){ scan_free(&scan); snap_free(out); return cvc_fail(CVC_ERR,"oom"); }
            captured=1;
        }
    }
    scan_free(&scan);
    /* If root is a present regular file but not captured (ineligible), reject. */
    if(!captured){
        /* root is either a directory containing only ineligible files, or an
           ineligible file, or absent. If it is present as a regular file it is
           ineligible -> reject. */
        WStat s2; uint16_t *a2=w_repo_to_abs(repo->root16,root);
        if(a2){ int pr=(w_stat(a2,&s2)==0 && s2.exists); free(a2); if(pr){ snap_free(out); return cvc_fail(CVC_ERR,"resolution contains ineligible file"); } }
        /* present empty directory -> empty resolution (delete contents) */
    }
    snap_sort(out);
    return CVC_OK;
}

/* Materialize the final merge tree and move the current ref to `cid`.
 * Assumes preflight already done by caller. On failure restores old tree. */
static CvcStatus merge_commit_and_move(Repo *repo, const char *branch,
                                       const Snapshot *current,
                                       const Snapshot *final_snap,
                                       const uint8_t cid[32]){
    CvcStatus st=mat_preflight(repo,current,final_snap,1);
    if(st!=CVC_OK) return st;
    st=mat_materialize(repo,current,final_snap);
    if(st!=CVC_OK) return st;
    st=set_ref_born(repo,branch,cid);
    if(st!=CVC_OK){
        CvcStatus rb=mat_materialize(repo,final_snap,current);
        if(rb!=CVC_OK) return cvc_fail(CVC_ERR,"ref update failed; and failed to restore old tree");
        return cvc_fail(CVC_ERR,"ref update failed; restored old tree");
    }
    return CVC_OK;
}

/* Create the intended merge commit (parents orig+target) and write the
 * finalizing merge state. The commit is durable but unreachable until the ref
 * moves. */
static CvcStatus merge_enter_finalizing(Repo *repo, const MergeState *ms,
                                        const Snapshot *final_snap,
                                        uint8_t finalizing_id[32]){
    uint8_t root_tree[32];
    CvcStatus st=snap_build_tree(repo,final_snap,root_tree);
    if(st!=CVC_OK) return st;
    uint8_t parents[64]; memcpy(parents,ms->orig_commit,32); memcpy(parents+32,ms->target_commit,32);
    st=make_commit(repo,root_tree,parents,2,ms->message?ms->message:"",ms->message?strlen(ms->message):0,finalizing_id);
    if(st!=CVC_OK) return st;
    /* write finalizing state durably (provisional + resolutions + phase + id) */
    MergeState fs; merge_state_init(&fs);
    fs.orig_branch=strdup(ms->orig_branch?ms->orig_branch:"");
    memcpy(fs.orig_commit,ms->orig_commit,32);
    fs.target_branch=strdup(ms->target_branch?ms->target_branch:"");
    memcpy(fs.target_commit,ms->target_commit,32);
    fs.message=strdup(ms->message?ms->message:"");
    for(size_t i=0;i<ms->provisional.len;i++)
        if(snap_add(&fs.provisional,ms->provisional.items[i].path,ms->provisional.items[i].type,ms->provisional.items[i].id)!=0){ merge_state_free(&fs); return cvc_fail(CVC_ERR,"oom"); }
    fs.n_conflicts=ms->n_conflicts;
    fs.conflicts=(ConflictEntry*)calloc(ms->n_conflicts?ms->n_conflicts:1,sizeof(ConflictEntry));
    if(!fs.conflicts){ merge_state_free(&fs); return cvc_fail(CVC_ERR,"oom"); }
    for(size_t i=0;i<ms->n_conflicts;i++){
        fs.conflicts[i].path=strdup(ms->conflicts[i].path);
        fs.conflicts[i].resolved=ms->conflicts[i].resolved;
        fs.conflicts[i].has_resolution=ms->conflicts[i].has_resolution;
        snap_init(&fs.conflicts[i].resolution);
        for(size_t j=0;j<ms->conflicts[i].resolution.len;j++)
            if(snap_add(&fs.conflicts[i].resolution,ms->conflicts[i].resolution.items[j].path,ms->conflicts[i].resolution.items[j].type,ms->conflicts[i].resolution.items[j].id)!=0){ merge_state_free(&fs); return cvc_fail(CVC_ERR,"oom"); }
    }
    fs.phase=MERGE_PHASE_FINALIZING;
    fs.finalizing_has_id=1;
    memcpy(fs.finalizing_commit,finalizing_id,32);
    st=merge_state_save(repo,&fs);
    merge_state_free(&fs);
    return st;
}

/* Verify the frozen merge-controlled working-tree projection for a finalizing
 * retry: the on-disk tree must exactly match `final_snap` (provisional +
 * resolutions). Returns 1 match, 0 mismatch, -1 error. */
static int frozen_projection_ok(Repo *repo, const Snapshot *final_snap){
    return wt_matches_snapshot(repo,final_snap);
}

/* --- merge <branch> --- */
static CvcStatus merge_cmd_branch(Repo *repo, RepoLock *lk, const char *target_branch, const char *msg_opt){
    /* No active merge state (caller checked). */
    char *cur_branch=NULL;
    if(repo_current_branch(repo,&cur_branch)!=0) return cvc_fail(CVC_ERR,"cannot read HEAD");
    if(strcmp(cur_branch,target_branch)==0){
        free(cur_branch); repo_unlock(lk); repo_free(repo);
        print_utf8("already on branch; nothing to merge\n");
        return CVC_OK;
    }
    if(!repo_branch_exists(repo,target_branch)){
        free(cur_branch); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR,"branch does not exist");
    }
    uint8_t head[32]; int hborn=head_commit(repo,head);
    uint8_t target[32]; int tborn=0;
    repo_read_branch(repo,target_branch,target,&tborn);

    if(!tborn){
        /* target unborn */
        if(!hborn){ free(cur_branch); repo_unlock(lk); repo_free(repo); print_utf8("no commits to merge\n"); return CVC_OK; }
        free(cur_branch); repo_unlock(lk); repo_free(repo);
        print_utf8("no commits to merge\n");
        return CVC_OK;
    }
    if(!hborn){
        /* current unborn, target born: fast-forward from empty history */
        Snapshot tgt; snap_init(&tgt);
        CvcStatus st=snap_from_commit(repo,target,&tgt);
        if(st!=CVC_OK){ snap_free(&tgt); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
        Snapshot empty; snap_init(&empty);
        st=mat_preflight(repo,&empty,&tgt,1);
        if(st==CVC_OK) st=mat_materialize(repo,&empty,&tgt);
        snap_free(&tgt); snap_free(&empty);
        free(cur_branch);
        if(st==CVC_OK) st=set_ref_born(repo,cur_branch,target);
        repo_unlock(lk); repo_free(repo);
        return st;
    }
    /* both born */
    Snapshot cur_snap; snap_init(&cur_snap);
    CvcStatus st=snap_from_commit(repo,head,&cur_snap);
    if(st!=CVC_OK){ snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
    int dirty=wt_differs(repo,&cur_snap);
    if(dirty>0){ snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"working tree has uncommitted changes; cannot merge"); }
    if(dirty<0){ snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"cannot scan working tree"); }
    if(cvc_is_ancestor(repo,target,head)){ /* target ancestor of head: up to date */
        snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo);
        print_utf8("already up to date\n");
        return CVC_OK;
    }
    if(cvc_is_ancestor(repo,head,target)){ /* fast-forward */
        Snapshot tgt; snap_init(&tgt);
        st=snap_from_commit(repo,target,&tgt);
        if(st!=CVC_OK){ snap_free(&tgt); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
        st=mat_preflight(repo,&cur_snap,&tgt,1);
        if(st==CVC_OK) st=mat_materialize(repo,&cur_snap,&tgt);
        snap_free(&tgt);
        if(st==CVC_OK) st=set_ref_born(repo,cur_branch,target);
        snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo);
        if(st!=CVC_OK) return st;
        char hex[65]; id_hex(target,hex);
        out_fmt("fast-forwarded to %s\n",hex);
        return CVC_OK;
    }
    /* divergent */
    uint8_t base[32];
    int bc=cvc_merge_base(repo,head,target,base);
    if(bc==1){ snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"unrelated histories; merge rejected"); }
    if(bc<0){ snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"merge base error"); }
    ThreeWayResult tw; threeway_result_init(&tw);
    st=cvc_merge_threeway(repo,base,head,target,&tw);
    if(st!=CVC_OK){ threeway_result_free(&tw); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
    if(tw.case_collision){
        char msg[2048];
        snprintf(msg,sizeof msg,"platform namespace collision: %s and %s",tw.collision_path_a,tw.collision_path_b);
        threeway_result_free(&tw); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR,"%s",msg);
    }

    /* default message */
    char defmsg[160];
    if(!msg_opt){ snprintf(defmsg,sizeof defmsg,"Merge branch '%s'",target_branch); msg_opt=defmsg; }

    if(tw.conflicts.len==0){
        /* clean divergent merge */
        uint8_t root_tree[32];
        st=snap_build_tree(repo,&tw.provisional,root_tree);
        if(st!=CVC_OK){ threeway_result_free(&tw); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
        uint8_t parents[64]; memcpy(parents,head,32); memcpy(parents+32,target,32);
        uint8_t cid[32];
        st=make_commit(repo,root_tree,parents,2,msg_opt,strlen(msg_opt),cid);
        if(st!=CVC_OK){ threeway_result_free(&tw); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
        st=merge_commit_and_move(repo,cur_branch,&cur_snap,&tw.provisional,cid);
        snap_free(&cur_snap); free(cur_branch);
        if(st!=CVC_OK){ threeway_result_free(&tw); repo_unlock(lk); repo_free(repo); return st; }
        char hex[65]; id_hex(cid,hex);
        out_fmt("merged %s\n",hex);
        threeway_result_free(&tw);
        repo_unlock(lk); repo_free(repo);
        return CVC_OK;
    }

    /* conflicts: persist merge state + materialize WT (provisional + ours at
       conflict roots) + write text-conflict markers. */
    MergeState ms; merge_state_init(&ms);
    ms.orig_branch=strdup(cur_branch);
    memcpy(ms.orig_commit,head,32);
    ms.target_branch=strdup(target_branch);
    memcpy(ms.target_commit,target,32);
    ms.message=strdup(msg_opt);
    for(size_t i=0;i<tw.provisional.len;i++)
        if(snap_add(&ms.provisional,tw.provisional.items[i].path,tw.provisional.items[i].type,tw.provisional.items[i].id)!=0){ threeway_result_free(&tw); merge_state_free(&ms); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"oom"); }
    ms.n_conflicts=tw.conflicts.len;
    ms.conflicts=(ConflictEntry*)calloc(tw.conflicts.len?tw.conflicts.len:1,sizeof(ConflictEntry));
    if(!ms.conflicts){ threeway_result_free(&tw); merge_state_free(&ms); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"oom"); }
    for(size_t i=0;i<tw.conflicts.len;i++){
        ms.conflicts[i].path=strdup(tw.conflicts.items[i]);
        ms.conflicts[i].resolved=0;
        ms.conflicts[i].has_resolution=0;
        snap_init(&ms.conflicts[i].resolution);
    }
    ms.phase=MERGE_PHASE_CONFLICT;
    ms.finalizing_has_id=0;

    /* Materialize WT to provisional + ours-at-conflicts. */
    Snapshot wt_target; snap_init(&wt_target);
    st=build_conflict_wt(&ms.provisional,ms.conflicts,ms.n_conflicts,&cur_snap,&wt_target);
    if(st!=CVC_OK){ snap_free(&wt_target); threeway_result_free(&tw); merge_state_free(&ms); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
    st=mat_preflight(repo,&cur_snap,&wt_target,1);
    if(st==CVC_OK) st=mat_materialize(repo,&cur_snap,&wt_target);
    snap_free(&wt_target);
    if(st!=CVC_OK){ threeway_result_free(&tw); merge_state_free(&ms); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }

    /* Write text-conflict markers (from textual list). */
    for(size_t i=0;i<tw.n_textual;i++){
        TextConflict *tc=&tw.textual[i];
        char olab[160], tlab[160];
        snprintf(olab,sizeof olab,"%s",ms.orig_branch);
        snprintf(tlab,sizeof tlab,"%s",ms.target_branch);
        st=conflict_markers_write(repo,tc->path,tc->ours_id,tc->theirs_id,olab,tlab);
        if(st!=CVC_OK){ threeway_result_free(&tw); merge_state_free(&ms); snap_free(&cur_snap); free(cur_branch); repo_unlock(lk); repo_free(repo); return st; }
    }

    st=merge_state_save(repo,&ms);
    merge_state_free(&ms);
    snap_free(&cur_snap); free(cur_branch);
    if(st!=CVC_OK){ threeway_result_free(&tw); repo_unlock(lk); repo_free(repo); return st; }
    out_fmt("merge conflicts; resolve them then run 'cvc merge --continue'\n");
    threeway_result_free(&tw);
    repo_unlock(lk); repo_free(repo);
    return cvc_fail(CVC_ERR,"merge has conflicts");
}

/* --- merge --continue --- */
static CvcStatus merge_cmd_continue(Repo *repo, RepoLock *lk, const char *msg_opt){
    MergeState ms; merge_state_init(&ms);
    CvcStatus st=merge_state_load(repo,&ms);
    if(st!=CVC_OK){ repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR_NOMERGE,"no merge in progress"); }
    /* verify HEAD still names recorded branch */
    char *cur=NULL;
    if(repo_current_branch(repo,&cur)!=0 || strcmp(cur,ms.orig_branch)!=0){
        free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR,"merge state does not match current branch");
    }
    uint8_t head[32]; int hborn=head_commit(repo,head);
    if(hborn!=1){ free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"merge state corrupt: current branch unborn"); }

    if(ms.phase==MERGE_PHASE_FINALIZING){
        /* retry or cleanup */
        int rf=ref_equals(repo,ms.orig_branch,ms.orig_commit);
        int ri=ref_equals(repo,ms.orig_branch,ms.finalizing_commit);
        if(rf<0||ri<0){ free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"cannot read branch ref"); }
        if(ri==1){
            /* already completed: cleanup stale finalizing state */
            merge_state_remove(repo);
            free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
            print_utf8("merge already completed\n");
            return CVC_OK;
        }
        if(rf!=1){
            free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
            return cvc_fail(CVC_ERR,"unexpected branch ref during finalizing; integrity error");
        }
        if(msg_opt){
            free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
            return cvc_fail(CVC_ERR_USAGE,"-m is not accepted during finalizing retry");
        }
        /* build final snapshot from state */
        Snapshot final_snap; snap_init(&final_snap);
        st=merge_tree_snapshot(&ms,&final_snap);
        if(st!=CVC_OK){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
        /* verify intended commit object still valid */
        ObjectData od; int okc=(obj_read(repo,ms.finalizing_commit,&od)==CVC_OK && od.type=='c');
        if(okc) object_free(&od);
        if(!okc){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR_INTEGRITY,"intended merge commit missing or invalid"); }
        /* verify frozen projection */
        int proj=frozen_projection_ok(repo,&final_snap);
        if(proj<0){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"cannot scan working tree"); }
        if(proj==0){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"merge-controlled working tree changed; restore it or abort"); }
        /* move ref to intended commit */
        st=set_ref_born(repo,ms.orig_branch,ms.finalizing_commit);
        if(st!=CVC_OK){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
        merge_state_remove(repo);
        snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
        print_utf8("merge finalized\n");
        return CVC_OK;
    }

    /* conflict phase: require all resolved */
    int all_resolved=1;
    for(size_t i=0;i<ms.n_conflicts;i++) if(!ms.conflicts[i].has_resolution){ all_resolved=0; break; }
    if(!all_resolved){ free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"not all conflicts resolved"); }
    /* ref must still equal orig */
    int rf=ref_equals(repo,ms.orig_branch,ms.orig_commit);
    if(rf!=1){ free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"merge state does not match recorded original commit"); }
    /* build final snapshot */
    Snapshot final_snap; snap_init(&final_snap);
    st=merge_tree_snapshot(&ms,&final_snap);
    if(st!=CVC_OK){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    /* verify WT equals final snapshot (provisional + resolutions) */
    int ok=wt_matches_snapshot(repo,&final_snap);
    if(ok<0){ snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"cannot scan working tree"); }
    if(ok==0){
        snap_free(&final_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR,"working tree does not match recorded resolution; run 'cvc resolve' again or abort");
    }
    /* enter finalizing: create commit + write finalizing state */
    uint8_t fid[32];
    st=merge_enter_finalizing(repo,&ms,&final_snap,fid);
    snap_free(&final_snap);
    if(st!=CVC_OK){ free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    /* re-load finalizing state (it now has fid) and move ref */
    MergeState fs; merge_state_init(&fs);
    if(merge_state_load(repo,&fs)!=CVC_OK){ free(cur); merge_state_free(&ms); merge_state_free(&fs); repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR,"cannot re-read merge state"); }
    st=set_ref_born(repo,fs.orig_branch,fs.finalizing_commit);
    if(st!=CVC_OK){ merge_state_free(&fs); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    merge_state_remove(repo);
    merge_state_free(&fs);
    free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
    char hex[65]; id_hex(fid,hex);
    out_fmt("merged %s\n",hex);
    return CVC_OK;
}

/* --- merge --abort --- */
static CvcStatus merge_cmd_abort(Repo *repo, RepoLock *lk){
    MergeState ms; merge_state_init(&ms);
    CvcStatus st=merge_state_load(repo,&ms);
    if(st!=CVC_OK){ repo_unlock(lk); repo_free(repo); return cvc_fail(CVC_ERR_NOMERGE,"no merge in progress"); }
    char *cur=NULL;
    if(repo_current_branch(repo,&cur)!=0 || strcmp(cur,ms.orig_branch)!=0){
        free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR,"merge state does not match current branch");
    }
    /* If finalizing and ref already at intended commit: cleanup only, no rollback. */
    int ri=ref_equals(repo,ms.orig_branch,ms.finalizing_commit);
    if(ms.phase==MERGE_PHASE_FINALIZING && ri==1){
        merge_state_remove(repo);
        free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
        return cvc_fail(CVC_ERR_NOMERGE,"no active merge to abort");
    }
    /* Restore pre-merge HEAD tracked snapshot. */
    uint8_t head[32]; int hborn=head_commit(repo,head);
    Snapshot orig_snap; snap_init(&orig_snap);
    if(hborn==1){
        st=snap_from_commit(repo,ms.orig_commit,&orig_snap);
        if(st!=CVC_OK){ snap_free(&orig_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    }
    Snapshot merged_snap; snap_init(&merged_snap);
    st=build_conflict_wt(&ms.provisional,ms.conflicts,ms.n_conflicts,&orig_snap,&merged_snap);
    if(st!=CVC_OK){ snap_free(&orig_snap); snap_free(&merged_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    st=mat_preflight(repo,&merged_snap,&orig_snap,1);
    if(st==CVC_OK) st=mat_materialize(repo,&merged_snap,&orig_snap);
    snap_free(&merged_snap);
    if(st!=CVC_OK){ snap_free(&orig_snap); free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo); return st; }
    snap_free(&orig_snap);
    st=merge_state_remove(repo);
    free(cur); merge_state_free(&ms); repo_unlock(lk); repo_free(repo);
    if(st!=CVC_OK) return st;
    print_utf8("merge aborted\n");
    return CVC_OK;
}

CvcStatus cmd_merge_dispatch(char **av, int n){
    /* forms: merge <branch> [-m msg] | merge --continue [-m msg] | merge --abort */
    int cont=0, abort=0;
    char *branch=NULL, *msg=NULL; int have_msg=0, seen_m=0, seen_cont=0, seen_abort=0;
    for(int i=0;i<n;i++){
        if(strcmp(av[i],"--continue")==0){
            if(seen_cont) return cvc_fail(CVC_ERR_USAGE,"duplicate --continue");
            seen_cont=1; cont=1; continue;
        }
        if(strcmp(av[i],"--abort")==0){
            if(seen_abort) return cvc_fail(CVC_ERR_USAGE,"duplicate --abort");
            seen_abort=1; abort=1; continue;
        }
        if(strcmp(av[i],"-m")==0){
            if(seen_m) return cvc_fail(CVC_ERR_USAGE,"duplicate -m");
            if(i+1>=n) return cvc_fail(CVC_ERR_USAGE,"-m requires an argument");
            seen_m=1; msg=av[++i]; have_msg=1;
            if(msg[0]=='\0') return cvc_fail(CVC_ERR_USAGE,"message must be nonempty");
            if(!utf8_valid_no_nul((const uint8_t*)msg,strlen(msg))) return cvc_fail(CVC_ERR_USAGE,"message not valid UTF-8");
            continue;
        }
        if(av[i][0]=='-' && av[i][1]!='-') return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
        if(av[i][0]=='-') return cvc_fail(CVC_ERR_USAGE,"unknown option %s",av[i]);
        if(!branch){ branch=av[i]; continue; }
        return cvc_fail(CVC_ERR_USAGE,"too many arguments");
    }
    if(abort && cont) return cvc_fail(CVC_ERR_USAGE,"cannot use --continue and --abort together");
    if(cont && branch) return cvc_fail(CVC_ERR_USAGE,"--continue takes no branch");
    if(cont && abort) return cvc_fail(CVC_ERR_USAGE,"cannot use --continue and --abort together");
    if(abort && have_msg) return cvc_fail(CVC_ERR_USAGE,"--abort takes no -m");
    if(!cont && !abort && !branch) return cvc_fail(CVC_ERR_USAGE,"usage: cvc merge <branch> [-m <message>] | cvc merge --continue [-m <message>] | cvc merge --abort");
    if(cont && branch) return cvc_fail(CVC_ERR_USAGE,"--continue takes no branch");

    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;

    if(abort) return merge_cmd_abort(&repo,&lk);
    if(cont) return merge_cmd_continue(&repo,&lk, have_msg?msg:NULL);
    /* branch form: no active merge state allowed */
    MergeState ms; merge_state_init(&ms);
    int has_merge=(merge_state_load(&repo,&ms)==CVC_OK);
    if(has_merge){
        /* stale-completed finalizing cleanup before new merge */
        if(ms.phase==MERGE_PHASE_FINALIZING){
            int ri=ref_equals(&repo,ms.orig_branch,ms.finalizing_commit);
            if(ri==1){ merge_state_remove(&repo); has_merge=0; }
        }
        if(has_merge){
            merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
            return cvc_fail(CVC_ERR_MERGEDIRTY,"a merge is already in progress");
        }
    }
    merge_state_free(&ms);
    st=merge_cmd_branch(&repo,&lk,branch,have_msg?msg:NULL);
    /* merge_cmd_branch already unlocks+freeps repo */
    return st;
}

/* ================================================================== */
/*  Command: resolve                                                   */
/* ================================================================== */
CvcStatus cmd_resolve_dispatch(char **av, int n){
    if(n!=1) return cvc_fail(CVC_ERR_USAGE,"usage: cvc resolve <path>");
    const char *path=av[0];
    if(path_operand_valid(path)!=0) return cvc_fail(CVC_ERR_USAGE,"invalid path operand");

    Repo repo; RepoLock lk;
    CvcStatus st=open_repo(&repo,&lk,1);
    if(st!=CVC_OK) return st;
    MergeState ms; merge_state_init(&ms);
    st=merge_state_load(&repo,&ms);
    if(st!=CVC_OK){ repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR_NOMERGE,"no merge in progress"); }
    if(ms.phase==MERGE_PHASE_FINALIZING){
        merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
        return cvc_fail(CVC_ERR,"resolve is forbidden during merge finalizing");
    }
    /* path must be an exact recorded conflict root */
    long ci=-1;
    for(size_t i=0;i<ms.n_conflicts;i++){
        if(strcmp(ms.conflicts[i].path,path)==0){ ci=(long)i; break; }
    }
    if(ci<0){
        merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
        return cvc_fail(CVC_ERR,"path is not a conflict root");
    }
    /* verify HEAD still matches recorded branch/commit */
    char *cur=NULL;
    if(repo_current_branch(&repo,&cur)!=0 || strcmp(cur,ms.orig_branch)!=0){
        free(cur); merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
        return cvc_fail(CVC_ERR,"merge state does not match current branch");
    }
    int rf=ref_equals(&repo,ms.orig_branch,ms.orig_commit);
    free(cur);
    if(rf!=1){ merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo); return cvc_fail(CVC_ERR,"merge state does not match recorded original commit"); }
    /* capture current WT state at conflict root */
    Snapshot cap; snap_init(&cap);
    st=resolve_capture(&repo,path,&cap);
    if(st!=CVC_OK){ snap_free(&cap); merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo); return st; }
    /* record resolution */
    snap_free(&ms.conflicts[ci].resolution);
    ms.conflicts[ci].resolution=cap; /* transfer ownership */
    ms.conflicts[ci].has_resolution=1;
    ms.conflicts[ci].resolved=1;
    st=merge_state_save(&repo,&ms);
    merge_state_free(&ms); repo_unlock(&lk); repo_free(&repo);
    if(st!=CVC_OK) return st;
    out_fmt("resolved %s\n",path);
    return CVC_OK;
}

/* ================================================================== */
/*  Forward decls (implemented in other modules)                       */
/* ================================================================== */
CvcStatus cmd_save_dispatch(char **av, int n);CvcStatus cmd_diff_dispatch(char **av, int n);
CvcStatus cmd_branch_dispatch(char **av, int n);
CvcStatus cmd_switch_dispatch(char **av, int n);
CvcStatus cmd_restore_dispatch(char **av, int n);
CvcStatus cmd_rollback_dispatch(char **av, int n);
CvcStatus cmd_verify_dispatch(char **av, int n);
CvcStatus cmd_merge_dispatch(char **av, int n);
CvcStatus cmd_resolve_dispatch(char **av, int n);

/* ================================================================== */
/*  Command dispatcher                                                  */
/* ================================================================== */

int cvc_main(int argc, wchar_t **wargv){
    char **av=NULL;
    if(convert_argv(argc,wargv,&av)!=0){
        err_fmt("cvc: invalid UTF-8 command line\n");
        return 1;
    }
    if(argc<2){
        print_utf8(usage_text());
        free_argv(av);
        return 1;
    }
    const char *cmd=av[1];
    char **rest=&av[2];
    int nrest=argc-2;

    CvcStatus st;
    if(strcmp(cmd,"help")==0 || strcmp(cmd,"--help")==0 || strcmp(cmd,"-h")==0){
        st=cmd_help();
    } else if(strcmp(cmd,"init")==0){
        st=cmd_init(rest,nrest);
    } else if(strcmp(cmd,"config")==0){
        st=cmd_config(rest,nrest);
    } else if(strcmp(cmd,"status")==0){
        st=cmd_status(rest,nrest);
    } else if(strcmp(cmd,"log")==0){
        st=cmd_log(rest,nrest);
    } else if(strcmp(cmd,"save")==0){
        st=cmd_save_dispatch(rest,nrest);
    } else if(strcmp(cmd,"diff")==0){
        st=cmd_diff_dispatch(rest,nrest);
    } else if(strcmp(cmd,"branch")==0){
        st=cmd_branch_dispatch(rest,nrest);
    } else if(strcmp(cmd,"switch")==0){
        st=cmd_switch_dispatch(rest,nrest);
    } else if(strcmp(cmd,"restore")==0){
        st=cmd_restore_dispatch(rest,nrest);
    } else if(strcmp(cmd,"rollback")==0){
        st=cmd_rollback_dispatch(rest,nrest);
    } else if(strcmp(cmd,"merge")==0){
        st=cmd_merge_dispatch(rest,nrest);
    } else if(strcmp(cmd,"resolve")==0){
        st=cmd_resolve_dispatch(rest,nrest);
    } else if(strcmp(cmd,"verify")==0){
        st=cmd_verify_dispatch(rest,nrest);
    } else {
        err_fmt("cvc: unknown command \"%s\"\n",cmd);
        err_fmt("%s", usage_text());
        free_argv(av);
        return 1;
    }
    free_argv(av);
    if(st!=CVC_OK){
        if(cvc_errbuf[0]) err_fmt("cvc: %s\n", cvc_errbuf);
        return (int)st;
    }
    return 0;
}
