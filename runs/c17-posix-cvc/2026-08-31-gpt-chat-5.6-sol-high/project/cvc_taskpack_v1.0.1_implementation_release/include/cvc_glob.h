#ifndef CVC_GLOB_H
#define CVC_GLOB_H
#include "cvc_common.h"
int cvc_glob_validate(const char *pat);
int cvc_glob_match(const char *pat, const char *path);
int cvc_patterns_select(const CvcStrVec *include, const CvcStrVec *exclude, const char *path);
int cvc_parse_pattern_csv(const char *arg, CvcStrVec *out);
#endif
