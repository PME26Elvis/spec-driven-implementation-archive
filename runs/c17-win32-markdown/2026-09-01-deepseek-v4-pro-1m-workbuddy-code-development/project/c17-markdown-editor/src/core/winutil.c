/* winutil.c - Windows platform helpers. */
#include "winutil.h"
#include "ce_common.h"
#include "buf.h"
#include "sha256.h"
#include <windows.h>
#include <wchar.h>

wchar_t *wu_u8_to_w(const char *u8){
    if(!u8) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, u8, -1, NULL, 0);
    if(n <= 0){ n = MultiByteToWideChar(CP_UTF8, 0, u8, -1, NULL, 0); }
    if(n <= 0) return NULL;
    wchar_t *w = ce_malloc((size_t)n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, n);
    return w;
}

char *wu_w_to_u8(const wchar_t *w){
    if(!w) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if(n <= 0) return NULL;
    char *u = ce_malloc((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, u, n, NULL, NULL);
    return u;
}

char *wu_norm_sep(const char *path){
    char *p = ce_strdup(path);
    for(char *q = p; *q; q++) if(*q == '/') *q = '\\';
    return p;
}

bool wu_is_absolute(const char *path){
    if(!path) return false;
    if(path[0] == '\\' || path[0] == '/') return true;  /* rooted / UNC */
    if(((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') return true;
    return false;
}

wchar_t *wu_u8_to_native(const char *u8){
    char *norm = wu_norm_sep(u8);
    wchar_t *w;
    /* extended-length prefix for long absolute paths */
    size_t n = strlen(norm);
    if(wu_is_absolute(norm) && n > 260 && !(norm[0] == '\\' && norm[1] == '\\' && norm[2] == '?')){
        /* build \\?\X:\... */
        wchar_t *base = wu_u8_to_w(norm);
        if(!base){ ce_free(norm); return NULL; }
        size_t bl = wcslen(base);
        wchar_t *p = ce_malloc((bl + 5) * sizeof(wchar_t));
        if(norm[0] == '\\' && norm[1] == '\\'){
            /* UNC: \\?\UNC\server\share */
            wcscpy(p, L"\\\\?\\UNC");
            wcscat(p, base + 1);
        } else {
            wcscpy(p, L"\\\\?\\");
            wcscat(p, base);
        }
        ce_free(base);
        ce_free(norm);
        return p;
    }
    w = wu_u8_to_w(norm);
    ce_free(norm);
    return w;
}

bool wu_exists(const char *path){
    wchar_t *w = wu_u8_to_native(path);
    if(!w) return false;
    DWORD a = GetFileAttributesW(w);
    ce_free(w);
    return a != INVALID_FILE_ATTRIBUTES;
}

bool wu_is_dir(const char *path){
    wchar_t *w = wu_u8_to_native(path);
    if(!w) return false;
    DWORD a = GetFileAttributesW(w);
    ce_free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool wu_is_file(const char *path){
    wchar_t *w = wu_u8_to_native(path);
    if(!w) return false;
    DWORD a = GetFileAttributesW(w);
    ce_free(w);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool wu_is_reparse_point(const char *path){
    wchar_t *w = wu_u8_to_native(path);
    if(!w) return false;
    DWORD a = GetFileAttributesW(w);
    ce_free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
}

char *wu_read_file(const char *path, size_t *out_len){
    wchar_t *w = wu_u8_to_native(path);
    if(!w) return NULL;
    HANDLE h = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    ce_free(w);
    if(h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER sz;
    if(!GetFileSizeEx(h, &sz)){ CloseHandle(h); return NULL; }
    size_t len = (size_t)sz.QuadPart;
    char *buf = ce_malloc(len + 1);
    size_t got = 0;
    DWORD rd = 0;
    while(got < len){
        DWORD chunk = (DWORD)((len - got) > 0x40000000 ? 0x40000000 : (len - got));
        if(!ReadFile(h, buf + got, chunk, &rd, NULL) || rd == 0) break;
        got += rd;
    }
    CloseHandle(h);
    if(got != len){ ce_free(buf); return NULL; }
    buf[len] = 0;
    if(out_len) *out_len = len;
    return buf;
}

bool wu_write_file(const char *path, const void *data, size_t len){
    /* stage to temp in same dir, then replace */
    char *dir = ce_strdup(path);
    char *slash = strrchr(dir, '\\');
    if(slash) *slash = 0; else { dir[0] = '.'; dir[1] = 0; }
    char *tmp = ce_malloc(strlen(dir) + 64);
    snprintf(tmp, strlen(dir) + 64, "%s\\.wutmp_%u", dir, (unsigned)GetCurrentProcessId());
    wchar_t *wtmp = wu_u8_to_native(tmp);
    wchar_t *wpath = wu_u8_to_native(path);
    bool ok = false;
    if(wtmp && wpath){
        HANDLE h = CreateFileW(wtmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(h != INVALID_HANDLE_VALUE){
            DWORD wr = 0;
            size_t off = 0;
            bool all = true;
            while(off < len){
                DWORD chunk = (DWORD)((len - off) > 0x40000000 ? 0x40000000 : (len - off));
                if(!WriteFile(h, (const char*)data + off, chunk, &wr, NULL) || wr == 0){ all = false; break; }
                off += wr;
            }
            if(all && FlushFileBuffers(h)) ok = true;
            CloseHandle(h);
            if(ok){
                if(!MoveFileExW(wtmp, wpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)){
                    ok = false;
                }
            }
            if(!ok) DeleteFileW(wtmp);
        }
    }
    if(wtmp) ce_free(wtmp);
    if(wpath) ce_free(wpath);
    ce_free(tmp);
    ce_free(dir);
    return ok;
}

bool wu_walk_dir(const char *root, bool follow_reparse, wu_walk_cb cb, void *ctx){
    char *norm = wu_norm_sep(root);
    size_t rl = strlen(norm);
    /* build search pattern */
    ce_buf pat; ce_buf_init(&pat);
    ce_buf_append_str(&pat, norm);
    if(rl == 0 || norm[rl-1] != '\\') ce_buf_append_c(&pat, '\\');
    ce_buf_append_c(&pat, '*');
    wchar_t *wpat = wu_u8_to_w(pat.data);
    ce_buf_free(&pat);
    if(!wpat){ ce_free(norm); return false; }

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wpat, &fd);
    ce_free(wpat);
    if(h == INVALID_HANDLE_VALUE){ ce_free(norm); return false; }

    ce_buf full; ce_buf_init(&full);
    do {
        if(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        char *name = wu_w_to_u8(fd.cFileName);
        ce_buf_clear(&full);
        ce_buf_append_str(&full, norm);
        if(full.len == 0 || full.data[full.len-1] != '\\') ce_buf_append_c(&full, '\\');
        ce_buf_append_str(&full, name);
        ce_free(name);
        bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool is_reparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if(is_dir){
            if(is_reparse && !follow_reparse){
                /* report as leaf dir but do not descend */
                if(cb(ctx, full.data, 1) != 0){ FindClose(h); ce_buf_free(&full); ce_free(norm); return true; }
                continue;
            }
            if(cb(ctx, full.data, 1) != 0){ FindClose(h); ce_buf_free(&full); ce_free(norm); return true; }
            /* descend into normal dirs always; reparse dirs only when allowed */
            wu_walk_dir(full.data, follow_reparse, cb, ctx);
        } else {
            if(cb(ctx, full.data, 0) != 0){ FindClose(h); ce_buf_free(&full); ce_free(norm); return true; }
        }
    } while(FindNextFileW(h, &fd));
    FindClose(h);
    ce_buf_free(&full);
    ce_free(norm);
    return true;
}

bool wu_file_sha256(const char *path, uint8_t out[32]){
    size_t len = 0;
    char *data = wu_read_file(path, &len);
    if(!data) return false;
    ce_sha256_hash(data, len, out);
    ce_free(data);
    return true;
}

double wu_now_seconds(void){
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
