#include "cvc_glob.h"
#include <string.h>
#include <stdlib.h>
int cvc_glob_validate(const char*p){if(!p||!*p||!cvc_utf8_valid((const unsigned char*)p,strlen(p)))return 0;int run=0;for(const unsigned char*s=(const unsigned char*)p;*s;s++){if(*s=='*'){if(++run>=3)return 0;}else run=0;}return 1;}
/* Byte-oriented matcher with double-star slash zero-directory semantics. */
static int m(const unsigned char*p,const unsigned char*s){while(*p){if(p[0]=='*'&&p[1]=='*'){p+=2;if(*p=='/'){p++;if(m(p,s))return 1;for(const unsigned char*q=s;*q;q++)if(*q=='/'&&m(p,q+1))return 1;return 0;}if(m(p,s))return 1;for(const unsigned char*q=s;*q;q++)if(m(p,q+1))return 1;return 0;}if(*p=='*'){p++;if(m(p,s))return 1;for(const unsigned char*q=s;*q&&*q!='/';q++)if(m(p,q+1))return 1;return 0;}if(*p=='?'){if(!*s||*s=='/')return 0;p++;s++;continue;}if(*p!=*s)return 0;p++;s++;}return *s==0;}
int cvc_glob_match(const char*p,const char*s){return cvc_glob_validate(p)&&m((const unsigned char*)p,(const unsigned char*)s);}
int cvc_patterns_select(const CvcStrVec*in,const CvcStrVec*ex,const char*path){int yes=0;for(size_t i=0;i<in->n;i++)if(cvc_glob_match(in->v[i],path)){yes=1;break;}if(!yes)return 0;for(size_t i=0;i<ex->n;i++)if(cvc_glob_match(ex->v[i],path))return 0;return 1;}
int cvc_parse_pattern_csv(const char*arg,CvcStrVec*out){cvc_strvec_init(out);if(!arg||!*arg)return cvc_errorf("empty pattern list");const char*st=arg;for(const char*p=arg;;p++){if(*p==','||*p==0){size_t n=(size_t)(p-st);if(!n){cvc_strvec_free(out);return cvc_errorf("empty pattern in comma list");}char*t=cvc_xstrndup(st,n);if(!cvc_glob_validate(t)){free(t);cvc_strvec_free(out);return cvc_errorf("invalid glob pattern");}cvc_strvec_push(out,t);free(t);if(!*p)break;st=p+1;}}return 0;}
