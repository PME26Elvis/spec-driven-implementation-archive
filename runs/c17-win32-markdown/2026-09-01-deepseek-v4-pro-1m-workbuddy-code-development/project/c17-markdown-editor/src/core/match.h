/* match.h - deterministic path/pattern matching for Workstream A. */
#ifndef CE_MATCH_H
#define CE_MATCH_H

#include <stdbool.h>
#include <stddef.h>

/* Wildcard match: '*' matches any run (including empty), '?' matches one char,
 * other characters literal. fold_case enables ASCII case-insensitivity. */
bool ce_fnmatch(const char *pattern, const char *text, bool fold_case);

/* Match a '/'-normalized relative path against a pattern. If pattern ends with
 * '/' it matches the directory and everything beneath it (subtree). */
bool ce_path_match(const char *pattern, const char *path, bool fold_case);

/* 1 if path matches any of the N patterns. */
bool ce_path_match_any(const char **patterns, size_t n, const char *path, bool fold_case);

#endif /* CE_MATCH_H */
