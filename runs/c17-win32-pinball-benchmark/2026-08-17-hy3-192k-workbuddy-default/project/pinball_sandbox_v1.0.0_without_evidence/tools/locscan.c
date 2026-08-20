/* locscan.c - categorized physical line counter (doc 11).
 * Reads a JSON or YAML config, walks a directory tree, counts physical/
 * blank/nonblank lines per category, excludes binaries and configured paths,
 * and emits a human table and optional JSON. C17, no third-party deps. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <io.h>
#include "json.h"
#include "yaml.h"

#define MAXCAT 32
typedef struct { char name[64]; long files, phys, blank, nonblank; } Cat;

static Cat g_cat[MAXCAT]; static int g_ncat=0;
static long g_excluded_files=0;
static char g_err[256];

static int cat_index(const char *name){
    for(int i=0;i<g_ncat;i++) if(strcmp(g_cat[i].name,name)==0) return i;
    if(g_ncat<MAXCAT){ strncpy(g_cat[g_ncat].name,name,63); g_cat[g_ncat].name[63]=0; g_cat[g_ncat].files=0; g_cat[g_ncat].phys=0; g_cat[g_ncat].blank=0; g_cat[g_ncat].nonblank=0; return g_ncat++; }
    return 0;
}

/* simple fnmatch supporting * and ? */
static int fnmatch_s(const char *pat, const char *str){
    while(*pat){
        if(*pat=='*'){ pat++; if(!*pat) return 1; while(*str){ if(fnmatch_s(pat,str)) return 1; str++; } return 0; }
        if(*pat=='?'){ if(!*str) return 0; pat++; str++; continue; }
        if(*pat!=*str) return 0;
        pat++; str++;
    }
    return *str==0;
}
static int ext_of(const char *path){ const char *d=strrchr(path,'.'); return d? (int)(d-path) : -1; }
static const char *ext_ptr(const char *path){ const char *d=strrchr(path,'.'); return d? d+1:""; }

/* config */
static char **g_inc_ext=NULL; static int g_inc_n=0;
static char **g_exc_ext=NULL; static int g_exc_n=0;
static char **g_exc_path=NULL; static int g_exc_path_n=0;
typedef struct { char pat[256]; char cat[64]; int is_ext; } Rule;
static Rule g_rules[256]; static int g_nrules=0;
static long g_max_bytes=0; /* 0 = unlimited */
static int g_follow_sym=0;

static void split_push(char ***arr,int *n,const char *v){
    *arr=(char**)realloc(*arr,(*n+1)*sizeof(char*)); (*arr)[*n]=strdup(v); (*n)++;
}
static void load_config(JValue *cfg){
    JValue *v;
    if((v=json_obj_get(cfg,"include_extensions")) && v->type==J_ARR)
        for(int i=0;i<v->u.arr.count;i++) if(v->u.arr.items[i]->type==J_STR) split_push(&g_inc_ext,&g_inc_n,v->u.arr.items[i]->u.s);
    if((v=json_obj_get(cfg,"exclude_extensions")) && v->type==J_ARR)
        for(int i=0;i<v->u.arr.count;i++) if(v->u.arr.items[i]->type==J_STR) split_push(&g_exc_ext,&g_exc_n,v->u.arr.items[i]->u.s);
    if((v=json_obj_get(cfg,"exclude_paths")) && v->type==J_ARR)
        for(int i=0;i<v->u.arr.count;i++) if(v->u.arr.items[i]->type==J_STR) split_push(&g_exc_path,&g_exc_path_n,v->u.arr.items[i]->u.s);
    if((v=json_obj_get(cfg,"category_rules")) && v->type==J_ARR){
        for(int i=0;i<v->u.arr.count && g_nrules<256;i++){
            JValue *r=v->u.arr.items[i]; if(r->type!=J_OBJ) continue;
            JValue *pat=json_obj_get(r,"pattern"); JValue *ext=json_obj_get(r,"extension"); JValue *cat=json_obj_get(r,"category");
            if(!cat || cat->type!=J_STR) continue;
            if(pat && pat->type==J_STR){ strncpy(g_rules[g_nrules].pat,pat->u.s,255); g_rules[g_nrules].pat[255]=0; g_rules[g_nrules].is_ext=0; }
            else if(ext && ext->type==J_STR){ snprintf(g_rules[g_nrules].pat,sizeof g_rules[g_nrules].pat,"*.%s",ext->u.s); g_rules[g_nrules].is_ext=1; }
            else continue;
            strncpy(g_rules[g_nrules].cat,cat->u.s,63); g_rules[g_nrules].cat[63]=0;
            g_nrules++;
        }
    }
    if((v=json_obj_get(cfg,"max_file_bytes")) && (v->type==J_INT)) g_max_bytes=(long)v->u.i;
    if((v=json_obj_get(cfg,"follow_symlinks")) && v->type==J_BOOL) g_follow_sym=v->u.b;
}

static int is_binary(const char *path){
    FILE *f=fopen(path,"rb"); if(!f) return 0;
    unsigned char b[512]; int n=(int)fread(b,1,512,f); fclose(f);
    for(int i=0;i<n;i++) if(b[i]==0) return 1; /* NUL => binary */
    return 0;
}
static int in_list(char **arr,int n,const char *v){
    for(int i=0;i<n;i++) if(strcmp(arr[i],v)==0) return 1; return 0;
}
static int path_excluded(const char *rel){
    for(int i=0;i<g_exc_path_n;i++){
        const char *p=g_exc_path[i];
        if(strstr(rel,p)) return 1;
    }
    return 0;
}
static const char *categorize(const char *rel){
    /* try rules in order, matching against relative path then basename */
    const char *base=strrchr(rel,'/'); if(!base) base=strrchr(rel,'\\'); base=base?base+1:rel;
    const char *ext=ext_ptr(base);
    for(int i=0;i<g_nrules;i++){
        if(g_rules[i].is_ext){
            if(strcasecmp(ext,g_rules[i].pat+1)==0) return g_rules[i].cat; /* pat = "*.ext" */
        } else {
            if(fnmatch_s(g_rules[i].pat,rel)) return g_rules[i].cat;
            if(fnmatch_s(g_rules[i].pat,base)) return g_rules[i].cat;
        }
    }
    return "other_counted";
}

static int count_lines(const char *path, long *phys, long *blank, long *nonblank){
    FILE *f=fopen(path,"rb"); if(!f) return 0;
    *phys=0; *blank=0; *nonblank=0;
    char buf[65536]; int has=0; int len=0; int c;
    while((c=fgetc(f))!=EOF){
        if(c=='\n'){ (*phys)++; if(!has) (*blank)++; else (*nonblank)++; has=0; len=0; continue; }
        if(c=='\r') continue;
        buf[len<65535?len:65535]=c; if(len<65535) len++;
        if(!isspace((unsigned char)c)) has=1;
    }
    if(len>0 || has){ (*phys)++; if(!has) (*blank)++; else (*nonblank)++; } /* final unterminated line */
    fclose(f);
    return 1;
}

static void walk(const char *root, const char *rel){
    DIR *d=opendir(root); if(!d) return;
    struct dirent *e;
    while((e=readdir(d))){
        if(strcmp(e->d_name,".")==0||strcmp(e->d_name,"..")==0) continue;
        char full[2048]; snprintf(full,sizeof full,"%s/%s",root,e->d_name);
        char rrel[2048]; snprintf(rrel,sizeof rrel,"%s%s%s",rel?rel:"",rel?"/":"",e->d_name);
        int is_dir=0;
        /* determine dir via stat */
        struct stat st; if(stat(full,&st)==0) is_dir = (st.st_mode & S_IFDIR)!=0;
        if(is_dir){
            if(path_excluded(rrel)){ g_excluded_files++; continue; }
            walk(full, rrel);
        } else {
            if(path_excluded(rrel)){ g_excluded_files++; continue; }
            const char *ext=ext_ptr(e->d_name);
            if(g_exc_n && in_list(g_exc_ext,g_exc_n,ext)){ g_excluded_files++; continue; }
            if(is_binary(full)){ g_excluded_files++; continue; }
            if(g_max_bytes>0){ if(stat(full,&st)==0 && st.st_size>g_max_bytes){ g_excluded_files++; continue; } }
            /* include filter */
            if(g_inc_n && !in_list(g_inc_ext,g_inc_n,ext)){ g_excluded_files++; continue; }
            long ph,bl,nb;
            if(!count_lines(full,&ph,&bl,&nb)){ g_excluded_files++; continue; }
            const char *cat=categorize(rrel);
            int ci=cat_index(cat);
            g_cat[ci].files++;
            g_cat[ci].phys+=ph; g_cat[ci].blank+=bl; g_cat[ci].nonblank+=nb;
        }
    }
    closedir(d);
}

int main(int argc,char**argv){
    const char *root="."; const char *config=NULL; int json_out=0;
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--json")==0) json_out=1;
        else if(strcmp(argv[i],"--config")==0 && i+1<argc) config=argv[++i];
        else if(strcmp(argv[i],"-h")==0||strcmp(argv[i],"--help")==0){
            printf("locscan [root] [--config file] [--json]\n"); return 0;
        }
        else if(argv[i][0]!='-') root=argv[i];
    }
    if(!config){
        if(access(".locscan.json",0)==0) config=".locscan.json";
        else if(access(".locscan.yaml",0)==0) config=".locscan.yaml";
    }
    char *text=NULL; JValue *cfg=NULL;
    if(config){
        FILE *f=fopen(config,"rb"); if(!f){ fprintf(stderr,"locscan: cannot open config %s\n",config); return 2; }
        fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET); text=malloc(n+1); fread(text,1,n,f); text[n]=0; fclose(f);
        char err[128];
        if(strstr(config,".yaml")||strstr(config,".yml")) cfg=yaml_parse(text,err,sizeof err);
        else cfg=json_parse(text,err,sizeof err);
        if(!cfg){ fprintf(stderr,"locscan: config parse error: %s\n",err); free(text); return 2; }
        load_config(cfg);
    } else {
        /* minimal default config */
        load_config((JValue*)&(JValue){J_OBJ});
    }
    walk(root,NULL);

    if(json_out){
        printf("{\"root\":\"%s\",\"config\":\"%s\",\"totals\":{", root, config?config:"(default)");
        long tf=0,tp=0,tb=0,tn=0;
        for(int i=0;i<g_ncat;i++){ tf+=g_cat[i].files; tp+=g_cat[i].phys; tb+=g_cat[i].blank; tn+=g_cat[i].nonblank; }
        printf("\"files\":%ld,\"physical\":%ld,\"blank\":%ld,\"nonblank\":%ld},", tf,tp,tb,tn);
        printf("\"categories\":[");
        for(int i=0;i<g_ncat;i++){
            printf("%s{\"category\":\"%s\",\"files\":%ld,\"physical\":%ld,\"blank\":%ld,\"nonblank\":%ld}",
                   i?",":"", g_cat[i].name,g_cat[i].files,g_cat[i].phys,g_cat[i].blank,g_cat[i].nonblank);
        }
        printf("],\"excluded_files\":%ld}\n", g_excluded_files);
    } else {
        printf("%-14s %8s %10s %8s %9s\n","Category","Files","Physical","Blank","Nonblank");
        long tf=0,tp=0,tb=0,tn=0;
        for(int i=0;i<g_ncat;i++){
            printf("%-14s %8ld %10ld %8ld %9ld\n", g_cat[i].name, g_cat[i].files, g_cat[i].phys, g_cat[i].blank, g_cat[i].nonblank);
            tf+=g_cat[i].files; tp+=g_cat[i].phys; tb+=g_cat[i].blank; tn+=g_cat[i].nonblank;
        }
        printf("%-14s %8ld %10ld %8ld %9ld\n","TOTAL", tf, tp, tb, tn);
        printf("Excluded files: %ld\n", g_excluded_files);
    }
    if(cfg) json_free(cfg);
    if(text) free(text);
    return 0;
}
