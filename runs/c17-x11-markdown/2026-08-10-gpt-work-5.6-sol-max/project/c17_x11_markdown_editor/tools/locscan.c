#include "mdedit/core.h"
#include "mdedit/json.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

enum { EXIT_USAGE=2, EXIT_CONFIG=3, EXIT_INPUT=4, EXIT_OUTPUT=5, EXIT_VERIFY=6, EXIT_INTERNAL=7 };

typedef struct { char **items; size_t len; size_t cap; } Strings;

typedef struct {
    Strings include_extensions;
    Strings source_extensions;
    Strings test_extensions;
    Strings documentation_extensions;
    Strings config_build_extensions;
    Strings exclude_dirs;
    Strings exclude_paths;
    Strings generated_paths;
    Strings include_overrides;
    bool follow_directory_symlinks;
} Config;

typedef enum { CAT_SOURCE,CAT_TEST,CAT_DOC,CAT_CONFIG,CAT_OTHER } Category;

typedef struct {
    char *path;
    uint64_t bytes;
    uint64_t lines;
    Category category;
    bool unreadable;
} FileInfo;

typedef struct { FileInfo *items; size_t len; size_t cap; } Files;

typedef struct { dev_t device; ino_t inode; } DirIdentity;
typedef struct { DirIdentity *items; size_t len; size_t cap; } Identities;

static void strings_free(Strings *s) {
    for (size_t i=0U;i<s->len;++i) free(s->items[i]);
    free(s->items); memset(s,0,sizeof(*s));
}

static bool strings_add(Strings *s,const char *value) {
    if (s->len==s->cap) {
        size_t next=s->cap==0U?8U:s->cap*2U; char **items=realloc(s->items,next*sizeof(*items));
        if (items==NULL) return false;
        s->items=items;
        s->cap=next;
    }
    s->items[s->len]=md_strdup(value);
    if (s->items[s->len]==NULL) return false;
    ++s->len; return true;
}

static void config_free(Config *c) {
    strings_free(&c->include_extensions); strings_free(&c->source_extensions);
    strings_free(&c->test_extensions); strings_free(&c->documentation_extensions);
    strings_free(&c->config_build_extensions); strings_free(&c->exclude_dirs);
    strings_free(&c->exclude_paths); strings_free(&c->generated_paths);
    strings_free(&c->include_overrides); memset(c,0,sizeof(*c));
}

static Strings *config_list(Config *c,const char *key) {
    if (strcmp(key,"include_extensions")==0) return &c->include_extensions;
    if (strcmp(key,"source_extensions")==0) return &c->source_extensions;
    if (strcmp(key,"test_extensions")==0) return &c->test_extensions;
    if (strcmp(key,"documentation_extensions")==0) return &c->documentation_extensions;
    if (strcmp(key,"config_build_extensions")==0) return &c->config_build_extensions;
    if (strcmp(key,"exclude_dirs")==0) return &c->exclude_dirs;
    if (strcmp(key,"exclude_paths")==0) return &c->exclude_paths;
    if (strcmp(key,"generated_paths")==0) return &c->generated_paths;
    if (strcmp(key,"include_overrides")==0) return &c->include_overrides;
    return NULL;
}

static char *trim(char *s) {
    while (*s==' '||*s=='\t') ++s;
    size_t n=strlen(s); while (n>0U&&(s[n-1U]==' '||s[n-1U]=='\t'||s[n-1U]=='\r'||s[n-1U]=='\n')) s[--n]='\0';
    return s;
}

static bool unquote(char *s,char **value) {
    s=trim(s); size_t n=strlen(s);
    if (n>=2U&&((s[0]=='"'&&s[n-1U]=='"')||(s[0]=='\''&&s[n-1U]=='\''))) {
        s[n-1U]='\0'; ++s;
    }
    if (*s=='\0') return false;
    *value=s;
    return true;
}

static bool parse_yaml(Config *config,const char *text,size_t len,char *error,size_t error_cap) {
    char *copy=md_strndup(text,len); if (copy==NULL) return false;
    Strings *active=NULL; size_t line_no=0U; char *save=NULL; char *line=strtok_r(copy,"\n",&save);
    while (line!=NULL) {
        ++line_no; char *p=trim(line);
        if (*p=='\0'||*p=='#') { line=strtok_r(NULL,"\n",&save); continue; }
        if (*p=='-') {
            if (active==NULL) goto malformed;
            char *value=NULL; if (!unquote(p+1,&value)||!strings_add(active,value)) goto malformed;
            line=strtok_r(NULL,"\n",&save); continue;
        }
        char *colon=strchr(p,':'); if (colon==NULL) goto malformed;
        *colon='\0'; char *key=trim(p),*value=trim(colon+1); active=config_list(config,key);
        if (strcmp(key,"follow_directory_symlinks")==0) {
            active=NULL;
            if (strcmp(value,"true")==0) config->follow_directory_symlinks=true;
            else if (strcmp(value,"false")==0) config->follow_directory_symlinks=false;
            else goto malformed;
        } else if (active==NULL) goto malformed;
        else if (*value!='\0') {
            if (value[0]!='[') goto malformed;
            size_t n=strlen(value); if (n<2U||value[n-1U]!=']') goto malformed;
            value[n-1U]='\0'; char *list=value+1; char *list_save=NULL; char *item=strtok_r(list,",",&list_save);
            while (item!=NULL) { char *decoded=NULL; if (!unquote(item,&decoded)||!strings_add(active,decoded)) goto malformed; item=strtok_r(NULL,",",&list_save); }
        }
        line=strtok_r(NULL,"\n",&save);
    }
    free(copy); return true;
malformed:
    if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"Malformed YAML near line %zu",line_no);
    free(copy); return false;
}

static bool json_list(const MdJson *root,const char *key,Strings *out,char *error,size_t error_cap) {
    const MdJson *value=md_json_get(root,key);
    if (value==NULL||value->type!=MD_JSON_ARRAY) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"JSON key %s must be an array",key);
        return false;
    }
    for (size_t i=0U;i<value->as.array.len;++i) {
        const char *s=md_json_string(value->as.array.items[i]);
        if (s==NULL||!strings_add(out,s)) { if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"Invalid string in %s",key); return false; }
    }
    return true;
}

static bool parse_json_config(Config *config,const char *text,size_t len,char *error,size_t error_cap) {
    MdJsonError parse; MdJson *root=md_json_parse(text,len,&parse);
    if (root==NULL) { if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"Malformed JSON at %zu:%zu: %s",parse.line,parse.column,parse.message); return false; }
    bool follow=false;
    bool ok=root->type==MD_JSON_OBJECT&&
        json_list(root,"include_extensions",&config->include_extensions,error,error_cap)&&
        json_list(root,"source_extensions",&config->source_extensions,error,error_cap)&&
        json_list(root,"test_extensions",&config->test_extensions,error,error_cap)&&
        json_list(root,"documentation_extensions",&config->documentation_extensions,error,error_cap)&&
        json_list(root,"config_build_extensions",&config->config_build_extensions,error,error_cap)&&
        json_list(root,"exclude_dirs",&config->exclude_dirs,error,error_cap)&&
        json_list(root,"exclude_paths",&config->exclude_paths,error,error_cap)&&
        json_list(root,"generated_paths",&config->generated_paths,error,error_cap)&&
        json_list(root,"include_overrides",&config->include_overrides,error,error_cap)&&
        md_json_bool(md_json_get(root,"follow_directory_symlinks"),&follow);
    if (ok) config->follow_directory_symlinks=follow;
    else if (error!=NULL&&error[0]=='\0') (void)snprintf(error,error_cap,"Invalid locscan JSON schema");
    md_json_free(root); return ok;
}

static bool load_config(const char *path,Config *config,char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes);
    if (!md_read_file(path,&bytes,error,error_cap)) { md_bytes_free(&bytes); return false; }
    const char *ext=strrchr(path,'.');
    bool ok=ext!=NULL&&strcmp(ext,".json")==0?
        parse_json_config(config,(const char *)bytes.data,bytes.len,error,error_cap):
        parse_yaml(config,(const char *)bytes.data,bytes.len,error,error_cap);
    md_bytes_free(&bytes); return ok;
}

static bool string_matches(const Strings *patterns,const char *text) {
    for (size_t i=0U;i<patterns->len;++i) if (md_wildmatch(patterns->items[i],text)) return true;
    return false;
}

static bool component_matches(const Strings *patterns,const char *path) {
    char copy[MD_PATH_MAX]; if (strlen(path)>=sizeof(copy)) return false; strcpy(copy,path);
    char *save=NULL,*component=strtok_r(copy,"/",&save);
    while (component!=NULL) {
        if (string_matches(patterns,component)) return true;
        component=strtok_r(NULL,"/",&save);
    }
    return false;
}

static const char *extension_of(const char *path) {
    const char *base=strrchr(path,'/'); base=base==NULL?path:base+1;
    const char *dot=strrchr(base,'.'); return dot==NULL?"":dot;
}

static bool extension_in(const Strings *extensions,const char *path) {
    const char *ext=extension_of(path);
    for (size_t i=0U;i<extensions->len;++i) if (strcmp(extensions->items[i],ext)==0) return true;
    return false;
}

static bool files_push(Files *files,FileInfo info) {
    if (files->len==files->cap) {
        size_t next=files->cap==0U?64U:files->cap*2U; FileInfo *items=realloc(files->items,next*sizeof(*items));
        if (items==NULL) return false;
        files->items=items;
        files->cap=next;
    }
    files->items[files->len++]=info; return true;
}

static int file_compare(const void *left,const void *right) {
    return strcmp(((const FileInfo *)left)->path,((const FileInfo *)right)->path);
}

static bool identity_seen(Identities *ids,dev_t device,ino_t inode) {
    for (size_t i=0U;i<ids->len;++i) if (ids->items[i].device==device&&ids->items[i].inode==inode) return true;
    if (ids->len==ids->cap) {
        size_t next=ids->cap==0U?32U:ids->cap*2U; DirIdentity *items=realloc(ids->items,next*sizeof(*items));
        if (items==NULL) return true;
        ids->items=items;
        ids->cap=next;
    }
    ids->items[ids->len++]=(DirIdentity){device,inode}; return false;
}

static Category classify(const Config *config,const char *path) {
    bool test_extension=extension_in(&config->test_extensions,path);
    bool source_extension=extension_in(&config->source_extensions,path);
    const char *base=strrchr(path,'/'); base=base==NULL?path:base+1;
    bool test_location=strncmp(path,"tests/",6U)==0||strstr(path,"/tests/")!=NULL||
                       strncmp(base,"test_",5U)==0||strstr(base,"_test.")!=NULL;
    if (test_extension&&(test_location||!source_extension)) return CAT_TEST;
    if (extension_in(&config->source_extensions,path)) return CAT_SOURCE;
    if (extension_in(&config->documentation_extensions,path)) return CAT_DOC;
    if (extension_in(&config->config_build_extensions,path)) return CAT_CONFIG;
    return CAT_OTHER;
}

static bool binary_text(const uint8_t *data,size_t len) {
    size_t check=MD_MIN(len,8192U);
    for (size_t i=0U;i<check;++i) if (data[i]==0U) return true;
    return !md_utf8_validate((const char *)data,len,NULL);
}

static uint64_t physical_lines(const uint8_t *data,size_t len) {
    if (len==0U) return 0U;
    uint64_t lines=0U; for (size_t i=0U;i<len;++i) if (data[i]=='\n') ++lines;
    if (data[len-1U]!='\n') ++lines;
    return lines;
}

static bool scan_tree(const char *root,const char *relative,const Config *config,Files *files,
                      Identities *ids,bool *had_error) {
    char full[MD_PATH_MAX];
    if (relative[0]=='\0') strcpy(full,root); else if (!md_path_join(full,root,relative)) return false;
    struct stat root_stat;
    if (stat(full,&root_stat)!=0||identity_seen(ids,root_stat.st_dev,root_stat.st_ino)) return true;
    DIR *dir=opendir(full); if (dir==NULL) { fprintf(stderr,"locscan: cannot read %s: %s\n",full,strerror(errno)); *had_error=true; return true; }
    struct dirent *de;
    while ((de=readdir(dir))!=NULL) {
        if (strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
        char rel[MD_PATH_MAX],path[MD_PATH_MAX];
        if (relative[0]=='\0') { if (strlen(de->d_name)>=sizeof(rel)) continue; strcpy(rel,de->d_name); }
        else if (!md_path_join(rel,relative,de->d_name)) continue;
        if (!md_path_join(path,root,rel)) continue;
        bool override=string_matches(&config->include_overrides,rel);
        struct stat st; if (lstat(path,&st)!=0) { fprintf(stderr,"locscan: cannot stat %s\n",path); *had_error=true; continue; }
        bool is_dir=S_ISDIR(st.st_mode),is_link=S_ISLNK(st.st_mode);
        if (is_link&&config->follow_directory_symlinks) {
            struct stat target; if (stat(path,&target)==0) is_dir=S_ISDIR(target.st_mode);
        }
        if (is_dir) {
            if (!override&&(component_matches(&config->exclude_dirs,rel)||string_matches(&config->exclude_paths,rel)||
                           string_matches(&config->generated_paths,rel))) continue;
            if (is_link&&!config->follow_directory_symlinks) continue;
            if (!scan_tree(root,rel,config,files,ids,had_error)) { closedir(dir); return false; }
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        if (!override&&(string_matches(&config->exclude_paths,rel)||string_matches(&config->generated_paths,rel))) continue;
        if (!override&&!extension_in(&config->include_extensions,rel)) continue;
        MdBytes bytes; md_bytes_init(&bytes); char error[256]; FileInfo info={0};
        info.path=md_strdup(rel); info.bytes=st.st_size<0?0U:(uint64_t)st.st_size; info.category=classify(config,rel);
        if (info.path==NULL) { md_bytes_free(&bytes); closedir(dir); return false; }
        if (!md_read_file(path,&bytes,error,sizeof(error))) { info.unreadable=true; *had_error=true; fprintf(stderr,"locscan: %s\n",error); }
        else if (binary_text(bytes.data,bytes.len)) { free(info.path); md_bytes_free(&bytes); continue; }
        else { info.bytes=(uint64_t)bytes.len; info.lines=physical_lines(bytes.data,bytes.len); }
        md_bytes_free(&bytes);
        if (!files_push(files,info)) { free(info.path); closedir(dir); return false; }
    }
    (void)closedir(dir); return true;
}

static const char *category_name(Category c) {
    static const char *names[]={"source","test","documentation","config_build","other"}; return names[(size_t)c];
}

static bool write_json_report(const char *path,const Files *files) {
    uint64_t lines[5]={0},bytes[5]={0}; size_t counts[5]={0};
    for (size_t i=0U;i<files->len;++i) { size_t c=(size_t)files->items[i].category; lines[c]+=files->items[i].lines; bytes[c]+=files->items[i].bytes; ++counts[c]; }
    MdBuf json; md_buf_init(&json);
    if (!md_buf_append_cstr(&json,"{\n  \"schema_version\": 1,\n  \"categories\": {\n")) goto fail;
    for (size_t c=0U;c<5U;++c) if (!md_buf_appendf(&json,"    \"%s\": {\"files\": %zu, \"lines\": %llu, \"bytes\": %llu}%s\n",
        category_name((Category)c),counts[c],(unsigned long long)lines[c],(unsigned long long)bytes[c],c<4U?",":"")) goto fail;
    uint64_t total=0U; for (size_t c=0U;c<5U;++c) total+=lines[c];
    if (!md_buf_appendf(&json,"  },\n  \"grand_authored_lines\": %llu,\n  \"documentation_lines\": %llu,\n  \"files\": [\n",
        (unsigned long long)total,(unsigned long long)lines[CAT_DOC])) goto fail;
    for (size_t i=0U;i<files->len;++i) {
        if (!md_buf_append_cstr(&json,"    {\"path\": ")||!md_json_write_escaped(&json,files->items[i].path,strlen(files->items[i].path))||
            !md_buf_appendf(&json,", \"category\": \"%s\", \"lines\": %llu, \"bytes\": %llu, \"unreadable\": %s}%s\n",
                category_name(files->items[i].category),(unsigned long long)files->items[i].lines,
                (unsigned long long)files->items[i].bytes,files->items[i].unreadable?"true":"false",i+1U<files->len?",":"")) goto fail;
    }
    if (!md_buf_append_cstr(&json,"  ]\n}\n")) goto fail;
    char error[256]; bool ok=strcmp(path,"-")==0?fwrite(json.data,1U,json.len,stdout)==json.len:
        md_write_file_atomic(path,json.data,json.len,error,sizeof(error));
    if (!ok&&strcmp(path,"-")!=0) fprintf(stderr,"locscan: %s\n",error);
    md_buf_free(&json); return ok;
fail:
    md_buf_free(&json); return false;
}

static void usage(void) {
    fputs("usage: locscan --root DIR --config FILE [--json FILE|-] [--details]\n",stderr);
}

int main(int argc,char **argv) {
    const char *root=NULL,*config_path=NULL,*json_path=NULL; bool details=false;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--root")==0&&i+1<argc) root=argv[++i];
        else if (strcmp(argv[i],"--config")==0&&i+1<argc) config_path=argv[++i];
        else if (strcmp(argv[i],"--json")==0&&i+1<argc) json_path=argv[++i];
        else if (strcmp(argv[i],"--details")==0) details=true;
        else { usage(); return EXIT_USAGE; }
    }
    if (root==NULL||config_path==NULL) { usage(); return EXIT_USAGE; }
    struct stat st; if (stat(root,&st)!=0||!S_ISDIR(st.st_mode)) { fprintf(stderr,"locscan: missing root directory: %s\n",root); return EXIT_INPUT; }
    Config config={0}; char error[256]={0};
    if (!load_config(config_path,&config,error,sizeof(error))) { fprintf(stderr,"locscan: %s\n",error); config_free(&config); return EXIT_CONFIG; }
    Files files={0}; Identities ids={0}; bool had_error=false;
    if (!scan_tree(root,"",&config,&files,&ids,&had_error)) { fprintf(stderr,"locscan: out of memory\n"); config_free(&config); free(ids.items); return EXIT_INTERNAL; }
    if (files.len>1U) qsort(files.items,files.len,sizeof(*files.items),file_compare);
    uint64_t totals[5]={0}; size_t counts[5]={0};
    for (size_t i=0U;i<files.len;++i) { size_t c=(size_t)files.items[i].category; totals[c]+=files.items[i].lines; ++counts[c]; }
    uint64_t grand=0U; for (size_t c=0U;c<5U;++c) { grand+=totals[c]; printf("%-15s %6zu files %10llu lines\n",category_name((Category)c),counts[c],(unsigned long long)totals[c]); }
    printf("grand authored text: %llu lines\nhuman documentation: %llu lines\n",(unsigned long long)grand,(unsigned long long)totals[CAT_DOC]);
    if (details) for (size_t i=0U;i<files.len;++i) printf("%10llu  %-15s %s%s\n",(unsigned long long)files.items[i].lines,category_name(files.items[i].category),files.items[i].path,files.items[i].unreadable?" [UNREADABLE]":"");
    int result=0;
    if (json_path!=NULL&&!write_json_report(json_path,&files)) result=EXIT_OUTPUT;
    else if (had_error) result=EXIT_INPUT;
    for (size_t i=0U;i<files.len;++i) free(files.items[i].path);
    free(files.items); free(ids.items); config_free(&config); return result;
}
