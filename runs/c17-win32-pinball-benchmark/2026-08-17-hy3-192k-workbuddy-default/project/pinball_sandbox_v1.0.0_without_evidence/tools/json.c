/* json.c - minimal JSON parser. See json.h. */
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { const char *p; char *err; int errcap; int failed; } P;

static JValue *jp_value(P *p);

static void fail(P *p, const char *msg){
    if(!p->failed && p->err){
        snprintf(p->err, p->errcap, "%s", msg);
        p->failed = 1;
    }
}

static void skip_ws(P *p){
    while(*p->p && isspace((unsigned char)*p->p)) p->p++;
}

static JValue *mk(JType t){
    JValue *v = (JValue*)calloc(1, sizeof(JValue));
    if(v) v->type = t;
    return v;
}

static void obj_put(JValue *o, char *key, JValue *val){
    if(o->u.obj.count >= o->u.obj.cap){
        int nc = o->u.obj.cap? o->u.obj.cap*2 : 8;
        o->u.obj.keys = (char**)realloc(o->u.obj.keys, nc*sizeof(char*));
        o->u.obj.vals = (JValue**)realloc(o->u.obj.vals, nc*sizeof(JValue*));
        o->u.obj.cap = nc;
    }
    o->u.obj.keys[o->u.obj.count] = key;
    o->u.obj.vals[o->u.obj.count] = val;
    o->u.obj.count++;
}

static void arr_push(JValue *a, JValue *v){
    if(a->u.arr.count >= a->u.arr.cap){
        int nc = a->u.arr.cap? a->u.arr.cap*2 : 8;
        a->u.arr.items = (JValue**)realloc(a->u.arr.items, nc*sizeof(JValue*));
        a->u.arr.cap = nc;
    }
    a->u.arr.items[a->u.arr.count++] = v;
}

static char *dupn(const char *s, int n){
    char *r = (char*)malloc(n+1);
    memcpy(r, s, n); r[n]=0; return r;
}

static char *parse_string(P *p){
    if(*p->p != '"'){ fail(p,"expected string"); return NULL; }
    p->p++;
    char buf[4096]; int n=0;
    while(*p->p && *p->p != '"'){
        char c = *p->p++;
        if(c == '\\'){
            if(!*p->p){ fail(p,"bad escape"); return NULL; }
            char e = *p->p++;
            switch(e){
                case '"': c='"'; break;
                case '\\': c='\\'; break;
                case '/': c='/'; break;
                case 'n': c='\n'; break;
                case 't': c='\t'; break;
                case 'r': c='\r'; break;
                case 'b': c='\b'; break;
                case 'f': c='\f'; break;
                case 'u': {
                    if(strlen(p->p) < 4){ fail(p,"bad unicode"); return NULL; }
                    unsigned code=0; for(int i=0;i<4;i++){ char h=p->p[i];
                        int d = (h>='0'&&h<='9')?h-'0':(h>='a'&&h<='f')?h-'a'+10:(h>='A'&&h<='F')?h-'A'+10:-1;
                        if(d<0){ fail(p,"bad unicode"); return NULL; } code = code*16 + (unsigned)d; }
                    p->p += 4;
                    /* encode as UTF-8 (BMP only) */
                    if(code < 0x80) c = (char)code;
                    else if(code < 0x800){ if(n+2<(int)sizeof(buf)){ buf[n++]=(char)(0xC0|(code>>6)); buf[n++]=(char)(0x80|(code&0x3F)); } }
                    else { if(n+3<(int)sizeof(buf)){ buf[n++]=(char)(0xE0|(code>>12)); buf[n++]=(char)(0x80|((code>>6)&0x3F)); buf[n++]=(char)(0x80|(code&0x3F)); } }
                    continue;
                }
                default: fail(p,"bad escape"); return NULL;
            }
        }
        if(n < (int)sizeof(buf)-1) buf[n++]=c;
    }
    if(*p->p != '"'){ fail(p,"unterminated string"); return NULL; }
    p->p++;
    return dupn(buf, n);
}

static JValue *jp_value(P *p){
    skip_ws(p);
    if(p->failed) return NULL;
    char c = *p->p;
    if(c == '{'){
        p->p++; JValue *o = mk(J_OBJ);
        skip_ws(p);
        if(*p->p == '}'){ p->p++; return o; }
        while(1){
            skip_ws(p);
            if(*p->p != '"'){ fail(p,"expected object key"); json_free(o); return NULL; }
            char *key = parse_string(p);
            if(!key) { json_free(o); return NULL; }
            skip_ws(p);
            if(*p->p != ':'){ fail(p,"expected ':'"); free(key); json_free(o); return NULL; }
            p->p++;
            JValue *val = jp_value(p);
            if(!val){ free(key); json_free(o); return NULL; }
            obj_put(o, key, val);
            skip_ws(p);
            if(*p->p == ','){ p->p++; continue; }
            if(*p->p == '}'){ p->p++; break; }
            fail(p,"expected ',' or '}'"); json_free(o); return NULL;
        }
        return o;
    }
    if(c == '['){
        p->p++; JValue *a = mk(J_ARR);
        skip_ws(p);
        if(*p->p == ']'){ p->p++; return a; }
        while(1){
            JValue *val = jp_value(p);
            if(!val){ json_free(a); return NULL; }
            arr_push(a, val);
            skip_ws(p);
            if(*p->p == ','){ p->p++; continue; }
            if(*p->p == ']'){ p->p++; break; }
            fail(p,"expected ',' or ']'"); json_free(a); return NULL;
        }
        return a;
    }
    if(c == '"'){
        char *s = parse_string(p);
        if(!s) return NULL;
        JValue *v = mk(J_STR); v->u.s = s; return v;
    }
    if(c == 't'){ if(strncmp(p->p,"true",4)==0){ p->p+=4; JValue*v=mk(J_BOOL); v->u.b=1; return v; } }
    if(c == 'f'){ if(strncmp(p->p,"false",5)==0){ p->p+=5; JValue*v=mk(J_BOOL); v->u.b=0; return v; } }
    if(c == 'n'){ if(strncmp(p->p,"null",4)==0){ p->p+=4; return mk(J_NULL); } }
    if(c=='-' || (c>='0'&&c<='9')){
        char *end; double d = strtod(p->p, &end);
        int is_int = 1; const char *q=p->p;
        if(*q=='-') q++;
        while(*q){ if(*q=='.'||*q=='e'||*q=='E'){ is_int=0; break; } q++; }
        JValue *v;
        if(is_int && d >= -9.2e18 && d <= 9.2e18){
            v = mk(J_INT); v->u.i = (long long)d;
        } else { v = mk(J_DBL); v->u.d = d; }
        p->p = end; return v;
    }
    fail(p,"unexpected token"); return NULL;
}

JValue *json_parse(const char *text, char *err, int errcap){
    if(err){ err[0]=0; }
    P p = { text, err, errcap, 0 };
    JValue *v = jp_value(&p);
    if(p.failed){ if(v) json_free(v); return NULL; }
    skip_ws(&p);
    if(*p.p){ fail(&p,"trailing characters"); if(v) json_free(v); return NULL; }
    return v;
}

void json_free(JValue *v){
    if(!v) return;
    int i;
    switch(v->type){
        case J_STR: free(v->u.s); break;
        case J_ARR: for(i=0;i<v->u.arr.count;i++) json_free(v->u.arr.items[i]); free(v->u.arr.items); break;
        case J_OBJ: for(i=0;i<v->u.obj.count;i++){ free(v->u.obj.keys[i]); json_free(v->u.obj.vals[i]); } free(v->u.obj.keys); free(v->u.obj.vals); break;
        default: break;
    }
    free(v);
}

JValue *json_obj_get(const JValue *o, const char *key){
    if(!o || o->type!=J_OBJ) return NULL;
    for(int i=0;i<o->u.obj.count;i++) if(strcmp(o->u.obj.keys[i],key)==0) return o->u.obj.vals[i];
    return NULL;
}
JValue *json_arr_get(const JValue *a, int idx){
    if(!a || a->type!=J_ARR) return NULL;
    if(idx<0||idx>=a->u.arr.count) return NULL;
    return a->u.arr.items[idx];
}
int       json_is_obj(const JValue *v){ return v && v->type==J_OBJ; }
int       json_is_arr(const JValue *v){ return v && v->type==J_ARR; }
int       json_is_str(const JValue *v){ return v && v->type==J_STR; }
int       json_is_int(const JValue *v){ return v && v->type==J_INT; }
int       json_is_bool(const JValue *v){ return v && v->type==J_BOOL; }
const char *json_str(const JValue *v){ return (v && v->type==J_STR)? v->u.s : ""; }
long long   json_int(const JValue *v){ return (v && v->type==J_INT)? v->u.i : (v && v->type==J_DBL? (long long)v->u.d : 0); }
int        json_bool(const JValue *v){ return (v && v->type==J_BOOL)? v->u.b : 0; }
double     json_dbl(const JValue *v){ return (v && v->type==J_DBL)? v->u.d : (v && v->type==J_INT? (double)v->u.i : 0); }

/* ---- pretty print ---- */
static void pr(JValue *v, char *b, int cap, int *n, int indent){
    if(*n >= cap) return;
    char tmp[64];
    switch(v->type){
        case J_NULL: { const char *s="null"; for(int i=0;s[i];i++) if(*n<cap) b[(*n)++]=s[i]; break; }
        case J_BOOL: { const char *s=v->u.b?"true":"false"; for(int i=0;s[i];i++) if(*n<cap) b[(*n)++]=s[i]; break; }
        case J_INT: { int k=snprintf(tmp,sizeof tmp,"%lld",v->u.i); for(int i=0;i<k && *n<cap;i++) b[(*n)++]=tmp[i]; break; }
        case J_DBL: { int k=snprintf(tmp,sizeof tmp,"%.10g",v->u.d); for(int i=0;i<k && *n<cap;i++) b[(*n)++]=tmp[i]; break; }
        case J_STR: {
            if(*n<cap) b[(*n)++]='"';
            for(int i=0;v->u.s[i] && *n<cap;i++){ char c=v->u.s[i];
                if(c=='"'||c=='\\'){ if(*n<cap) b[(*n)++]='\\'; if(*n<cap) b[(*n)++]=c; }
                else if(c=='\n'){ if(*n<cap) b[(*n)++]='\\'; if(*n<cap) b[(*n)++]='n'; }
                else if(c=='\t'){ if(*n<cap) b[(*n)++]='\\'; if(*n<cap) b[(*n)++]='t'; }
                else if(*n<cap) b[(*n)++]=c;
            }
            if(*n<cap) b[(*n)++]='"'; break;
        }
        case J_ARR: {
            if(*n<cap) b[(*n)++]='[';
            for(int i=0;i<v->u.arr.count;i++){
                pr(v->u.arr.items[i], b, cap, n, indent+1);
                if(i+1<v->u.arr.count && *n<cap) b[(*n)++]=',';
            }
            if(*n<cap) b[(*n)++]=']'; break;
        }
        case J_OBJ: {
            if(*n<cap) b[(*n)++]='{';
            for(int i=0;i<v->u.obj.count;i++){
                if(*n<cap) b[(*n)++]='"';
                for(int j=0;v->u.obj.keys[i][j] && *n<cap;j++) if(*n<cap) b[(*n)++]=v->u.obj.keys[i][j];
                if(*n<cap) b[(*n)++]='"'; if(*n<cap) b[(*n)++]=':'; if(*n<cap) b[(*n)++]=' ';
                pr(v->u.obj.vals[i], b, cap, n, indent+1);
                if(i+1<v->u.obj.count && *n<cap) b[(*n)++]=',';
            }
            if(*n<cap) b[(*n)++]='}';
            break;
        }
    }
}
int json_print(const JValue *v, char *buf, int cap){
    int n=0; pr((JValue*)v, buf, cap, &n, 0);
    if(n<cap) buf[n]=0; else buf[cap-1]=0;
    return n;
}
