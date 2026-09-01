#include "materialize.h"
#include "win32.h"
#include "utf8.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Preflight                                                            */
/* ------------------------------------------------------------------ */

/* Determine if a path has an existing working-tree entry and whether it is
 * "tracked" (part of `current`). Returns via out params. */
static int check_existing(Repo *repo, const char *path, const Snapshot *current,
                          int *exists, int *is_tracked, int *is_dir){
    *exists=0; *is_tracked=0; *is_dir=0;
    long idx = snap_find(current, path);
    if(idx>=0) *is_tracked=1;
    uint16_t *abs=w_repo_to_abs(repo->root16, path);
    if(!abs) return -1;
    WStat st;
    if(w_stat(abs,&st)==0 && st.exists){
        *exists=1;
        *is_dir=st.is_dir;
    }
    free(abs);
    /* A directory that only serves as a container for tracked leaves is
     * "effectively tracked": e.g. switching from a branch where `path` is a
     * directory (holding tracked descendants) to a branch where `path` is a
     * regular file must be allowed to replace that directory. */
    if(!*is_tracked && *is_dir && current->len>0){
        size_t plen = strlen(path);
        /* build "<path>/" */
        char *pref=(char*)malloc(plen+2);
        if(!pref) return -1;
        memcpy(pref,path,plen); pref[plen]='/'; pref[plen+1]=0;
        long lo=0, hi=(long)current->len-1;
        /* binary search for first leaf >= pref, then check prefix */
        long first=-1;
        while(lo<=hi){
            long mid=(lo+hi)/2;
            int c=strcmp(current->items[mid].path, pref);
            if(c<0) lo=mid+1; else { first=mid; hi=mid-1; }
        }
        if(first>=0 && first<(long)current->len && strncmp(current->items[first].path,pref,plen+1)==0)
            *is_tracked=1;
        free(pref);
    }
    return 0;
}

CvcStatus mat_preflight(Repo *repo, const Snapshot *current, const Snapshot *target,
                        int replace_tracked){
    /* For each target path, check the existing entry. */
    size_t i;
    for(i=0;i<target->len;i++){
        const char *path = target->items[i].path;
        int exists, is_tracked, is_dir;
        if(check_existing(repo,path,current,&exists,&is_tracked,&is_dir)!=0)
            return cvc_fail(CVC_ERR,"oom");
        if(!exists) continue;
        if(is_tracked && replace_tracked){
            /* allowed to replace tracked state (switch/rollback/merge) */
            continue;
        }
        if(is_tracked && !replace_tracked){
            /* restore: exact tracked path may be replaced */
            continue;
        }
        /* untracked existing entry at target path -> collision */
        uint8_t ttype = target->items[i].type;
        int t_is_dir = (ttype==OBJ_TREE_SUBTREE);
        if(t_is_dir){
            /* target needs directory container; untracked directory allowed,
               but we must check descendants individually. If untracked file,
               collision. */
            if(!is_dir){
                return cvc_fail(CVC_ERR,"collision: untracked entry at %s", path);
            }
            /* untracked dir container is allowed; descendants checked below */
            continue;
        } else {
            /* target needs file/symlink; existing untracked dir or file
               (non-tracked) is a collision */
            if(is_dir){
                return cvc_fail(CVC_ERR,"collision: untracked directory at %s", path);
            }
            /* untracked regular file or unsupported reparse -> collision */
            return cvc_fail(CVC_ERR,"collision: untracked file at %s", path);
        }
    }
    /* Check that removing tracked paths does not require deleting unrelated
     * untracked descendants. This is inherently handled by only deleting
     * exact tracked leaves; directory pruning below preserves untracked. */
    return CVC_OK;
}

/* ------------------------------------------------------------------ */
/* Materialization                                                      */
/* ------------------------------------------------------------------ */

/* A rollback journal entry: records how to undo an operation. */
typedef enum { RB_CREATE, RB_DELETE, RB_REPLACE } RBKind;
typedef struct {
    RBKind kind;
    char *path;         /* repo-relative */
    /* for RB_REPLACE: saved old content (bytes) or old symlink target/kind */
    Bytes old_bytes;
    uint8_t old_type;   /* 0 none, 1 blob, 2 file symlink, 4 dir symlink */
    int applied;
} RBEntry;

typedef struct {
    RBEntry *items;
    size_t len, cap;
} Rollback;

static int rb_add(Rollback *rb, RBKind kind, const char *path){
    if(rb->len==rb->cap){
        size_t nc=rb->cap?rb->cap*2:16;
        RBEntry *n=(RBEntry*)realloc(rb->items,nc*sizeof(RBEntry));
        if(!n) return -1;
        rb->items=n; rb->cap=nc;
    }
    RBEntry *e=&rb->items[rb->len];
    memset(e,0,sizeof *e);
    e->kind=kind;
    e->path=strdup(path);
    bytes_init(&e->old_bytes);
    if(!e->path) return -1;
    rb->len++;
    return 0;
}
static void rb_free(Rollback *rb){
    size_t i; for(i=0;i<rb->len;i++){ free(rb->items[i].path); bytes_free(&rb->items[i].old_bytes); }
    free(rb->items);
    memset(rb,0,sizeof *rb);
}

/* Read a working-tree entry into a generic representation for backup. */
static CvcStatus read_wt_entry(Repo *repo, const char *path, Bytes *content,
                               uint8_t *type, int *exists){
    *exists=0; *type=0; bytes_init(content);
    uint16_t *abs=w_repo_to_abs(repo->root16,path);
    if(!abs) return cvc_fail(CVC_ERR,"oom");
    WStat st;
    if(w_stat(abs,&st)!=0 || !st.exists){ free(abs); return CVC_OK; }
    *exists=1;
    if(st.is_symlink){
        char *print=NULL; int is_dir=0;
        if(w_symlink_read(abs,&print,&is_dir)==0){
            bytes_append(content,print,strlen(print));
            *type = is_dir ? OBJ_TREE_DIR_SYMLINK : OBJ_TREE_FILE_SYMLINK;
            free(print);
        } else {
            *type = st.is_dir ? OBJ_TREE_DIR_SYMLINK : OBJ_TREE_FILE_SYMLINK;
        }
    } else if(st.is_dir){
        *type = OBJ_TREE_SUBTREE;
    } else {
        *type = OBJ_TREE_BLOB;
        CvcStatus s=w_read_file(abs,content);
        free(abs);
        return s;
    }
    free(abs);
    return CVC_OK;
}

/* Ensure a directory container exists at `path` (and parents), reusing
 * existing untracked dirs, creating as needed. */
static CvcStatus ensure_dir(Repo *repo, const char *path){
    /* create ancestors */
    char *copy=strdup(path);
    if(!copy) return cvc_fail(CVC_ERR,"oom");
    /* build progressively */
    /* We'll iterate over '/'-separated components creating dirs. */
    char *buf=(char*)malloc(strlen(path)+1);
    if(!buf){ free(copy); return cvc_fail(CVC_ERR,"oom"); }
    size_t bl=0;
    const char *start=copy;
    while(1){
        const char *slash=strchr(start,'/');
        size_t seglen = slash? (size_t)(slash-start) : strlen(start);
        if(bl>0) buf[bl++]='/';
        memcpy(buf+bl,start,seglen); bl+=seglen; buf[bl]=0;
        /* ensure buf exists as dir */
        uint16_t *abs=w_repo_to_abs(repo->root16,buf);
        if(!abs){ free(copy); free(buf); return cvc_fail(CVC_ERR,"oom"); }
        WStat st;
        if(w_stat(abs,&st)==0 && st.exists){
            if(!st.is_dir || st.is_reparse){
                free(abs); free(copy); free(buf);
                return cvc_fail(CVC_ERR,"path component not a safe directory");
            }
        } else {
            if(w_mkdir(abs)!=0){
                /* may already exist as dir */
                free(abs); free(copy); free(buf);
                return cvc_fail(CVC_ERR,"cannot create directory");
            }
        }
        free(abs);
        if(!slash) break;
        start=slash+1;
    }
    free(copy); free(buf);
    return CVC_OK;
}

/* Write a leaf (blob or symlink) at path. Returns CVC_OK. */
static CvcStatus write_leaf(Repo *repo, const char *path, uint8_t type, const uint8_t id[32]){
    if(type==OBJ_TREE_BLOB){
        ObjectData od;
        CvcStatus st=obj_read(repo,id,&od);
        if(st!=CVC_OK || od.type!='b'){ object_free(&od); return cvc_fail(CVC_ERR,"missing blob"); }
        uint16_t *abs=w_repo_to_abs(repo->root16,path);
        if(!abs){ object_free(&od); return cvc_fail(CVC_ERR,"oom"); }
        int rc=w_write_file_atomic(abs, od.payload.data, od.payload.len);
        object_free(&od);
        free(abs);
        return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot write file");
    } else if(type==OBJ_TREE_FILE_SYMLINK || type==OBJ_TREE_DIR_SYMLINK){
        ObjectData od;
        CvcStatus st=obj_read(repo,id,&od);
        if(st!=CVC_OK || od.type!='s'){ object_free(&od); return cvc_fail(CVC_ERR,"missing symlink"); }
        uint16_t *abs=w_repo_to_abs(repo->root16,path);
        if(!abs){ object_free(&od); return cvc_fail(CVC_ERR,"oom"); }
        int is_dir = (type==OBJ_TREE_DIR_SYMLINK);
        /* If something exists there (e.g. a regular file), remove first. */
        if(w_exists(abs)){
            WStat st2;
            if(w_stat(abs,&st2)==0){
                if(st2.is_dir && !st2.is_reparse){
                    /* dir: cannot replace with symlink directly; remove dir if empty */
                    w_delete_path(abs,1);
                } else {
                    w_delete_path(abs, st2.is_dir);
                }
            }
        }
        char *print=(char*)malloc(od.payload.len+1);
        if(!print){ object_free(&od); free(abs); return cvc_fail(CVC_ERR,"oom"); }
        memcpy(print,od.payload.data,od.payload.len); print[od.payload.len]=0;
        int rc=w_symlink_create(abs, print, is_dir);
        free(print);
        object_free(&od);
        free(abs);
        return rc==0?CVC_OK:cvc_fail(CVC_ERR,"cannot create symbolic link");
    }
    return cvc_fail(CVC_ERR,"bad leaf type");
}

/* Recursively remove a directory `dir` if it is empty (no entries at all).
 * Returns CVC_OK (whether or not removed). Used to clear an empty directory
 * container so a target file/symlink can be written in its place after its
 * tracked contents were removed. Only truly-empty dirs are removed; any
 * remaining (untracked) content stops the prune.
 */
typedef struct { int has_any; int has_non_dir; } DirProbe;
static int prune_probe_cb(const uint16_t *abs_dir, const WDirEntry *e, void *ctx){
    (void)abs_dir;
    DirProbe *p=(DirProbe*)ctx;
    p->has_any=1;
    if(!e->is_dir) p->has_non_dir=1;
    return 0;
}
/* Remove `dir` if empty, then walk up removing now-empty ancestors (so a
 * target file can replace a whole former directory subtree). */
static CvcStatus prune_empty_dir(Repo *repo, const char *dir){
    char *cur=strdup(dir);
    if(!cur) return cvc_fail(CVC_ERR,"oom");
    CvcStatus status=CVC_OK;
    while(cur[0]){
        uint16_t *abs=w_repo_to_abs(repo->root16,cur);
        if(!abs){ free(cur); return cvc_fail(CVC_ERR,"oom"); }
        WStat st;
        if(w_stat(abs,&st)!=0 || !st.exists || !st.is_dir || st.is_reparse){
            free(abs); free(cur); return status;
        }
        DirProbe p; p.has_any=0; p.has_non_dir=0;
        if(wdir_list(abs,prune_probe_cb,&p)!=0){ free(abs); free(cur); return cvc_fail(CVC_ERR,"cannot read directory"); }
        int removed=0;
        if(!p.has_any){ w_delete_path(abs,1); removed=1; }
        free(abs);
        if(!removed){ free(cur); return status; }
        /* walk up */
        char *slash=strrchr(cur,'/');
        if(!slash){ free(cur); return status; }
        *slash=0;
    }
    free(cur);
    return status;
}

/* Apply materialization with rollback. We materialize the whole tree:
 *  1. Ensure all directory containers.
 *  2. Remove tracked leaves not in target (with backups).
 *  3. Prune now-empty tracked directory containers (so a file can replace a
 *     former directory, e.g. file<->dir transitions across branches).
 *  4. For each target leaf: if existing differs, backup & replace.
 * On any failure, undo applied operations.
 */
CvcStatus mat_materialize(Repo *repo, const Snapshot *current, const Snapshot *target){
    Rollback rb; memset(&rb,0,sizeof rb);
    CvcStatus status=CVC_OK;
    size_t i;

    /* 1. Ensure directory containers for all target paths (and prune empty
     *    tracked dirs is handled later). */
    for(i=0;i<target->len;i++){
        /* ensure parent dirs */
        const char *path=target->items[i].path;
        const char *slash=strrchr(path,'/');
        if(slash){
            char *dir=(char*)malloc((size_t)(slash-path)+1);
            if(!dir) return cvc_fail(CVC_ERR,"oom");
            memcpy(dir,path,(size_t)(slash-path)); dir[slash-path]=0;
            CvcStatus s=ensure_dir(repo,dir);
            free(dir);
            if(s!=CVC_OK) goto fail;
        }
    }

    /* 2. Remove tracked leaves in current but not in target. Do this BEFORE
     * writing target leaves so a target file/symlink may replace a directory
     * container left by the removed branch (file<->dir transitions). */
    for(i=0;i<current->len;i++){
        if(snap_find(target,current->items[i].path)<0){
            /* delete this tracked path */
            const char *path=current->items[i].path;
            uint16_t *abs=w_repo_to_abs(repo->root16,path);
            if(!abs) goto oom;
            WStat st;
            if(w_stat(abs,&st)==0 && st.exists){
                /* only delete if it's a leaf we manage; must be file/symlink */
                if(!st.is_dir){
                    if(rb_add(&rb,RB_DELETE,path)!=0){ free(abs); goto oom; }
                    RBEntry *e=&rb.items[rb.len-1];
                    Bytes c; bytes_init(&c); uint8_t t=0; int ex=0;
                    if(read_wt_entry(repo,path,&c,&t,&ex)==CVC_OK){
                        e->old_type=t;
                        bytes_append(&e->old_bytes,c.data,c.len);
                        bytes_free(&c);
                    }
                    if(w_delete_path(abs,0)!=0){ free(abs); status=cvc_fail(CVC_ERR,"cannot delete file"); goto fail; }
                }
            }
            free(abs);
        }
    }

    /* 3. Prune empty tracked directory containers that are no longer needed.
     * Walk tracked leaves in `current`; for each, ensure its ancestor dirs
     * that are empty are removed (so a file can take their place). Only prune
     * directories whose entire contents were tracked (already removed). We
     * remove the deepest empty dirs first by iterating leaves with no
     * remaining tracked children. */
    for(i=0;i<current->len;i++){
        const char *path=current->items[i].path;
        const char *slash=strrchr(path,'/');
        if(!slash) continue;
        /* skip if path still in target (dir is still needed) */
        if(snap_find(target,path)>=0) continue;
        char *dir=(char*)malloc((size_t)(slash-path)+1);
        if(!dir) goto oom;
        memcpy(dir,path,(size_t)(slash-path)); dir[slash-path]=0;
        CvcStatus pr=prune_empty_dir(repo,dir);
        free(dir);
        if(pr!=CVC_OK){ status=pr; goto fail; }
    }

    /* 4. Process target leaves: replace/keep. */
    for(i=0;i<target->len;i++){
        const char *path=target->items[i].path;
        uint8_t ttype=target->items[i].type;
        /* check existing */
        uint16_t *abs=w_repo_to_abs(repo->root16,path);
        if(!abs) goto oom;
        WStat st;
        int exists=0;
        if(w_stat(abs,&st)==0 && st.exists) exists=1;
        free(abs);
        /* Compare existing with desired. */
        if(exists){
            /* read existing */
            Bytes oldc; bytes_init(&oldc); uint8_t oldtype=0; int ex2=0;
            CvcStatus s=read_wt_entry(repo,path,&oldc,&oldtype,&ex2);
            if(s!=CVC_OK){ bytes_free(&oldc); status=s; goto fail; }
            int same=0;
            if(oldtype==ttype && ttype==OBJ_TREE_BLOB){
                /* blob id is SHA-256 of envelope "blob <len>\0<payload>" */
                uint8_t check[32];
                Bytes env; bytes_init(&env);
                char lb[32]; snprintf(lb,sizeof lb,"%zu",oldc.len);
                bytes_append_cstr(&env,"blob "); bytes_append_cstr(&env,lb);
                bytes_append_byte(&env,0); bytes_append(&env,oldc.data,oldc.len);
                sha256_one(env.data,env.len,check);
                bytes_free(&env);
                same = memcmp(check,target->items[i].id,32)==0;
            } else if(oldtype==ttype && (ttype==OBJ_TREE_FILE_SYMLINK||ttype==OBJ_TREE_DIR_SYMLINK)){
                uint8_t check[32];
                Bytes env; bytes_init(&env);
                char lb[32]; snprintf(lb,sizeof lb,"%zu",oldc.len);
                bytes_append_cstr(&env,"symlink "); bytes_append_cstr(&env,lb);
                bytes_append_byte(&env,0); bytes_append(&env,oldc.data,oldc.len);
                sha256_one(env.data,env.len,check);
                bytes_free(&env);
                same = memcmp(check,target->items[i].id,32)==0;
            }
            if(!same){
                /* backup old then write new */
                if(rb_add(&rb,RB_REPLACE,path)!=0){ bytes_free(&oldc); goto oom; }
                RBEntry *e=&rb.items[rb.len-1];
                e->old_type=oldtype;
                bytes_append(&e->old_bytes,oldc.data,oldc.len);
                bytes_free(&oldc);
                CvcStatus w=write_leaf(repo,path,ttype,target->items[i].id);
                if(w!=CVC_OK){ status=w; goto fail; }
            } else {
                bytes_free(&oldc);
            }
        } else {
            /* create new leaf */
            if(rb_add(&rb,RB_CREATE,path)!=0) goto oom;
            CvcStatus w=write_leaf(repo,path,ttype,target->items[i].id);
            if(w!=CVC_OK){ status=w; goto fail; }
        }
    }

    rb_free(&rb);
    return CVC_OK;

oom:
    status=cvc_fail(CVC_ERR,"out of memory");
    goto fail;
fail:
    /* Undo applied operations in reverse order. */
    {
        size_t k;
        for(k=rb.len;k>0;k--){
            RBEntry *e=&rb.items[k-1];
            uint16_t *abs=w_repo_to_abs(repo->root16,e->path);
            if(!abs) continue;
            if(e->kind==RB_CREATE){
                /* remove created leaf */
                WStat st;
                if(w_stat(abs,&st)==0 && st.exists && !st.is_dir){
                    w_delete_path(abs, st.is_dir);
                }
            } else if(e->kind==RB_REPLACE || e->kind==RB_DELETE){
                /* restore old content */
                if(e->old_type==OBJ_TREE_BLOB){
                    w_write_file_atomic(abs, e->old_bytes.data, e->old_bytes.len);
                } else if(e->old_type==OBJ_TREE_FILE_SYMLINK || e->old_type==OBJ_TREE_DIR_SYMLINK){
                    char *print=(char*)malloc(e->old_bytes.len+1);
                    if(print){
                        memcpy(print,e->old_bytes.data,e->old_bytes.len); print[e->old_bytes.len]=0;
                        w_symlink_create(abs, print, e->old_type==OBJ_TREE_DIR_SYMLINK);
                        free(print);
                    }
                } else if(e->kind==RB_DELETE){
                    /* was deleted; if old_type dir was not, it was a leaf that didn't exist? */
                }
            }
            free(abs);
        }
    }
    rb_free(&rb);
    return status;
}
