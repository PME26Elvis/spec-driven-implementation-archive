#ifndef CVC_CONFIG_H
#define CVC_CONFIG_H
#include "cvc_common.h"
typedef struct {
    int show_diffstat;
    CvcStrVec tracking_include, tracking_exclude;
    CvcStrVec diffstat_include, diffstat_exclude;
} CvcConfig;
void cvc_config_init_defaults(CvcConfig *c);
void cvc_config_free(CvcConfig *c);
int cvc_config_load(const char *path, CvcConfig *c);
void cvc_config_show(const CvcConfig *c);
#endif
