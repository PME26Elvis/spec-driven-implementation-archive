#ifndef CVC_DIFF_H
#define CVC_DIFF_H
#include "cvc_common.h"
typedef struct { const unsigned char *p; size_t n; } CvcLine;
typedef struct { CvcLine *v; size_t n; } CvcLines;
typedef struct { char op; size_t ai, bi; } CvcDiffOp;
typedef struct { CvcDiffOp *v; size_t n, cap; } CvcDiffOps;
void cvc_lines_split(const unsigned char *p,size_t n,CvcLines *out);
void cvc_lines_free(CvcLines *l);
int cvc_myers_diff(const CvcLines *a,const CvcLines *b,CvcDiffOps *out);
void cvc_diffops_free(CvcDiffOps *o);
void cvc_diff_counts(const CvcDiffOps *o,uint64_t *ins,uint64_t *del);
int cvc_three_way_text(const unsigned char *base,size_t bn,const unsigned char *ours,size_t on,const unsigned char *theirs,size_t tn,CvcBuf *result,int *conflict);
#endif
