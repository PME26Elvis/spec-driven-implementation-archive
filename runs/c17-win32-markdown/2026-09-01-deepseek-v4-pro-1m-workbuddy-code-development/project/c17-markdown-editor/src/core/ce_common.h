/* ce_common.h - shared types and small helpers for the C17 Markdown editor.
 * This header is pure C17 and has no Windows dependency, so it can be included
 * by both the Workstream A utilities and the GUI application.
 */
#ifndef CE_COMMON_H
#define CE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------- allocator */

void *ce_malloc(size_t n);
void *ce_calloc(size_t n, size_t sz);
void *ce_realloc(void *p, size_t n);
char *ce_strdup(const char *s);
char *ce_strndup(const char *s, size_t n);
void  ce_free(void *p);

/* ---------------------------------------------------------------- small util */

#define CE_ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

int ce_strcasecmp(const char *a, const char *b);   /* ASCII case-insensitive */
int ce_strncasecmp(const char *a, const char *b, size_t n);

/* Return 1 if s starts with prefix (ASCII, case-sensitive). */
int ce_starts_with(const char *s, const char *prefix);
int ce_ends_with(const char *s, const char *suffix);

/* ---------------------------------------------------------------- logging */

/* Diagnostics go to stderr. Keep a tiny level knob used by tools. */
void ce_log_warn(const char *fmt, ...);
void ce_log_error(const char *fmt, ...);

#endif /* CE_COMMON_H */
