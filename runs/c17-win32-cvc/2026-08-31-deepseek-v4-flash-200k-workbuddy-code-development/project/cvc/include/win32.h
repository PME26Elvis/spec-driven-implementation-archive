#ifndef CVC_WIN32_H
#define CVC_WIN32_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "util.h"

/* A single directory entry discovered by the scanner. */
typedef struct {
    uint16_t *name16;      /* UTF-16 name (unpaired-surrogate safe) */
    size_t name16_len;     /* number of UTF-16 code units (no NUL) */
    char *name8;           /* canonical UTF-8 name, or NULL if unversionable
                              (unpaired surrogate / over-long / bad) */
    int is_dir;            /* FILE_ATTRIBUTE_DIRECTORY set */
    int is_reparse;        /* FILE_ATTRIBUTE_REPARSE_POINT set */
    uint32_t reparse_tag;  /* valid only if is_reparse */
    int is_symlink;        /* reparse tag == IO_REPARSE_TAG_SYMLINK */
    int symlink_is_dir;    /* for symlink: FILE_ATTRIBUTE_DIRECTORY on the link */
} WDirEntry;

/* Callback for directory enumeration. Returns 0 to continue, nonzero to stop.
 * `depth` is 0 at the enumerated directory's immediate children. */
typedef int (*wdir_cb)(const uint16_t *abs_dir_path, const WDirEntry *e, void *ctx);

/* Enumerate directory at abs_dir_path (extended-length wide path).
 * The absolute path of each child is NOT precomputed; the callback receives
 * the parent path and the entry. Returns 0 on success, -1 on failure. */
int wdir_list(const uint16_t *abs_dir_path, wdir_cb cb, void *ctx);

/* --- Symlink readback --------------------------------------------------- */
/* Read a Windows symbolic link without target traversal. On success returns
 * 0 and sets *printname8 (heap UTF-8) and *is_dir (link kind). On non-symlink
 * or error returns -1. A link with empty/invalid PrintName -> -1. */
int w_symlink_read(const uint16_t *abs_path, char **printname8, int *is_dir);

/* Create a symbolic link. target_utf8 is the PrintName. is_dir sets the
 * directory-link kind. Returns 0 on success, -1 on failure. */
int w_symlink_create(const uint16_t *abs_link_path, const char *target_utf8, int is_dir);

/* --- Metadata / classification ----------------------------------------- */
typedef struct {
    int exists;
    int is_dir;
    int is_reparse;
    uint32_t reparse_tag;
    int is_symlink;
    int symlink_is_dir;
    uint64_t size;         /* file size (regular file) */
} WStat;

/* Stat without opening (GetFileAttributesExW / FindFirst). Does not follow
 * reparse points for classification (reports the reparse point itself).
 * Returns 0 and fills *st, or -1 if not present. */
int w_stat(const uint16_t *abs_path, WStat *st);

/* --- Path helpers ------------------------------------------------------- */
/* Convert a canonical repo path + repo root into an extended-length
 * absolute wide path. repo_root16 is the native root (no trailing slash).
 * Returns heap wide string or NULL. */
uint16_t *w_repo_to_abs(const uint16_t *repo_root16, const char *repo_rel_path);

/* Absolute wide path of the current directory. Heap; caller frees. */
uint16_t *w_getcwd16(void);

/* Combine base dir (wide, extended) + one UTF-8 component into a wide path.
 * Returns heap or NULL. */
uint16_t *w_join(const uint16_t *base16, const char *comp8);

/* --- File I/O ------------------------------------------------------------ */
/* Read whole file (binary) using CreateFileW + ReadFile. Returns CVC_OK. */
CvcStatus w_read_file(const uint16_t *abs_path, Bytes *out);

/* Whether file exists (for collision checks). */
int w_exists(const uint16_t *abs_path);

/* Delete a single file or empty directory by pathname (RemoveDirectory for
 * dirs). Returns 0 on success, -1 on failure. */
int w_delete_path(const uint16_t *abs_path, int is_dir);

/* Create a directory (single level). Returns 0 success, -1 if already exists
 * or failed. */
int w_mkdir(const uint16_t *abs_path);

/* --- Materialization primitives (atomic) ------------------------------- */
/* Atomically install a new regular file at dest_path with content `data`.
 * Uses temp file on same volume + FlushFileBuffers + MoveFileExW replace.
 * overwrite must be true (callers preflight collisions). Returns 0/-1. */
int w_write_file_atomic(const uint16_t *dest_abs, const uint8_t *data, size_t len);
int w_write_file_durable(const uint16_t *dest_abs, const uint8_t *data, size_t len);

/* Move a file/dir from src_abs to dst_abs (same volume), replacing existing
 * file/dir. Returns 0 on success, -1 on failure. */
int w_move_replace(const uint16_t *src_abs, const uint16_t *dst_abs);

/* --- Repository locking -------------------------------------------------- */
typedef struct {
    HANDLE handle;
} RepoLock;

/* Open the lock file (create-new handled by init; here open existing without
 * truncation, share RW so competing processes can open). Returns 0 or -1. */
int w_lock_open(const uint16_t *lock_path, RepoLock *lk);
/* Acquire byte-range lock: exclusive=1 for writers, 0 for readers.
 * Returns 1 if acquired, 0 if busy, -1 on error. */
int w_lock_acquire(RepoLock *lk, int exclusive);
void w_lock_release(RepoLock *lk);
void w_lock_close(RepoLock *lk);

/* --- Misc --------------------------------------------------------------- */
/* Volume max component length (TCHARs) for the volume of abs_path. */
uint32_t w_volume_component_limit(const uint16_t *abs_path);

/* Recompute the "actual" spelling of the repository root: returns the real
 * path with the volume. */
uint16_t *w_realpath(const uint16_t *abs_path);

/* Extended-length: convert a normal wide path to a \\?\ extended path.
 * Returns heap or NULL. If already extended, returns copy. */
uint16_t *w_extended(const uint16_t *path);

/* Print UTF-8 bytes to stdout/stderr as-is (binary-safe). */
void w_out_stdout(const char *s, size_t n);
void w_out_stderr(const char *s, size_t n);

/* Get wall-clock unix time. Honors CVC_TEST_TIMESTAMP if present and valid
 * (for commit creation); caller should call w_timestamp_valid() which checks.
 * Returns 0 on failure. */
int64_t w_unix_time(void);

/* Validate CVC_TEST_TIMESTAMP. Returns 1 if absent or matching
 * 0|-?[1-9][0-9]* fitting int64_t; 0 if present but malformed. When present
 * and valid, writes the parsed value to *out (may be NULL). */
int w_timestamp_valid(int64_t *out);

/* Get current unix time as int64. */
int64_t w_wall_clock(void);

/* Resolve an extended \\?\ absolute path to a plain drive path (for
 * diagnostics). Heap. */
uint16_t *w_pretty_path(const uint16_t *abs_ext);

#endif
