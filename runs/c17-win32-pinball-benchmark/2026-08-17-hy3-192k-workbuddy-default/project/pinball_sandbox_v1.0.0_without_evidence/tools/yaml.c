/* yaml.c - minimal YAML subset parser. See yaml.h. */
#include "yaml.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { const char *text; char *err; int errcap; int failed; } YP;

static void yfail(YP *p, const char *m){ if(!p->failed && p->err){ snprintf(p->err,p->errcap,"%s",m); p->failed=1; } }

/* count leading spaces (indent). Tabs not supported as indent. */
static int indent_of(const char *s){
    int n=0; while(s[n]==' ') n++; return n;
}

/* strip a trailing comment that is outside quotes; also trim trailing ws */
static void strip_comment(char *s){
    int q=0; char qc=0;
    for(int i=0;s[i];i++){
        if(q){ if(s[i]==qc) q=0; continue; }
        if(s[i]=='"'||s[i]=='\''){ q=1; qc=s[i]; continue; }
        if(s[i]=='#' && (i==0 || s[i-1]==' ')){ s[i]=0; break; }
    }
    /* trailing ws */
    int n=(int)strlen(s); while(n>0 && (s[n-1]==' '||s[n-1]=='\t')) s[--n]=0;
}

/* parse a scalar token (after ':' or after '- ') into a JValue */
static JValue *scalar(const char *tok, YP *p){
    /* quoted? */
    if(tok[0]=='"' || tok[0]=='\''){
        char qc=tok[0]; int n=(int)strlen(tok);
        if(n<2 || tok[n-1]!=qc){ yfail(p,"unterminated YAML quoted scalar"); return NULL; }
        char *s=(char*)malloc(n); int m=0;
        for(int i=1;i<n-1;i++){ char c=tok[i]; if(c=='\\' && qc=='"' && i+1<n-1){ i++; char e=tok[i];
            switch(e){ case 'n': s[m++]='\n'; break; case 't': s[m++]='\t'; break; case '"': s[m++]='"'; break; case '\\': s[m++]='\\'; break; default: s[m++]=e; } }
            else s[m++]=c; }
        s[m]=0; JValue *v=(JValue*)calloc(1,sizeof(JValue)); v->type=J_STR; v->u.s=s; return v;
    }
    if(strcmp(tok,"true")==0||strcmp(tok,"True")==0){ JValue*v=calloc(1,sizeof(JValue)); v->type=J_BOOL; v->u.b=1; return v; }
    if(strcmp(tok,"false")==0||strcmp(tok,"False")==0){ JValue*v=calloc(1,sizeof(JValue)); v->type=J_BOOL; v->u.b=0; return v; }
    if(strcmp(tok,"null")==0||strcmp(tok,"~")==0||tok[0]==0){ return calloc(1,sizeof(JValue)); }
    /* int */
    int is_int=1; for(int i=0;tok[i];i++){ char c=tok[i]; if((i==0&&c=='-')||(c>='0'&&c<='9')) continue; is_int=0; break; }
    if(is_int && tok[0]){ JValue*v=calloc(1,sizeof(JValue)); v->type=J_INT; v->u.i=strtoll(tok,NULL,10); return v; }
    /* string */
    JValue *v=(JValue*)calloc(1,sizeof(JValue)); v->type=J_STR; v->u.s=strdup(tok); return v;
}

typedef struct { int indent; char *content; int is_seq; char *seq_after; } Line;

static JValue *parse_block(YP *p, const char *text, Line *lines, int nlines, int *idx, int parent_indent);

static JValue *parse_mapping(YP *p, Line *lines, int nlines, int *idx, int indent){
    JValue *o=(JValue*)calloc(1,sizeof(JValue)); o->type=J_OBJ;
    while(*idx < nlines){
        Line *L=&lines[*idx];
        if(L->indent < indent) break;
        if(L->indent > indent){ /* shouldn't happen at mapping level */ break; }
        if(L->is_seq) break; /* mapping line expected */
        /* split key: value */
        char *colon = strchr(L->content, ':');
        if(!colon){ yfail(p,"expected 'key: value'"); json_free(o); return NULL; }
        int klen=(int)(colon-L->content);
        while(klen>0 && L->content[klen-1]==' ') klen--;
        char *key=(char*)malloc(klen+1); memcpy(key,L->content,klen); key[klen]=0;
        char *val = colon+1; while(*val==' ') val++;
        if(val[0]==0){
            /* nested block */
            (*idx)++;
            if(*idx<nlines && lines[*idx].indent>indent){
                JValue *child=parse_block(p, NULL, lines, nlines, idx, indent);
                if(!child){ free(key); json_free(o); return NULL; }
                /* store */
                if(o->u.obj.count>=o->u.obj.cap){ int nc=o->u.obj.cap?o->u.obj.cap*2:8; o->u.obj.keys=realloc(o->u.obj.keys,nc*sizeof(char*)); o->u.obj.vals=realloc(o->u.obj.vals,nc*sizeof(JValue*)); o->u.obj.cap=nc; }
                o->u.obj.keys[o->u.obj.count]=key; o->u.obj.vals[o->u.obj.count]=child; o->u.obj.count++;
            } else {
                /* empty value -> null */
                if(o->u.obj.count>=o->u.obj.cap){ int nc=o->u.obj.cap?o->u.obj.cap*2:8; o->u.obj.keys=realloc(o->u.obj.keys,nc*sizeof(char*)); o->u.obj.vals=realloc(o->u.obj.vals,nc*sizeof(JValue*)); o->u.obj.cap=nc; }
                o->u.obj.keys[o->u.obj.count]=key; o->u.obj.vals[o->u.obj.count]=calloc(1,sizeof(JValue)); o->u.obj.count++;
            }
        } else {
            JValue *child=scalar(val,p);
            if(!child){ free(key); json_free(o); return NULL; }
            if(o->u.obj.count>=o->u.obj.cap){ int nc=o->u.obj.cap?o->u.obj.cap*2:8; o->u.obj.keys=realloc(o->u.obj.keys,nc*sizeof(char*)); o->u.obj.vals=realloc(o->u.obj.vals,nc*sizeof(JValue*)); o->u.obj.cap=nc; }
            o->u.obj.keys[o->u.obj.count]=key; o->u.obj.vals[o->u.obj.count]=child; o->u.obj.count++;
            (*idx)++;
        }
    }
    return o;
}

static JValue *parse_sequence(YP *p, Line *lines, int nlines, int *idx, int indent){
    JValue *a=(JValue*)calloc(1,sizeof(JValue)); a->type=J_ARR;
    while(*idx<nlines){
        Line *L=&lines[*idx];
        if(L->indent<indent) break;
        if(L->indent>indent) break;
        if(!L->is_seq) break;
        char *after=L->seq_after; /* content after '- ' */
        if(after && after[0] && strchr(after,':')){
            /* sequence of mappings: '- key: val' possibly followed by sibling
               'key: val' lines indented deeper than the '-' (same mapping item) */
            JValue *item=(JValue*)calloc(1,sizeof(JValue)); item->type=J_OBJ;
            /* parse the inline first key:value */
            {
                char *colon=strchr(after,':'); int klen=(int)(colon-after); while(klen>0&&after[klen-1]==' ')klen--;
                char *key=malloc(klen+1); memcpy(key,after,klen); key[klen]=0;
                char *val=colon+1; while(*val==' ')val++;
                JValue *child = val[0]? scalar(val,p) : NULL;
                if(!child){
                    /* value is a nested block on following deeper lines */
                    if(*idx+1<nlines && lines[*idx+1].indent>indent){
                        (*idx)++;
                        child=parse_block(p,NULL,lines,nlines,idx,indent);
                    } else child=calloc(1,sizeof(JValue));
                }
                if(item->u.obj.count>=item->u.obj.cap){ int nc=item->u.obj.cap?item->u.obj.cap*2:4; item->u.obj.keys=realloc(item->u.obj.keys,nc*sizeof(char*)); item->u.obj.vals=realloc(item->u.obj.vals,nc*sizeof(JValue*)); item->u.obj.cap=nc; }
                item->u.obj.keys[item->u.obj.count]=key; item->u.obj.vals[item->u.obj.count]=child; item->u.obj.count++;
            }
            /* consume sibling mapping lines (deeper than the '-', not a sequence) */
            while(*idx+1<nlines && lines[*idx+1].indent>indent && !lines[*idx+1].is_seq){
                (*idx)++;
                Line *sl=&lines[*idx];
                char *colon=strchr(sl->content,':'); if(!colon) break;
                int klen=(int)(colon-sl->content); while(klen>0&&sl->content[klen-1]==' ')klen--;
                char *key=malloc(klen+1); memcpy(key,sl->content,klen); key[klen]=0;
                char *val=colon+1; while(*val==' ')val++;
                JValue *child = val[0]? scalar(val,p) : NULL;
                if(!child){
                    if(*idx+1<nlines && lines[*idx+1].indent>sl->indent){
                        (*idx)++; child=parse_block(p,NULL,lines,nlines,idx,sl->indent);
                    } else child=calloc(1,sizeof(JValue));
                }
                if(item->u.obj.count>=item->u.obj.cap){ int nc=item->u.obj.cap?item->u.obj.cap*2:4; item->u.obj.keys=realloc(item->u.obj.keys,nc*sizeof(char*)); item->u.obj.vals=realloc(item->u.obj.vals,nc*sizeof(JValue*)); item->u.obj.cap=nc; }
                item->u.obj.keys[item->u.obj.count]=key; item->u.obj.vals[item->u.obj.count]=child; item->u.obj.count++;
            }
            if(a->u.arr.count>=a->u.arr.cap){ int nc=a->u.arr.cap?a->u.arr.cap*2:8; a->u.arr.items=realloc(a->u.arr.items,nc*sizeof(JValue*)); a->u.arr.cap=nc; }
            a->u.arr.items[a->u.arr.count++]=item;
            /* advance past the consumed '- key: val' item (its inline line plus any
               deeper sibling lines already advanced by the inner while loop above) */
            (*idx)++;
        } else {
            JValue *item = after && after[0]? scalar(after,p) : calloc(1,sizeof(JValue));
            if(!item){ json_free(a); return NULL; }
            if(a->u.arr.count>=a->u.arr.cap){ int nc=a->u.arr.cap?a->u.arr.cap*2:8; a->u.arr.items=realloc(a->u.arr.items,nc*sizeof(JValue*)); a->u.arr.cap=nc; }
            a->u.arr.items[a->u.arr.count++]=item;
            (*idx)++;
        }
    }
    return a;
}

static JValue *parse_block(YP *p, const char *text, Line *lines, int nlines, int *idx, int parent_indent){
    (void)text;
    if(*idx>=nlines) return calloc(1,sizeof(JValue));
    int indent=lines[*idx].indent;
    if(lines[*idx].is_seq) return parse_sequence(p,lines,nlines,idx,indent);
    return parse_mapping(p,lines,nlines,idx,indent);
}

JValue *yaml_parse(const char *text, char *err, int errcap){
    if(err) err[0]=0;
    YP p={text,err,errcap,0};
    /* tokenize into lines */
    int cap=64; Line *lines=(Line*)calloc(cap,sizeof(Line));
    int n=0;
    const char *cur=text;
    while(*cur){
        /* build one logical line (drop trailing CR) */
        char buf[8192]; int bn=0;
        while(*cur && *cur!='\n'){ if(bn<(int)sizeof(buf)-1) buf[bn++]=*cur; cur++; }
        if(*cur=='\n') cur++;
        buf[bn]=0;
        /* process buf if not blank/comment-only */
        char tmp[8192]; memcpy(tmp,buf,bn+1);
        strip_comment(tmp);
        int ind=indent_of(tmp);
        int has=0; for(int k=0;tmp[k];k++) if(tmp[k]!=' '){has=1;break;}
        if(has){
            int is_seq=0; char *after=NULL;
            char *content=strdup(tmp+ind);
            if(content[0]=='-' && (content[1]==' '||content[1]==0)){ is_seq=1; after=strdup(content[2]?content+2:""); }
            if(n>=cap){ cap*=2; lines=realloc(lines,cap*sizeof(Line)); }
            lines[n].indent=ind; lines[n].content=content; lines[n].is_seq=is_seq; lines[n].seq_after=after; n++;
        }
    }
    int idx=0;
    JValue *root=parse_block(&p,text,lines,n,&idx,0);
    for(int k=0;k<n;k++){ free(lines[k].content); if(lines[k].seq_after) free(lines[k].seq_after); }
    free(lines);
    if(p.failed){ if(root) json_free(root); return NULL; }
    return root;
}
