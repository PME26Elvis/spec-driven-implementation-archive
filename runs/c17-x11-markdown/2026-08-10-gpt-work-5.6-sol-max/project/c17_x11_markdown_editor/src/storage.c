#include "mdedit/storage.h"
#include "mdedit/image.h"
#include "mdedit/json.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static MdFaultInjection storage_fault={MD_FAULT_NONE,0U};
static size_t history_max_versions=200U;
static uint64_t history_max_bytes=UINT64_C(64)*1024U*1024U;

static void storage_error(char *error,size_t cap,const char *fmt,...) {
    if (error==NULL||cap==0U) return;
    va_list ap; va_start(ap,fmt); (void)vsnprintf(error,cap,fmt,ap); va_end(ap);
}

void md_storage_set_fault(MdFaultInjection fault) { storage_fault=fault; }
void md_storage_clear_fault(void) { storage_fault=(MdFaultInjection){MD_FAULT_NONE,0U}; }

static bool hash_disk(const char *path,uint8_t digest[32],bool *exists,char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes);
    if (!md_read_file(path,&bytes,error,error_cap)) {
        if (errno==ENOENT) { *exists=false; md_bytes_free(&bytes); return true; }
        md_bytes_free(&bytes); return false;
    }
    *exists=true; md_sha256(bytes.data,bytes.len,digest); md_bytes_free(&bytes); return true;
}

static bool bytes_equal(const uint8_t a[32],const uint8_t b[32]) {
    unsigned diff=0U; for (size_t i=0U;i<32U;++i) diff|=(unsigned)(a[i]^b[i]); return diff==0U;
}

static bool injected_write(int fd,const uint8_t *data,size_t len,size_t *total,
                           char *error,size_t error_cap) {
    if (storage_fault.kind==MD_FAULT_ENOSPC) {
        errno=ENOSPC; storage_error(error,error_cap,"Injected ENOSPC while writing replacement"); return false;
    }
    if (storage_fault.kind==MD_FAULT_EACCES) {
        errno=EACCES; storage_error(error,error_cap,"Injected EACCES while writing replacement"); return false;
    }
    size_t at=0U;
    while (at<len) {
        size_t request=len-at;
        if (storage_fault.kind==MD_FAULT_PARTIAL_WRITE) {
            if (*total>=storage_fault.fail_after) {
                errno=EIO; storage_error(error,error_cap,"Injected partial-write failure after %zu bytes",*total); return false;
            }
            request=MD_MIN(request,storage_fault.fail_after-*total);
        }
        ssize_t n=write(fd,data+at,request);
        if (n<0) {
            if (errno==EINTR) continue;
            storage_error(error,error_cap,"Write failed: %s",strerror(errno)); return false;
        }
        if (n==0) { storage_error(error,error_cap,"Write returned zero bytes"); return false; }
        at+=(size_t)n; *total+=(size_t)n;
    }
    return true;
}

bool md_safe_save_document(MdDocument *doc,const char *path,bool explicit_overwrite,
                           char *error,size_t error_cap) {
    const char *target=path==NULL||path[0]=='\0'?doc->path:path;
    if (target[0]=='\0') { storage_error(error,error_cap,"Save As path is required for an untitled document"); return false; }
    uint8_t disk_hash[32]; bool disk_exists=false;
    if (!hash_disk(target,disk_hash,&disk_exists,error,error_cap)) return false;
    if (disk_exists&&strcmp(target,doc->path)!=0&&!explicit_overwrite) {
        storage_error(error,error_cap,"Save As destination already exists; explicit overwrite confirmation is required");
        return false;
    }
    if (strcmp(target,doc->path)==0&&doc->has_disk_sha256&&disk_exists&&
        !bytes_equal(disk_hash,doc->disk_sha256)&&!explicit_overwrite) {
        doc->conflict=true;
        storage_error(error,error_cap,"The file changed on disk; compare, reload, Save As, or explicitly overwrite");
        return false;
    }
    char dir[MD_PATH_MAX];
    if (!md_path_dirname(dir,target)) { storage_error(error,error_cap,"Destination path is too long"); return false; }
    static unsigned counter=0U;
    char temp[MD_PATH_MAX];
    int n=snprintf(temp,sizeof(temp),"%s/.mdedit-save-%ld-%u",dir,(long)getpid(),++counter);
    if (n<0||(size_t)n>=sizeof(temp)) { storage_error(error,error_cap,"Temporary save path is too long"); return false; }
    mode_t mode=0666; struct stat old;
    if (stat(target,&old)==0) mode=old.st_mode&0777;
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,mode);
    if (fd<0) { storage_error(error,error_cap,"Cannot create replacement for %s: %s; edited data remains in memory",target,strerror(errno)); return false; }
    size_t total=0U;
    if (!injected_write(fd,(const uint8_t *)doc->source.data,doc->source.len,&total,error,error_cap)) {
        (void)close(fd); (void)unlink(temp); return false;
    }
    if (fsync(fd)!=0) {
        storage_error(error,error_cap,"Cannot flush replacement for %s: %s; original remains intact",target,strerror(errno));
        (void)close(fd); (void)unlink(temp); return false;
    }
    int close_result=close(fd);
    if (storage_fault.kind==MD_FAULT_CLOSE) { close_result=-1; errno=EIO; }
    if (close_result!=0) {
        storage_error(error,error_cap,"Close/flush failed for %s: %s; original remains intact",target,strerror(errno));
        (void)unlink(temp); return false;
    }
    if (storage_fault.kind==MD_FAULT_RENAME) {
        errno=EIO; storage_error(error,error_cap,"Injected atomic-rename failure; original remains intact, recovery copy: %s",temp); return false;
    }
    if (rename(temp,target)!=0) {
        storage_error(error,error_cap,"Cannot atomically replace %s: %s; original remains intact, recovery copy: %s",target,strerror(errno),temp); return false;
    }
    int dfd=open(dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (dfd>=0) { (void)fsync(dfd); (void)close(dfd); }
    if (strlen(target)>=sizeof(doc->path)) { storage_error(error,error_cap,"Saved path is too long for the document model"); return false; }
    strcpy(doc->path,target);
    char basename[MD_PATH_MAX];
    if (md_path_basename(basename,target)&&strlen(basename)<sizeof(doc->display_name)) strcpy(doc->display_name,basename);
    doc->untitled=false; doc->orphaned=false; doc->conflict=false; doc->dirty=false;
    md_sha256(doc->source.data,doc->source.len,doc->disk_sha256); doc->has_disk_sha256=true;
    uint8_t id_hash[32]; md_sha256(target,strlen(target),id_hash); md_hex_encode(id_hash,32U,doc->id);
    return true;
}

void md_preferences_defaults(MdPreferences *prefs) {
    *prefs=(MdPreferences){false,16,1.35,false,true,30,MD_MODE_SOURCE,true,true};
}

bool md_preferences_path(char out[MD_PATH_MAX]) {
    const char *base=getenv("XDG_CONFIG_HOME");
    char fallback[MD_PATH_MAX];
    if (base==NULL||base[0]=='\0') {
        const char *home=getenv("HOME"); if (home==NULL||home[0]=='\0') return false;
        if (!md_path_join(fallback,home,".config")) return false;
        base=fallback;
    }
    char dir[MD_PATH_MAX]; if (!md_path_join(dir,base,"mdeditor")) return false;
    char ignored[128]; if (!md_mkdirs(dir,0700,ignored,sizeof(ignored))) return false;
    return md_path_join(out,dir,"preferences.json");
}

static bool json_mode(const MdJson *value,MdEditorMode *mode) {
    const char *s=md_json_string(value); if (s==NULL) return false;
    if (strcmp(s,"source")==0) *mode=MD_MODE_SOURCE;
    else if (strcmp(s,"split")==0) *mode=MD_MODE_SPLIT;
    else if (strcmp(s,"preview")==0) *mode=MD_MODE_PREVIEW;
    else if (strcmp(s,"rendered")==0) *mode=MD_MODE_RENDERED;
    else return false;
    return true;
}

bool md_preferences_load(MdPreferences *prefs,char *warning,size_t warning_cap) {
    md_preferences_defaults(prefs);
    char path[MD_PATH_MAX]; if (!md_preferences_path(path)) { storage_error(warning,warning_cap,"Cannot determine preferences path; defaults are active"); return false; }
    MdBytes bytes; md_bytes_init(&bytes); char read_error[256];
    if (!md_read_file(path,&bytes,read_error,sizeof(read_error))) {
        md_bytes_free(&bytes); if (errno==ENOENT) return true;
        storage_error(warning,warning_cap,"Preferences could not be read; defaults are active"); return false;
    }
    MdJsonError parse_error; MdJson *root=md_json_parse((const char *)bytes.data,bytes.len,&parse_error);
    md_bytes_free(&bytes);
    bool ok=root!=NULL&&root->type==MD_JSON_OBJECT;
    uint64_t number=0U; bool boolean=false; MdEditorMode mode=MD_MODE_SOURCE;
    const char *theme=ok?md_json_string(md_json_get(root,"theme")):NULL;
    if (theme==NULL||(strcmp(theme,"light")!=0&&strcmp(theme,"dark")!=0)) ok=false;
    if (ok) prefs->dark_theme=strcmp(theme,"dark")==0;
    if (!ok||!md_json_u64(md_json_get(root,"font_size"),&number)||number<10U||number>32U) ok=false;
    else prefs->font_size=(int)number;
    const MdJson *spacing=ok?md_json_get(root,"line_spacing"):NULL;
    if (spacing==NULL||spacing->type!=MD_JSON_NUMBER||spacing->as.number<1.0||spacing->as.number>1.8) ok=false;
    else prefs->line_spacing=spacing->as.number;
    if (!ok||!md_json_bool(md_json_get(root,"embed_images"),&boolean)) ok=false; else prefs->default_embed_images=boolean;
    if (!ok||!md_json_bool(md_json_get(root,"autosave_enabled"),&boolean)) ok=false; else prefs->autosave_enabled=boolean;
    if (!ok||!md_json_u64(md_json_get(root,"autosave_interval"),&number)||number<10U||number>300U) ok=false; else prefs->autosave_interval=(int)number;
    if (!ok||!json_mode(md_json_get(root,"default_mode"),&mode)) ok=false; else prefs->default_mode=mode;
    if (!ok||!md_json_bool(md_json_get(root,"sync_scroll"),&boolean)) ok=false; else prefs->sync_scroll=boolean;
    if (!ok||!md_json_bool(md_json_get(root,"restore_session"),&boolean)) ok=false; else prefs->restore_session=boolean;
    md_json_free(root);
    if (!ok) {
        md_preferences_defaults(prefs);
        char corrupt[MD_PATH_MAX]; int n=snprintf(corrupt,sizeof(corrupt),"%s.corrupt-%llu",path,(unsigned long long)md_now_unix());
        if (n>0&&(size_t)n<sizeof(corrupt)) (void)rename(path,corrupt);
        storage_error(warning,warning_cap,"Preferences were malformed and preserved as a .corrupt file; defaults are active");
        return false;
    }
    return true;
}

bool md_preferences_save(const MdPreferences *prefs,char *error,size_t error_cap) {
    char path[MD_PATH_MAX]; if (!md_preferences_path(path)) { storage_error(error,error_cap,"Cannot determine preferences path"); return false; }
    const char *mode=prefs->default_mode==MD_MODE_SPLIT?"split":prefs->default_mode==MD_MODE_PREVIEW?"preview":prefs->default_mode==MD_MODE_RENDERED?"rendered":"source";
    MdBuf json; md_buf_init(&json);
    bool ok=md_buf_appendf(&json,"{\n  \"schema_version\": 1,\n  \"theme\": \"%s\",\n  \"font_size\": %d,\n  \"line_spacing\": %.2f,\n  \"embed_images\": %s,\n  \"autosave_enabled\": %s,\n  \"autosave_interval\": %d,\n  \"default_mode\": \"%s\",\n  \"sync_scroll\": %s,\n  \"restore_session\": %s\n}\n",
        prefs->dark_theme?"dark":"light",prefs->font_size,prefs->line_spacing,
        prefs->default_embed_images?"true":"false",prefs->autosave_enabled?"true":"false",
        prefs->autosave_interval,mode,prefs->sync_scroll?"true":"false",prefs->restore_session?"true":"false");
    if (!ok) { md_buf_free(&json); storage_error(error,error_cap,"Out of memory writing preferences"); return false; }
    ok=md_write_file_atomic(path,json.data,json.len,error,error_cap); md_buf_free(&json); return ok;
}

static bool recent_path(char out[MD_PATH_MAX]) {
    char preferences[MD_PATH_MAX],dir[MD_PATH_MAX];
    return md_preferences_path(preferences)&&md_path_dirname(dir,preferences)&&md_path_join(out,dir,"recent.json");
}

static void recent_list_free(char ***items,size_t *count) {
    for (size_t i=0U;i<*count;++i) free((*items)[i]);
    free(*items); *items=NULL; *count=0U;
}

static bool recent_list_add(char ***items,size_t *count,size_t maximum,const char *path) {
    char resolved[MD_PATH_MAX]; const char *value=realpath(path,resolved); if (value==NULL) value=path;
    size_t existing=*count;
    for (size_t i=0U;i<*count;++i) if (strcmp((*items)[i],value)==0) { existing=i; break; }
    char *copy=md_strdup(value); if (copy==NULL) return false;
    if (existing<*count) { free((*items)[existing]); memmove(*items+1U,*items,existing*sizeof(**items)); (*items)[0]=copy; return true; }
    size_t next=MD_MIN(*count+1U,maximum); char **grown=realloc(*items,next*sizeof(*grown)); if (grown==NULL) { free(copy); return false; }
    *items=grown;
    if (*count==maximum) free((*items)[maximum-1U]);
    if (next>1U) memmove(*items+1U,*items,(next-1U)*sizeof(**items));
    (*items)[0]=copy; *count=next; return true;
}

bool md_recent_add_file(MdWorkspace *ws,const char *path) { return recent_list_add(&ws->recent_files,&ws->recent_file_count,20U,path); }
bool md_recent_add_workspace(MdWorkspace *ws,const char *path) { return recent_list_add(&ws->recent_workspaces,&ws->recent_workspace_count,10U,path); }
void md_recent_clear_files(MdWorkspace *ws) { recent_list_free(&ws->recent_files,&ws->recent_file_count); }
void md_recent_clear_workspaces(MdWorkspace *ws) { recent_list_free(&ws->recent_workspaces,&ws->recent_workspace_count); }

static bool recent_list_remove(char ***items,size_t *count,const char *path) {
    char resolved[MD_PATH_MAX]; const char *value=realpath(path,resolved); if (value==NULL) value=path;
    for (size_t i=0U;i<*count;++i) {
        if (strcmp((*items)[i],value)!=0&&strcmp((*items)[i],path)!=0) continue;
        free((*items)[i]);
        memmove(*items+i,*items+i+1U,(*count-i-1U)*sizeof(**items));
        --*count;
        if (*count==0U) { free(*items); *items=NULL; }
        return true;
    }
    return false;
}

bool md_recent_remove_file(MdWorkspace *ws,const char *path) {
    return recent_list_remove(&ws->recent_files,&ws->recent_file_count,path);
}

bool md_recent_remove_workspace(MdWorkspace *ws,const char *path) {
    return recent_list_remove(&ws->recent_workspaces,&ws->recent_workspace_count,path);
}

bool md_recent_save(const MdWorkspace *ws,char *error,size_t error_cap) {
    char path[MD_PATH_MAX]; if (!recent_path(path)) { storage_error(error,error_cap,"Cannot determine recent-items path"); return false; }
    MdBuf json; md_buf_init(&json); bool ok=md_buf_append_cstr(&json,"{\n  \"schema_version\": 1,\n  \"workspaces\": [");
    for (size_t i=0U;ok&&i<ws->recent_workspace_count;++i) ok=(i==0U||md_buf_append_cstr(&json,", "))&&md_json_write_escaped(&json,ws->recent_workspaces[i],strlen(ws->recent_workspaces[i]));
    if (ok) ok=md_buf_append_cstr(&json,"],\n  \"files\": [");
    for (size_t i=0U;ok&&i<ws->recent_file_count;++i) ok=(i==0U||md_buf_append_cstr(&json,", "))&&md_json_write_escaped(&json,ws->recent_files[i],strlen(ws->recent_files[i]));
    if (ok) ok=md_buf_append_cstr(&json,"]\n}\n");
    if (ok) ok=md_write_file_atomic(path,json.data,json.len,error,error_cap); else storage_error(error,error_cap,"Out of memory saving recent items");
    md_buf_free(&json); return ok;
}

static bool recent_load_array(const MdJson *root,const char *key,char ***items,size_t *count,size_t maximum) {
    const MdJson *array=md_json_get(root,key); if (array==NULL||array->type!=MD_JSON_ARRAY||array->as.array.len>maximum) return false;
    char **loaded=array->as.array.len==0U?NULL:calloc(array->as.array.len,sizeof(*loaded)); if (array->as.array.len>0U&&loaded==NULL) return false;
    for (size_t i=0U;i<array->as.array.len;++i) { const char *value=md_json_string(array->as.array.items[i]); if (value==NULL||(loaded[i]=md_strdup(value))==NULL) { for (size_t j=0U;j<=i;++j) free(loaded[j]); free(loaded); return false; } }
    recent_list_free(items,count); *items=loaded; *count=array->as.array.len; return true;
}

bool md_recent_load(MdWorkspace *ws,char *warning,size_t warning_cap) {
    char path[MD_PATH_MAX]; if (!recent_path(path)) { storage_error(warning,warning_cap,"Cannot determine recent-items path"); return false; }
    MdBytes bytes; md_bytes_init(&bytes); char read_error[256];
    if (!md_read_file(path,&bytes,read_error,sizeof(read_error))) { md_bytes_free(&bytes); return errno==ENOENT; }
    MdJsonError parse; MdJson *root=md_json_parse((const char *)bytes.data,bytes.len,&parse); md_bytes_free(&bytes); uint64_t schema=0U;
    bool ok=root!=NULL&&root->type==MD_JSON_OBJECT&&md_json_u64(md_json_get(root,"schema_version"),&schema)&&schema==1U&&
            recent_load_array(root,"workspaces",&ws->recent_workspaces,&ws->recent_workspace_count,10U)&&
            recent_load_array(root,"files",&ws->recent_files,&ws->recent_file_count,20U);
    md_json_free(root);
    if (!ok) { md_recent_clear_files(ws); md_recent_clear_workspaces(ws); char corrupt[MD_PATH_MAX]; int n=snprintf(corrupt,sizeof(corrupt),"%s.corrupt-%llu",path,(unsigned long long)md_now_unix()); if (n>0&&(size_t)n<sizeof(corrupt)) (void)rename(path,corrupt); storage_error(warning,warning_cap,"Recent items were malformed and preserved; starting with empty lists"); }
    return ok;
}

void md_workspace_init(MdWorkspace *ws) {
    memset(ws,0,sizeof(*ws)); ws->sidebar_width=280.0;
}

void md_workspace_free(MdWorkspace *ws) {
    free(ws->entries);
    for (size_t i=0U;i<ws->recent_file_count;++i) free(ws->recent_files[i]);
    for (size_t i=0U;i<ws->recent_workspace_count;++i) free(ws->recent_workspaces[i]);
    for (size_t i=0U;i<ws->collapsed_directory_count;++i) free(ws->collapsed_directories[i]);
    free(ws->recent_files); free(ws->recent_workspaces); free(ws->collapsed_directories); md_workspace_init(ws);
}

bool md_workspace_directory_collapsed(const MdWorkspace *ws,const char *relative) {
    for (size_t i=0U;i<ws->collapsed_directory_count;++i)
        if (strcmp(ws->collapsed_directories[i],relative)==0) return true;
    return false;
}

bool md_workspace_set_directory_collapsed(MdWorkspace *ws,const char *relative,bool collapsed) {
    char normalized[MD_PATH_MAX];
    if (!md_path_normalize_relative(relative,normalized)||strcmp(normalized,".")==0) return false;
    size_t found=ws->collapsed_directory_count;
    for (size_t i=0U;i<ws->collapsed_directory_count;++i)
        if (strcmp(ws->collapsed_directories[i],normalized)==0) { found=i; break; }
    if (!collapsed) {
        if (found==ws->collapsed_directory_count) return true;
        free(ws->collapsed_directories[found]);
        memmove(ws->collapsed_directories+found,ws->collapsed_directories+found+1U,
                (ws->collapsed_directory_count-found-1U)*sizeof(*ws->collapsed_directories));
        --ws->collapsed_directory_count;
        return true;
    }
    if (found<ws->collapsed_directory_count) return true;
    if (ws->collapsed_directory_count==ws->collapsed_directory_cap) {
        size_t next=ws->collapsed_directory_cap==0U?8U:ws->collapsed_directory_cap*2U;
        char **grown=realloc(ws->collapsed_directories,next*sizeof(*grown));
        if (grown==NULL) return false;
        ws->collapsed_directories=grown; ws->collapsed_directory_cap=next;
    }
    char *copy=md_strdup(normalized); if (copy==NULL) return false;
    ws->collapsed_directories[ws->collapsed_directory_count++]=copy;
    return true;
}

typedef struct {
    char *name;
    bool directory;
    bool symlink;
    uint64_t size;
} ScanItem;

static int scan_item_compare(const void *left,const void *right) {
    const ScanItem *a=left,*b=right;
    if (a->directory!=b->directory) return a->directory?-1:1;
    int folded=strcasecmp(a->name,b->name); if (folded!=0) return folded;
    return strcmp(a->name,b->name);
}

static bool workspace_entry(MdWorkspace *ws,const char *relative,bool directory,
                            bool symlink,int depth,uint64_t size) {
    if (ws->count==ws->cap) {
        size_t next=ws->cap==0U?64U:ws->cap*2U;
        if (next<ws->cap) return false;
        MdTreeEntry *entries=realloc(ws->entries,next*sizeof(*entries));
        if (entries==NULL) return false;
        ws->entries=entries; ws->cap=next;
    }
    if (strlen(relative)>=sizeof(ws->entries[ws->count].path)) return false;
    MdTreeEntry *entry=&ws->entries[ws->count++];
    strcpy(entry->path,relative); entry->is_directory=directory; entry->is_symlink=symlink;
    entry->depth=depth; entry->size=size; return true;
}

static void scan_items_free(ScanItem *items,size_t count) {
    for (size_t i=0U;i<count;++i) free(items[i].name);
    free(items);
}

static bool workspace_scan_dir(MdWorkspace *ws,const char *relative,int depth,
                               char *error,size_t error_cap) {
    if (depth>128) { storage_error(error,error_cap,"Workspace nesting exceeds 128 levels"); return false; }
    char full[MD_PATH_MAX];
    if (relative[0]=='\0') strcpy(full,ws->root);
    else if (!md_path_join(full,ws->root,relative)) { storage_error(error,error_cap,"Workspace path is too long"); return false; }
    DIR *dir=opendir(full);
    if (dir==NULL) { storage_error(error,error_cap,"Cannot read workspace directory %s: %s",full,strerror(errno)); return false; }
    ScanItem *items=NULL; size_t count=0U,cap=0U; struct dirent *de;
    while ((de=readdir(dir))!=NULL) {
        if (strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0||
            (relative[0]=='\0'&&strcmp(de->d_name,".mdeditor")==0)) continue;
        char child_rel[MD_PATH_MAX],child_full[MD_PATH_MAX];
        if (relative[0]=='\0') {
            if (strlen(de->d_name)>=sizeof(child_rel)) { closedir(dir); scan_items_free(items,count); storage_error(error,error_cap,"Workspace filename is too long"); return false; }
            strcpy(child_rel,de->d_name);
        } else if (!md_path_join(child_rel,relative,de->d_name)) { closedir(dir); scan_items_free(items,count); storage_error(error,error_cap,"Workspace path is too long"); return false; }
        if (!md_path_join(child_full,ws->root,child_rel)) { closedir(dir); scan_items_free(items,count); return false; }
        struct stat st;
        if (lstat(child_full,&st)!=0) { closedir(dir); scan_items_free(items,count); storage_error(error,error_cap,"Cannot inspect %s: %s",child_full,strerror(errno)); return false; }
        if (count==cap) {
            size_t next=cap==0U?16U:cap*2U; ScanItem *grown=realloc(items,next*sizeof(*items));
            if (grown==NULL) { closedir(dir); scan_items_free(items,count); storage_error(error,error_cap,"Out of memory scanning workspace"); return false; }
            items=grown; cap=next;
        }
        items[count].name=md_strdup(de->d_name);
        if (items[count].name==NULL) { closedir(dir); scan_items_free(items,count); storage_error(error,error_cap,"Out of memory scanning workspace"); return false; }
        items[count].symlink=S_ISLNK(st.st_mode);
        items[count].directory=S_ISDIR(st.st_mode);
        items[count].size=st.st_size<0?0U:(uint64_t)st.st_size; ++count;
    }
    if (closedir(dir)!=0) { scan_items_free(items,count); storage_error(error,error_cap,"Cannot close workspace directory %s",full); return false; }
    if (count>1U) qsort(items,count,sizeof(*items),scan_item_compare);
    for (size_t i=0U;i<count;++i) {
        char child_rel[MD_PATH_MAX];
        if (relative[0]=='\0') strcpy(child_rel,items[i].name);
        else if (!md_path_join(child_rel,relative,items[i].name)) { scan_items_free(items,count); return false; }
        if (!workspace_entry(ws,child_rel,items[i].directory,items[i].symlink,depth,items[i].size)) {
            scan_items_free(items,count); storage_error(error,error_cap,"Out of memory recording workspace tree"); return false;
        }
        if (items[i].directory&&!items[i].symlink&&
            !workspace_scan_dir(ws,child_rel,depth+1,error,error_cap)) { scan_items_free(items,count); return false; }
    }
    scan_items_free(items,count); return true;
}

bool md_workspace_scan(MdWorkspace *ws,char *error,size_t error_cap) {
    ws->count=0U;
    if (ws->root[0]=='\0') { storage_error(error,error_cap,"No workspace is open"); return false; }
    return workspace_scan_dir(ws,"",0,error,error_cap);
}

bool md_workspace_open(MdWorkspace *ws,const char *root,char *error,size_t error_cap) {
    char resolved[MD_PATH_MAX];
    if (realpath(root,resolved)==NULL) { storage_error(error,error_cap,"Cannot open workspace %s: %s",root,strerror(errno)); return false; }
    struct stat st;
    if (stat(resolved,&st)!=0||!S_ISDIR(st.st_mode)) { storage_error(error,error_cap,"Workspace root is not a directory: %s",resolved); return false; }
    if (strlen(resolved)>=sizeof(ws->root)) { storage_error(error,error_cap,"Workspace path is too long"); return false; }
    strcpy(ws->root,resolved);
    char metadata[MD_PATH_MAX];
    if (!md_path_join(metadata,ws->root,".mdeditor")||!md_mkdirs(metadata,0700,error,error_cap)) return false;
    return md_workspace_scan(ws,error,error_cap);
}

bool md_workspace_save_session(const MdWorkspace *ws,const MdDocument *docs,size_t doc_count,
                               size_t active_doc,char *error,size_t error_cap) {
    if (ws->root[0]=='\0') return true;
    MdBuf json; md_buf_init(&json);
    if (!md_buf_appendf(&json,"{\n  \"schema_version\": 1,\n  \"active_tab\": %zu,\n  \"sidebar_width\": %.2f,\n  \"sidebar_collapsed\": %s,\n  \"collapsed_directories\": [",
        active_doc,ws->sidebar_width,ws->sidebar_collapsed?"true":"false")) goto oom;
    for (size_t i=0U;i<ws->collapsed_directory_count;++i) {
        if ((i>0U&&!md_buf_append_cstr(&json,", "))||
            !md_json_write_escaped(&json,ws->collapsed_directories[i],strlen(ws->collapsed_directories[i]))) goto oom;
    }
    if (!md_buf_append_cstr(&json,"],\n  \"tabs\": [\n")) goto oom;
    for (size_t i=0U;i<doc_count;++i) {
        const MdDocument *doc=&docs[i];
        if (!md_buf_append_cstr(&json,"    {\"path\": ")||
            !md_json_write_escaped(&json,doc->path,strlen(doc->path))||
            !md_buf_appendf(&json,", \"mode\": %d, \"cursor\": %zu, \"anchor\": %zu, \"source_scroll\": %.3f, \"preview_scroll\": %.3f, \"zoom\": %.3f, \"split_ratio\": %.3f, \"dirty\": %s}%s\n",
                (int)doc->mode,doc->cursor,doc->anchor,doc->source_scroll,doc->preview_scroll,
                doc->zoom,doc->split_ratio,doc->dirty?"true":"false",i+1U<doc_count?",":"")) goto oom;
    }
    if (!md_buf_append_cstr(&json,"  ]\n}\n")) goto oom;
    char path[MD_PATH_MAX]; if (!md_path_join(path,ws->root,".mdeditor/session.json")) goto oom;
    {
        bool ok=md_write_file_atomic(path,json.data,json.len,error,error_cap); md_buf_free(&json); return ok;
    }
oom:
    md_buf_free(&json); storage_error(error,error_cap,"Out of memory writing workspace session"); return false;
}

bool md_workspace_load_session(MdWorkspace *ws,MdBuf *json,char *warning,size_t warning_cap) {
    json->len=0U; if (json->data!=NULL) json->data[0]='\0';
    if (ws->root[0]=='\0') return true;
    char path[MD_PATH_MAX]; if (!md_path_join(path,ws->root,".mdeditor/session.json")) return false;
    MdBytes bytes; md_bytes_init(&bytes); char read_error[256];
    if (!md_read_file(path,&bytes,read_error,sizeof(read_error))) { md_bytes_free(&bytes); return errno==ENOENT; }
    MdJsonError parse_error; MdJson *root=md_json_parse((const char *)bytes.data,bytes.len,&parse_error);
    bool valid=root!=NULL&&root->type==MD_JSON_OBJECT&&md_json_get(root,"tabs")!=NULL&&
               md_json_get(root,"tabs")->type==MD_JSON_ARRAY;
    md_json_free(root);
    if (!valid) {
        char corrupt[MD_PATH_MAX]; int n=snprintf(corrupt,sizeof(corrupt),"%s.corrupt-%llu",path,(unsigned long long)md_now_unix());
        if (n>0&&(size_t)n<sizeof(corrupt)) (void)rename(path,corrupt);
        storage_error(warning,warning_cap,"Workspace session was corrupt and preserved; starting with an empty session");
        md_bytes_free(&bytes); return false;
    }
    bool ok=md_buf_assign(json,(const char *)bytes.data,bytes.len); md_bytes_free(&bytes);
    if (!ok) storage_error(warning,warning_cap,"Out of memory loading workspace session");
    return ok;
}

static bool workspace_target(const MdWorkspace *ws,const char *relative,char out[MD_PATH_MAX],
                             char *error,size_t error_cap) {
    char normalized[MD_PATH_MAX];
    if (!md_path_normalize_relative(relative,normalized)||strcmp(normalized,".")==0||
        !md_path_join(out,ws->root,normalized)) { storage_error(error,error_cap,"Unsafe or invalid workspace-relative path"); return false; }
    return true;
}

bool md_workspace_create_file(MdWorkspace *ws,const char *relative,bool directory,
                              char *error,size_t error_cap) {
    char path[MD_PATH_MAX]; if (!workspace_target(ws,relative,path,error,error_cap)) return false;
    if (directory) {
        if (mkdir(path,0755)!=0) { storage_error(error,error_cap,"Cannot create folder %s: %s",path,strerror(errno)); return false; }
    } else {
        int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
        if (fd<0) { storage_error(error,error_cap,"Cannot create file %s: %s",path,strerror(errno)); return false; }
        if (close(fd)!=0) { storage_error(error,error_cap,"Cannot close new file %s: %s",path,strerror(errno)); return false; }
    }
    return md_workspace_scan(ws,error,error_cap);
}

bool md_workspace_rename(MdWorkspace *ws,const char *old_relative,const char *new_relative,
                         char *error,size_t error_cap) {
    char old_path[MD_PATH_MAX],new_path[MD_PATH_MAX];
    if (!workspace_target(ws,old_relative,old_path,error,error_cap)||
        !workspace_target(ws,new_relative,new_path,error,error_cap)) return false;
    struct stat st;
    if (lstat(new_path,&st)==0) { storage_error(error,error_cap,"Rename destination already exists: %s",new_path); return false; }
    if (rename(old_path,new_path)!=0) { storage_error(error,error_cap,"Cannot rename %s: %s",old_path,strerror(errno)); return false; }
    return md_workspace_scan(ws,error,error_cap);
}

static bool workspace_delete_tree(const char *path,char *error,size_t error_cap) {
    struct stat st;
    if (lstat(path,&st)!=0) {
        storage_error(error,error_cap,"Cannot inspect delete target %s: %s",path,strerror(errno));
        return false;
    }
    if (!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode)) {
        if (unlink(path)!=0) { storage_error(error,error_cap,"Cannot delete %s: %s",path,strerror(errno)); return false; }
        return true;
    }
    DIR *dir=opendir(path);
    if (dir==NULL) { storage_error(error,error_cap,"Cannot open folder for deletion %s: %s",path,strerror(errno)); return false; }
    struct dirent *entry; bool ok=true;
    while (ok&&(entry=readdir(dir))!=NULL) {
        if (strcmp(entry->d_name,".")==0||strcmp(entry->d_name,"..")==0) continue;
        char child[MD_PATH_MAX];
        if (!md_path_join(child,path,entry->d_name)) { storage_error(error,error_cap,"Delete path is too long"); ok=false; break; }
        ok=workspace_delete_tree(child,error,error_cap);
    }
    if (closedir(dir)!=0&&ok) { storage_error(error,error_cap,"Cannot close folder during deletion: %s",strerror(errno)); ok=false; }
    if (ok&&rmdir(path)!=0) { storage_error(error,error_cap,"Cannot delete folder %s: %s",path,strerror(errno)); ok=false; }
    return ok;
}

bool md_workspace_delete(MdWorkspace *ws,const char *relative,bool recursive,
                         char *error,size_t error_cap) {
    char path[MD_PATH_MAX]; if (!workspace_target(ws,relative,path,error,error_cap)) return false;
    struct stat st;
    if (lstat(path,&st)!=0) { storage_error(error,error_cap,"Delete target does not exist: %s",path); return false; }
    bool ok;
    if (S_ISDIR(st.st_mode)&&!S_ISLNK(st.st_mode)&&!recursive) {
        ok=rmdir(path)==0;
        if (!ok) storage_error(error,error_cap,"Folder is not empty; confirm recursive deletion to continue: %s",path);
    } else ok=workspace_delete_tree(path,error,error_cap);
    if (!ok) return false;
    (void)md_workspace_set_directory_collapsed(ws,relative,false);
    return md_workspace_scan(ws,error,error_cap);
}

#define HISTORY_HEADER_SIZE 76U
#define HISTORY_FLAG_FULL 1U
#define HISTORY_FLAG_COMPRESSED 2U
#define HISTORY_FLAG_PINNED 4U

typedef struct {
    uint64_t sequence;
    uint64_t timestamp;
    uint32_t flags;
    uint64_t raw_size;
    uint64_t payload_size;
    uint8_t payload_sha[32];
} HistoryHeader;

static void store_u32(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); p[2]=(uint8_t)(v>>16U); p[3]=(uint8_t)(v>>24U);
}

static void store_u64(uint8_t *p,uint64_t v) {
    for (size_t i=0U;i<8U;++i) p[i]=(uint8_t)(v>>(i*8U));
}

static uint32_t load_u32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}

static uint64_t load_u64(const uint8_t *p) {
    uint64_t v=0U; for (size_t i=0U;i<8U;++i) v|=(uint64_t)p[i]<<(i*8U); return v;
}

static void history_header_write(uint8_t out[HISTORY_HEADER_SIZE],const HistoryHeader *h) {
    memcpy(out,"MDH1",4U); store_u32(out+4U,1U); store_u64(out+8U,h->sequence);
    store_u64(out+16U,h->timestamp); store_u32(out+24U,h->flags);
    store_u64(out+28U,h->raw_size); store_u64(out+36U,h->payload_size);
    memcpy(out+44U,h->payload_sha,32U);
}

static bool history_header_read(const uint8_t *data,size_t len,HistoryHeader *h) {
    if (len<HISTORY_HEADER_SIZE||memcmp(data,"MDH1",4U)!=0||load_u32(data+4U)!=1U) return false;
    h->sequence=load_u64(data+8U); h->timestamp=load_u64(data+16U); h->flags=load_u32(data+24U);
    h->raw_size=load_u64(data+28U); h->payload_size=load_u64(data+36U); memcpy(h->payload_sha,data+44U,32U);
    return h->sequence!=0U&&h->payload_size==len-HISTORY_HEADER_SIZE&&h->raw_size<=SIZE_MAX;
}

void md_version_list_init(MdVersionList *list) { memset(list,0,sizeof(*list)); }

void md_version_list_free(MdVersionList *list) { free(list->items); memset(list,0,sizeof(*list)); }

static int version_compare(const void *left,const void *right) {
    const MdVersionInfo *a=left,*b=right;
    return a->sequence<b->sequence?-1:a->sequence>b->sequence?1:0;
}

static bool version_push(MdVersionList *list,MdVersionInfo info) {
    if (list->count==list->cap) {
        size_t next=list->cap==0U?16U:list->cap*2U;
        MdVersionInfo *items=realloc(list->items,next*sizeof(*items));
        if (items==NULL) return false;
        list->items=items;
        list->cap=next;
    }
    list->items[list->count++]=info; return true;
}

static bool history_dir(char out[MD_PATH_MAX],const char *root,const MdDocument *doc) {
    return root[0]!='\0'&&md_path_join(out,root,doc->id);
}

bool md_history_list(const char *history_root,const MdDocument *doc,MdVersionList *list,
                     char *error,size_t error_cap) {
    md_version_list_free(list);
    char dir_path[MD_PATH_MAX]; if (!history_dir(dir_path,history_root,doc)) { storage_error(error,error_cap,"History root is unavailable"); return false; }
    DIR *dir=opendir(dir_path);
    if (dir==NULL) { if (errno==ENOENT) return true; storage_error(error,error_cap,"Cannot read history directory: %s",strerror(errno)); return false; }
    struct dirent *de;
    while ((de=readdir(dir))!=NULL) {
        size_t n=strlen(de->d_name); if (n<5U||strcmp(de->d_name+n-4U,".mhv")!=0) continue;
        char path[MD_PATH_MAX]; if (!md_path_join(path,dir_path,de->d_name)) { closedir(dir); return false; }
        MdBytes bytes; md_bytes_init(&bytes); char local_error[256];
        if (!md_read_file(path,&bytes,local_error,sizeof(local_error))) { md_bytes_free(&bytes); closedir(dir); storage_error(error,error_cap,"Cannot read history record %s",path); return false; }
        HistoryHeader h;
        if (!history_header_read(bytes.data,bytes.len,&h)) { md_bytes_free(&bytes); closedir(dir); storage_error(error,error_cap,"Corrupt history header: %s",path); return false; }
        MdVersionInfo info={h.sequence,h.timestamp,(h.flags&HISTORY_FLAG_PINNED)!=0U,
                            (h.flags&HISTORY_FLAG_FULL)!=0U,(uint64_t)bytes.len,{0}};
        strcpy(info.record_path,path); md_bytes_free(&bytes);
        if (!version_push(list,info)) { closedir(dir); md_version_list_free(list); storage_error(error,error_cap,"Out of memory listing history"); return false; }
    }
    if (closedir(dir)!=0) { md_version_list_free(list); storage_error(error,error_cap,"Cannot close history directory"); return false; }
    if (list->count>1U) qsort(list->items,list->count,sizeof(*list->items),version_compare);
    for (size_t i=1U;i<list->count;++i) {
        if (list->items[i-1U].sequence>=list->items[i].sequence) { md_version_list_free(list); storage_error(error,error_cap,"Duplicate/out-of-order history sequence"); return false; }
    }
    return true;
}

static bool history_payload(const char *path,HistoryHeader *header,MdBytes *raw,
                            char *error,size_t error_cap) {
    MdBytes file; md_bytes_init(&file);
    if (!md_read_file(path,&file,error,error_cap)) { md_bytes_free(&file); return false; }
    if (!history_header_read(file.data,file.len,header)) { md_bytes_free(&file); storage_error(error,error_cap,"History record header/integrity length is invalid: %s",path); return false; }
    const uint8_t *payload=file.data+HISTORY_HEADER_SIZE;
    if ((header->flags&HISTORY_FLAG_COMPRESSED)!=0U) {
        if (!md_lzss_decompress(payload,(size_t)header->payload_size,(size_t)header->raw_size,raw,error,error_cap)) { md_bytes_free(&file); return false; }
    } else if (!md_bytes_append(raw,payload,(size_t)header->payload_size)||raw->len!=(size_t)header->raw_size) {
        md_bytes_free(&file); storage_error(error,error_cap,"History payload allocation/size mismatch"); return false;
    }
    uint8_t sha[32]; md_sha256(raw->data,raw->len,sha); md_bytes_free(&file);
    if (!bytes_equal(sha,header->payload_sha)) { raw->len=0U; storage_error(error,error_cap,"History payload SHA-256 mismatch: %s",path); return false; }
    return true;
}

bool md_history_reconstruct(const MdVersionList *list,size_t index,MdBuf *source,
                            char *error,size_t error_cap) {
    if (index>=list->count) { storage_error(error,error_cap,"History version index is out of range"); return false; }
    size_t start=index;
    while (start>0U&&!list->items[start].full_snapshot) --start;
    if (!list->items[start].full_snapshot) { storage_error(error,error_cap,"History chain has no full snapshot"); return false; }
    MdBuf current; md_buf_init(&current);
    for (size_t i=start;i<=index;++i) {
        HistoryHeader header; MdBytes payload; md_bytes_init(&payload);
        if (!history_payload(list->items[i].record_path,&header,&payload,error,error_cap)) { md_bytes_free(&payload); md_buf_free(&current); return false; }
        if ((header.flags&HISTORY_FLAG_FULL)!=0U) {
            if (!md_buf_assign(&current,(const char *)payload.data,payload.len)) { md_bytes_free(&payload); md_buf_free(&current); storage_error(error,error_cap,"Out of memory reconstructing snapshot"); return false; }
        } else {
            MdBuf next; md_buf_init(&next);
            if (!md_delta_apply(current.data,current.len,payload.data,payload.len,&next,error,error_cap)) {
                md_bytes_free(&payload); md_buf_free(&current); md_buf_free(&next); return false;
            }
            md_buf_free(&current); current=next;
        }
        md_bytes_free(&payload);
    }
    bool ok=md_buf_assign(source,current.data,current.len); md_buf_free(&current);
    if (!ok) storage_error(error,error_cap,"Out of memory returning reconstructed version");
    return ok;
}

static bool history_write_record(const char *path,const HistoryHeader *header,
                                 const uint8_t *payload,size_t payload_len,
                                 char *error,size_t error_cap) {
    MdBytes file; md_bytes_init(&file); uint8_t bytes[HISTORY_HEADER_SIZE]; history_header_write(bytes,header);
    bool ok=md_bytes_append(&file,bytes,sizeof(bytes))&&md_bytes_append(&file,payload,payload_len);
    if (!ok) { md_bytes_free(&file); storage_error(error,error_cap,"Out of memory writing history record"); return false; }
    ok=md_write_file_atomic(path,file.data,file.len,error,error_cap); md_bytes_free(&file); return ok;
}

static bool history_prune(const char *root,const MdDocument *doc,char *error,size_t error_cap) {
    MdVersionList list; md_version_list_init(&list);
    if (!md_history_list(root,doc,&list,error,error_cap)) return false;
    uint64_t total=0U; for (size_t i=0U;i<list.count;++i) total+=list.items[i].encoded_size;
    while (list.count>history_max_versions||total>history_max_bytes) {
        size_t remove_start=SIZE_MAX,remove_end=SIZE_MAX;
        for (size_t start=0U;start<list.count;) {
            size_t end=start+1U; while (end<list.count&&!list.items[end].full_snapshot) ++end;
            if (end>=list.count) break;
            bool pinned=false; for (size_t i=start;i<end;++i) if (list.items[i].pinned) { pinned=true; break; }
            if (!pinned) { remove_start=start; remove_end=end; break; }
            start=end;
        }
        if (remove_start==SIZE_MAX) { storage_error(error,error_cap,"Pinned or newest history chains exceed the automatic retention target"); md_version_list_free(&list); return true; }
        for (size_t i=remove_start;i<remove_end;++i) {
            total-=list.items[i].encoded_size;
            if (unlink(list.items[i].record_path)!=0) { md_version_list_free(&list); storage_error(error,error_cap,"Cannot prune history record: %s",strerror(errno)); return false; }
        }
        memmove(list.items+remove_start,list.items+remove_end,(list.count-remove_end)*sizeof(*list.items)); list.count-=remove_end-remove_start;
    }
    md_version_list_free(&list); return true;
}

void md_history_set_retention_limits(size_t max_versions,uint64_t max_bytes) {
    history_max_versions=MD_MAX(max_versions,1U); history_max_bytes=MD_MAX(max_bytes,1U);
}

void md_history_reset_retention_limits(void) {
    history_max_versions=200U; history_max_bytes=UINT64_C(64)*1024U*1024U;
}

bool md_history_create(const char *history_root,const MdDocument *doc,bool explicit_create,
                       MdVersionInfo *created,char *error,size_t error_cap) {
    char dir[MD_PATH_MAX]; if (!history_dir(dir,history_root,doc)||!md_mkdirs(dir,0700,error,error_cap)) return false;
    MdVersionList list; md_version_list_init(&list);
    if (!md_history_list(history_root,doc,&list,error,error_cap)) return false;
    MdBuf previous; md_buf_init(&previous);
    if (list.count>0U) {
        if (!md_history_reconstruct(&list,list.count-1U,&previous,error,error_cap)) { md_version_list_free(&list); return false; }
        if (!explicit_create&&previous.len==doc->source.len&&memcmp(previous.data,doc->source.data,previous.len)==0) {
            md_buf_free(&previous); md_version_list_free(&list); if (created!=NULL) memset(created,0,sizeof(*created)); return true;
        }
    }
    uint64_t sequence=list.count==0U?1U:list.items[list.count-1U].sequence+1U;
    bool full=sequence==1U||((sequence-1U)%20U)==0U;
    MdBytes raw; md_bytes_init(&raw);
    if (full) {
        if (!md_bytes_append(&raw,doc->source.data,doc->source.len)) goto oom;
    } else if (!md_delta_encode(previous.data,previous.len,doc->source.data,doc->source.len,&raw,error,error_cap)) {
        md_buf_free(&previous); md_version_list_free(&list); md_bytes_free(&raw); return false;
    }
    MdBytes compressed; md_bytes_init(&compressed);
    if (!md_lzss_compress(raw.data,raw.len,&compressed)) goto oom;
    bool use_compressed=compressed.len<raw.len;
    const uint8_t *payload=use_compressed?compressed.data:raw.data;
    size_t payload_len=use_compressed?compressed.len:raw.len;
    HistoryHeader header={sequence,md_now_unix(),(full?HISTORY_FLAG_FULL:0U)|(use_compressed?HISTORY_FLAG_COMPRESSED:0U),
                          (uint64_t)raw.len,(uint64_t)payload_len,{0}};
    md_sha256(raw.data,raw.len,header.payload_sha);
    char record[MD_PATH_MAX]; int n=snprintf(record,sizeof(record),"%s/%020llu.mhv",dir,(unsigned long long)sequence);
    if (n<0||(size_t)n>=sizeof(record)||!history_write_record(record,&header,payload,payload_len,error,error_cap)) {
        md_buf_free(&previous); md_version_list_free(&list); md_bytes_free(&raw); md_bytes_free(&compressed); return false;
    }
    if (created!=NULL) {
        *created=(MdVersionInfo){sequence,header.timestamp,false,full,HISTORY_HEADER_SIZE+payload_len,{0}};
        strcpy(created->record_path,record);
    }
    md_buf_free(&previous); md_version_list_free(&list); md_bytes_free(&raw); md_bytes_free(&compressed);
    return history_prune(history_root,doc,error,error_cap);
oom:
    md_buf_free(&previous); md_version_list_free(&list); md_bytes_free(&raw); md_bytes_free(&compressed);
    storage_error(error,error_cap,"Out of memory creating history version"); return false;
}

bool md_history_pin(const MdVersionList *list,size_t index,bool pinned,char *error,size_t error_cap) {
    if (index>=list->count) { storage_error(error,error_cap,"History version index is out of range"); return false; }
    MdBytes file; md_bytes_init(&file);
    if (!md_read_file(list->items[index].record_path,&file,error,error_cap)) return false;
    HistoryHeader h; if (!history_header_read(file.data,file.len,&h)) { md_bytes_free(&file); storage_error(error,error_cap,"History record is corrupt"); return false; }
    if (pinned) h.flags|=HISTORY_FLAG_PINNED; else h.flags&=~HISTORY_FLAG_PINNED;
    history_header_write(file.data,&h);
    bool ok=md_write_file_atomic(list->items[index].record_path,file.data,file.len,error,error_cap); md_bytes_free(&file); return ok;
}

bool md_history_delete(const MdVersionList *list,size_t index,char *error,size_t error_cap) {
    if (index>=list->count) { storage_error(error,error_cap,"History version index is out of range"); return false; }
    if (list->items[index].pinned) { storage_error(error,error_cap,"Pinned versions must be unpinned before deletion"); return false; }
    if (index+1U<list->count&&!list->items[index+1U].full_snapshot) {
        storage_error(error,error_cap,"Deleting this version would break a retained delta chain"); return false;
    }
    if (unlink(list->items[index].record_path)!=0) { storage_error(error,error_cap,"Cannot delete history version: %s",strerror(errno)); return false; }
    return true;
}

#define RECOVERY_HEADER_SIZE 100U

typedef struct {
    uint64_t timestamp;
    uint32_t flags;
    uint32_t path_len;
    uint32_t id_len;
    uint64_t source_len;
    uint8_t baseline[32];
    uint8_t source_sha[32];
} RecoveryHeader;

static void recovery_header_write(uint8_t out[RECOVERY_HEADER_SIZE],const RecoveryHeader *h) {
    memcpy(out,"MDR1",4U); store_u32(out+4U,1U); store_u64(out+8U,h->timestamp);
    store_u32(out+16U,h->flags); store_u32(out+20U,h->path_len); store_u32(out+24U,h->id_len);
    store_u64(out+28U,h->source_len); memcpy(out+36U,h->baseline,32U); memcpy(out+68U,h->source_sha,32U);
}

static bool recovery_header_read(const uint8_t *data,size_t len,RecoveryHeader *h) {
    if (len<RECOVERY_HEADER_SIZE||memcmp(data,"MDR1",4U)!=0||load_u32(data+4U)!=1U) return false;
    h->timestamp=load_u64(data+8U); h->flags=load_u32(data+16U); h->path_len=load_u32(data+20U);
    h->id_len=load_u32(data+24U); h->source_len=load_u64(data+28U);
    memcpy(h->baseline,data+36U,32U); memcpy(h->source_sha,data+68U,32U);
    uint64_t expected=(uint64_t)RECOVERY_HEADER_SIZE+h->path_len+h->id_len+h->source_len;
    return h->path_len<MD_PATH_MAX&&h->id_len==64U&&h->source_len<=SIZE_MAX&&expected==len;
}

void md_recovery_list_init(MdRecoveryList *list) { memset(list,0,sizeof(*list)); }

void md_recovery_list_free(MdRecoveryList *list) { free(list->items); memset(list,0,sizeof(*list)); }

static bool recovery_push(MdRecoveryList *list,MdRecoveryInfo info) {
    if (list->count==list->cap) {
        size_t next=list->cap==0U?8U:list->cap*2U;
        MdRecoveryInfo *items=realloc(list->items,next*sizeof(*items));
        if (items==NULL) return false;
        list->items=items; list->cap=next;
    }
    list->items[list->count++]=info; return true;
}

bool md_recovery_write(const char *root,const MdDocument *doc,char *error,size_t error_cap) {
    if (root==NULL||root[0]=='\0') { storage_error(error,error_cap,"Recovery root is unavailable"); return false; }
    if (!md_mkdirs(root,0700,error,error_cap)) return false;
    size_t path_len=strlen(doc->path),id_len=strlen(doc->id);
    if (path_len>=MD_PATH_MAX||id_len!=64U) { storage_error(error,error_cap,"Document recovery identity is invalid"); return false; }
    RecoveryHeader h={md_now_unix(),doc->untitled?1U:0U,(uint32_t)path_len,(uint32_t)id_len,
                      (uint64_t)doc->source.len,{0},{0}};
    if (doc->has_disk_sha256) memcpy(h.baseline,doc->disk_sha256,32U);
    md_sha256(doc->source.data,doc->source.len,h.source_sha);
    uint8_t header[RECOVERY_HEADER_SIZE]; recovery_header_write(header,&h);
    MdBytes record; md_bytes_init(&record);
    if (!md_bytes_append(&record,header,sizeof(header))||!md_bytes_append(&record,doc->path,path_len)||
        !md_bytes_append(&record,doc->id,id_len)||!md_bytes_append(&record,doc->source.data,doc->source.len)) {
        md_bytes_free(&record); storage_error(error,error_cap,"Out of memory writing recovery record"); return false;
    }
    char path[MD_PATH_MAX]; int n=snprintf(path,sizeof(path),"%s/%s.mrec",root,doc->id);
    if (n<0||(size_t)n>=sizeof(path)) { md_bytes_free(&record); storage_error(error,error_cap,"Recovery path is too long"); return false; }
    bool ok=md_write_file_atomic(path,record.data,record.len,error,error_cap); md_bytes_free(&record); return ok;
}

static bool recovery_info_from_file(const char *path,MdRecoveryInfo *info) {
    MdBytes bytes; md_bytes_init(&bytes); char ignored[128];
    memset(info,0,sizeof(*info)); strcpy(info->record_path,path);
    if (!md_read_file(path,&bytes,ignored,sizeof(ignored))) { md_bytes_free(&bytes); return false; }
    RecoveryHeader h;
    if (!recovery_header_read(bytes.data,bytes.len,&h)) { md_bytes_free(&bytes); return false; }
    size_t at=RECOVERY_HEADER_SIZE;
    memcpy(info->document_path,bytes.data+at,h.path_len); info->document_path[h.path_len]='\0'; at+=h.path_len;
    memcpy(info->document_id,bytes.data+at,h.id_len); info->document_id[h.id_len]='\0'; at+=h.id_len;
    uint8_t sha[32]; md_sha256(bytes.data+at,(size_t)h.source_len,sha);
    info->timestamp=h.timestamp; info->untitled=(h.flags&1U)!=0U; info->valid=bytes_equal(sha,h.source_sha);
    md_bytes_free(&bytes); return info->valid;
}

static int recovery_compare(const void *left,const void *right) {
    const MdRecoveryInfo *a=left,*b=right;
    if (a->valid!=b->valid) return a->valid?-1:1;
    return a->timestamp>b->timestamp?-1:a->timestamp<b->timestamp?1:strcmp(a->record_path,b->record_path);
}

bool md_recovery_scan(const char *root,MdRecoveryList *list,char *warning,size_t warning_cap) {
    md_recovery_list_free(list);
    DIR *dir=opendir(root);
    if (dir==NULL) { if (errno==ENOENT) return true; storage_error(warning,warning_cap,"Cannot read recovery directory: %s",strerror(errno)); return false; }
    bool corrupt=false; struct dirent *de;
    while ((de=readdir(dir))!=NULL) {
        size_t n=strlen(de->d_name); if (n<6U||strcmp(de->d_name+n-5U,".mrec")!=0) continue;
        char path[MD_PATH_MAX]; if (!md_path_join(path,root,de->d_name)) { closedir(dir); return false; }
        MdRecoveryInfo info; bool valid=recovery_info_from_file(path,&info);
        if (!valid) { corrupt=true; memset(info.document_path,0,sizeof(info.document_path)); strcpy(info.document_path,"(corrupt recovery record)"); info.valid=false; }
        if (!recovery_push(list,info)) { closedir(dir); md_recovery_list_free(list); storage_error(warning,warning_cap,"Out of memory scanning recovery records"); return false; }
    }
    (void)closedir(dir); if (list->count>1U) qsort(list->items,list->count,sizeof(*list->items),recovery_compare);
    if (corrupt) storage_error(warning,warning_cap,"One or more corrupt recovery records were skipped; valid records remain available");
    return true;
}

bool md_recovery_open(const MdRecoveryInfo *info,MdBuf *source,char *error,size_t error_cap) {
    if (!info->valid) { storage_error(error,error_cap,"Recovery record is marked corrupt"); return false; }
    MdBytes bytes; md_bytes_init(&bytes);
    if (!md_read_file(info->record_path,&bytes,error,error_cap)) return false;
    RecoveryHeader h;
    if (!recovery_header_read(bytes.data,bytes.len,&h)) { md_bytes_free(&bytes); storage_error(error,error_cap,"Recovery record header is corrupt"); return false; }
    size_t at=RECOVERY_HEADER_SIZE+h.path_len+h.id_len; uint8_t sha[32];
    md_sha256(bytes.data+at,(size_t)h.source_len,sha);
    if (!bytes_equal(sha,h.source_sha)||!md_utf8_validate((const char *)bytes.data+at,(size_t)h.source_len,NULL)) {
        md_bytes_free(&bytes); storage_error(error,error_cap,"Recovery content failed integrity/UTF-8 validation"); return false;
    }
    bool ok=md_buf_assign(source,(const char *)bytes.data+at,(size_t)h.source_len); md_bytes_free(&bytes);
    if (!ok) storage_error(error,error_cap,"Out of memory opening recovery record");
    return ok;
}

bool md_recovery_remove(const MdRecoveryInfo *info,char *error,size_t error_cap) {
    if (unlink(info->record_path)!=0&&errno!=ENOENT) { storage_error(error,error_cap,"Cannot remove recovery record: %s",strerror(errno)); return false; }
    return true;
}

static bool relative_between(const char *from_dir,const char *to_path,char out[MD_PATH_MAX]) {
    char from_real[MD_PATH_MAX],to_real[MD_PATH_MAX];
    if (realpath(from_dir,from_real)==NULL||realpath(to_path,to_real)==NULL) return false;
    size_t common=0U,last_slash=0U;
    while (from_real[common]!='\0'&&to_real[common]!='\0'&&from_real[common]==to_real[common]) {
        if (from_real[common]=='/') last_slash=common;
        ++common;
    }
    if (from_real[common]=='\0'&&to_real[common]=='/') last_slash=common;
    if (to_real[common]=='\0'&&from_real[common]=='/') last_slash=common;
    size_t at=0U;
    const char *from_tail=from_real+last_slash+1U;
    if (from_tail[0]!='\0') {
        if (at+3U>=MD_PATH_MAX) return false;
        memcpy(out+at,"../",3U); at+=3U;
        for (size_t i=0U;from_tail[i]!='\0';++i) {
            if (from_tail[i]=='/') {
                if (at+3U>=MD_PATH_MAX) return false;
                memcpy(out+at,"../",3U); at+=3U;
            }
        }
    }
    const char *tail=to_real+last_slash+1U; size_t tail_len=strlen(tail);
    if (at+tail_len>=MD_PATH_MAX) return false;
    memcpy(out+at,tail,tail_len+1U); return true;
}

static bool asset_directory(const char *workspace_root,const char *document_path,
                            char out[MD_PATH_MAX],char *error,size_t error_cap) {
    if (workspace_root!=NULL&&workspace_root[0]!='\0') {
        if (!md_path_join(out,workspace_root,"assets")) { storage_error(error,error_cap,"Asset path is too long"); return false; }
    } else {
        if (document_path==NULL||document_path[0]=='\0') { storage_error(error,error_cap,"Save the standalone document before using relative assets"); return false; }
        char dir[MD_PATH_MAX],base[MD_PATH_MAX];
        if (!md_path_dirname(dir,document_path)||!md_path_basename(base,document_path)) return false;
        char *dot=strrchr(base,'.'); if (dot!=NULL) *dot='\0';
        char asset_name[MD_PATH_MAX]; int n=snprintf(asset_name,sizeof(asset_name),"%s.assets",base);
        if (n<0||(size_t)n>=sizeof(asset_name)||!md_path_join(out,dir,asset_name)) return false;
    }
    return md_mkdirs(out,0755,error,error_cap);
}

static bool file_same_bytes(const char *path,const uint8_t *bytes,size_t len) {
    MdBytes existing; md_bytes_init(&existing); char ignored[64];
    bool ok=md_read_file(path,&existing,ignored,sizeof(ignored))&&existing.len==len&&
            (len==0U||memcmp(existing.data,bytes,len)==0);
    md_bytes_free(&existing); return ok;
}

static bool asset_store(const char *asset_dir,const char *preferred,MdImageFormat format,
                        const uint8_t *bytes,size_t len,char stored[MD_PATH_MAX],
                        char *error,size_t error_cap) {
    char name[MD_PATH_MAX];
    if (preferred!=NULL&&preferred[0]!='\0') {
        if (!md_path_basename(name,preferred)) return false;
    } else {
        uint8_t hash[32]; char hex[65]; md_sha256(bytes,len,hash); md_hex_encode(hash,32U,hex);
        int n=snprintf(name,sizeof(name),"embedded-%.12s%s",hex,md_image_extension(format));
        if (n<0||(size_t)n>=sizeof(name)) return false;
    }
    char candidate[MD_PATH_MAX]; if (!md_path_join(candidate,asset_dir,name)) return false;
    struct stat st;
    if (lstat(candidate,&st)==0&&!file_same_bytes(candidate,bytes,len)) {
        uint8_t hash[32]; char hex[65]; md_sha256(bytes,len,hash); md_hex_encode(hash,32U,hex);
        char stem[MD_PATH_MAX],ext[64]; strcpy(stem,name); ext[0]='\0';
        char *dot=strrchr(stem,'.'); if (dot!=NULL) { (void)snprintf(ext,sizeof(ext),"%s",dot); *dot='\0'; }
        bool chosen=false;
        for (unsigned suffix=0U;suffix<1000U;++suffix) {
            int n=suffix==0U?snprintf(name,sizeof(name),"%s-%.10s%s",stem,hex,ext):
                              snprintf(name,sizeof(name),"%s-%.10s-%u%s",stem,hex,suffix,ext);
            if (n<0||(size_t)n>=sizeof(name)||!md_path_join(candidate,asset_dir,name)) return false;
            if (lstat(candidate,&st)!=0) { if (errno!=ENOENT) return false; chosen=true; break; }
            if (file_same_bytes(candidate,bytes,len)) { chosen=true; break; }
        }
        if (!chosen) { storage_error(error,error_cap,"Cannot find a non-colliding asset filename"); return false; }
    }
    if (lstat(candidate,&st)!=0) {
        if (errno!=ENOENT||!md_write_file_atomic(candidate,bytes,len,error,error_cap)) return false;
    }
    strcpy(stored,candidate); return true;
}

bool md_asset_import_relative(const char *workspace_root,const char *document_path,
                              const char *source_image,char relative_out[MD_PATH_MAX],
                              char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes); MdImage image; md_image_init(&image);
    if (!md_image_load(source_image,&image,&bytes,error,error_cap)) { md_image_free(&image); md_bytes_free(&bytes); return false; }
    char dir[MD_PATH_MAX],stored[MD_PATH_MAX],doc_dir[MD_PATH_MAX];
    bool ok=asset_directory(workspace_root,document_path,dir,error,error_cap)&&
            asset_store(dir,source_image,image.format,bytes.data,bytes.len,stored,error,error_cap)&&
            md_path_dirname(doc_dir,document_path)&&relative_between(doc_dir,stored,relative_out);
    if (!ok&&error!=NULL&&error[0]=='\0') storage_error(error,error_cap,"Cannot compute relative asset path");
    md_image_free(&image); md_bytes_free(&bytes); return ok;
}

bool md_asset_externalize(const char *workspace_root,const char *document_path,
                          const char *data_uri,size_t data_uri_len,char relative_out[MD_PATH_MAX],
                          char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes); MdImageFormat format=MD_IMAGE_UNKNOWN;
    if (!md_image_parse_data_uri(data_uri,data_uri_len,&format,&bytes,error,error_cap)) { md_bytes_free(&bytes); return false; }
    char dir[MD_PATH_MAX],stored[MD_PATH_MAX],doc_dir[MD_PATH_MAX];
    bool ok=asset_directory(workspace_root,document_path,dir,error,error_cap)&&
            asset_store(dir,NULL,format,bytes.data,bytes.len,stored,error,error_cap)&&
            md_path_dirname(doc_dir,document_path)&&relative_between(doc_dir,stored,relative_out);
    if (!ok&&error!=NULL&&error[0]=='\0') storage_error(error,error_cap,"Cannot externalize embedded image");
    md_bytes_free(&bytes); return ok;
}

typedef struct {
    bool single;
    const MdDocument *doc;
    const char *asset_dir;
    const char *asset_ref_prefix;
} ExportContext;

typedef struct {
    size_t whole_start;
    size_t whole_end;
    size_t destination_start;
    size_t destination_end;
    bool html;
} StorageImageReference;

static bool storage_html_name_char(char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_';
}

static bool next_image_reference(const char *source,size_t len,size_t from,
                                 StorageImageReference *reference) {
    for (size_t at=from;at<len;++at) {
        if (at+2U<=len&&source[at]=='!'&&source[at+1U]=='[') {
            size_t close=at+2U; bool escaped=false;
            while (close<len) {
                if (escaped) escaped=false;
                else if (source[close]=='\\') escaped=true;
                else if (source[close]==']') break;
                ++close;
            }
            if (close+1U>=len||source[close+1U]!='(') continue;
            size_t p=close+2U;
            while (p<len&&(source[p]==' '||source[p]=='\t'||source[p]=='\n'||source[p]=='\r')) ++p;
            size_t destination=p,destination_end=p,outer_end=p;
            if (p<len&&source[p]=='<') {
                destination=++p; escaped=false;
                while (p<len) {
                    if (escaped) escaped=false;
                    else if (source[p]=='\\') escaped=true;
                    else if (source[p]=='>') break;
                    ++p;
                }
                if (p>=len) continue;
                destination_end=p++;
            }
            bool in_quote=false; char quote=0; escaped=false; unsigned nesting=0U;
            for (outer_end=p;outer_end<len;++outer_end) {
                char c=source[outer_end];
                if (escaped) { escaped=false; continue; }
                if (c=='\\') { escaped=true; continue; }
                if (in_quote) { if (c==quote) in_quote=false; continue; }
                if (c=='\''||c=='\"') { in_quote=true; quote=c; continue; }
                if (c=='(') ++nesting;
                else if (c==')') { if (nesting==0U) break; --nesting; }
            }
            if (outer_end<len) {
                if (source[destination-1U]!='<') {
                    destination_end=outer_end;
                    while (destination_end>destination&&(source[destination_end-1U]==' '||source[destination_end-1U]=='\t'||source[destination_end-1U]=='\n'||source[destination_end-1U]=='\r')) --destination_end;
                    unsigned depth=0U; escaped=false;
                    for (size_t q=destination;q<destination_end;++q) {
                        char c=source[q];
                        if (escaped) { escaped=false; continue; }
                        if (c=='\\') { escaped=true; continue; }
                        if (c=='(') { ++depth; continue; }
                        if (c==')'&&depth>0U) { --depth; continue; }
                        if (depth!=0U||(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')) continue;
                        size_t title=q; while (title<destination_end&&(source[title]==' '||source[title]=='\t'||source[title]=='\n'||source[title]=='\r')) ++title;
                        if (title>=destination_end||(source[title]!='\"'&&source[title]!='\''&&source[title]!='(')) continue;
                        char title_open=source[title],title_close=title_open=='('?')':title_open; size_t finish=title+1U; bool title_escaped=false;
                        while (finish<destination_end) {
                            if (title_escaped) title_escaped=false;
                            else if (source[finish]=='\\') title_escaped=true;
                            else if (source[finish]==title_close) break;
                            ++finish;
                        }
                        if (finish>=destination_end) continue;
                        size_t tail=finish+1U; while (tail<destination_end&&(source[tail]==' '||source[tail]=='\t'||source[tail]=='\n'||source[tail]=='\r')) ++tail;
                        if (tail==destination_end) { destination_end=q; while (destination_end>destination&&(source[destination_end-1U]==' '||source[destination_end-1U]=='\t'||source[destination_end-1U]=='\n'||source[destination_end-1U]=='\r')) --destination_end; break; }
                    }
                }
                if (destination_end==destination) continue;
                *reference=(StorageImageReference){at,outer_end+1U,destination,destination_end,false};
                return true;
            }
        }
        if (at+4U<=len&&source[at]=='<'&&strncasecmp(source+at,"<img",4U)==0&&
            (at+4U==len||source[at+4U]=='>'||source[at+4U]=='/'||source[at+4U]==' '||source[at+4U]=='\t')) {
            size_t tag_end=at+4U; while (tag_end<len&&source[tag_end]!='>') ++tag_end;
            if (tag_end>=len) continue;
            size_t p=at+4U;
            while (p<tag_end) {
                while (p<tag_end&&(source[p]==' '||source[p]=='\t'||source[p]=='/'||source[p]=='\n'||source[p]=='\r')) ++p;
                size_t name=p; while (p<tag_end&&storage_html_name_char(source[p])) ++p;
                size_t name_len=p-name; while (p<tag_end&&(source[p]==' '||source[p]=='\t')) ++p;
                if (p>=tag_end||source[p]!='=') { while (p<tag_end&&source[p]!=' '&&source[p]!='\t') ++p; continue; }
                ++p; while (p<tag_end&&(source[p]==' '||source[p]=='\t')) ++p;
                char quote=p<tag_end&&(source[p]=='\''||source[p]=='\"')?source[p++]:0;
                size_t value=p;
                if (quote!=0) while (p<tag_end&&source[p]!=quote) ++p;
                else while (p<tag_end&&source[p]!=' '&&source[p]!='\t'&&source[p]!='>') ++p;
                if (name_len==3U&&strncasecmp(source+name,"src",3U)==0) {
                    *reference=(StorageImageReference){at,tag_end+1U,value,p,true}; return true;
                }
                if (quote!=0&&p<tag_end) ++p;
            }
            at=tag_end;
        }
    }
    return false;
}

static bool html_unescape_attribute(const char *source,size_t len,MdBuf *out) {
    out->len=0U; if (out->data!=NULL) out->data[0]='\0';
    for (size_t at=0U;at<len;) {
        if (source[at]!='&') { if (!md_buf_append(out,source+at,1U)) return false; ++at; continue; }
        size_t semi=at+1U; while (semi<len&&semi-at<=12U&&source[semi]!=';') ++semi;
        if (semi>=len||source[semi]!=';') { if (!md_buf_append(out,source+at,1U)) return false; ++at; continue; }
        const char *entity=source+at+1U; size_t entity_len=semi-at-1U; uint32_t cp=0U; bool known=true;
        if (entity_len==3U&&memcmp(entity,"amp",3U)==0) cp='&';
        else if (entity_len==4U&&memcmp(entity,"quot",4U)==0) cp='\"';
        else if (entity_len==2U&&memcmp(entity,"lt",2U)==0) cp='<';
        else if (entity_len==2U&&memcmp(entity,"gt",2U)==0) cp='>';
        else if (entity_len==4U&&memcmp(entity,"apos",4U)==0) cp='\'';
        else if (entity_len>=2U&&entity[0]=='#') {
            unsigned base=10U; size_t p=1U;
            if (p<entity_len&&(entity[p]=='x'||entity[p]=='X')) { base=16U; ++p; }
            if (p==entity_len) known=false;
            for (;known&&p<entity_len;++p) {
                unsigned digit;
                if (entity[p]>='0'&&entity[p]<='9') digit=(unsigned)(entity[p]-'0');
                else if (base==16U&&entity[p]>='a'&&entity[p]<='f') digit=(unsigned)(entity[p]-'a')+10U;
                else if (base==16U&&entity[p]>='A'&&entity[p]<='F') digit=(unsigned)(entity[p]-'A')+10U;
                else { known=false; break; }
                if (cp>(UINT32_MAX-digit)/base) { known=false; break; }
                cp=cp*base+digit;
            }
            if (cp==0U||cp>0x10ffffU||(cp>=0xd800U&&cp<=0xdfffU)) known=false;
        } else known=false;
        if (!known) { if (!md_buf_append(out,source+at,semi-at+1U)) return false; at=semi+1U; continue; }
        char encoded[4]; size_t encoded_len=0U;
        if (cp<0x80U) encoded[encoded_len++]=(char)cp;
        else if (cp<0x800U) { encoded[encoded_len++]=(char)(0xc0U|(cp>>6U)); encoded[encoded_len++]=(char)(0x80U|(cp&0x3fU)); }
        else if (cp<0x10000U) { encoded[encoded_len++]=(char)(0xe0U|(cp>>12U)); encoded[encoded_len++]=(char)(0x80U|((cp>>6U)&0x3fU)); encoded[encoded_len++]=(char)(0x80U|(cp&0x3fU)); }
        else { encoded[encoded_len++]=(char)(0xf0U|(cp>>18U)); encoded[encoded_len++]=(char)(0x80U|((cp>>12U)&0x3fU)); encoded[encoded_len++]=(char)(0x80U|((cp>>6U)&0x3fU)); encoded[encoded_len++]=(char)(0x80U|(cp&0x3fU)); }
        if (!md_buf_append(out,encoded,encoded_len)) return false;
        at=semi+1U;
    }
    return true;
}

static bool html_escape_attribute(const char *source,size_t len,MdBuf *out) {
    for (size_t at=0U;at<len;++at) {
        const char *escaped=NULL;
        switch (source[at]) {
            case '&': escaped="&amp;"; break;
            case '\"': escaped="&quot;"; break;
            case '\'': escaped="&#39;"; break;
            case '<': escaped="&lt;"; break;
            case '>': escaped="&gt;"; break;
            default: break;
        }
        if (escaped!=NULL) { if (!md_buf_append_cstr(out,escaped)) return false; }
        else if (!md_buf_append(out,source+at,1U)) return false;
    }
    return true;
}

static bool reference_value(const char *source,const StorageImageReference *reference,MdBuf *decoded) {
    size_t len=reference->destination_end-reference->destination_start;
    decoded->len=0U; if (decoded->data!=NULL) decoded->data[0]='\0';
    return reference->html?html_unescape_attribute(source+reference->destination_start,len,decoded):
                           md_buf_append(decoded,source+reference->destination_start,len);
}

static bool reference_is_relative(const char *source,size_t len) {
    return len>0U&&source[0]!='/'&&source[0]!='#'&&
           !(len>=5U&&memcmp(source,"data:",5U)==0)&&
           !(len>=7U&&memcmp(source,"http://",7U)==0)&&
           !(len>=8U&&memcmp(source,"https://",8U)==0);
}

bool md_document_has_relative_images(const MdDocument *doc) {
    StorageImageReference reference; size_t at=0U;
    while (next_image_reference(doc->source.data,doc->source.len,at,&reference)) {
        MdBuf value; md_buf_init(&value); bool ok=md_buf_reserve(&value,0U)&&reference_value(doc->source.data,&reference,&value);
        bool relative=ok&&reference_is_relative(value.data,value.len); md_buf_free(&value);
        if (relative) return true;
        at=reference.whole_end;
    }
    return false;
}

static bool resolve_image_source(const MdDocument *doc,const char *source,size_t len,
                                 MdBytes *bytes,MdImageFormat *format,
                                 char *error,size_t error_cap) {
    if (len>=5U&&memcmp(source,"data:",5U)==0)
        return md_image_parse_data_uri(source,len,format,bytes,error,error_cap);
    if ((len>=7U&&memcmp(source,"http://",7U)==0)||(len>=8U&&memcmp(source,"https://",8U)==0)) {
        storage_error(error,error_cap,"Remote images are not fetched by offline portable export"); return false;
    }
    char value[MD_PATH_MAX]; if (len>=sizeof(value)) { storage_error(error,error_cap,"Image path is too long"); return false; }
    memcpy(value,source,len); value[len]='\0';
    char path[MD_PATH_MAX];
    if (value[0]=='/') strcpy(path,value);
    else { char dir[MD_PATH_MAX]; if (!md_path_dirname(dir,doc->path)||!md_path_join(path,dir,value)) return false; }
    MdImage image; md_image_init(&image);
    bool ok=md_image_load(path,&image,bytes,error,error_cap); *format=image.format; md_image_free(&image); return ok;
}

static bool export_destination(ExportContext *ctx,const char *source,size_t len,MdBuf *out,
                               char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes); MdImageFormat format=MD_IMAGE_UNKNOWN;
    if (!resolve_image_source(ctx->doc,source,len,&bytes,&format,error,error_cap)) { md_bytes_free(&bytes); return false; }
    bool ok=false;
    if (ctx->single) {
        MdBuf uri; md_buf_init(&uri); ok=md_image_make_data_uri(format,bytes.data,bytes.len,&uri)&&md_buf_append(out,uri.data,uri.len); md_buf_free(&uri);
    } else {
        char stored[MD_PATH_MAX];
        if (asset_store(ctx->asset_dir,NULL,format,bytes.data,bytes.len,stored,error,error_cap)) {
            char base[MD_PATH_MAX]; ok=md_path_basename(base,stored)&&md_buf_appendf(out,"%s/%s",ctx->asset_ref_prefix,base);
        }
    }
    md_bytes_free(&bytes); if (!ok&&error!=NULL&&error[0]=='\0') storage_error(error,error_cap,"Cannot transform image for portable export");
    return ok;
}

static bool export_transform(ExportContext *ctx,MdBuf *out,char *error,size_t error_cap) {
    const char *source=ctx->doc->source.data; size_t len=ctx->doc->source.len,at=0U;
    StorageImageReference reference;
    while (next_image_reference(source,len,at,&reference)) {
        MdBuf value,replacement; md_buf_init(&value); md_buf_init(&replacement);
        bool ok=md_buf_reserve(&value,0U)&&md_buf_reserve(&replacement,0U)&&
                reference_value(source,&reference,&value)&&
                export_destination(ctx,value.data,value.len,&replacement,error,error_cap)&&
                md_buf_append(out,source+at,reference.destination_start-at)&&
                (reference.html?html_escape_attribute(replacement.data,replacement.len,out):
                                md_buf_append(out,replacement.data,replacement.len));
        md_buf_free(&value); md_buf_free(&replacement); if (!ok) return false;
        at=reference.destination_end;
    }
    return md_buf_append(out,source+at,len-at);
}

static bool relocate_transform(const MdDocument *doc,const char *destination,MdBuf *out,
                               char *error,size_t error_cap) {
    const char *source=doc->source.data; size_t len=doc->source.len,at=0U;
    StorageImageReference reference;
    while (next_image_reference(source,len,at,&reference)) {
        MdBuf value,replacement; md_buf_init(&value); md_buf_init(&replacement);
        if (!md_buf_reserve(&value,0U)||!md_buf_reserve(&replacement,0U)||!reference_value(source,&reference,&value)) {
            md_buf_free(&value); md_buf_free(&replacement); storage_error(error,error_cap,"Out of memory reading image reference"); return false;
        }
        size_t source_len=value.len; const char *image_source=value.data;
        if (!md_buf_append(out,source+at,reference.destination_start-at)) { md_buf_free(&value); md_buf_free(&replacement); return false; }
        if (reference_is_relative(image_source,source_len)) {
            char path_value[MD_PATH_MAX],old_dir[MD_PATH_MAX],absolute[MD_PATH_MAX],relative[MD_PATH_MAX];
            if (source_len>=sizeof(path_value)) { md_buf_free(&value); md_buf_free(&replacement); storage_error(error,error_cap,"Relative image path is too long during Save As"); return false; }
            memcpy(path_value,image_source,source_len); path_value[source_len]='\0';
            if (!md_path_dirname(old_dir,doc->path)||!md_path_join(absolute,old_dir,path_value)||
                !md_asset_import_relative("",destination,absolute,relative,error,error_cap)||
                !md_buf_append_cstr(&replacement,relative)) { md_buf_free(&value); md_buf_free(&replacement); return false; }
        } else if (!md_buf_append(&replacement,image_source,source_len)) { md_buf_free(&value); md_buf_free(&replacement); return false; }
        bool appended=reference.html?html_escape_attribute(replacement.data,replacement.len,out):
                                     md_buf_append(out,replacement.data,replacement.len);
        md_buf_free(&value); md_buf_free(&replacement); if (!appended) return false;
        at=reference.destination_end;
    }
    return md_buf_append(out,source+at,len-at);
}

bool md_save_as_with_relocation(MdDocument *doc,const char *destination,
                                MdRelocationPolicy policy,bool overwrite,
                                char *error,size_t error_cap) {
    if (policy==MD_RELOCATE_KEEP_REFERENCES||doc->path[0]=='\0'||!md_document_has_relative_images(doc))
        return md_safe_save_document(doc,destination,overwrite,error,error_cap);
    char old_dir[MD_PATH_MAX],new_dir[MD_PATH_MAX];
    if (!md_path_dirname(old_dir,doc->path)||!md_path_dirname(new_dir,destination)) {
        storage_error(error,error_cap,"Save As destination path is too long"); return false;
    }
    char old_real[MD_PATH_MAX],new_real[MD_PATH_MAX];
    const char *old_key=realpath(old_dir,old_real); if (old_key==NULL) old_key=old_dir;
    const char *new_key=realpath(new_dir,new_real); if (new_key==NULL) new_key=new_dir;
    if (strcmp(old_key,new_key)==0) return md_safe_save_document(doc,destination,overwrite,error,error_cap);
    MdBuf transformed; md_buf_init(&transformed);
    if (!relocate_transform(doc,destination,&transformed,error,error_cap)) { md_buf_free(&transformed); return false; }
    MdDocument temporary; md_document_init(&temporary,0U);
    bool ok=md_document_set_source(&temporary,transformed.data,transformed.len,true,error,error_cap)&&
            md_safe_save_document(&temporary,destination,overwrite,error,error_cap);
    if (ok) {
        ok=md_document_replace(doc,0U,doc->source.len,transformed.data,transformed.len,
                               "Save As: copy and rebase images",false,error,error_cap);
        if (ok) {
            (void)snprintf(doc->path,sizeof(doc->path),"%s",temporary.path);
            (void)snprintf(doc->display_name,sizeof(doc->display_name),"%s",temporary.display_name);
            (void)snprintf(doc->id,sizeof(doc->id),"%s",temporary.id);
            memcpy(doc->disk_sha256,temporary.disk_sha256,32U); doc->has_disk_sha256=true;
            doc->untitled=false; doc->orphaned=false; doc->conflict=false; doc->dirty=false;
        }
    }
    md_document_free(&temporary); md_buf_free(&transformed); return ok;
}

bool md_export_portable_single(const MdDocument *doc,const char *destination,
                               char *error,size_t error_cap) {
    if (doc->path[0]=='\0') { storage_error(error,error_cap,"Save the source document before portable export"); return false; }
    ExportContext ctx={true,doc,NULL,NULL}; MdBuf transformed; md_buf_init(&transformed);
    if (!export_transform(&ctx,&transformed,error,error_cap)) { md_buf_free(&transformed); return false; }
    bool ok=md_write_file_atomic(destination,transformed.data,transformed.len,error,error_cap); md_buf_free(&transformed); return ok;
}

bool md_export_portable_assets(const MdDocument *doc,const char *destination,
                               char *error,size_t error_cap) {
    if (doc->path[0]=='\0') { storage_error(error,error_cap,"Save the source document before portable export"); return false; }
    char dest_dir[MD_PATH_MAX],base[MD_PATH_MAX];
    if (!md_path_dirname(dest_dir,destination)||!md_path_basename(base,destination)) return false;
    char *dot=strrchr(base,'.'); if (dot!=NULL) *dot='\0';
    char asset_name[MD_PATH_MAX]; int n=snprintf(asset_name,sizeof(asset_name),"%s.assets",base);
    char asset_dir[MD_PATH_MAX];
    if (n<0||(size_t)n>=sizeof(asset_name)||!md_path_join(asset_dir,dest_dir,asset_name)) return false;
    struct stat st;
    if (lstat(asset_dir,&st)==0) { storage_error(error,error_cap,"Portable export asset directory already exists: %s",asset_dir); return false; }
    if (errno!=ENOENT) { storage_error(error,error_cap,"Cannot inspect portable export destination: %s",strerror(errno)); return false; }
    if (lstat(destination,&st)==0) { storage_error(error,error_cap,"Portable export destination already exists: %s",destination); return false; }
    if (errno!=ENOENT) { storage_error(error,error_cap,"Cannot inspect portable Markdown destination: %s",strerror(errno)); return false; }
    static unsigned export_counter=0U;
    char stage_dir[MD_PATH_MAX],stage_markdown[MD_PATH_MAX];
    n=snprintf(stage_dir,sizeof(stage_dir),"%s/.mdedit-export-assets-%ld-%u",dest_dir,(long)getpid(),++export_counter);
    if (n<0||(size_t)n>=sizeof(stage_dir)) { storage_error(error,error_cap,"Portable export staging path is too long"); return false; }
    n=snprintf(stage_markdown,sizeof(stage_markdown),"%s/.mdedit-export-markdown-%ld-%u",dest_dir,(long)getpid(),export_counter);
    if (n<0||(size_t)n>=sizeof(stage_markdown)) { storage_error(error,error_cap,"Portable export staging path is too long"); return false; }
    if (mkdir(stage_dir,0755)!=0) { storage_error(error,error_cap,"Cannot create portable export staging directory: %s",strerror(errno)); return false; }
    ExportContext ctx={false,doc,stage_dir,asset_name}; MdBuf transformed; md_buf_init(&transformed);
    bool transformed_ok=export_transform(&ctx,&transformed,error,error_cap);
    bool markdown_ok=transformed_ok&&md_write_file_atomic(stage_markdown,transformed.data,transformed.len,error,error_cap);
    md_buf_free(&transformed);
    if (!markdown_ok) {
        char cleanup_error[256]; (void)workspace_delete_tree(stage_dir,cleanup_error,sizeof(cleanup_error));
        (void)unlink(stage_markdown);
        storage_error(error,error_cap,"Portable asset export failed transactionally; no final package was installed and the original document remains unchanged");
        return false;
    }
    if (rename(stage_dir,asset_dir)!=0) {
        char saved[512]; (void)snprintf(saved,sizeof(saved),"%s",error==NULL?"":error);
        char cleanup_error[256]; (void)workspace_delete_tree(stage_dir,cleanup_error,sizeof(cleanup_error)); (void)unlink(stage_markdown);
        storage_error(error,error_cap,"Cannot install portable asset directory: %s; no final package was installed%s%s",strerror(errno),saved[0]==0?"":"; ",saved);
        return false;
    }
    if (rename(stage_markdown,destination)!=0) {
        int saved_errno=errno;
        if (rename(asset_dir,stage_dir)==0) { char cleanup_error[256]; (void)workspace_delete_tree(stage_dir,cleanup_error,sizeof(cleanup_error)); }
        (void)unlink(stage_markdown);
        storage_error(error,error_cap,"Cannot install portable Markdown file: %s; asset package was rolled back",strerror(saved_errno));
        return false;
    }
    int dfd=open(dest_dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (dfd>=0) { (void)fsync(dfd); (void)close(dfd); }
    return true;
}
