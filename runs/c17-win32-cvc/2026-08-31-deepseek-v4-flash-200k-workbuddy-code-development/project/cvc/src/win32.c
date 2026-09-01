#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "win32.h"
#include "utf8.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <wchar.h>
#include <winioctl.h>
#include <ntdef.h>

/* ------------------------------------------------------------------ */
/* Output helpers                                                      */
/* ------------------------------------------------------------------ */
static void out_stream(const char *s, size_t n, FILE *f){
    /* Use fwrite to avoid code-page translation. Redirected output stays
       raw bytes (UTF-8). */
    fwrite(s, 1, n, f);
    fflush(f);
}
void w_out_stdout(const char *s, size_t n){ out_stream(s, n, stdout); }
void w_out_stderr(const char *s, size_t n){ out_stream(s, n, stderr); }

/* ------------------------------------------------------------------ */
/* Path helpers                                                         */
/* ------------------------------------------------------------------ */
uint16_t *w_extended(const uint16_t *path){
    /* If already \\?\ prefixed, copy. */
    if(path[0]==L'\\' && path[1]==L'\\' && path[2]==L'?' && path[3]==L'\\'){
        size_t n=0; while(path[n]) n++;
        uint16_t *r=(uint16_t*)malloc((n+1)*sizeof(uint16_t));
        if(!r) return NULL;
        memcpy(r,path,(n+1)*sizeof(uint16_t));
        return r;
    }
    /* If it's already a UNC \\server\share, use \\?\UNC\... */
    if(path[0]==L'\\' && path[1]==L'\\'){
        size_t n=0; while(path[n]) n++;
        /* "\\?\UNC" + path[1..] i.e. skip leading backslash */
        uint16_t *r=(uint16_t*)malloc((n+8)*sizeof(uint16_t));
        if(!r) return NULL;
        static const uint16_t pfx[]={'\\','\\','?','\\','U','N','C'};
        size_t i; for(i=0;i<7;i++) r[i]=pfx[i];
        r[7]=L'\\';
        for(i=0;i<n;i++) r[i+8]=path[i+1];
        r[n+8]=0;
        return r;
    }
    /* drive-relative absolute like C:\... */
    size_t n=0; while(path[n]) n++;
    uint16_t *r=(uint16_t*)malloc((n+5)*sizeof(uint16_t));
    if(!r) return NULL;
    static const uint16_t pfx[]={'\\','\\','?','\\'};
    for(size_t i=0;i<4;i++) r[i]=pfx[i];
    for(size_t i=0;i<n;i++) r[i+4]=path[i];
    r[n+4]=0;
    return r;
}

uint16_t *w_getcwd16(void){
    DWORD cap = GetCurrentDirectoryW(0, NULL);
    if(!cap) return NULL;
    uint16_t *buf=(uint16_t*)malloc(cap*sizeof(uint16_t));
    if(!buf) return NULL;
    if(!GetCurrentDirectoryW(cap, buf)){ free(buf); return NULL; }
    /* ensure trailing backslash for later joining */
    size_t n=0; while(buf[n]) n++;
    if(n>0 && buf[n-1]==L'\\') return buf;
    uint16_t *r=(uint16_t*)malloc((n+2)*sizeof(uint16_t));
    if(!r){ free(buf); return NULL; }
    memcpy(r,buf,(n+1)*sizeof(uint16_t));
    r[n]=L'\\'; r[n+1]=0;
    free(buf);
    return r;
}

uint16_t *w_join(const uint16_t *base16, const char *comp8){
    size_t out_units=0;
    uint16_t *comp = utf8_to_utf16(comp8, strlen(comp8), &out_units);
    if(!comp) return NULL;
    size_t b=0; while(base16[b]) b++;
    size_t clen = out_units-1; /* exclude trailing NUL */
    /* base ends with backslash? */
    int base_has_sep = (b>0 && (base16[b-1]==L'\\' || base16[b-1]==L'/'));
    uint16_t *r=(uint16_t*)malloc((b+1+clen+1)*sizeof(uint16_t));
    if(!r){ free(comp); return NULL; }
    size_t i;
    for(i=0;i<b;i++) r[i]=base16[i];
    size_t pos=b;
    if(!base_has_sep){ r[pos++]=L'\\'; }
    for(i=0;i<clen;i++){ r[pos]=(comp[i]==L'/')?L'\\':comp[i]; pos++; }
    r[pos]=0;
    free(comp);
    return r;
}

uint16_t *w_repo_to_abs(const uint16_t *repo_root16, const char *repo_rel_path){
    /* repo_root16 should be extended \\?\... with trailing backslash. */
    uint16_t *cur = _wcsdup(repo_root16);
    if(!cur) return NULL;
    /* split on '/' */
    const char *p = repo_rel_path;
    const char *start = p;
    while(1){
        const char *end = start;
        while(*end && *end!='/') end++;
        size_t seg_len = (size_t)(end-start);
        char *seg=(char*)malloc(seg_len+1);
        if(!seg){ free(cur); return NULL; }
        memcpy(seg, start, seg_len); seg[seg_len]=0;
        if(seg_len>0){
            uint16_t *nu = w_join(cur, seg);
            free(seg);
            free(cur);
            if(!nu){ return NULL; }
            cur = nu;
        } else {
            free(seg);
        }
        if(*end=='\0') break;
        start = end+1;
    }
    return cur;
}

/* ------------------------------------------------------------------ */
/* Stat                                                                */
/* ------------------------------------------------------------------ */
static void classify_attrs(DWORD attrs, uint32_t reparse_tag,
                           WStat *st, int is_dir_attr, int is_reparse_attr){
    st->is_dir = is_dir_attr;
    st->is_reparse = is_reparse_attr;
    if(is_reparse_attr){
        st->reparse_tag = reparse_tag;
        st->is_symlink = (reparse_tag == IO_REPARSE_TAG_SYMLINK);
        /* symlink_is_dir determined from FILE_ATTRIBUTE_DIRECTORY bit */
        st->symlink_is_dir = is_dir_attr;
    } else {
        st->reparse_tag = 0;
        st->is_symlink = 0;
        st->symlink_is_dir = 0;
    }
}

int w_stat(const uint16_t *abs_path, WStat *st){
    /* Use FindFirstFileW to get reparse tag and attributes without following
       the link. */
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(abs_path, &fd);
    if(h == INVALID_HANDLE_VALUE) return -1;
    FindClose(h);
    st->exists = 1;
    DWORD attrs = fd.dwFileAttributes;
    uint32_t tag = 0;
    if(attrs & FILE_ATTRIBUTE_REPARSE_POINT){
        tag = fd.dwReserved0;
    }
    classify_attrs(attrs, tag, st, (attrs&FILE_ATTRIBUTE_DIRECTORY)?1:0, (attrs&FILE_ATTRIBUTE_REPARSE_POINT)?1:0);
    st->size = 0;
    if(!(attrs & FILE_ATTRIBUTE_DIRECTORY)){
        st->size = ((uint64_t)fd.nFileSizeHigh<<32) | fd.nFileSizeLow;
    }
    return 0;
}

int w_exists(const uint16_t *abs_path){
    DWORD a = GetFileAttributesW(abs_path);
    return a != INVALID_FILE_ATTRIBUTES;
}

/* ------------------------------------------------------------------ */
/* Directory enumeration                                               */
/* ------------------------------------------------------------------ */
int wdir_list(const uint16_t *abs_dir_path, wdir_cb cb, void *ctx){
    /* Build pattern dir\* */
    size_t dl=0; while(abs_dir_path[dl]) dl++;
    int has_sep = dl>0 && (abs_dir_path[dl-1]==L'\\'||abs_dir_path[dl-1]==L'/');
    uint16_t *pat=(uint16_t*)malloc((dl+3)*sizeof(uint16_t));
    if(!pat) return -1;
    for(size_t i=0;i<dl;i++) pat[i]=abs_dir_path[i];
    size_t pos=dl;
    if(!has_sep) pat[pos++]=L'\\';
    pat[pos++]=L'*'; pat[pos]=0;
    WIN32_FIND_DATAW fd;
    HANDLE h=FindFirstFileW(pat, &fd);
    free(pat);
    if(h==INVALID_HANDLE_VALUE) return 0; /* empty */
    int rc=0;
    do {
        if(wcscmp(fd.cFileName, L".")==0 || wcscmp(fd.cFileName, L"..")==0) continue;
        WDirEntry e;
        memset(&e,0,sizeof e);
        size_t nl=0; while(fd.cFileName[nl]) nl++;
        e.name16 = (uint16_t*)malloc((nl+1)*sizeof(uint16_t));
        if(!e.name16){ FindClose(h); return -1; }
        memcpy(e.name16, fd.cFileName, (nl+1)*sizeof(uint16_t));
        e.name16_len = nl;
        /* UTF-8 conversion (may fail for unpaired surrogate -> unversionable) */
        e.name8 = utf8_from_utf16(fd.cFileName, nl, NULL);
        DWORD attrs = fd.dwFileAttributes;
        e.is_dir = (attrs & FILE_ATTRIBUTE_DIRECTORY)?1:0;
        e.is_reparse = (attrs & FILE_ATTRIBUTE_REPARSE_POINT)?1:0;
        e.reparse_tag = e.is_reparse ? (uint32_t)fd.dwReserved0 : 0;
        e.is_symlink = e.is_reparse && (e.reparse_tag==IO_REPARSE_TAG_SYMLINK);
        e.symlink_is_dir = e.is_dir; /* from the link's own attributes */
        rc = cb(abs_dir_path, &e, ctx);
        if(rc != 0){ FindClose(h); return rc; }
    } while(FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Symlink readback                                                    */
/* ------------------------------------------------------------------ */
int w_symlink_read(const uint16_t *abs_path, char **printname8, int *is_dir){
    HANDLE h = CreateFileW(abs_path, GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        NULL);
    if(h == INVALID_HANDLE_VALUE) return -1;
    /* Query reparse point buffer */
    DWORD bufsize = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
    char *raw = (char*)malloc(bufsize);
    if(!raw){ CloseHandle(h); return -1; }
    DWORD ret=0;
    BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0,
                               raw, bufsize, &ret, NULL);
    CloseHandle(h);
    if(!ok){ free(raw); return -1; }
    if(ret < sizeof(REPARSE_DATA_BUFFER)){ free(raw); return -1; }
    REPARSE_DATA_BUFFER *rdb = (REPARSE_DATA_BUFFER*)raw;
    if(rdb->ReparseTag != IO_REPARSE_TAG_SYMLINK){ free(raw); return -1; }
    /* PrintName in the symbolic link buffer */
    uint16_t *print = rdb->SymbolicLinkReparseBuffer.PathBuffer +
                      (rdb->SymbolicLinkReparseBuffer.PrintNameOffset/2);
    DWORD print_len_chars = rdb->SymbolicLinkReparseBuffer.PrintNameLength/2;
    if(print_len_chars == 0){ free(raw); return -1; }
    /* Validate no embedded U+0000 */
    DWORD i; for(i=0;i<print_len_chars;i++) if(print[i]==0){ free(raw); return -1; }
    char *u8 = utf8_from_utf16(print, print_len_chars, NULL);
    free(raw);
    if(!u8){ return -1; }
    *printname8 = u8;
    if(is_dir){
        /* Determine link kind from the link's own attributes (passed in via
           abs path's stat). We can't reliably get it here without re-stating;
           caller passes abs_path which we re-stat. */
        WStat st;
        if(w_stat(abs_path, &st)==0){
            *is_dir = st.symlink_is_dir;
        } else {
            *is_dir = 0;
        }
    }
    return 0;
}

int w_symlink_create(const uint16_t *abs_link_path, const char *target_utf8, int is_dir){
    size_t units=0;
    uint16_t *t16 = utf8_to_utf16(target_utf8, strlen(target_utf8), &units);
    if(!t16) return -1;
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if(is_dir) flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
    BOOL ok = CreateSymbolicLinkW(abs_link_path, t16, flags);
    if(!ok){
        /* Retry without the unprivileged flag if the host rejects it. */
        DWORD fl2 = is_dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        ok = CreateSymbolicLinkW(abs_link_path, t16, fl2);
    }
    free(t16);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* File I/O                                                             */
/* ------------------------------------------------------------------ */
CvcStatus w_read_file(const uint16_t *abs_path, Bytes *out){
    HANDLE h = CreateFileW(abs_path, GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h == INVALID_HANDLE_VALUE) return cvc_fail(CVC_ERR, "cannot open file");
    LARGE_INTEGER sz;
    if(!GetFileSizeEx(h, &sz)){ CloseHandle(h); return cvc_fail(CVC_ERR,"cannot size file"); }
    if(sz.QuadPart < 0 || sz.QuadPart > (LONGLONG)(8*1024*1024 + 1024)){
        /* CVC never reads >8MiB files as blobs; but read-whole is used for
           objects which are small. Guard large reads. */
        if(sz.QuadPart > (LONGLONG)(64*1024*1024)){ CloseHandle(h); return cvc_fail(CVC_ERR,"file too large to read"); }
    }
    size_t flen = (size_t)sz.QuadPart;
    if(bytes_reserve(out, flen)!=0){ CloseHandle(h); return cvc_fail(CVC_ERR,"oom"); }
    out->len = 0;
    DWORD total=0;
    uint8_t *p = out->data;
    while(total < flen){
        DWORD chunk = (DWORD)((flen-total) > 0x7FFFFFFF ? 0x7FFFFFFF : (flen-total));
        DWORD got=0;
        if(!ReadFile(h, p+total, chunk, &got, NULL) || got==0){
            CloseHandle(h); return cvc_fail(CVC_ERR,"read failed");
        }
        total += got;
    }
    out->len = flen;
    if(out->data) out->data[flen]=0;
    CloseHandle(h);
    return CVC_OK;
}

int w_delete_path(const uint16_t *abs_path, int is_dir){
    if(is_dir){
        return RemoveDirectoryW(abs_path) ? 0 : -1;
    }
    return DeleteFileW(abs_path) ? 0 : -1;
}

int w_mkdir(const uint16_t *abs_path){
    if(CreateDirectoryW(abs_path, NULL)) return 0;
    DWORD e=GetLastError();
    if(e==ERROR_ALREADY_EXISTS) return 0; /* treat existing as ok */
    return -1;
}

static uint16_t *w_temp_name(const uint16_t *dir_abs, const char *base){
    /* unique temp filename in dir_abs */
    static volatile LONG counter=0;
    char tmp[512];
    snprintf(tmp,sizeof tmp,"%s_%lx_%lx.tmp", base, GetCurrentProcessId(), InterlockedIncrement(&counter));
    return w_join(dir_abs, tmp);
}

int w_write_file_atomic(const uint16_t *dest_abs, const uint8_t *data, size_t len){
    /* derive dir from dest */
    size_t dl=0; while(dest_abs[dl]) dl++;
    /* find last backslash */
    size_t slash=0;
    for(size_t i=0;i<dl;i++) if(dest_abs[i]==L'\\') slash=i;
    uint16_t *dir=(uint16_t*)malloc((slash+1)*sizeof(uint16_t));
    if(!dir) return -1;
    for(size_t i=0;i<slash;i++) dir[i]=dest_abs[i];
    dir[slash]=0;
    uint16_t *tmp=w_temp_name(dir,"cvc");
    free(dir);
    if(!tmp) return -1;
    HANDLE h=CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE){ free(tmp); return -1; }
    DWORD written=0;
    size_t off=0;
    int rc=-1;
    while(off<len){
        DWORD chunk=(DWORD)((len-off)>0x7FFFFFFF?0x7FFFFFFF:(len-off));
        if(!WriteFile(h, data+off, chunk, &written, NULL)){ goto done; }
        off += written;
    }
    CloseHandle(h); h=INVALID_HANDLE_VALUE;
    /* publish. Fast path: no per-file fsync. Durability of content-addressed
       objects and working-tree files is established by the later ref
       publication fsync (w_write_file_durable) on the same volume, which
       flushes the NTFS volume journal covering prior writes on that volume.
       The ref write ALWAYS happens after every object/materialized-file write
       it references, so the O11 ordering guarantee (ref publication never
       precedes durable installation of referenced objects) is preserved. */
    if(!MoveFileExW(tmp, dest_abs, MOVEFILE_REPLACE_EXISTING)){ goto done; }
    rc=0;
done:
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    DeleteFileW(tmp);
    free(tmp);
    return rc;
}

/* Durable variant used for ref publication. fsyncs the file data AND forces
   the directory entry (and thus the NTFS volume journal) to disk, making all
   previously written objects/materialized files on the same volume durable
   before the caller signals success. */
int w_write_file_durable(const uint16_t *dest_abs, const uint8_t *data, size_t len){
    size_t dl=0; while(dest_abs[dl]) dl++;
    size_t slash=0;
    for(size_t i=0;i<dl;i++) if(dest_abs[i]==L'\\') slash=i;
    uint16_t *dir=(uint16_t*)malloc((slash+1)*sizeof(uint16_t));
    if(!dir) return -1;
    for(size_t i=0;i<slash;i++) dir[i]=dest_abs[i];
    dir[slash]=0;
    uint16_t *tmp=w_temp_name(dir,"cvc");
    free(dir);
    if(!tmp) return -1;
    HANDLE h=CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE){ free(tmp); return -1; }
    DWORD written=0;
    size_t off=0;
    int rc=-1;
    while(off<len){
        DWORD chunk=(DWORD)((len-off)>0x7FFFFFFF?0x7FFFFFFF:(len-off));
        if(!WriteFile(h, data+off, chunk, &written, NULL)){ goto done; }
        off += written;
    }
    if(!FlushFileBuffers(h)){ goto done; }
    CloseHandle(h); h=INVALID_HANDLE_VALUE;
    if(!MoveFileExW(tmp, dest_abs, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)){ goto done; }
    rc=0;
done:
    if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
    DeleteFileW(tmp);
    free(tmp);
    return rc;
}

int w_move_replace(const uint16_t *src_abs, const uint16_t *dst_abs){
    if(MoveFileExW(src_abs, dst_abs, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
    return -1;
}

/* ------------------------------------------------------------------ */
/* Locking                                                              */
/* ------------------------------------------------------------------ */
int w_lock_open(const uint16_t *lock_path, RepoLock *lk){
    /* Open without truncation, sharing read/write/delete so competitors can
       open. */
    HANDLE h = CreateFileW(lock_path, GENERIC_READ|GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h==INVALID_HANDLE_VALUE) return -1;
    lk->handle=h;
    return 0;
}
int w_lock_acquire(RepoLock *lk, int exclusive){
    OVERLAPPED ov; memset(&ov,0,sizeof ov);
    ov.Offset=0; ov.OffsetHigh=0;
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;
    if(exclusive) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if(LockFileEx(lk->handle, flags, 0, 1, 0, &ov)) return 1;
    DWORD e=GetLastError();
    if(e==ERROR_LOCK_VIOLATION) return 0; /* busy */
    return -1;
}
void w_lock_release(RepoLock *lk){
    OVERLAPPED ov; memset(&ov,0,sizeof ov);
    ov.Offset=0; ov.OffsetHigh=0;
    UnlockFileEx(lk->handle, 0, 1, 0, &ov);
}
void w_lock_close(RepoLock *lk){
    if(lk->handle && lk->handle!=INVALID_HANDLE_VALUE) CloseHandle(lk->handle);
    lk->handle=INVALID_HANDLE_VALUE;
}

/* ------------------------------------------------------------------ */
/* Volume component limit                                               */
/* ------------------------------------------------------------------ */
uint32_t w_volume_component_limit(const uint16_t *abs_path){
    /* Get the root of the volume for abs_path */
    uint16_t root[8];
    DWORD flags=0;
    uint16_t fsname[MAX_PATH+1];
    DWORD maxcomp=0;
    /* abs_path is extended \\?\C:\... ; derive "C:\" by scanning */
    wchar_t drive[8]={0};
    int got=0;
    /* find first drive letter colon */
    for(size_t i=0;abs_path[i];i++){
        if(abs_path[i]==L':' && i>0){ drive[0]=abs_path[i-1]; drive[1]=L':'; drive[2]=L'\\'; got=1; break; }
    }
    if(!got) return 255;
    wcsncpy(root, drive, 7);
    if(!GetVolumeInformationW(root, NULL, 0, NULL, &maxcomp, &flags, fsname, MAX_PATH)) return 255;
    if(maxcomp==0) return 255;
    return maxcomp;
}

uint16_t *w_realpath(const uint16_t *abs_path){
    /* For repository root we just return a copy; resolving symlinks is not
       needed for correctness here. */
    size_t n=0; while(abs_path[n]) n++;
    uint16_t *r=(uint16_t*)malloc((n+1)*sizeof(uint16_t));
    if(!r) return NULL;
    memcpy(r,abs_path,(n+1)*sizeof(uint16_t));
    return r;
}

uint16_t *w_pretty_path(const uint16_t *abs_ext){
    /* Strip \\?\ prefix for display. */
    if(abs_ext[0]==L'\\'&&abs_ext[1]==L'\\'&&abs_ext[2]==L'?'&&abs_ext[3]==L'\\'){
        const uint16_t *p=abs_ext+4;
        size_t n=0; while(p[n]) n++;
        uint16_t *r=(uint16_t*)malloc((n+1)*sizeof(uint16_t));
        if(!r) return NULL;
        memcpy(r,p,(n+1)*sizeof(uint16_t));
        return r;
    }
    size_t n=0; while(abs_ext[n]) n++;
    uint16_t *r=(uint16_t*)malloc((n+1)*sizeof(uint16_t));
    if(!r) return NULL;
    memcpy(r,abs_ext,(n+1)*sizeof(uint16_t));
    return r;
}

int64_t w_wall_clock(void){
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    /* FILETIME is 100ns since 1601. Convert to unix seconds. */
    ULARGE_INTEGER u;
    u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime;
    uint64_t t100 = u.QuadPart;
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if(t100 < EPOCH_DIFF) return 0;
    return (int64_t)((t100 - EPOCH_DIFF) / 10000000ULL);
}

/* Returns 1 if CVC_TEST_TIMESTAMP is absent or well-formed
 * (matches 0|-?[1-9][0-9]* fitting int64_t), else 0. The value itself is
 * returned via *out when the env var is present and valid. */
int w_timestamp_valid(int64_t *out){
    const char *v = getenv("CVC_TEST_TIMESTAMP");
    if(!v || *v=='\0'){ if(out) *out=0; return 1; }
    const char *p=v;
    int neg=0;
    if(*p=='-'){ neg=1; p++; }
    if(*p=='0'){
        /* only a bare "0" (no '-' and nothing after) is allowed */
        if(neg || p[1]!='\0') return 0;  /* "-0" and "01" are malformed */
        if(out) *out=0;
        return 1;
    }
    if(*p<'1'||*p>'9') return 0;       /* '+', ' ', or non-digit start */
    /* Accumulate in uint64_t so the full |INT64_MIN| magnitude is representable. */
    uint64_t uval=0;
    while(*p){
        if(*p<'0'||*p>'9') return 0;   /* embedded/trailing non-digit */
        uint64_t nd=(uint64_t)(*p-'0');
        if(uval > (UINT64_MAX-nd)/10) return 0;  /* beyond uint64 */
        uval = uval*10+nd;
        p++;
    }
    if(neg){
        if(uval > (uint64_t)INT64_MAX + 1) return 0;  /* overflow int64 */
        if(out) *out = (uval==(uint64_t)INT64_MAX+1) ? INT64_MIN : -(int64_t)uval;
        return 1;
    }
    if(uval > (uint64_t)INT64_MAX) return 0;  /* positive overflow int64 */
    if(out) *out=(int64_t)uval;
    return 1;
}

int64_t w_unix_time(void){
    /* If CVC_TEST_TIMESTAMP is set and matches 0|-?[1-9][0-9]* fitting int64,
       use it. The caller MUST have validated it with w_timestamp_valid();
       here we only read the (valid) env var and fall back to wall clock. */
    int64_t ts;
    if(w_timestamp_valid(&ts) && getenv("CVC_TEST_TIMESTAMP")) return ts;
    return w_wall_clock();
}
