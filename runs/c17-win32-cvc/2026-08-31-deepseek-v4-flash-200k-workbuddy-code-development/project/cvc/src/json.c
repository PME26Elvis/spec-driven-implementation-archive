#include "json.h"
#include "utf8.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    const char *s;
    size_t len;
    size_t pos;
    JErr *err;
} P;

static void jerr(P *p, size_t off, const char *msg){
    p->err->has_error = 1;
    p->err->err_offset = off;
    snprintf(p->err->err_msg, sizeof(p->err->err_msg), "%s", msg);
}

static int fail(P *p, const char *msg){ jerr(p, p->pos, msg); return -1; }

static void skip_ws(P *p){
    while(p->pos < p->len){
        char c = p->s[p->pos];
        if(c==' '||c=='\t'||c=='\n'||c=='\r') p->pos++;
        else break;
    }
}

static int hexval(char c){
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}

static JVal *mkval(JType t){
    JVal *v = (JVal*)calloc(1, sizeof(JVal));
    if(v) v->type = t;
    return v;
}

/* Parse a JSON string literal starting at p->pos (assumed '"').
 * Produces a heap NUL-terminated byte string (may contain NUL in middle,
 * captured by v->str_len). */
static int parse_string(P *p, char **out, size_t *out_len){
    if(p->pos >= p->len || p->s[p->pos] != '"') return fail(p, "expected string");
    p->pos++;
    uint8_t *buf = NULL; size_t len=0, cap=0;
    while(p->pos < p->len){
        unsigned char c = (unsigned char)p->s[p->pos];
        if(c == '"'){
            p->pos++;
            if(utf16_push_utf8(&buf, &len, &cap, 0) != 0){ free(buf); return fail(p, "out of memory"); }
            *out = (char*)buf; *out_len = (len>0)?len-1:0; /* exclude appended NUL terminator */
            return 0;
        }
        if(c == '\\'){
            p->pos++;
            if(p->pos >= p->len){ free(buf); return fail(p, "unterminated escape"); }
            char e = p->s[p->pos];
            switch(e){
                case '"': if(utf16_push_utf8(&buf,&len,&cap,'"')!=0) goto oom; p->pos++; break;
                case '\\': if(utf16_push_utf8(&buf,&len,&cap,'\\')!=0) goto oom; p->pos++; break;
                case '/': if(utf16_push_utf8(&buf,&len,&cap,'/')!=0) goto oom; p->pos++; break;
                case 'b': if(utf16_push_utf8(&buf,&len,&cap,0x08)!=0) goto oom; p->pos++; break;
                case 'f': if(utf16_push_utf8(&buf,&len,&cap,0x0C)!=0) goto oom; p->pos++; break;
                case 'n': if(utf16_push_utf8(&buf,&len,&cap,0x0A)!=0) goto oom; p->pos++; break;
                case 'r': if(utf16_push_utf8(&buf,&len,&cap,0x0D)!=0) goto oom; p->pos++; break;
                case 't': if(utf16_push_utf8(&buf,&len,&cap,0x09)!=0) goto oom; p->pos++; break;
                case 'u': {
                    p->pos++;
                    if(p->pos+4 > p->len){ free(buf); return fail(p, "invalid \\u escape"); }
                    int h0=hexval(p->s[p->pos]),h1=hexval(p->s[p->pos+1]),
                        h2=hexval(p->s[p->pos+2]),h3=hexval(p->s[p->pos+3]);
                    if(h0<0||h1<0||h2<0||h3<0){ free(buf); return fail(p, "invalid hex in \\u"); }
                    uint32_t u = (uint32_t)((h0<<12)|(h1<<8)|(h2<<4)|h3);
                    p->pos += 4;
                    if(u>=0xD800 && u<=0xDBFF){
                        /* need low surrogate */
                        if(p->pos+6 <= p->len && p->s[p->pos]=='\\' && p->s[p->pos+1]=='u'){
                            int g0=hexval(p->s[p->pos+2]),g1=hexval(p->s[p->pos+3]),
                                g2=hexval(p->s[p->pos+4]),g3=hexval(p->s[p->pos+5]);
                            if(g0<0||g1<0||g2<0||g3<0){ free(buf); return fail(p,"invalid low surrogate"); }
                            uint32_t lo=(uint32_t)((g0<<12)|(g1<<8)|(g2<<4)|g3);
                            if(lo>=0xDC00 && lo<=0xDFFF){
                                uint32_t cp = 0x10000 + ((u-0xD800)<<10) + (lo-0xDC00);
                                if(utf16_push_utf8(&buf,&len,&cap,cp)!=0) goto oom;
                                p->pos += 6;
                                break;
                            }
                        }
                        free(buf); return fail(p, "unpaired high surrogate");
                    }
                    if(u>=0xDC00 && u<=0xDFFF){ free(buf); return fail(p, "unpaired low surrogate"); }
                    if(utf16_push_utf8(&buf,&len,&cap,u)!=0) goto oom;
                    break;
                }
                default: free(buf); return fail(p, "invalid escape");
            }
            continue;
        }
        if(c < 0x20){ free(buf); return fail(p, "unescaped control in string"); }
        /* literal multibyte: decode to validate UTF-8 */
        {
            uint32_t cp;
            int k = utf8_decode((const uint8_t*)p->s + p->pos, p->len - p->pos, &cp);
            if(k == 0){ free(buf); return fail(p, "invalid UTF-8 in string"); }
            if(utf16_push_utf8(&buf, &len, &cap, cp) != 0) goto oom;
            p->pos += (size_t)k;
        }
    }
    free(buf);
    return fail(p, "unterminated string");
oom:
    free(buf);
    return fail(p, "out of memory");
}

static int json_parse_value(P *p, JVal **out);

static int parse_number(P *p, JVal *v){
    size_t start = p->pos;
    size_t i = p->pos;
    if(i < p->len && p->s[i]=='-') i++;
    if(i >= p->len || !(p->s[i]>='0'&&p->s[i]<='9')) return fail(p, "invalid number");
    if(p->s[i]=='0'){ i++; }
    else { while(i<p->len && p->s[i]>='0'&&p->s[i]<='9') i++; }
    if(i < p->len && p->s[i]=='.'){
        i++;
        if(i>=p->len || !(p->s[i]>='0'&&p->s[i]<='9')) return fail(p, "invalid fraction");
        while(i<p->len && p->s[i]>='0'&&p->s[i]<='9') i++;
    }
    if(i < p->len && (p->s[i]=='e'||p->s[i]=='E')){
        i++;
        if(i<p->len && (p->s[i]=='+'||p->s[i]=='-')) i++;
        if(i>=p->len || !(p->s[i]>='0'&&p->s[i]<='9')) return fail(p, "invalid exponent");
        while(i<p->len && p->s[i]>='0'&&p->s[i]<='9') i++;
    }
    /* copy token */
    size_t n = i - start;
    v->num = (char*)malloc(n+1);
    if(!v->num) return fail(p, "out of memory");
    memcpy(v->num, p->s+start, n);
    v->num[n]=0;
    p->pos = i;
    return 0;
}

static int parse_array(P *p, JVal *v){
    p->pos++; /* '[' */
    skip_ws(p);
    if(p->pos < p->len && p->s[p->pos]==']'){ p->pos++; return 0; }
    while(1){
        skip_ws(p);
        JVal *item = NULL;
        if(json_parse_value(p, &item) != 0) return -1;
        if(v->arr_len == v->arr_cap){
            size_t nc = v->arr_cap ? v->arr_cap*2 : 8;
            JVal **na = (JVal**)realloc(v->arr, nc*sizeof(JVal*));
            if(!na){ json_free(item); return fail(p,"out of memory"); }
            v->arr = na; v->arr_cap = nc;
        }
        v->arr[v->arr_len++] = item;
        skip_ws(p);
        if(p->pos < p->len && p->s[p->pos]==','){ p->pos++; continue; }
        if(p->pos < p->len && p->s[p->pos]==']'){ p->pos++; return 0; }
        return fail(p, "expected ',' or ']'");
    }
}

static int parse_object(P *p, JVal *v){
    p->pos++; /* '{' */
    skip_ws(p);
    if(p->pos < p->len && p->s[p->pos]=='}'){ p->pos++; return 0; }
    while(1){
        skip_ws(p);
        if(p->pos >= p->len || p->s[p->pos] != '"') return fail(p, "expected object key string");
        char *key=NULL; size_t klen=0;
        if(parse_string(p, &key, &klen) != 0) return -1;
        /* Reject NUL in key (repository keys cannot contain NUL). */
        if(memchr(key, 0, klen) != NULL){ free(key); return fail(p, "NUL in object key"); }
        skip_ws(p);
        if(p->pos >= p->len || p->s[p->pos] != ':'){ free(key); return fail(p, "expected ':'"); }
        p->pos++;
        skip_ws(p);
        JVal *val = NULL;
        if(json_parse_value(p, &val) != 0){ free(key); return -1; }
        /* duplicate key check after decoding */
        size_t k;
        for(k=0;k<v->obj_len;k++){
            if(v->obj[k].val && v->obj[k].key && strcmp(v->obj[k].key, key)==0){
                char m[128]; snprintf(m,sizeof m,"duplicate key \"%s\"", key);
                free(key); json_free(val); return fail(p, m);
            }
        }
        if(v->obj_len == v->obj_cap){
            size_t nc = v->obj_cap ? v->obj_cap*2 : 8;
            JMember *no = (JMember*)realloc(v->obj, nc*sizeof(JMember));
            if(!no){ free(key); json_free(val); return fail(p,"out of memory"); }
            v->obj = no; v->obj_cap = nc;
        }
        v->obj[v->obj_len].key = key;
        v->obj[v->obj_len].val = val;
        v->obj_len++;
        skip_ws(p);
        if(p->pos < p->len && p->s[p->pos]==','){ p->pos++; continue; }
        if(p->pos < p->len && p->s[p->pos]=='}'){ p->pos++; return 0; }
        return fail(p, "expected ',' or '}'");
    }
}

static int json_parse_value(P *p, JVal **out){
    skip_ws(p);
    if(p->pos >= p->len) return fail(p, "unexpected end of input");
    char c = p->s[p->pos];
    JVal *v = NULL;
    if(c=='{'){ v=mkval(J_OBJ); if(!v) return fail(p,"oom"); if(parse_object(p,v)!=0){json_free(v);return -1;} }
    else if(c=='['){ v=mkval(J_ARR); if(!v) return fail(p,"oom"); if(parse_array(p,v)!=0){json_free(v);return -1;} }
    else if(c=='"'){ v=mkval(J_STR); if(!v) return fail(p,"oom"); size_t sl; if(parse_string(p,&v->str,&sl)!=0){json_free(v);return -1;} v->str_len=sl; }
    else if(c=='t'){ if(p->len-p->pos>=4 && memcmp(p->s+p->pos,"true",4)==0){v=mkval(J_BOOL); if(!v)return fail(p,"oom"); v->b=1; p->pos+=4;} else return fail(p,"invalid literal"); }
    else if(c=='f'){ if(p->len-p->pos>=5 && memcmp(p->s+p->pos,"false",5)==0){v=mkval(J_BOOL); if(!v)return fail(p,"oom"); v->b=0; p->pos+=5;} else return fail(p,"invalid literal"); }
    else if(c=='n'){ if(p->len-p->pos>=4 && memcmp(p->s+p->pos,"null",4)==0){v=mkval(J_NULL); if(!v)return fail(p,"oom"); p->pos+=4;} else return fail(p,"invalid literal"); }
    else if(c=='-'||(c>='0'&&c<='9')){ v=mkval(J_NUM); if(!v) return fail(p,"oom"); if(parse_number(p,v)!=0){json_free(v);return -1;} }
    else return fail(p, "unexpected character");
    *out = v;
    return 0;
}

int json_parse(const char *text, size_t len, JVal **out, JErr *err){
    memset(err, 0, sizeof(*err));
    /* Reject UTF-8 BOM at start. */
    if(len >= 3 && (unsigned char)text[0]==0xEF && (unsigned char)text[1]==0xBB && (unsigned char)text[2]==0xBF){
        err->has_error=1; err->err_offset=0;
        snprintf(err->err_msg,sizeof err->err_msg,"UTF-8 BOM rejected");
        return -1;
    }
    P p; p.s=text; p.len=len; p.pos=0; p.err=err;
    JVal *root=NULL;
    if(json_parse_value(&p, &root) != 0) return -1;
    skip_ws(&p);
    if(p.pos < p.len){
        if(root) json_free(root);
        return fail(&p, "trailing data after JSON value");
    }
    *out = root;
    return 0;
}

void json_free(JVal *v){
    if(!v) return;
    if(v->type==J_ARR){
        size_t i; for(i=0;i<v->arr_len;i++) json_free(v->arr[i]);
        free(v->arr);
    } else if(v->type==J_OBJ){
        size_t i; for(i=0;i<v->obj_len;i++){ free(v->obj[i].key); json_free(v->obj[i].val); }
        free(v->obj);
    }
    free(v->num);
    free(v->str);
    free(v);
}

JMember *json_find(const JVal *obj, const char *key){
    size_t i;
    if(!obj || obj->type != J_OBJ) return NULL;
    for(i=0;i<obj->obj_len;i++){
        if(obj->obj[i].key && strcmp(obj->obj[i].key, key)==0) return &obj->obj[i];
    }
    return NULL;
}
