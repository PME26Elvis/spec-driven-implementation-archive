/* sdk_win.h - Unicode Win32 filesystem and path layer.
 *
 * Normative sources:
 *   docs/02 sections 6 and 9   (Unicode *W APIs only, traversal safety)
 *   docs/19 sections 5, 7, 29  (ignore semantics, path model, error mapping)
 *   docs/26 sections 11-15     (path normalisation, case identity, reparse
 *                               points, read-only metadata, atomic replace,
 *                               locking)
 *
 * Every external path is UTF-16; every canonical/report/object path is strict
 * UTF-8 with '/' separators.
 */
#ifndef SDK_WIN_H
#define SDK_WIN_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

/* ------------------------------------------------------------------ */
/* Strict UTF-16 <-> UTF-8 conversion                                  */
/* ------------------------------------------------------------------ */

/* Converts UTF-16 to UTF-8. Rejects unpaired surrogates.
 * Returns a malloc'ed NUL terminated buffer, or NULL on failure. */
char    *sdk_utf16_to_utf8(const wchar_t *w, size_t wlen, size_t *out_len);

/* Converts UTF-8 to UTF-16. Rejects overlong forms, surrogate code points,
 * truncated sequences and values above U+10FFFF. */
wchar_t *sdk_utf8_to_utf16(const char *s, size_t slen, size_t *out_len);

/* Validates that `s`/`len` is well formed UTF-8 (same rules as above). */
int      sdk_utf8_validate(const char *s, size_t len);

/* ------------------------------------------------------------------ */
/* Wide string helpers                                                 */
/* ------------------------------------------------------------------ */

wchar_t *sdk_wcsdup_n(const wchar_t *w, size_t len);
wchar_t *sdk_wpath_join(const wchar_t *base, const wchar_t *leaf);
/* Windows ordinal comparison. `ignore_case` uses CompareStringOrdinal with
 * bIgnoreCase=TRUE, i.e. never the user locale (docs/26 section 11.3). */
int      sdk_ord_cmp_w(const wchar_t *a, const wchar_t *b, int ignore_case);
/* Same ordering for canonical UTF-8 paths; converts internally. */
int      sdk_ord_cmp_utf8(const char *a, const char *b, int ignore_case);
/* Canonical ordering used for tree/index/report entries: ignore-case first,
 * exact ordinal as tie-breaker (docs/19 sections 4.2 and 7). */
int      sdk_canon_path_cmp(const char *a, const char *b);

/* ------------------------------------------------------------------ */
/* Path component validation (docs/26 section 11.2)                    */
/* ------------------------------------------------------------------ */

typedef enum sdk_path_reject {
    SDK_PATH_OK = 0,
    SDK_PATH_REJ_EMPTY,
    SDK_PATH_REJ_DOT,
    SDK_PATH_REJ_DOTDOT,
    SDK_PATH_REJ_NUL,
    SDK_PATH_REJ_COLON,
    SDK_PATH_REJ_TRAILING_SPACE_DOT,
    SDK_PATH_REJ_RESERVED_DEVICE,
    SDK_PATH_REJ_SEPARATOR,
    SDK_PATH_REJ_TOO_LONG,
    SDK_PATH_REJ_DEPTH
} sdk_path_reject;

const char *sdk_path_reject_name(sdk_path_reject r);

/* Validates a single path component (no '/' allowed). */
sdk_path_reject sdk_path_component_check(const char *comp, size_t len);

/* Validates a whole root-relative canonical UTF-8 path with '/' separators.
 * Rejects absolute paths, "..", empty components and everything the
 * component check rejects. */
sdk_path_reject sdk_path_relative_check(const char *path, size_t len,
                                        unsigned max_depth);

/* ------------------------------------------------------------------ */
/* Ignore pattern matching (docs/19 section 5)                         */
/* ------------------------------------------------------------------ */

/* `pattern` and `path` are canonical UTF-8, '/' separated, root relative.
 * `is_dir` marks the candidate as a directory so that a trailing '/' pattern
 * can match the directory itself as well as its descendants.
 * Matching is Windows ordinal ignore-case. */
int sdk_ignore_match(const char *pattern, const char *path, int is_dir);

/* ------------------------------------------------------------------ */
/* Failure injection (docs/10, docs/22 G7). Off by default.            */
/* ------------------------------------------------------------------ */

typedef enum sdk_fault {
    SDK_FAULT_TEMP_CREATE = 0,
    SDK_FAULT_WRITE,
    SDK_FAULT_FLUSH,
    SDK_FAULT_CLOSE,
    SDK_FAULT_MOVEFILE,
    SDK_FAULT_REPLACEFILE,
    SDK_FAULT_BACKUP,
    SDK_FAULT_ROLLBACK,
    SDK_FAULT_ALLOC,
    SDK_FAULT_READ,
    SDK_FAULT_SETATTR,
    SDK_FAULT_COUNT
} sdk_fault;

void        sdk_fault_reset(void);
void        sdk_fault_set(sdk_fault f, int enable);
int         sdk_fault_enabled(sdk_fault f);
const char *sdk_fault_name(sdk_fault f);
/* Consumes one activation; returns 1 when the caller must simulate failure. */
int         sdk_fault_trip(sdk_fault f);

/* ------------------------------------------------------------------ */
/* File metadata and IO                                                */
/* ------------------------------------------------------------------ */

typedef struct sdk_fileinfo {
    uint64_t size;
    int64_t  mtime_100ns;    /* raw FILETIME as a signed 100ns tick count */
    int      is_directory;
    int      is_reparse_point;
    int      is_readonly;
    int      exists;
} sdk_fileinfo;

sdk_status sdk_stat_w(const wchar_t *path, sdk_fileinfo *out);

/* Reads an entire file. Rejects sizes above `max_bytes`. */
sdk_status sdk_file_read_all_w(const wchar_t *path, size_t max_bytes,
                               uint8_t **out_data, size_t *out_len,
                               uint32_t *out_win32_error);

/* Non-atomic write; used for scratch/report files. */
sdk_status sdk_file_write_all_w(const wchar_t *path, const void *data, size_t len,
                                uint32_t *out_win32_error);

/* Atomic create-or-replace (docs/26 section 14).
 *
 * When `backup_path` is non-NULL and the target already exists, the previous
 * content is preserved there via ReplaceFileW. `stage` receives a stable
 * English stage name when the operation fails. */
sdk_status sdk_file_write_atomic_w(const wchar_t *target,
                                   const wchar_t *backup_path,
                                   const void *data, size_t len,
                                   const char **stage,
                                   uint32_t *out_win32_error);

sdk_status sdk_mkdir_w(const wchar_t *path, uint32_t *out_win32_error);
sdk_status sdk_mkdir_parents_w(const wchar_t *path, uint32_t *out_win32_error);
sdk_status sdk_delete_file_w(const wchar_t *path, uint32_t *out_win32_error);
sdk_status sdk_rmdir_w(const wchar_t *path, uint32_t *out_win32_error);
sdk_status sdk_remove_tree_w(const wchar_t *path);
sdk_status sdk_set_readonly_w(const wchar_t *path, int readonly,
                              uint32_t *out_win32_error);

/* ------------------------------------------------------------------ */
/* Directory enumeration                                               */
/* ------------------------------------------------------------------ */

typedef struct sdk_dirent {
    wchar_t     *name_w;   /* component only */
    char        *name_u8;  /* strict UTF-8 of the same component */
    sdk_fileinfo info;
} sdk_dirent;

typedef struct sdk_dirlist {
    sdk_dirent *items;
    size_t      count;
} sdk_dirlist;

/* Lists a directory, sorted by ordinal ignore-case then exact ordinal.
 * "." and ".." are never returned. Entries whose names are not valid UTF-16
 * -> UTF-8 are reported through `out_bad_name_count`. */
sdk_status sdk_dir_list_w(const wchar_t *path, sdk_dirlist *out,
                          size_t *out_bad_name_count,
                          uint32_t *out_win32_error);
void       sdk_dirlist_free(sdk_dirlist *l);

/* ------------------------------------------------------------------ */
/* Locking (docs/26 section 15)                                        */
/* ------------------------------------------------------------------ */

typedef struct sdk_lock {
    wchar_t *path;
    void    *handle;   /* HANDLE */
} sdk_lock;

sdk_status sdk_lock_acquire_w(const wchar_t *path, const char *operation,
                              sdk_lock *out, uint32_t *out_win32_error);
sdk_status sdk_lock_release(sdk_lock *l, uint32_t *out_win32_error);

/* ------------------------------------------------------------------ */
/* Misc platform services                                             */
/* ------------------------------------------------------------------ */

/* Absolute path of the current working directory (malloc'ed). */
wchar_t *sdk_getcwd_w(void);
/* Absolute path of the running executable image (malloc'ed). */
wchar_t *sdk_module_path_w(void);
/* Directory that contains the running executable image (malloc'ed). */
wchar_t *sdk_module_dir_w(void);
/* Fully qualified path with long-path prefix when required (malloc'ed). */
wchar_t *sdk_full_path_w(const wchar_t *path);
/* Resolves the final path of a directory handle once; used for root reparse
 * resolution (docs/26 section 11.2). Returns malloc'ed path or NULL. */
wchar_t *sdk_final_directory_path_w(const wchar_t *path);
/* Strips a leading \\?\ prefix for report output (docs/19 section 29). */
const wchar_t *sdk_strip_longpath_prefix(const wchar_t *path);

/* Wall clock in Unix epoch milliseconds (UTC). */
int64_t sdk_now_epoch_ms(void);
/* Monotonic high resolution counter in milliseconds. */
uint64_t sdk_monotonic_ms(void);
/* Monotonic high resolution counter in microseconds. */
uint64_t sdk_monotonic_us(void);

/* Cryptographically secure random bytes via BCryptGenRandom with
 * BCRYPT_USE_SYSTEM_PREFERRED_RNG. Never falls back to a weak source. */
sdk_status sdk_random_bytes(void *out, size_t len);

/* Reads an environment variable as UTF-8 (malloc'ed), or NULL when unset. */
char *sdk_getenv_utf8(const wchar_t *name);

#endif /* SDK_WIN_H */
