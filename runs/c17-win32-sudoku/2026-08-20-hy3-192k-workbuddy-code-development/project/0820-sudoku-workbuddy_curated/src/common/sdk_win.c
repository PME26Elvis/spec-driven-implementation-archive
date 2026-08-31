/* sdk_win.c - Unicode Win32 filesystem and path layer. */

#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include "common/sdk_win.h"

#include <windows.h>
#include <bcrypt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ------------------------------------------------------------------ */
/* UTF conversion                                                      */
/* ------------------------------------------------------------------ */

char *sdk_utf16_to_utf8(const wchar_t *w, size_t wlen, size_t *out_len) {
    sdk_buf b;
    sdk_buf_init(&b);
    for (size_t i = 0; i < wlen; ++i) {
        uint32_t cp = (uint32_t)(uint16_t)w[i];
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            if (i + 1 >= wlen) {
                sdk_buf_free(&b);
                return NULL;
            }
            uint32_t lo = (uint32_t)(uint16_t)w[i + 1];
            if (lo < 0xDC00u || lo > 0xDFFFu) {
                sdk_buf_free(&b);
                return NULL;
            }
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
            ++i;
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            sdk_buf_free(&b);
            return NULL;
        }

        if (cp <= 0x7Fu) {
            sdk_buf_append_u8(&b, (uint8_t)cp);
        } else if (cp <= 0x7FFu) {
            sdk_buf_append_u8(&b, (uint8_t)(0xC0u | (cp >> 6)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | (cp & 0x3Fu)));
        } else if (cp <= 0xFFFFu) {
            sdk_buf_append_u8(&b, (uint8_t)(0xE0u | (cp >> 12)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | (cp & 0x3Fu)));
        } else {
            sdk_buf_append_u8(&b, (uint8_t)(0xF0u | (cp >> 18)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu)));
            sdk_buf_append_u8(&b, (uint8_t)(0x80u | (cp & 0x3Fu)));
        }
        if (b.failed) {
            sdk_buf_free(&b);
            return NULL;
        }
    }
    if (!sdk_buf_append_u8(&b, 0)) {
        sdk_buf_free(&b);
        return NULL;
    }
    if (out_len) {
        *out_len = b.len - 1;
    }
    return (char *)b.data;
}

int sdk_utf8_validate(const char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t need;
        uint32_t cp;
        if (c < 0x80u) {
            i++;
            continue;
        } else if ((c & 0xE0u) == 0xC0u) {
            need = 1;
            cp = c & 0x1Fu;
        } else if ((c & 0xF0u) == 0xE0u) {
            need = 2;
            cp = c & 0x0Fu;
        } else if ((c & 0xF8u) == 0xF0u) {
            need = 3;
            cp = c & 0x07u;
        } else {
            return 0;
        }
        if (i + need >= len + 0 && i + need > len - 1) {
            return 0;
        }
        for (size_t k = 1; k <= need; ++k) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0u) != 0x80u) {
                return 0;
            }
            cp = (cp << 6) | (uint32_t)(cc & 0x3Fu);
        }
        if (need == 1 && cp < 0x80u) return 0;          /* overlong */
        if (need == 2 && cp < 0x800u) return 0;         /* overlong */
        if (need == 3 && cp < 0x10000u) return 0;       /* overlong */
        if (cp > 0x10FFFFu) return 0;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return 0;   /* surrogate */
        i += need + 1;
    }
    return 1;
}

wchar_t *sdk_utf8_to_utf16(const char *s, size_t slen, size_t *out_len) {
    if (!sdk_utf8_validate(s, slen)) {
        return NULL;
    }
    /* Worst case: one UTF-16 unit per byte, plus surrogate pairs for 4-byte
     * sequences (4 bytes -> 2 units), so slen units is always enough. */
    wchar_t *w = (wchar_t *)malloc((slen + 1) * sizeof(wchar_t));
    if (!w) {
        return NULL;
    }
    size_t wi = 0;
    size_t i = 0;
    while (i < slen) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        size_t need;
        if (c < 0x80u) {
            cp = c;
            need = 0;
        } else if ((c & 0xE0u) == 0xC0u) {
            cp = c & 0x1Fu;
            need = 1;
        } else if ((c & 0xF0u) == 0xE0u) {
            cp = c & 0x0Fu;
            need = 2;
        } else {
            cp = c & 0x07u;
            need = 3;
        }
        for (size_t k = 1; k <= need; ++k) {
            cp = (cp << 6) | (uint32_t)((unsigned char)s[i + k] & 0x3Fu);
        }
        i += need + 1;
        if (cp <= 0xFFFFu) {
            w[wi++] = (wchar_t)cp;
        } else {
            cp -= 0x10000u;
            w[wi++] = (wchar_t)(0xD800u + (cp >> 10));
            w[wi++] = (wchar_t)(0xDC00u + (cp & 0x3FFu));
        }
    }
    w[wi] = 0;
    if (out_len) {
        *out_len = wi;
    }
    return w;
}

/* ------------------------------------------------------------------ */
/* Wide string helpers                                                 */
/* ------------------------------------------------------------------ */

wchar_t *sdk_wcsdup_n(const wchar_t *w, size_t len) {
    wchar_t *p = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!p) {
        return NULL;
    }
    if (len) {
        memcpy(p, w, len * sizeof(wchar_t));
    }
    p[len] = 0;
    return p;
}

wchar_t *sdk_wpath_join(const wchar_t *base, const wchar_t *leaf) {
    size_t bl = base ? wcslen(base) : 0;
    size_t ll = leaf ? wcslen(leaf) : 0;
    int need_sep = (bl > 0 && base[bl - 1] != L'\\' && base[bl - 1] != L'/');
    size_t total = bl + (need_sep ? 1u : 0u) + ll;
    wchar_t *p = (wchar_t *)malloc((total + 1) * sizeof(wchar_t));
    if (!p) {
        return NULL;
    }
    if (bl) {
        memcpy(p, base, bl * sizeof(wchar_t));
    }
    size_t at = bl;
    if (need_sep) {
        p[at++] = L'\\';
    }
    if (ll) {
        memcpy(p + at, leaf, ll * sizeof(wchar_t));
    }
    at += ll;
    p[at] = 0;
    return p;
}

int sdk_ord_cmp_w(const wchar_t *a, const wchar_t *b, int ignore_case) {
    int r = CompareStringOrdinal(a, -1, b, -1, ignore_case ? TRUE : FALSE);
    if (r == CSTR_LESS_THAN) return -1;
    if (r == CSTR_GREATER_THAN) return 1;
    if (r == CSTR_EQUAL) return 0;
    /* CompareStringOrdinal only fails on invalid arguments; fall back to a
     * deterministic byte comparison so ordering never becomes undefined. */
    return wcscmp(a, b) < 0 ? -1 : (wcscmp(a, b) > 0 ? 1 : 0);
}

int sdk_ord_cmp_utf8(const char *a, const char *b, int ignore_case) {
    wchar_t *wa = sdk_utf8_to_utf16(a, strlen(a), NULL);
    wchar_t *wb = sdk_utf8_to_utf16(b, strlen(b), NULL);
    int r;
    if (!wa || !wb) {
        r = strcmp(a, b);
        r = (r < 0) ? -1 : (r > 0 ? 1 : 0);
    } else {
        r = sdk_ord_cmp_w(wa, wb, ignore_case);
    }
    free(wa);
    free(wb);
    return r;
}

int sdk_canon_path_cmp(const char *a, const char *b) {
    int r = sdk_ord_cmp_utf8(a, b, 1);
    if (r != 0) {
        return r;
    }
    return sdk_ord_cmp_utf8(a, b, 0);
}

/* ------------------------------------------------------------------ */
/* Path validation                                                     */
/* ------------------------------------------------------------------ */

const char *sdk_path_reject_name(sdk_path_reject r) {
    switch (r) {
    case SDK_PATH_OK:                     return "ok";
    case SDK_PATH_REJ_EMPTY:              return "empty_component";
    case SDK_PATH_REJ_DOT:                return "dot_component";
    case SDK_PATH_REJ_DOTDOT:             return "dotdot_component";
    case SDK_PATH_REJ_NUL:                return "nul_byte";
    case SDK_PATH_REJ_COLON:              return "colon_or_stream";
    case SDK_PATH_REJ_TRAILING_SPACE_DOT: return "trailing_space_or_dot";
    case SDK_PATH_REJ_RESERVED_DEVICE:    return "reserved_device_name";
    case SDK_PATH_REJ_SEPARATOR:          return "separator_in_component";
    case SDK_PATH_REJ_TOO_LONG:           return "path_too_long";
    case SDK_PATH_REJ_DEPTH:              return "path_depth_exceeded";
    }
    return "unknown";
}

static int ascii_upper(int c) {
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

static int is_reserved_device(const char *comp, size_t len) {
    static const char *const names[] = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
        /* Windows also treats the superscript digit forms as devices. */
        "CONIN$", "CONOUT$"
    };
    /* The device name applies to the stem, i.e. up to the first '.'. */
    size_t stem = 0;
    while (stem < len && comp[stem] != '.') {
        ++stem;
    }
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        size_t nl = strlen(names[i]);
        if (nl != stem) {
            continue;
        }
        size_t k = 0;
        while (k < nl && ascii_upper((unsigned char)comp[k]) == names[i][k]) {
            ++k;
        }
        if (k == nl) {
            return 1;
        }
    }
    return 0;
}

sdk_path_reject sdk_path_component_check(const char *comp, size_t len) {
    if (len == 0) {
        return SDK_PATH_REJ_EMPTY;
    }
    if (len == 1 && comp[0] == '.') {
        return SDK_PATH_REJ_DOT;
    }
    if (len == 2 && comp[0] == '.' && comp[1] == '.') {
        return SDK_PATH_REJ_DOTDOT;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)comp[i];
        if (c == 0) {
            return SDK_PATH_REJ_NUL;
        }
        if (c == '/' || c == '\\') {
            return SDK_PATH_REJ_SEPARATOR;
        }
        if (c == ':') {
            return SDK_PATH_REJ_COLON;
        }
    }
    if (comp[len - 1] == ' ' || comp[len - 1] == '.') {
        return SDK_PATH_REJ_TRAILING_SPACE_DOT;
    }
    if (is_reserved_device(comp, len)) {
        return SDK_PATH_REJ_RESERVED_DEVICE;
    }
    return SDK_PATH_OK;
}

sdk_path_reject sdk_path_relative_check(const char *path, size_t len,
                                        unsigned max_depth) {
    if (len == 0) {
        return SDK_PATH_REJ_EMPTY;
    }
    if (len > SDK_LIMIT_VCS_PATH_BYTES) {
        return SDK_PATH_REJ_TOO_LONG;
    }
    if (path[0] == '/' || path[0] == '\\') {
        return SDK_PATH_REJ_EMPTY; /* absolute paths are not root relative */
    }
    unsigned depth = 0;
    size_t start = 0;
    for (size_t i = 0; i <= len; ++i) {
        if (i == len || path[i] == '/') {
            sdk_path_reject r = sdk_path_component_check(path + start, i - start);
            if (r != SDK_PATH_OK) {
                return r;
            }
            ++depth;
            if (max_depth && depth > max_depth) {
                return SDK_PATH_REJ_DEPTH;
            }
            start = i + 1;
        } else if (path[i] == '\\') {
            return SDK_PATH_REJ_SEPARATOR;
        }
    }
    return SDK_PATH_OK;
}

/* ------------------------------------------------------------------ */
/* Ignore pattern matching                                             */
/* ------------------------------------------------------------------ */

static int icase_byte_eq(unsigned char a, unsigned char b) {
    /* Pattern matching operates on canonical UTF-8. Case folding is applied
     * to the ASCII range only, which matches Windows ordinal ignore-case for
     * the ASCII subset and leaves multi-byte sequences compared exactly. */
    return ascii_upper(a) == ascii_upper(b);
}

/* Glob matcher supporting '*' (no '/'), '**' (any, including '/') and a
 * literal '?'. Backtracking is bounded by the pattern and path length. */
static int glob_match(const char *pat, size_t pl, const char *str, size_t sl) {
    size_t pi = 0, si = 0;
    /* Single-star backtrack state. */
    size_t star_pi = (size_t)-1, star_si = 0;
    /* Double-star backtrack state. */
    size_t dstar_pi = (size_t)-1, dstar_si = 0;

    for (;;) {
        if (pi < pl && pat[pi] == '*') {
            if (pi + 1 < pl && pat[pi + 1] == '*') {
                pi += 2;
                /* A double star followed by a separator also matches zero
                 * directories, which the backtrack state below provides. */
                dstar_pi = pi;
                dstar_si = si;
                star_pi = (size_t)-1;
                continue;
            }
            ++pi;
            star_pi = pi;
            star_si = si;
            continue;
        }
        if (pi < pl && si < sl && icase_byte_eq((unsigned char)pat[pi],
                                                (unsigned char)str[si])) {
            ++pi;
            ++si;
            continue;
        }
        if (pi == pl && si == sl) {
            return 1;
        }
        /* Try to extend the most recent single star (cannot cross '/'). */
        if (star_pi != (size_t)-1 && star_si < sl && str[star_si] != '/') {
            si = ++star_si;
            pi = star_pi;
            continue;
        }
        /* Then extend the most recent double star (may cross '/'). */
        if (dstar_pi != (size_t)-1 && dstar_si < sl) {
            si = ++dstar_si;
            pi = dstar_pi;
            star_pi = (size_t)-1;
            continue;
        }
        return 0;
    }
}

int sdk_ignore_match(const char *pattern, const char *path, int is_dir) {
    size_t pl = strlen(pattern);
    size_t sl = strlen(path);
    if (pl == 0) {
        return 0;
    }
    /* A leading '/' anchors at the root; matching is root relative anyway. */
    if (pattern[0] == '/') {
        ++pattern;
        --pl;
        if (pl == 0) {
            return 0;
        }
    }
    if (pattern[pl - 1] == '/') {
        /* Directory pattern: matches the directory itself and everything
         * beneath it. */
        size_t base = pl - 1;
        if (is_dir && glob_match(pattern, base, path, sl)) {
            return 1;
        }
        /* descendants: pattern + "/**" */
        for (size_t i = 0; i < sl; ++i) {
            if (path[i] == '/' && glob_match(pattern, base, path, i)) {
                return 1;
            }
        }
        return 0;
    }
    if (glob_match(pattern, pl, path, sl)) {
        return 1;
    }
    /* A plain pattern that names a directory also excludes its descendants:
     * "build" excludes "build/x". This mirrors the default-exclude list of
     * docs/03 section 4 where entries are written as directory names. */
    for (size_t i = 0; i < sl; ++i) {
        if (path[i] == '/' && glob_match(pattern, pl, path, i)) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Failure injection                                                   */
/* ------------------------------------------------------------------ */

static int g_fault[SDK_FAULT_COUNT];

void sdk_fault_reset(void) {
    for (int i = 0; i < SDK_FAULT_COUNT; ++i) {
        g_fault[i] = 0;
    }
}

void sdk_fault_set(sdk_fault f, int enable) {
    if ((int)f >= 0 && (int)f < SDK_FAULT_COUNT) {
        g_fault[f] = enable ? 1 : 0;
    }
}

int sdk_fault_enabled(sdk_fault f) {
    if ((int)f < 0 || (int)f >= SDK_FAULT_COUNT) {
        return 0;
    }
    return g_fault[f];
}

int sdk_fault_trip(sdk_fault f) {
    return sdk_fault_enabled(f);
}

const char *sdk_fault_name(sdk_fault f) {
    switch (f) {
    case SDK_FAULT_TEMP_CREATE:  return "temp_create";
    case SDK_FAULT_WRITE:        return "write";
    case SDK_FAULT_FLUSH:        return "flush_file_buffers";
    case SDK_FAULT_CLOSE:        return "close_handle";
    case SDK_FAULT_MOVEFILE:     return "move_file_ex";
    case SDK_FAULT_REPLACEFILE:  return "replace_file";
    case SDK_FAULT_BACKUP:       return "backup";
    case SDK_FAULT_ROLLBACK:     return "rollback";
    case SDK_FAULT_ALLOC:        return "allocation";
    case SDK_FAULT_READ:         return "read";
    case SDK_FAULT_SETATTR:      return "set_attributes";
    case SDK_FAULT_COUNT:        break;
    }
    return "unknown";
}

/* ------------------------------------------------------------------ */
/* Long path helper                                                    */
/* ------------------------------------------------------------------ */

const wchar_t *sdk_strip_longpath_prefix(const wchar_t *path) {
    if (path && path[0] == L'\\' && path[1] == L'\\' && path[2] == L'?' &&
        path[3] == L'\\') {
        if (wcsncmp(path + 4, L"UNC\\", 4) == 0) {
            return path + 6; /* leave "\\server\share" shape */
        }
        return path + 4;
    }
    return path;
}

wchar_t *sdk_full_path_w(const wchar_t *path) {
    DWORD n = GetFullPathNameW(path, 0, NULL, NULL);
    if (n == 0) {
        return NULL;
    }
    wchar_t *tmp = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!tmp) {
        return NULL;
    }
    DWORD got = GetFullPathNameW(path, n, tmp, NULL);
    if (got == 0 || got >= n) {
        free(tmp);
        return NULL;
    }
    /* Prefix long absolute drive paths so the 260 character limit does not
     * apply. The manifest also declares longPathAware. */
    if (wcslen(tmp) >= MAX_PATH - 12 && tmp[0] != L'\\') {
        size_t l = wcslen(tmp);
        wchar_t *pre = (wchar_t *)malloc((l + 5) * sizeof(wchar_t));
        if (!pre) {
            free(tmp);
            return NULL;
        }
        wcscpy(pre, L"\\\\?\\");
        wcscat(pre, tmp);
        free(tmp);
        return pre;
    }
    return tmp;
}

wchar_t *sdk_getcwd_w(void) {
    DWORD n = GetCurrentDirectoryW(0, NULL);
    if (n == 0) {
        return NULL;
    }
    wchar_t *p = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!p) {
        return NULL;
    }
    DWORD got = GetCurrentDirectoryW(n, p);
    if (got == 0 || got >= n) {
        free(p);
        return NULL;
    }
    return p;
}

wchar_t *sdk_module_path_w(void) {
    size_t cap = 512;
    for (;;) {
        wchar_t *p = (wchar_t *)malloc(cap * sizeof(wchar_t));
        if (!p) {
            return NULL;
        }
        DWORD n = GetModuleFileNameW(NULL, p, (DWORD)cap);
        if (n == 0) {
            free(p);
            return NULL;
        }
        if ((size_t)n < cap) {
            return p;
        }
        /* Buffer too small: GetModuleFileNameW truncates and, on modern
         * Windows, sets ERROR_INSUFFICIENT_BUFFER. Grow and retry until the
         * canonical Win32 path limit is exceeded. */
        free(p);
        if (cap >= SDK_LIMIT_WIN32_PATH_UNITS * 4u) {
            return NULL;
        }
        cap *= 2u;
    }
}

wchar_t *sdk_module_dir_w(void) {
    wchar_t *full = sdk_module_path_w();
    if (!full) {
        return NULL;
    }
    size_t n = wcslen(full);
    while (n > 0 && full[n - 1] != L'\\' && full[n - 1] != L'/') {
        --n;
    }
    if (n <= 1) {
        free(full);
        return NULL;
    }
    /* Drop the trailing separator unless it is the root of a drive. */
    if (n >= 2 && full[n - 2] == L':') {
        full[n] = L'\0';
    } else {
        full[n - 1] = L'\0';
    }
    return full;
}

wchar_t *sdk_final_directory_path_w(const wchar_t *path) {
    HANDLE h = CreateFileW(path, 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    DWORD n = GetFinalPathNameByHandleW(h, NULL, 0, FILE_NAME_NORMALIZED);
    if (n == 0) {
        CloseHandle(h);
        return NULL;
    }
    wchar_t *buf = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!buf) {
        CloseHandle(h);
        return NULL;
    }
    DWORD got = GetFinalPathNameByHandleW(h, buf, n, FILE_NAME_NORMALIZED);
    CloseHandle(h);
    if (got == 0 || got >= n) {
        free(buf);
        return NULL;
    }
    return buf;
}

/* ------------------------------------------------------------------ */
/* File metadata                                                       */
/* ------------------------------------------------------------------ */

sdk_status sdk_stat_w(const wchar_t *path, sdk_fileinfo *out) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    memset(out, 0, sizeof *out);
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &d)) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
            out->exists = 0;
            return SDK_ERR_NOT_FOUND;
        }
        return SDK_ERR_IO;
    }
    out->exists = 1;
    out->size = ((uint64_t)d.nFileSizeHigh << 32) | (uint64_t)d.nFileSizeLow;
    ULARGE_INTEGER t;
    t.LowPart = d.ftLastWriteTime.dwLowDateTime;
    t.HighPart = d.ftLastWriteTime.dwHighDateTime;
    out->mtime_100ns = (int64_t)t.QuadPart;
    out->is_directory = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
    out->is_reparse_point =
        (d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0;
    out->is_readonly = (d.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 1 : 0;
    return SDK_OK;
}

sdk_status sdk_file_read_all_w(const wchar_t *path, size_t max_bytes,
                               uint8_t **out_data, size_t *out_len,
                               uint32_t *out_win32_error) {
    *out_data = NULL;
    *out_len = 0;
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    HANDLE h = CreateFileW(path, GENERIC_READ,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (out_win32_error) {
            *out_win32_error = (uint32_t)e;
        }
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
            return SDK_ERR_NOT_FOUND;
        }
        return SDK_ERR_IO;
    }
    /* Only regular disk files are read. */
    if (GetFileType(h) != FILE_TYPE_DISK) {
        CloseHandle(h);
        return SDK_ERR_DATA;
    }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        CloseHandle(h);
        return SDK_ERR_IO;
    }
    if (sz.QuadPart < 0 || (uint64_t)sz.QuadPart > (uint64_t)max_bytes) {
        CloseHandle(h);
        return SDK_ERR_LIMIT;
    }
    size_t len = (size_t)sz.QuadPart;
    uint8_t *buf = (uint8_t *)malloc(len ? len : 1);
    if (!buf || sdk_fault_trip(SDK_FAULT_ALLOC)) {
        free(buf);
        CloseHandle(h);
        return SDK_ERR_NOMEM;
    }
    size_t got = 0;
    while (got < len) {
        DWORD chunk = (DWORD)((len - got > 0x00100000u) ? 0x00100000u : (len - got));
        DWORD rd = 0;
        if (sdk_fault_trip(SDK_FAULT_READ) ||
            !ReadFile(h, buf + got, chunk, &rd, NULL)) {
            if (out_win32_error) {
                *out_win32_error = (uint32_t)GetLastError();
            }
            free(buf);
            CloseHandle(h);
            return SDK_ERR_IO;
        }
        if (rd == 0) {
            break;
        }
        got += rd;
    }
    CloseHandle(h);
    if (got != len) {
        free(buf);
        return SDK_ERR_IO;
    }
    *out_data = buf;
    *out_len = len;
    return SDK_OK;
}

static sdk_status write_handle_all(HANDLE h, const void *data, size_t len,
                                   uint32_t *werr) {
    const uint8_t *p = (const uint8_t *)data;
    size_t done = 0;
    while (done < len) {
        DWORD chunk = (DWORD)((len - done > 0x00100000u) ? 0x00100000u : (len - done));
        DWORD wr = 0;
        if (sdk_fault_trip(SDK_FAULT_WRITE) ||
            !WriteFile(h, p + done, chunk, &wr, NULL) || wr != chunk) {
            if (werr) {
                *werr = (uint32_t)GetLastError();
            }
            return SDK_ERR_IO;
        }
        done += wr;
    }
    return SDK_OK;
}

sdk_status sdk_file_write_all_w(const wchar_t *path, const void *data, size_t len,
                                uint32_t *out_win32_error) {
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        return SDK_ERR_IO;
    }
    sdk_status st = write_handle_all(h, data, len, out_win32_error);
    if (st == SDK_OK) {
        if (!FlushFileBuffers(h)) {
            if (out_win32_error) {
                *out_win32_error = (uint32_t)GetLastError();
            }
            st = SDK_ERR_IO;
        }
    }
    if (!CloseHandle(h) && st == SDK_OK) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        st = SDK_ERR_IO;
    }
    return st;
}

/* Builds a create-new temporary sibling path: "<target>.tmpNNNN". */
static wchar_t *make_temp_sibling(const wchar_t *target, unsigned attempt) {
    size_t tl = wcslen(target);
    wchar_t suffix[32];
    swprintf(suffix, 32, L".tmp%04u", attempt & 0xFFFFu);
    size_t sl = wcslen(suffix);
    wchar_t *p = (wchar_t *)malloc((tl + sl + 1) * sizeof(wchar_t));
    if (!p) {
        return NULL;
    }
    memcpy(p, target, tl * sizeof(wchar_t));
    memcpy(p + tl, suffix, (sl + 1) * sizeof(wchar_t));
    return p;
}

sdk_status sdk_file_write_atomic_w(const wchar_t *target,
                                   const wchar_t *backup_path,
                                   const void *data, size_t len,
                                   const char **stage,
                                   uint32_t *out_win32_error) {
    const char *dummy_stage = NULL;
    if (!stage) {
        stage = &dummy_stage;
    }
    *stage = "start";
    if (out_win32_error) {
        *out_win32_error = 0;
    }

    /* 1. Create a temporary sibling with create-new semantics. */
    HANDLE h = INVALID_HANDLE_VALUE;
    wchar_t *tmp = NULL;
    for (unsigned attempt = 0; attempt < 64u; ++attempt) {
        free(tmp);
        tmp = make_temp_sibling(target, attempt);
        if (!tmp) {
            *stage = "temp_path_alloc";
            return SDK_ERR_NOMEM;
        }
        if (sdk_fault_trip(SDK_FAULT_TEMP_CREATE)) {
            *stage = "temp_create";
            if (out_win32_error) {
                *out_win32_error = ERROR_ACCESS_DENIED;
            }
            free(tmp);
            return SDK_ERR_IO;
        }
        h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            break;
        }
        DWORD e = GetLastError();
        if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS) {
            *stage = "temp_create";
            if (out_win32_error) {
                *out_win32_error = (uint32_t)e;
            }
            free(tmp);
            return SDK_ERR_IO;
        }
    }
    if (h == INVALID_HANDLE_VALUE) {
        *stage = "temp_create";
        free(tmp);
        return SDK_ERR_IO;
    }

    /* 2. Write every byte. */
    sdk_status st = write_handle_all(h, data, len, out_win32_error);
    if (st != SDK_OK) {
        *stage = "write";
        CloseHandle(h);
        DeleteFileW(tmp);
        free(tmp);
        return st;
    }

    /* 3. Flush. */
    if (sdk_fault_trip(SDK_FAULT_FLUSH) || !FlushFileBuffers(h)) {
        *stage = "flush_file_buffers";
        if (out_win32_error) {
            *out_win32_error = sdk_fault_enabled(SDK_FAULT_FLUSH)
                                   ? ERROR_WRITE_FAULT
                                   : (uint32_t)GetLastError();
        }
        CloseHandle(h);
        DeleteFileW(tmp);
        free(tmp);
        return SDK_ERR_IO;
    }

    /* 4. Close. */
    if (sdk_fault_trip(SDK_FAULT_CLOSE) || !CloseHandle(h)) {
        *stage = "close_handle";
        if (out_win32_error) {
            *out_win32_error = sdk_fault_enabled(SDK_FAULT_CLOSE)
                                   ? ERROR_INVALID_HANDLE
                                   : (uint32_t)GetLastError();
        }
        DeleteFileW(tmp);
        free(tmp);
        return SDK_ERR_IO;
    }

    /* 5. Publish. */
    sdk_fileinfo ti;
    int target_exists = (sdk_stat_w(target, &ti) == SDK_OK && ti.exists);

    if (!target_exists) {
        if (sdk_fault_trip(SDK_FAULT_MOVEFILE) ||
            !MoveFileExW(tmp, target, MOVEFILE_WRITE_THROUGH)) {
            *stage = "move_file_ex";
            if (out_win32_error) {
                *out_win32_error = sdk_fault_enabled(SDK_FAULT_MOVEFILE)
                                       ? ERROR_ACCESS_DENIED
                                       : (uint32_t)GetLastError();
            }
            DeleteFileW(tmp);
            free(tmp);
            return SDK_ERR_IO;
        }
        free(tmp);
        *stage = "done";
        return SDK_OK;
    }

    if (sdk_fault_trip(SDK_FAULT_REPLACEFILE)) {
        *stage = "replace_file";
        if (out_win32_error) {
            *out_win32_error = ERROR_UNABLE_TO_MOVE_REPLACEMENT;
        }
        DeleteFileW(tmp);
        free(tmp);
        return SDK_ERR_IO;
    }
    if (backup_path && sdk_fault_trip(SDK_FAULT_BACKUP)) {
        *stage = "backup";
        if (out_win32_error) {
            *out_win32_error = ERROR_UNABLE_TO_REMOVE_REPLACED;
        }
        DeleteFileW(tmp);
        free(tmp);
        return SDK_ERR_IO;
    }
    if (!ReplaceFileW(target, tmp, backup_path, REPLACEFILE_WRITE_THROUGH,
                      NULL, NULL)) {
        *stage = "replace_file";
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        DeleteFileW(tmp);
        free(tmp);
        return SDK_ERR_IO;
    }
    free(tmp);
    *stage = "done";
    return SDK_OK;
}

sdk_status sdk_mkdir_w(const wchar_t *path, uint32_t *out_win32_error) {
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    if (CreateDirectoryW(path, NULL)) {
        return SDK_OK;
    }
    DWORD e = GetLastError();
    if (out_win32_error) {
        *out_win32_error = (uint32_t)e;
    }
    if (e == ERROR_ALREADY_EXISTS) {
        return SDK_ERR_EXISTS;
    }
    return SDK_ERR_IO;
}

sdk_status sdk_mkdir_parents_w(const wchar_t *path, uint32_t *out_win32_error) {
    size_t n = wcslen(path);
    wchar_t *copy = sdk_wcsdup_n(path, n);
    if (!copy) {
        return SDK_ERR_NOMEM;
    }
    /* Skip any \\?\ prefix and the drive/root portion. */
    size_t i = 0;
    if (n >= 4 && wcsncmp(copy, L"\\\\?\\", 4) == 0) {
        i = 4;
    }
    /* Drive letter root. */
    if (n >= i + 2 && copy[i + 1] == L':') {
        i += 2;
        if (i < n && (copy[i] == L'\\' || copy[i] == L'/')) {
            ++i;
        }
    }
    sdk_status st = SDK_OK;
    for (; i <= n; ++i) {
        if (i == n || copy[i] == L'\\' || copy[i] == L'/') {
            if (i == 0) {
                continue;
            }
            wchar_t saved = copy[i];
            copy[i] = 0;
            sdk_fileinfo fi;
            if (sdk_stat_w(copy, &fi) != SDK_OK || !fi.exists) {
                if (!CreateDirectoryW(copy, NULL)) {
                    DWORD e = GetLastError();
                    if (e != ERROR_ALREADY_EXISTS) {
                        if (out_win32_error) {
                            *out_win32_error = (uint32_t)e;
                        }
                        st = SDK_ERR_IO;
                        copy[i] = saved;
                        break;
                    }
                }
            }
            copy[i] = saved;
            if (i == n) {
                break;
            }
        }
    }
    free(copy);
    return st;
}

sdk_status sdk_delete_file_w(const wchar_t *path, uint32_t *out_win32_error) {
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    sdk_fileinfo fi;
    if (sdk_stat_w(path, &fi) == SDK_OK && fi.exists && fi.is_readonly) {
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    }
    if (DeleteFileW(path)) {
        return SDK_OK;
    }
    DWORD e = GetLastError();
    if (out_win32_error) {
        *out_win32_error = (uint32_t)e;
    }
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
        return SDK_ERR_NOT_FOUND;
    }
    return SDK_ERR_IO;
}

sdk_status sdk_rmdir_w(const wchar_t *path, uint32_t *out_win32_error) {
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    if (RemoveDirectoryW(path)) {
        return SDK_OK;
    }
    DWORD e = GetLastError();
    if (out_win32_error) {
        *out_win32_error = (uint32_t)e;
    }
    if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
        return SDK_ERR_NOT_FOUND;
    }
    return SDK_ERR_IO;
}

sdk_status sdk_set_readonly_w(const wchar_t *path, int readonly,
                              uint32_t *out_win32_error) {
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    if (sdk_fault_trip(SDK_FAULT_SETATTR)) {
        if (out_win32_error) {
            *out_win32_error = ERROR_ACCESS_DENIED;
        }
        return SDK_ERR_IO;
    }
    DWORD a = GetFileAttributesW(path);
    if (a == INVALID_FILE_ATTRIBUTES) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        return SDK_ERR_IO;
    }
    DWORD n = readonly ? (a | FILE_ATTRIBUTE_READONLY)
                       : (a & ~(DWORD)FILE_ATTRIBUTE_READONLY);
    if (n == a) {
        return SDK_OK;
    }
    if (!SetFileAttributesW(path, n)) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)GetLastError();
        }
        return SDK_ERR_IO;
    }
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Directory enumeration                                               */
/* ------------------------------------------------------------------ */

static int dirent_cmp(const void *a, const void *b) {
    const sdk_dirent *x = (const sdk_dirent *)a;
    const sdk_dirent *y = (const sdk_dirent *)b;
    int r = sdk_ord_cmp_w(x->name_w, y->name_w, 1);
    if (r != 0) {
        return r;
    }
    return sdk_ord_cmp_w(x->name_w, y->name_w, 0);
}

void sdk_dirlist_free(sdk_dirlist *l) {
    if (!l) {
        return;
    }
    for (size_t i = 0; i < l->count; ++i) {
        free(l->items[i].name_w);
        free(l->items[i].name_u8);
    }
    free(l->items);
    l->items = NULL;
    l->count = 0;
}

sdk_status sdk_dir_list_w(const wchar_t *path, sdk_dirlist *out,
                          size_t *out_bad_name_count,
                          uint32_t *out_win32_error) {
    out->items = NULL;
    out->count = 0;
    if (out_bad_name_count) {
        *out_bad_name_count = 0;
    }
    if (out_win32_error) {
        *out_win32_error = 0;
    }

    wchar_t *pattern = sdk_wpath_join(path, L"*");
    if (!pattern) {
        return SDK_ERR_NOMEM;
    }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(pattern, FindExInfoBasic, &fd,
                               FindExSearchNameMatch, NULL, 0);
    free(pattern);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (out_win32_error) {
            *out_win32_error = (uint32_t)e;
        }
        if (e == ERROR_FILE_NOT_FOUND) {
            return SDK_OK; /* empty directory */
        }
        if (e == ERROR_PATH_NOT_FOUND) {
            return SDK_ERR_NOT_FOUND;
        }
        return SDK_ERR_IO;
    }

    size_t cap = 0;
    sdk_status st = SDK_OK;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }
        if (out->count == cap) {
            size_t ncap = cap ? cap * 2 : 32;
            sdk_dirent *ni = (sdk_dirent *)realloc(out->items, ncap * sizeof *ni);
            if (!ni) {
                st = SDK_ERR_NOMEM;
                break;
            }
            out->items = ni;
            cap = ncap;
        }
        sdk_dirent *e = &out->items[out->count];
        memset(e, 0, sizeof *e);
        size_t nl = wcslen(fd.cFileName);
        e->name_w = sdk_wcsdup_n(fd.cFileName, nl);
        if (!e->name_w) {
            st = SDK_ERR_NOMEM;
            break;
        }
        e->name_u8 = sdk_utf16_to_utf8(fd.cFileName, nl, NULL);
        if (!e->name_u8) {
            if (out_bad_name_count) {
                (*out_bad_name_count)++;
            }
            free(e->name_w);
            continue; /* unconvertible name is skipped and counted */
        }
        e->info.exists = 1;
        e->info.size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;
        ULARGE_INTEGER t;
        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        e->info.mtime_100ns = (int64_t)t.QuadPart;
        e->info.is_directory =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        e->info.is_reparse_point =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0;
        e->info.is_readonly =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? 1 : 0;
        out->count++;
    } while (FindNextFileW(h, &fd));

    DWORD last = GetLastError();
    FindClose(h);
    if (st != SDK_OK) {
        sdk_dirlist_free(out);
        return st;
    }
    if (last != ERROR_NO_MORE_FILES && last != ERROR_SUCCESS) {
        if (out_win32_error) {
            *out_win32_error = (uint32_t)last;
        }
        sdk_dirlist_free(out);
        return SDK_ERR_IO;
    }
    if (out->count > 1) {
        qsort(out->items, out->count, sizeof out->items[0], dirent_cmp);
    }
    return SDK_OK;
}

sdk_status sdk_remove_tree_w(const wchar_t *path) {
    sdk_fileinfo fi;
    if (sdk_stat_w(path, &fi) != SDK_OK || !fi.exists) {
        return SDK_OK;
    }
    if (!fi.is_directory || fi.is_reparse_point) {
        return sdk_delete_file_w(path, NULL);
    }
    sdk_dirlist l;
    if (sdk_dir_list_w(path, &l, NULL, NULL) == SDK_OK) {
        for (size_t i = 0; i < l.count; ++i) {
            wchar_t *child = sdk_wpath_join(path, l.items[i].name_w);
            if (child) {
                sdk_remove_tree_w(child);
                free(child);
            }
        }
        sdk_dirlist_free(&l);
    }
    return sdk_rmdir_w(path, NULL);
}

/* ------------------------------------------------------------------ */
/* Locking                                                             */
/* ------------------------------------------------------------------ */

sdk_status sdk_lock_acquire_w(const wchar_t *path, const char *operation,
                              sdk_lock *out, uint32_t *out_win32_error) {
    out->path = NULL;
    out->handle = NULL;
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (out_win32_error) {
            *out_win32_error = (uint32_t)e;
        }
        if (e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS ||
            e == ERROR_SHARING_VIOLATION) {
            return SDK_ERR_BUSY;
        }
        return SDK_ERR_IO;
    }
    /* Lock content records pid, UTC time and the operation. Never a secret. */
    SYSTEMTIME st;
    GetSystemTime(&st);
    char text[256];
    int n = snprintf(text, sizeof text,
                     "pid=%lu utc=%04u-%02u-%02uT%02u:%02u:%02u.%03uZ op=%s\n",
                     (unsigned long)GetCurrentProcessId(),
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                     st.wSecond, st.wMilliseconds,
                     operation ? operation : "unspecified");
    if (n > 0) {
        DWORD wr = 0;
        WriteFile(h, text, (DWORD)n, &wr, NULL);
        FlushFileBuffers(h);
    }
    out->handle = (void *)h;
    out->path = sdk_wcsdup_n(path, wcslen(path));
    if (!out->path) {
        CloseHandle(h);
        DeleteFileW(path);
        out->handle = NULL;
        return SDK_ERR_NOMEM;
    }
    return SDK_OK;
}

sdk_status sdk_lock_release(sdk_lock *l, uint32_t *out_win32_error) {
    sdk_status st = SDK_OK;
    if (out_win32_error) {
        *out_win32_error = 0;
    }
    if (l->handle) {
        if (!CloseHandle((HANDLE)l->handle)) {
            if (out_win32_error) {
                *out_win32_error = (uint32_t)GetLastError();
            }
            st = SDK_ERR_IO;
        }
        l->handle = NULL;
    }
    if (l->path) {
        if (!DeleteFileW(l->path)) {
            DWORD e = GetLastError();
            if (e != ERROR_FILE_NOT_FOUND) {
                if (out_win32_error) {
                    *out_win32_error = (uint32_t)e;
                }
                st = SDK_ERR_IO;
            }
        }
        free(l->path);
        l->path = NULL;
    }
    return st;
}

/* ------------------------------------------------------------------ */
/* Clocks, RNG, environment                                            */
/* ------------------------------------------------------------------ */

int64_t sdk_now_epoch_ms(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    /* FILETIME epoch 1601-01-01 -> Unix epoch offset in 100ns ticks. */
    const uint64_t EPOCH_DIFF_100NS = 116444736000000000ull;
    if (t.QuadPart < EPOCH_DIFF_100NS) {
        return 0;
    }
    return (int64_t)((t.QuadPart - EPOCH_DIFF_100NS) / 10000ull);
}

static LARGE_INTEGER g_qpc_freq;
static int g_qpc_ready = 0;

static void qpc_init(void) {
    if (!g_qpc_ready) {
        if (!QueryPerformanceFrequency(&g_qpc_freq) || g_qpc_freq.QuadPart <= 0) {
            g_qpc_freq.QuadPart = 1;
        }
        g_qpc_ready = 1;
    }
}

uint64_t sdk_monotonic_us(void) {
    qpc_init();
    LARGE_INTEGER c;
    if (!QueryPerformanceCounter(&c)) {
        return 0;
    }
    /* Split to avoid overflow on long uptimes. */
    uint64_t q = (uint64_t)c.QuadPart / (uint64_t)g_qpc_freq.QuadPart;
    uint64_t r = (uint64_t)c.QuadPart % (uint64_t)g_qpc_freq.QuadPart;
    return q * 1000000ull + (r * 1000000ull) / (uint64_t)g_qpc_freq.QuadPart;
}

uint64_t sdk_monotonic_ms(void) {
    return sdk_monotonic_us() / 1000ull;
}

sdk_status sdk_random_bytes(void *out, size_t len) {
    if (len == 0) {
        return SDK_OK;
    }
    /* The only permitted OS crypto call (docs/26 section 16). */
    NTSTATUS s = BCryptGenRandom(NULL, (PUCHAR)out, (ULONG)len,
                                 BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (s != 0) {
        return SDK_ERR_IO;
    }
    return SDK_OK;
}

char *sdk_getenv_utf8(const wchar_t *name) {
    DWORD n = GetEnvironmentVariableW(name, NULL, 0);
    if (n == 0) {
        return NULL;
    }
    wchar_t *buf = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!buf) {
        return NULL;
    }
    DWORD got = GetEnvironmentVariableW(name, buf, n);
    if (got == 0 || got >= n) {
        free(buf);
        return NULL;
    }
    char *u8 = sdk_utf16_to_utf8(buf, got, NULL);
    free(buf);
    return u8;
}
