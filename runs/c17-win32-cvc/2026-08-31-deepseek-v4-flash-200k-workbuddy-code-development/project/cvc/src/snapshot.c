#include "snapshot.h"
#include "utf8.h"
#include "sha256.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void snap_init(Snapshot *s){ s->items=NULL; s->len=0; s->cap=0; }
void snap_free(Snapshot *s){
    size_t i; for(i=0;i<s->len;i++) free(s->items[i].path);
    free(s->items); snap_init(s);
}
int snap_add(Snapshot *s, const char *path, uint8_t type, const uint8_t id[32]){
    if(s->len==s->cap){
        size_t nc=s->cap?s->cap*2:32;
        SnapLeaf *nl=(SnapLeaf*)realloc(s->items,nc*sizeof(SnapLeaf));
        if(!nl) return -1;
        s->items=nl; s->cap=nc;
    }
    SnapLeaf *e=&s->items[s->len];
    e->path=strdup(path);
    if(!e->path) return -1;
    e->type=type; memcpy(e->id,id,32);
    s->len++;
    return 0;
}
static int leaf_cmp(const void *a,const void *b){
    return strcmp(((const SnapLeaf*)a)->path, ((const SnapLeaf*)b)->path);
}
int snap_sort(Snapshot *s){
    qsort(s->items,s->len,sizeof(SnapLeaf),leaf_cmp);
    size_t i;
    for(i=0;i+1<s->len;i++){
        if(strcmp(s->items[i].path,s->items[i+1].path)==0) return -1;
        /* case collision among sibling paths? Paths include '/'; case rule is
           per-sibling. We check per-directory in tree building. Skip here. */
    }
    return 0;
}
long snap_find(const Snapshot *s, const char *path){
    /* binary search */
    long lo=0, hi=(long)s->len-1;
    while(lo<=hi){
        long mid=(lo+hi)/2;
        int c=strcmp(s->items[mid].path, path);
        if(c==0) return mid;
        if(c<0) lo=mid+1; else hi=mid-1;
    }
    return -1;
}

/* ------- tree building from snapshot ------- */

/* Build a subtree recursively. `start` and `end` are the range of leaves in
 * s (sorted) that belong to this subtree, all under `prefix` (empty for root).
 * Sets root_id. */
static CvcStatus build_node(const Repo *repo, const Snapshot *s, size_t start, size_t end,
                            const char *prefix, uint8_t root_id[32]){
    /* Gather direct children of prefix. A child is the next path segment. */
    Tree t; tree_init(&t);
    size_t i=start;
    size_t plen = strlen(prefix);
    while(i<end){
        const char *path = s->items[i].path;
        if(plen>0){
            /* path must start with prefix + "/" */
            if(strncmp(path,prefix,plen)!=0 || path[plen]!='/'){ i++; continue; }
        }
        /* extract next component after prefix */
        const char *seg = path + (plen>0?plen+1:0);
        const char *slash = strchr(seg,'/');
        size_t seglen = slash? (size_t)(slash-seg) : strlen(seg);
        char *comp=(char*)malloc(seglen+1);
        if(!comp){ tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
        memcpy(comp,seg,seglen); comp[seglen]=0;
        /* find last leaf of this component */
        size_t j=i;
        while(j<end){
            const char *p2=s->items[j].path;
            if(plen>0 && strncmp(p2,prefix,plen)==0 && p2[plen]=='/' && strncmp(p2+(plen+1),comp,seglen)==0 &&
               (p2[plen+1+seglen]=='\0' || p2[plen+1+seglen]=='/')){ j++; }
            else if(plen==0 && strncmp(p2,comp,seglen)==0 && (p2[seglen]=='\0'||p2[seglen]=='/')){ j++; }
            else break;
        }
        /* j is one past the range for comp */
        /* Determine if comp is a leaf (no further slash) or subtree */
        if(s->items[i].path[plen>0?plen+1+seglen:seglen]=='\0'){
            /* leaf: all leaves in [i,j) must be the same leaf (only one) */
            uint8_t leaf_id[32]; uint8_t ltype=0;
            /* The single leaf is s->items[i] */
            memcpy(leaf_id,s->items[i].id,32); ltype=s->items[i].type;
            uint8_t entry_type;
            CvcStatus leaf_st=CVC_OK;
            switch(ltype){
                case OBJ_TREE_BLOB:
                    entry_type=OBJ_TREE_BLOB;
                    /* write the blob object now (content-addressed) */
                    {
                        /* content id == blob envelope id; we must materialize the
                           blob object. But the Snapshot only stores the id, not
                           content. For working-tree snapshots, save must have
                           written blobs before building the tree. To support
                           both, we ensure the object exists via obj_write_envelope;
                           if already present it reuses. If not present, we cannot
                           reconstruct content here — so save must write blobs.
                           We rely on save writing them (see cli). No-op here. */
                        leaf_st=CVC_OK;
                    }
                    break;
                case OBJ_TREE_FILE_SYMLINK:
                case OBJ_TREE_DIR_SYMLINK:
                    entry_type=ltype;
                    /* symlink object: same, save must have written it. */
                    leaf_st=CVC_OK;
                    break;
                default: tree_free(&t); free(comp); return cvc_fail(CVC_ERR,"bad leaf type");
            }
            if(leaf_st!=CVC_OK){ tree_free(&t); free(comp); return leaf_st; }
            if(tree_add(&t, entry_type, comp, leaf_id)!=0){ free(comp); tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
        } else {
            /* subtree: build child node for prefix+"/"+comp */
            char *childprefix=(char*)malloc(plen+seglen+2);
            if(!childprefix){ free(comp); tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
            if(plen>0){ memcpy(childprefix,prefix,plen); childprefix[plen]='/'; memcpy(childprefix+plen+1,comp,seglen); childprefix[plen+1+seglen]=0; }
            else { memcpy(childprefix,comp,seglen); childprefix[seglen]=0; }
            uint8_t child_id[32];
            CvcStatus st=build_node(repo,s,i,j,childprefix,child_id);
            free(childprefix);
            if(st!=CVC_OK){ free(comp); tree_free(&t); return st; }
            if(tree_add(&t, OBJ_TREE_SUBTREE, comp, child_id)!=0){ free(comp); tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
        }
        free(comp);
        i=j;
    }
    /* sort & validate */
    if(tree_sort_validate(&t)!=0){ tree_free(&t); return cvc_fail(CVC_ERR,"tree has duplicate/case-colliding siblings"); }
    /* encode + write */
    Bytes payload; 
    if(tree_encode(&t,&payload)!=0){ tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
    tree_free(&t);
    CvcStatus st=obj_write_envelope(repo,"tree",payload.data,payload.len,root_id);
    bytes_free(&payload);
    return st;
}

CvcStatus snap_build_tree(const Repo *repo, const Snapshot *s, uint8_t root_id[32]){
    return build_node(repo, s, 0, s->len, "", root_id);
}

/* ------- decode tree into snapshot ------- */

CvcStatus snap_from_tree(const Repo *repo, const uint8_t tree_id[32], const char *prefix, Snapshot *out){
    ObjectData od;
    CvcStatus st=obj_read(repo, tree_id, &od);
    if(st!=CVC_OK) return st;
    if(od.type!='t'){ object_free(&od); return cvc_fail(CVC_ERR,"expected tree"); }
    Tree t; 
    if(tree_decode(od.payload.data, od.payload.len, &t)!=0){ object_free(&od); return cvc_fail(CVC_ERR,"malformed tree"); }
    object_free(&od);
    size_t plen=strlen(prefix);
    size_t i;
    for(i=0;i<t.count;i++){
        TreeEntry *e=&t.entries[i];
        char *path;
        if(plen>0){
            path=(char*)malloc(plen+1+strlen(e->name)+1);
            if(!path){ tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
            sprintf(path,"%s/%s",prefix,e->name);
        } else {
            path=strdup(e->name);
            if(!path){ tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
        }
        if(e->type==OBJ_TREE_SUBTREE){
            CvcStatus s2=snap_from_tree(repo, e->id, path, out);
            free(path);
            if(s2!=CVC_OK){ tree_free(&t); return s2; }
        } else {
            uint8_t lt;
            if(e->type==OBJ_TREE_BLOB) lt=OBJ_TREE_BLOB;
            else if(e->type==OBJ_TREE_FILE_SYMLINK) lt=OBJ_TREE_FILE_SYMLINK;
            else if(e->type==OBJ_TREE_DIR_SYMLINK) lt=OBJ_TREE_DIR_SYMLINK;
            else { free(path); tree_free(&t); return cvc_fail(CVC_ERR,"bad tree entry type"); }
            if(snap_add(out,path,lt,e->id)!=0){ free(path); tree_free(&t); return cvc_fail(CVC_ERR,"oom"); }
            free(path);
        }
    }
    tree_free(&t);
    return CVC_OK;
}

CvcStatus snap_from_commit(const Repo *repo, const uint8_t commit_id[32], Snapshot *out){
    snap_init(out);
    ObjectData od;
    CvcStatus st=obj_read(repo, commit_id, &od);
    if(st!=CVC_OK) return st;
    if(od.type!='c'){ object_free(&od); return cvc_fail(CVC_ERR,"expected commit"); }
    Commit c;
    if(commit_decode(od.payload.data, od.payload.len, &c)!=0){ object_free(&od); return cvc_fail(CVC_ERR,"malformed commit"); }
    object_free(&od);
    st = snap_from_tree(repo, c.root_tree, "", out);
    commit_free(&c);
    if(st!=CVC_OK){ snap_free(out); return st; }
    if(snap_sort(out)!=0){ snap_free(out); return cvc_fail(CVC_ERR,"snapshot collision"); }
    return CVC_OK;
}

/* ------- compare ------- */
void diff_list_init(DiffList *d){ d->items=NULL; d->len=0; d->cap=0; }
void diff_list_free(DiffList *d){
    size_t i; for(i=0;i<d->len;i++) free(d->items[i].path);
    free(d->items); diff_list_init(d);
}
static int diff_add(DiffList *d, const char *path, int st, uint8_t ot,uint8_t nt,const uint8_t oid[32],const uint8_t nid[32]){
    if(d->len==d->cap){
        size_t nc=d->cap?d->cap*2:16;
        DiffResult *ni=(DiffResult*)realloc(d->items,nc*sizeof(DiffResult));
        if(!ni) return -1;
        d->items=ni; d->cap=nc;
    }
    DiffResult *r=&d->items[d->len];
    r->path=strdup(path); if(!r->path) return -1;
    r->status=st; r->old_type=ot; r->new_type=nt;
    if(oid) memcpy(r->old_id,oid,32); else memset(r->old_id,0,32);
    if(nid) memcpy(r->new_id,nid,32); else memset(r->new_id,0,32);
    d->len++;
    return 0;
}

CvcStatus snap_compare(const Snapshot *old_s, const Snapshot *new_s, DiffList *out){
    diff_list_init(out);
    size_t i=0,j=0;
    while(i<old_s->len || j<new_s->len){
        if(i>=old_s->len){
            if(diff_add(out,new_s->items[j].path,1,0,new_s->items[j].type,NULL,new_s->items[j].id)!=0){ diff_list_free(out); return cvc_fail(CVC_ERR,"oom"); }
            j++;
        } else if(j>=new_s->len){
            if(diff_add(out,old_s->items[i].path,3,old_s->items[i].type,0,old_s->items[i].id,NULL)!=0){ diff_list_free(out); return cvc_fail(CVC_ERR,"oom"); }
            i++;
        } else {
            int c=strcmp(old_s->items[i].path,new_s->items[j].path);
            if(c==0){
                if(old_s->items[i].type!=new_s->items[j].type || memcmp(old_s->items[i].id,new_s->items[j].id,32)!=0){
                    int st = (old_s->items[i].type!=new_s->items[j].type)?4:2;
                    if(diff_add(out,old_s->items[i].path,st,old_s->items[i].type,new_s->items[j].type,old_s->items[i].id,new_s->items[j].id)!=0){ diff_list_free(out); return cvc_fail(CVC_ERR,"oom"); }
                }
                i++; j++;
            } else if(c<0){
                if(diff_add(out,old_s->items[i].path,3,old_s->items[i].type,0,old_s->items[i].id,NULL)!=0){ diff_list_free(out); return cvc_fail(CVC_ERR,"oom"); }
                i++;
            } else {
                if(diff_add(out,new_s->items[j].path,1,0,new_s->items[j].type,NULL,new_s->items[j].id)!=0){ diff_list_free(out); return cvc_fail(CVC_ERR,"oom"); }
                j++;
            }
        }
    }
    return CVC_OK;
}
