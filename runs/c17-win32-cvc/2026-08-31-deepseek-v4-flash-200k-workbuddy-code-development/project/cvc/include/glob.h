#ifndef CVC_GLOB_H
#define CVC_GLOB_H

#include <stddef.h>

/* Validate a glob pattern. Returns 0 if valid, -1 if invalid.
 * Rejects runs of 3+ consecutive '*' and empty patterns. */
int glob_validate(const char *pattern);

/* Match repository path (canonical '/'-separated, no leading '/') against
 * a single glob pattern. Returns 1 match, 0 no match.
 * '*' zero+ non-separator; '?' one non-separator; '**' zero+ incl '/'. */
int glob_match(const char *pattern, const char *path);

/* Whether a pattern contains "**" (can match descendants). */
int glob_has_doublestar(const char *pattern);

#endif
