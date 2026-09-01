#include "util.h"
#include "utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char cvc_errbuf[1024] = {0};

CvcStatus cvc_fail(CvcStatus st, const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cvc_errbuf, sizeof(cvc_errbuf), fmt, ap);
    va_end(ap);
    return st;
}

int buf_printf(char **buf, size_t *cap, size_t *len, const char *fmt, ...){
    int need;
    va_list ap;
    va_start(ap, fmt);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if(need < 0) return -1;
    if(*len + (size_t)need + 1 > *cap){
        size_t ncap = *cap ? *cap : 64;
        while(ncap < *len + (size_t)need + 1) ncap *= 2;
        char *nb = (char*)realloc(*buf, ncap);
        if(!nb) return -1;
        *buf = nb; *cap = ncap;
    }
    va_start(ap, fmt);
    vsnprintf(*buf + *len, *cap - *len, fmt, ap);
    va_end(ap);
    *len += (size_t)need;
    return 0;
}

void bytes_init(Bytes *b){ b->data=NULL; b->len=0; b->cap=0; }
void bytes_free(Bytes *b){ free(b->data); b->data=NULL; b->len=b->cap=0; }

int bytes_reserve(Bytes *b, size_t extra){
    if(b->len + extra + 1 > b->cap){
        size_t ncap = b->cap ? b->cap : 64;
        while(ncap < b->len + extra + 1) ncap *= 2;
        uint8_t *nb = (uint8_t*)realloc(b->data, ncap);
        if(!nb) return -1;
        b->data = nb; b->cap = ncap;
    }
    return 0;
}
int bytes_append(Bytes *b, const void *data, size_t n){
    if(bytes_reserve(b, n) != 0) return -1;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = 0;
    return 0;
}
int bytes_append_byte(Bytes *b, uint8_t x){ return bytes_append(b, &x, 1); }
int bytes_append_cstr(Bytes *b, const char *s){ return bytes_append(b, s, strlen(s)); }
int bytes_append_u32(Bytes *b, uint32_t v){
    uint8_t p[4];
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
    return bytes_append(b,p,4);
}
int bytes_zt(Bytes *b){
    if(bytes_reserve(b, 1) != 0) return -1;
    b->data[b->len] = 0;
    return 0;
}

void strvec_init(StrVec *v){ v->items=NULL; v->len=0; v->cap=0; }
void strvec_free(StrVec *v){ strvec_clear(v); free(v->items); v->items=NULL; v->cap=0; }
void strvec_clear(StrVec *v){
    size_t i;
    for(i=0;i<v->len;i++) free(v->items[i]);
    v->len = 0;
}
int strvec_push(StrVec *v, char *owned){
    if(v->len == v->cap){
        size_t ncap = v->cap ? v->cap*2 : 8;
        char **ni = (char**)realloc(v->items, ncap * sizeof(char*));
        if(!ni) return -1;
        v->items = ni; v->cap = ncap;
    }
    v->items[v->len++] = owned;
    return 0;
}
int strvec_push_dup(StrVec *v, const char *s){
    char *d = (char*)malloc(strlen(s)+1);
    if(!d) return -1;
    strcpy(d, s);
    if(strvec_push(v, d) != 0){ free(d); return -1; }
    return 0;
}

void ptrvec_init(PtrVec *v){ v->items=NULL; v->len=0; v->cap=0; }
void ptrvec_free(PtrVec *v){ free(v->items); v->items=NULL; v->len=v->cap=0; }
int ptrvec_push(PtrVec *v, void *p){
    if(v->len == v->cap){
        size_t ncap = v->cap ? v->cap*2 : 8;
        void **ni = (void**)realloc(v->items, ncap * sizeof(void*));
        if(!ni) return -1;
        v->items = ni; v->cap = ncap;
    }
    v->items[v->len++] = p;
    return 0;
}

int cvc_memcmp(const void *a, const void *b, size_t n){ return memcmp(a,b,n); }
