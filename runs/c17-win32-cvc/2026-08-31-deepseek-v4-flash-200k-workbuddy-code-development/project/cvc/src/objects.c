#include "objects.h"
#include "repo.h"
#include "sha256.h"
#include "utf8.h"
#include "win32.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void tree_init(Tree *t){ t->entries=NULL; t->count=0; t->cap=0; }
void tree_free(Tree *t){
    size_t i; for(i=0;i<t->count;i++) free(t->entries[i].name);
    free(t->entries); tree_init(t);
}
int tree_add(Tree *t, uint8_t type, const char *name, const uint8_t id[32]){
    if(t->count==t->cap){
        size_t nc=t->cap?t->cap*2:8;
        TreeEntry *ne=(TreeEntry*)realloc(t->entries, nc*sizeof(TreeEntry));
        if(!ne) return -1;
        t->entries=ne; t->cap=nc;
    }
    TreeEntry *e=&t->entries[t->count];
    e->type=type;
    e->name=strdup(name);
    if(!e->name) return -1;
    memcpy(e->id,id,32);
    t->count++;
    return 0;
}

static int name_cmp(const void *a, const void *b){
    const TreeEntry *ea=(const TreeEntry*)a, *eb=(const TreeEntry*)b;
    return strcmp(ea->name, eb->name);
}

int tree_has_case_collision(const Tree *t){
    size_t i,j;
    for(i=0;i<t->count;i++){
        for(j=i+1;j<t->count;j++){
            if(utf8_ordinal_case_equal(t->entries[i].name, t->entries[j].name)) return 1;
        }
    }
    return 0;
}

int tree_sort_validate(Tree *t){
    qsort(t->entries, t->count, sizeof(TreeEntry), name_cmp);
    size_t i;
    for(i=0;i+1<t->count;i++){
        if(strcmp(t->entries[i].name, t->entries[i+1].name)==0) return -1; /* exact dup */
        if(utf8_ordinal_case_equal(t->entries[i].name, t->entries[i+1].name)) return -1; /* case collision */
    }
    return 0;
}

static void put_u32(uint8_t *p, uint32_t v){
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}
static uint32_t get_u32(const uint8_t *p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static int64_t get_i64(const uint8_t *p){
    return ((int64_t)((int8_t)p[0])<<56)|((int64_t)p[1]<<48)|((int64_t)p[2]<<40)|((int64_t)p[3]<<32)|
           ((int64_t)p[4]<<24)|((int64_t)p[5]<<16)|((int64_t)p[6]<<8)|p[7];
}
static uint64_t get_u64(const uint8_t *p){
    return ((uint64_t)p[0]<<56)|((uint64_t)p[1]<<48)|((uint64_t)p[2]<<40)|((uint64_t)p[3]<<32)|
           ((uint64_t)p[4]<<24)|((uint64_t)p[5]<<16)|((uint64_t)p[6]<<8)|p[7];
}

int tree_encode(const Tree *t, Bytes *out){
    bytes_init(out);
    uint8_t hdr[4];
    put_u32(hdr, (uint32_t)t->count);
    if(bytes_append(out, hdr, 4)!=0) return -1;
    size_t i;
    for(i=0;i<t->count;i++){
        /* entry_type(1) name_length(4) name id(32) */
        if(bytes_append(out, &t->entries[i].type, 1)!=0) return -1;
        uint8_t nlen[4];
        put_u32(nlen, (uint32_t)strlen(t->entries[i].name));
        if(bytes_append(out, nlen, 4)!=0) return -1;
        if(bytes_append(out, t->entries[i].name, strlen(t->entries[i].name))!=0) return -1;
        if(bytes_append(out, t->entries[i].id, 32)!=0) return -1;
    }
    return 0;
}

int tree_decode(const uint8_t *p, size_t len, Tree *t){
    tree_init(t);
    if(len < 4) return -1;
    size_t count = get_u32(p);
    if(count > 0xFFFFFF) return -1;
    size_t pos = 4;
    size_t i;
    for(i=0;i<count;i++){
        if(pos+1 > len) return -1;
        uint8_t type = p[pos]; pos+=1;
        if(pos+4 > len) return -1;
        size_t nl = get_u32(p+pos); pos+=4;
        if(pos+nl+32 > len) return -1;
        /* validate type */
        if(type!=OBJ_TREE_BLOB && type!=OBJ_TREE_FILE_SYMLINK &&
           type!=OBJ_TREE_SUBTREE && type!=OBJ_TREE_DIR_SYMLINK) return -1;
        char *name=(char*)malloc(nl+1);
        if(!name) return -1;
        memcpy(name, p+pos, nl); name[nl]=0; pos+=nl;
        uint8_t id[32]; memcpy(id, p+pos, 32); pos+=32;
        /* validate name: no NUL, valid UTF-8, no / \ or control, not . or .. */
        if(nl==0 || memchr(name,0,nl) || utf8_validate((const uint8_t*)name, nl)!=0){
            free(name); tree_free(t); return -1;
        }
        size_t j;
        for(j=0;j<nl;j++){
            unsigned char c=(unsigned char)name[j];
            if(c=='/'||c=='\\'||c<0x20||c==0x7f){ free(name); tree_free(t); return -1; }
            /* Win32-forbidden characters (spec 09 §5) */
            if(c=='<'||c=='>'||c==':'||c=='"'||c=='|'||c=='?'||c=='*'){ free(name); tree_free(t); return -1; }
        }
        if(strcmp(name,".")==0||strcmp(name,"..")==0){ free(name); tree_free(t); return -1; }
        /* trailing dot/space ambiguous (spec 09 §5) */
        if(name[nl-1]=='.'||name[nl-1]==' '){ free(name); tree_free(t); return -1; }
        /* reserved DOS device basename before first dot (spec 09 §5) */
        {
            char base[16]; size_t bi=0;
            for(size_t k=0;k<nl && bi<15;k++){ if(name[k]=='.') break; base[bi++]=name[k]; }
            base[bi]=0;
            if(bi>0){
                static const char *devs[]={"CON","PRN","AUX","NUL",
                    "COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
                    "LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9",
                    "COM\xc2\xb9","COM\xc2\xb2","COM\xc2\xb3","LPT\xc2\xb9","LPT\xc2\xb2","LPT\xc2\xb3"};
                for(size_t d=0;d<sizeof(devs)/sizeof(devs[0]);d++)
                    if(utf8_ordinal_case_equal(base,devs[d])){ free(name); tree_free(t); return -1; }
            }
        }
        if(tree_add(t, type, name, id)!=0){ free(name); tree_free(t); return -1; }
        free(name);
    }
    if(pos != len) return -1;
    if(tree_sort_validate(t)!=0){ tree_free(t); return -1; }
    return 0;
}

int commit_encode(const Commit *c, Bytes *out){
    bytes_init(out);
    if(bytes_append(out, c->root_tree, 32)!=0) return -1;
    if(bytes_append(out, &c->parent_count, 1)!=0) return -1;
    size_t i;
    for(i=0;i<c->parent_count;i++){
        if(bytes_append(out, c->parents[i], 32)!=0) return -1;
    }
    uint8_t ts[8];
    int64_t v=c->timestamp;
    for(i=0;i<8;i++) ts[i]=(uint8_t)((uint64_t)v >> (56-8*i));
    if(bytes_append(out, ts, 8)!=0) return -1;
    uint8_t ml[8];
    uint64_t mlen=(uint64_t)c->message_len;
    for(i=0;i<8;i++) ml[i]=(uint8_t)(mlen >> (56-8*i));
    if(bytes_append(out, ml, 8)!=0) return -1;
    if(c->message_len) if(bytes_append(out, c->message, c->message_len)!=0) return -1;
    return 0;
}

int commit_decode(const uint8_t *p, size_t len, Commit *c){
    memset(c,0,sizeof *c);
    if(len < 32+1+8+8) return -1;
    memcpy(c->root_tree, p, 32);
    size_t pos=32;
    uint8_t pc=p[pos]; pos+=1;
    if(pc>2) return -1;
    c->parent_count=pc;
    for(size_t i=0;i<pc;i++){
        if(pos+32>len) return -1;
        memcpy(c->parents[i], p+pos, 32); pos+=32;
    }
    if(pos+8>len) return -1;
    c->timestamp=get_i64(p+pos); pos+=8;
    if(pos+8>len) return -1;
    uint64_t mlen=get_u64(p+pos); pos+=8;
    if(mlen > (uint64_t)(len-pos)) return -1;
    if(pos+mlen != len) return -1;
    c->message=(char*)malloc(mlen+1);
    if(!c->message) return -1;
    memcpy(c->message, p+pos, mlen); c->message[mlen]=0; c->message_len=mlen;
    /* message must be nonempty valid UTF-8 */
    if(mlen==0 || utf8_validate((const uint8_t*)c->message, mlen)!=0){ free(c->message); c->message=NULL; return -1; }
    return 0;
}
void commit_free(Commit *c){ free(c->message); memset(c,0,sizeof *c); }

void object_envelope(const char *type, const uint8_t *payload, size_t plen, Bytes *out, uint8_t id[32]){
    bytes_init(out);
    /* <type> SP <len> NUL <payload> */
    bytes_append_cstr(out, type);
    bytes_append_byte(out, ' ');
    char lenbuf[32];
    snprintf(lenbuf,sizeof lenbuf,"%zu", plen);
    bytes_append_cstr(out, lenbuf);
    bytes_append_byte(out, 0);
    bytes_append(out, payload, plen);
    sha256_one(out->data, out->len, id);
}

void id_hex(const uint8_t id[32], char out[65]){ sha256_to_hex(id, out); }

uint16_t *obj_path(const Repo *repo, const uint8_t id[32]){
    char hex[65]; sha256_to_hex(id, hex);
    /* .cvc\objects\<2>\<62> */
    char rel[160];
    snprintf(rel,sizeof rel, "objects/%c%c/%s", hex[0], hex[1], hex+2);
    return w_repo_to_abs(repo->cvc16, rel);
}

CvcStatus obj_read(const Repo *repo, const uint8_t id[32], ObjectData *obj){
    memset(obj,0,sizeof *obj);
    uint16_t *path = obj_path(repo, id);
    if(!path) return cvc_fail(CVC_ERR,"oom");
    Bytes raw; bytes_init(&raw);
    CvcStatus st = w_read_file(path, &raw);
    free(path);
    if(st!=CVC_OK) return cvc_fail(CVC_ERR,"missing object");
    /* parse envelope: "<type> <len>\0<payload>" */
    size_t i=0;
    /* type up to space */
    size_t type_start=i;
    while(i<raw.len && raw.data[i]!=' ' && raw.data[i]!=0) i++;
    if(i>=raw.len || raw.data[i]!=' ') goto corrupt;
    size_t type_len = i-type_start;
    if(type_len==0) goto corrupt;
    char type[8]; if(type_len>=sizeof type) goto corrupt;
    memcpy(type, raw.data+type_start, type_len); type[type_len]=0;
    i++; /* skip space */
    size_t len_start=i;
    while(i<raw.len && raw.data[i]!=0) i++;
    if(i>=raw.len) goto corrupt;
    char lenbuf[32]; size_t llen=i-len_start;
    if(llen==0 || llen>=sizeof lenbuf) goto corrupt;
    memcpy(lenbuf, raw.data+len_start, llen); lenbuf[llen]=0;
    i++; /* skip NUL */
    /* validate len digits */
    for(size_t k=0;k<llen;k++) if(lenbuf[k]<'0'||lenbuf[k]>'9') goto corrupt;
    if(llen>1 && lenbuf[0]=='0') goto corrupt; /* no leading zero */
    unsigned long long plen=strtoull(lenbuf,NULL,10);
    if(i+plen != raw.len) goto corrupt;
    obj->payload.len=0;
    if(bytes_append(&obj->payload, raw.data+i, (size_t)plen)!=0){ bytes_free(&raw); return cvc_fail(CVC_ERR,"oom"); }
    bytes_free(&raw);
    if(strcmp(type,"blob")==0) obj->type='b';
    else if(strcmp(type,"symlink")==0) obj->type='s';
    else if(strcmp(type,"tree")==0) obj->type='t';
    else if(strcmp(type,"commit")==0) obj->type='c';
    else goto corrupt;
    return CVC_OK;
corrupt:
    bytes_free(&raw);
    return cvc_fail(CVC_ERR,"corrupt object");
}

CvcStatus obj_write_envelope(const Repo *repo, const char *type,
                             const uint8_t *payload, size_t plen, uint8_t id[32]){
    Bytes env; object_envelope(type, payload, plen, &env, id);
    uint16_t *path = obj_path(repo, id);
    if(!path){ bytes_free(&env); return cvc_fail(CVC_ERR,"oom"); }
    /* If valid object already exists, reuse. */
    WStat st;
    if(w_stat(path, &st)==0 && st.exists){
        /* verify hash */
        Bytes raw; bytes_init(&raw);
        if(w_read_file(path,&raw)==CVC_OK){
            uint8_t check[32];
            sha256_one(raw.data, raw.len, check);
            int same = memcmp(check, id, 32)==0;
            if(same && raw.len==env.len && memcmp(raw.data, env.data, env.len)==0){
                bytes_free(&raw); bytes_free(&env); free(path); return CVC_OK;
            }
            /* conflicts -> corrupt */
            bytes_free(&raw);
            char hx[65]; sha256_to_hex(id,hx);
            bytes_free(&env); free(path);
            return cvc_fail(CVC_ERR,"corrupt existing object at id %s", hx);
        }
        bytes_free(&raw);
    }
    free(path);
    /* write temp + flush + rename into .cvc/objects/<2>/<62> */
    /* ensure fan-out dir exists */
    char hex[65]; sha256_to_hex(id, hex);
    char rel_dir[80]; snprintf(rel_dir,sizeof rel_dir,"objects/%c%c",hex[0],hex[1]);
    uint16_t *fanout = w_repo_to_abs(repo->cvc16, rel_dir);
    if(!fanout){ bytes_free(&env); return cvc_fail(CVC_ERR,"oom"); }
    w_mkdir(fanout);
    free(fanout);
    uint16_t *dest = obj_path(repo, id);
    if(!dest){ bytes_free(&env); return cvc_fail(CVC_ERR,"oom"); }
    int rc = w_write_file_atomic(dest, env.data, env.len);
    bytes_free(&env);
    free(dest);
    if(rc!=0) return cvc_fail(CVC_ERR,"failed to write object");
    return CVC_OK;
}

void object_free(ObjectData *obj){ bytes_free(&obj->payload); }
