/* winutil.h - Windows platform helpers (Unicode paths, traversal, file I/O). */
#ifndef CE_WINUTIL_H
#define CE_WINUTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* UTF-8 <-> UTF-16 conversions. Returned buffers are malloc'd (caller frees). */
wchar_t *wu_u8_to_w(const char *u8);
char *wu_w_to_u8(const wchar_t *w);

/* Convert an absolute UTF-8 path to a native wide path, adding the \\?\ 
 * extended-length prefix when needed (long paths). Caller frees. */
wchar_t *wu_u8_to_native(const char *u8);

/* Normalize a path to use '\\' separators (native). Caller frees. */
char *wu_norm_sep(const char *path);

bool wu_is_absolute(const char *path);

/* Filesystem queries on UTF-8 paths. */
bool wu_exists(const char *path);
bool wu_is_dir(const char *path);
bool wu_is_file(const char *path);
/* 1 if path is a directory reparse point/junction/symlink. */
bool wu_is_reparse_point(const char *path);

/* Read an entire file (UTF-8 path). Returns malloc'd buffer (caller frees) or NULL. */
char *wu_read_file(const char *path, size_t *out_len);

/* Write an entire file (atomic: temp + replace). Returns true on success. */
bool wu_write_file(const char *path, const void *data, size_t len);

/* Recursive directory traversal callback. Return non-zero from cb to stop.
 * dirs_only: only report directories. follow_reparse: descend into reparse points.
 * cb receives a UTF-8 full path and a type flag (0 file, 1 dir). */
typedef int (*wu_walk_cb)(void *ctx, const char *path, int is_dir);
bool wu_walk_dir(const char *root, bool follow_reparse, wu_walk_cb cb, void *ctx);

/* SHA-256 of a file's bytes (returns 0 on failure). */
bool wu_file_sha256(const char *path, uint8_t out[32]);

/* High-resolution monotonic timer (seconds). */
double wu_now_seconds(void);

#endif /* CE_WINUTIL_H */
