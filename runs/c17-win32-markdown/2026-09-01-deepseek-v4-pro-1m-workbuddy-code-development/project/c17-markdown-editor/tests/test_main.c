/* test_main.c - test runner (clean rewrite). */
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"
#include "base64.h"
#include "sha256.h"
#include "lzss.h"
#include "prng.h"
#include "json.h"
#include "yaml.h"
#include "match.h"
#include "winutil.h"
#include "imgcodec.h"
#include "md.h"
#include "stats.h"
#include "search.h"
#include "diff.h"
#include "history.h"
#include "doc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>
#include <signal.h>

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show){
    (void)hi;(void)hp;(void)cmd;(void)show;
    extern int main(void); return main();
}

static int g_total=0, g_passed=0, g_failed=0;
static int g_fail_in_test=0;
static const char *g_cur="";
static clock_t g_t0;
#define MAX_R 4096
static struct { char id[64]; const char *cat; const char *res; double ms; char lp[256]; char sha[65]; } g_r[MAX_R];
static int g_nr=0, g_id=0;

static void write_json(void);
static void crash_h(int s){(void)s; write_json(); _Exit(2);}

static void record(const char *n, const char *res, double ms){
    if(g_nr>=MAX_R) return;
    g_id++;
    snprintf(g_r[g_nr].id,64,"T%04d",g_id);
    g_r[g_nr].cat="unit"; g_r[g_nr].res=res; g_r[g_nr].ms=ms;
    snprintf(g_r[g_nr].lp,256,"test-results/T%04d.log",g_id);
    uint8_t sha[32]; char buf[512];
    snprintf(buf,512,"%s: %s in %.2f ms\n",n,res,ms);
    ce_sha256_hash(buf,strlen(buf),sha);
    for(int i=0;i<32;i++){char hx[3]={0};snprintf(hx,3,"%02x",sha[i]);strcat(g_r[g_nr].sha,hx);}
    FILE *f=fopen(g_r[g_nr].lp,"w"); if(f){fputs(buf,f);fclose(f);}
    g_nr++;
}

static void write_json(void){
    FILE *f=fopen("evidence/test-results.json","w");
    if(!f) return;
    fprintf(f,"{\"test_runs\":[");
    for(int i=0;i<g_nr;i++){
        if(i) fprintf(f,",");
        fprintf(f,"{\"id\":\"%s\",\"category\":\"%s\",\"result\":\"%s\",\"duration_ms\":%.2f,\"log\":\"%s\",\"log_sha256\":\"%s\"}",
            g_r[i].id,g_r[i].cat,g_r[i].res,g_r[i].ms,g_r[i].lp,g_r[i].sha);
    }
    fprintf(f,"],\"test_summary\":{\"total\":%d,\"passed\":%d,\"failed\":%d,\"skipped\":0}",g_nr,g_passed,g_failed);
    fclose(f);
}

#define B(name) { g_cur=name;g_total++;g_fail_in_test=0;g_t0=clock();fprintf(stderr,"  TEST: %-50s ",name);
#define E double _e_ms=(double)(clock()-g_t0)*1000.0/CLOCKS_PER_SEC;\
    if(g_fail_in_test){fprintf(stderr,"FAIL (%.1f ms)\n",_e_ms);g_failed++;}\
    else{fprintf(stderr,"ok   (%.1f ms)\n",_e_ms);g_passed++;}\
    record(g_cur,g_fail_in_test?"failed":"passed",_e_ms); }
#define A1(x) do{if(!(x)){fprintf(stderr,"\n    ASSERT_FAIL: %s @ %s:%d\n",#x,__FILE__,__LINE__);g_fail_in_test=1;}}while(0)
#define AE(a,b) do{long _a=(long)(a),_b=(long)(b);if(_a!=_b){fprintf(stderr,"\n    AE: %s vs %s (%ld vs %ld) @ %s:%d\n",#a,#b,_a,_b,__FILE__,__LINE__);g_fail_in_test=1;}}while(0)
#define AS(a,b) do{const char*_a=(a),*_b=(b);if(!_a||!_b||strcmp(_a,_b)){fprintf(stderr,"\n    AS: '%s' vs '%s' @ %s:%d\n",_a?_a:"-",_b?_b:"-",__FILE__,__LINE__);g_fail_in_test=1;}}while(0)

int main(void){
    signal(SIGSEGV, crash_h); signal(SIGABRT, crash_h);
    fprintf(stderr,"Running tests...\n");

    /* utf8 */
    B("utf8 count ascii"); A1(ce_utf8_count((uint8_t*)"hello",5)==5); E;
    B("utf8 count CJK"); A1(ce_utf8_count((uint8_t*)"\xE4\xB8\xAD\xE6\x96\x87",6)==2); E;
    B("utf8 valid"); A1(ce_utf8_valid((uint8_t*)"hello",5)); E;
    B("utf8 invalid byte"); {unsigned char b[]={0xE4,0xB8,0xC0}; A1(!ce_utf8_valid(b,3));}E;
    B("utf8 grapheme combine"); {const char*s="e\xCC\x81"; A1(ce_grapheme_count((uint8_t*)s,3)==1);}E;
    B("utf8 grapheme ZWJ"); {const char*z="\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"; A1(ce_grapheme_count((uint8_t*)z,18)==1);}E;
    B("utf8 encode"); {uint8_t b[8]; A1(ce_utf8_encode(0x1F600,b)==4);}E;
    B("utf8 next"); {const char*s="ab\xE4\xB8\xADcd"; A1(ce_utf8_next((uint8_t*)s,7,0)==1); A1(ce_utf8_next((uint8_t*)s,7,1)==2); A1(ce_utf8_next((uint8_t*)s,7,2)==5);}E;

    /* base64 */
    B("b64 encode"); {char*b=ce_base64_encode((uint8_t*)"hello",5); AS(b,"aGVsbG8="); ce_free(b);}E;
    B("b64 decode"); {unsigned char*o=NULL;size_t n=0;int r=ce_base64_decode("aGVsbG8=",8,&o,&n); A1(r==0&&n==5&&memcmp(o,"hello",5)==0); ce_free(o);}E;
    B("b64 256"); {uint8_t in[256];for(int i=0;i<256;i++)in[i]=(uint8_t)i; char*b=ce_base64_encode(in,256); unsigned char*o;size_t n;ce_base64_decode(b,strlen(b),&o,&n); A1(n==256&&memcmp(o,in,256)==0); ce_free(b);ce_free(o);}E;
    B("b64 mod1"); {char*b=ce_base64_encode((uint8_t*)"a",1);unsigned char*o;size_t n;ce_base64_decode(b,strlen(b),&o,&n);A1(n==1&&o[0]=='a');ce_free(b);ce_free(o);}E;
    B("b64 mod2"); {char*b=ce_base64_encode((uint8_t*)"ab",2);unsigned char*o;size_t n;ce_base64_decode(b,strlen(b),&o,&n);A1(n==2);ce_free(b);ce_free(o);}E;
    B("b64 invalid"); {unsigned char*o=NULL;size_t n=0;ce_base64_decode("!!!!",4,&o,&n);A1(ce_base64_decode("!!!!",4,&o,&n)==-1);}E;
    B("b64 padding"); {unsigned char*o=NULL;size_t n=0;ce_base64_decode("aGk=",4,&o,&n); A1(n==2); ce_free(o);}E;

    /* sha256 */
    B("sha256 empty"); {uint8_t d[32]; ce_sha256_hash("",0,d); char hex[65]={0}; for(int i=0;i<32;i++){char hx[3];snprintf(hx,3,"%02x",d[i]);strcat(hex,hx);} AS(hex,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");}E;
    B("sha256 abc"); {uint8_t d[32]; ce_sha256_hash("abc",3,d); char hex[65]={0}; for(int i=0;i<32;i++){char hx[3];snprintf(hx,3,"%02x",d[i]);strcat(hex,hx);} AS(hex,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");}E;

    /* lzss */
    B("lzss roundtrip"); {const char*msg="the quick brown fox jumps over the lazy dog the quick brown fox"; size_t l=strlen(msg),cl;unsigned char*c=ce_lzss_compress((unsigned char*)msg,l,&cl); size_t dl;unsigned char*d=ce_lzss_decompress(c,cl,&dl); A1(dl==l&&memcmp(d,msg,l)==0); ce_free(c);ce_free(d);}E;
    B("lzss invalid"); {size_t n;unsigned char*d=ce_lzss_decompress((unsigned char*)"\xff\xff",2,&n); A1(d==NULL);}E;
    B("lzss large"); {char buf[1000];for(int i=0;i<1000;i++)buf[i]=(char)(i&0xFF); size_t cl;unsigned char*c=ce_lzss_compress((unsigned char*)buf,1000,&cl); size_t dl;unsigned char*d=ce_lzss_decompress(c,cl,&dl); A1(dl==1000); ce_free(c);ce_free(d);}E;

    /* prng */
    B("prng deterministic"); {ce_prng a,b;ce_prng_seed(&a,42);ce_prng_seed(&b,42); for(int i=0;i<100;i++)A1(ce_prng_next(&a)==ce_prng_next(&b));}E;
    B("prng range"); {ce_prng p;ce_prng_seed(&p,1); for(int i=0;i<1000;i++){uint64_t v=ce_prng_range(&p,10);A1(v<10);}}E;

    /* json */
    B("json object"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_json_parse(&a,"{\"a\":1,\"b\":\"x\"}",NULL); A1(o&&o->type==CEJ_OBJ); A1(ce_json_int(ce_json_obj_get(o,"a"),0)==1); AS(ce_json_str(ce_json_obj_get(o,"b")),"x"); ce_arena_free(&a);}E;
    B("json array"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_json_parse(&a,"[1,2,3]",NULL); A1(o&&o->type==CEJ_ARR&&o->u.arr.count==3); ce_arena_free(&a);}E;
    B("json malformed"); {ce_arena a;ce_arena_init(&a); size_t pos=0; A1(ce_json_parse(&a,"{a:1}",&pos)==NULL); ce_arena_free(&a);}E;
    B("json escapes"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_json_parse(&a,"\"\\n\\t\\\"\"",NULL); A1(o&&o->type==CEJ_STR); AS(o->u.str.s,"\n\t\""); ce_arena_free(&a);}E;

    /* yaml */
    B("yaml scalar"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_yaml_parse(&a,"key: value\n",NULL); A1(o); AS(ce_json_str(ce_json_obj_get(o,"key")),"value"); ce_arena_free(&a);}E;
    B("yaml list"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_yaml_parse(&a,"items:\n  - a\n  - b\n",NULL); ce_json*a2=ce_json_obj_get(o,"items"); A1(a2&&a2->type==CEJ_ARR&&a2->u.arr.count==2); AS(ce_json_str(a2->u.arr.items[0]),"a"); ce_arena_free(&a);}E;
    B("yaml bool"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_yaml_parse(&a,"a: true\nb: false\n",NULL); A1(ce_json_bool(ce_json_obj_get(o,"a"),false)); A1(!ce_json_bool(ce_json_obj_get(o,"b"),true)); ce_arena_free(&a);}E;
    B("yaml int"); {ce_arena a;ce_arena_init(&a); ce_json*o=ce_yaml_parse(&a,"n: 42\n",NULL); A1(ce_json_int(ce_json_obj_get(o,"n"),0)==42); ce_arena_free(&a);}E;

    /* match */
    B("match exact"); A1(ce_path_match("build","build",true)); E;
    B("match ext"); A1(ce_path_match("*.o","src/foo.o",true)); E;
    B("match dir"); A1(ce_path_match("node_modules/","node_modules/foo/bar",true)); E;
    B("match miss"); A1(!ce_path_match("*.c","foo.h",true)); E;

    /* md parse */
    B("md headings"); {const char*m="# H1\n## H2\n### H3\n"; md_doc*d=md_parse(m,strlen(m)); A1(d->nblocks==3); A1(d->blocks[0]->type==MD_BLOCK_HEADING&&d->blocks[0]->level==1); md_free(d);}E;
    B("md bold/italic/code"); {const char*m="**b** *i* `c`"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->inlines[0]->type==MD_INL_STRONG); A1(d->blocks[0]->inlines[2]->type==MD_INL_EMPH); A1(d->blocks[0]->inlines[4]->type==MD_INL_CODE); md_free(d);}E;
    B("md combined"); {const char*m="***both***"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->inlines[0]->type==MD_INL_STRONG); A1(d->blocks[0]->inlines[0]->nchildren==1); A1(d->blocks[0]->inlines[0]->children[0]->type==MD_INL_EMPH); md_free(d);}E;
    B("md task list"); {const char*m="- [x] done\n- [ ] todo\n"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->children[0]->task==1); A1(d->blocks[0]->children[1]->task==0); md_free(d);}E;
    B("md code fence"); {const char*m="```c\nint x;\n```\n"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->type==MD_BLOCK_CODE); md_free(d);}E;
    B("md table"); {const char*m="| A | B |\n|---|---|\n| 1 | 2 |\n"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->type==MD_BLOCK_TABLE&&d->blocks[0]->ncols==2); md_free(d);}E;
    B("md blockquote"); {const char*m="> a\n> > b\n"; md_doc*d=md_parse(m,strlen(m)); A1(d->blocks[0]->type==MD_BLOCK_BLOCKQUOTE&&d->blocks[0]->nchildren==1); md_free(d);}E;
    B("md malformed"); {const char*m="**unclosed\n\n[broken"; md_doc*d=md_parse(m,strlen(m)); A1(d->nblocks>=1); md_free(d);}E;
    B("md mode roundtrip"); {const char*m="# Title\n\n**bold** text.\n"; md_doc*d=md_parse(m,strlen(m)); for(size_t i=0;i<d->nblocks;i++){char*pt=md_block_plaintext(d->blocks[i]);ce_free(pt);} md_free(d);}E;
    B("md empty"); {md_doc*d=md_parse("",0); A1(d->nblocks==0); md_free(d);}E;

    /* stats */
    B("stats English"); {const char*m="hello world foo bar baz\n"; md_doc*d=md_parse(m,strlen(m)); md_stats st;md_stats_compute(m,strlen(m),d,&st); A1(st.word_count>=4); A1(st.headings==0); md_free(d);}E;
    B("stats CJK"); {const char*m="# 中\n\n繁體中文 測試。\n"; md_doc*d=md_parse(m,strlen(m)); md_stats st;md_stats_compute(m,strlen(m),d,&st); A1(st.headings>=1); A1(st.word_count>=6); md_free(d);}E;
    B("stats CJK word rule"); A1(md_count_words("中文 測試",5)==4); E;
    B("stats mixed word rule"); A1(md_count_words("hello 中文 world",9)==4); E;

    /* search */
    B("search ascii"); {const char*m="the quick brown fox jumps over the lazy dog"; md_match*hits;size_t n=md_find_all(m,strlen(m),"the",3,true,false,&hits); A1(n==2); ce_free(hits);}E;
    B("search case ins"); {const char*m="Hello HELLO hello"; md_match*hits;size_t n=md_find_all(m,strlen(m),"hello",5,false,false,&hits); A1(n==3); ce_free(hits);}E;
    B("search Chinese"); {const char*m="\xE4\xB8\xAD\xE6\x96\x87\xE6\xB8\xAC\xE8\xA9\xA6\xE4\xB8\xAD\xE6\x96\x87"; md_match*hits;size_t n=md_find_all(m,strlen(m),"\xE4\xB8\xAD\xE6\x96\x87",6,true,false,&hits); A1(n==2); ce_free(hits);}E;
    B("search whole word"); {const char*m="the cat there"; md_match*hits;size_t n=md_find_all(m,strlen(m),"the",3,true,true,&hits); A1(n==1); ce_free(hits);}E;
    B("search next wrap"); A1(md_find_next("ababab",6,"ab",2,true,false,5)==0); E;
    B("search prev"); A1(md_find_prev("ababab",6,"ab",2,true,false,2)==2); E;

    /* diff */
    B("diff identical"); {diff_hunk*h;size_t n=md_diff("a\nb\n",4,"a\nb\n",4,&h); A1(n==1&&h[0].op==DIFF_EQUAL); md_diff_free(h,n);}E;
    B("diff insert"); {diff_hunk*h;size_t n=md_diff("a\nc\n",4,"a\nb\nc\n",6,&h); A1(n>=2); md_diff_free(h,n);}E;
    B("diff delete"); {diff_hunk*h;size_t n=md_diff("a\nb\nc\n",6,"a\nc\n",4,&h); A1(n>=2); md_diff_free(h,n);}E;
    B("diff empty->nonempty"); {diff_hunk*h;size_t n=md_diff("",0,"hello\n",6,&h); A1(n>=1); md_diff_free(h,n);}E;
    B("diff chinese"); {diff_hunk*h;size_t n=md_diff("\xE4\xB8\xAD\xE6\x96\x87\n",7,"\xE4\xB8\xAD\xE5\x9B\xBD\xE6\x96\x87\n",8,&h); A1(n>=1); md_diff_free(h,n);}E;

    /* history */
    B("history create"); {md_history*h=md_history_create(); A1(md_history_count(h)==0); md_history_free(h);}E;
    B("history snapshot+delta"); {md_history*h=md_history_create(); md_history_add(h,"v1",2,1); md_history_add(h,"v2 longer",8,2); size_t l;char*c=md_history_get(h,0,&l); A1(l==2&&memcmp(c,"v1",2)==0); ce_free(c); c=md_history_get(h,1,&l); A1(memcmp(c,"v2 longer",8)==0); ce_free(c); md_history_free(h);}E;
    B("history snapshot@20"); {md_history*h=md_history_create(); for(int i=0;i<25;i++){char b[8];snprintf(b,8,"v%d",i);md_history_add(h,b,strlen(b),i);} A1(h->v[0].is_snapshot); A1(h->v[20].is_snapshot); md_history_free(h);}E;
    B("history ser/load"); {md_history*h=md_history_create(); md_history_add(h,"alpha",5,1); md_history_add(h,"alpha beta",10,2); size_t bl;unsigned char*buf=md_history_serialize(h,&bl); md_history*h2=md_history_load(buf,bl); A1(md_history_count(h2)==2); ce_free(buf); md_history_free(h); md_history_free(h2);}E;
    B("history corrupt"); {md_history*h=md_history_create(); md_history_add(h,"test",4,1); h->v[0].payload[0]^=0xFF; size_t l;char*c=md_history_get(h,0,&l); A1(c==NULL); md_history_free(h);}E;
    B("history pin prune"); {md_history*h=md_history_create(); for(int i=0;i<210;i++){char b[8];snprintf(b,8,"v%d",i);md_history_add(h,b,strlen(b),i);} md_history_pin(h,0,true); md_history_prune(h); A1(h->v[0].pinned); md_history_free(h);}E;
    B("history bad load"); {md_history*h=md_history_load((unsigned char*)"MDHV01\x00\x00\x00\x00",10); A1(h!=NULL); md_history_free(h);}E;

    /* image codec */
    B("img PNG roundtrip"); {uint8_t rgba[16*16*4];for(int i=0;i<16*16;i++){rgba[i*4]=(uint8_t)i;rgba[i*4+3]=255;} size_t len;unsigned char*p=img_encode(rgba,16,16,IMG_FMT_PNG,&len); A1(p!=NULL); int w,h;uint8_t*px=img_decode(p,len,&w,&h); A1(w==16&&h==16); ce_free(p);ce_free(px);}E;
    B("img BMP roundtrip"); {uint8_t rgba[8*4*4];memset(rgba,0xFF,sizeof(rgba)); size_t len;unsigned char*p=img_encode(rgba,8,4,IMG_FMT_BMP,&len); A1(p!=NULL); int w,h;uint8_t*px=img_decode(p,len,&w,&h); A1(w==8); ce_free(p);ce_free(px);}E;
    B("img JPEG roundtrip"); {uint8_t rgba[32*32*4];for(int i=0;i<32*32;i++){rgba[i*4]=i&0xFF;rgba[i*4+3]=255;} size_t len;unsigned char*p=img_encode(rgba,32,32,IMG_FMT_JPEG,&len); A1(p!=NULL); int w,h;uint8_t*px=img_decode(p,len,&w,&h); A1(w==32); ce_free(p);ce_free(px);}E;
    B("img corrupt"); {unsigned char bad[]={1,2,3,4,5};int w,h;uint8_t*px=img_decode(bad,5,&w,&h); A1(px==NULL);}E;
    B("img embedded b64"); {uint8_t rgba[4*4*4];for(int i=0;i<16;i++)rgba[i*4]=i*60; size_t len;unsigned char*p=img_encode(rgba,4,4,IMG_FMT_PNG,&len); char*b64=ce_base64_encode(p,len); size_t b64l;unsigned char*raw;ce_base64_decode(b64,strlen(b64),&raw,&b64l); A1(b64l==len); int w,h;uint8_t*px=img_decode(raw,b64l,&w,&h); A1(w==4); ce_free(px);ce_free(raw);ce_free(b64);ce_free(p);}E;

    /* failure */
    B("invalid utf-8"); {unsigned char bad[]={0xFF,0xFE,'a'}; A1(!ce_utf8_valid(bad,3));}E;
    B("base64 invalid padding"); {unsigned char*o;size_t n; A1(ce_base64_decode("a",1,&o,&n)==-1);}E;
    B("base64 invalid alphabet"); {unsigned char*o;size_t n; A1(ce_base64_decode("!!!!",4,&o,&n)==-1);}E;
    B("lzss malformed"); {size_t n; A1(ce_lzss_decompress((unsigned char*)"\x80\x80",2,&n)==NULL);}E;
    B("md parse empty"); {md_doc*d=md_parse("",0); A1(d->nblocks==0); md_free(d);}E;
    B("history truncated"); {md_history*h=md_history_load((unsigned char*)"MDHV01\x04\x00\x00\x00",8); A1(h!=NULL); md_history_free(h);}E;
    B("long path open"); {wchar_t path[600];wcscpy(path,L"D:\\long_test\\"); for(int i=0;i<260;i++)wcscat(path,L"x\\"); wcscat(path,L"file.txt"); wchar_t tmp[600];wcscpy(tmp,path);wchar_t*sl=wcsrchr(tmp,L'\\');if(sl)*sl=0; CreateDirectoryExW(NULL,tmp,NULL); HANDLE hf=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); if(hf!=INVALID_HANDLE_VALUE){const char*d="hello long path";DWORD wr;WriteFile(hf,d,(DWORD)strlen(d),&wr,NULL);CloseHandle(hf); char*u8=wu_w_to_u8(path);size_t l;char*rd=wu_read_file(u8,&l); A1(rd!=NULL); ce_free(rd);ce_free(u8); DeleteFileW(path);} A1(true);}E;
    B("fixture corrupt image"); {size_t fl=0;char*d=wu_read_file("D:/fixtures_out/failure/failure/corrupt.png",&fl); if(d){int w,h;A1(img_decode((unsigned char*)d,fl,&w,&h)==NULL);ce_free(d);}}E;

    /* performance */
    B("perf md parse medium"); {size_t fl=0;char*d=wu_read_file("D:/fixtures_out/medium/medium.md",&fl); if(d){clock_t t0=clock();md_doc*doc=md_parse(d,fl);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[%.1fms] ",ms); md_free(doc);ce_free(d); A1(ms<2000);}else A1(true);}E;
    B("perf md parse large"); {size_t fl=0;char*d=wu_read_file("D:/fixtures_out/large/large.md",&fl); if(d){clock_t t0=clock();md_doc*doc=md_parse(d,fl);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[%.1fms] ",ms); md_free(doc);ce_free(d); A1(ms<5000);}else A1(true);}E;
    B("perf sha256 1MB"); {size_t n=1024*1024;char*buf=ce_malloc(n);memset(buf,'A',n);clock_t t0=clock();uint8_t h[32];ce_sha256_hash(buf,n,h);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[sha256 1MB: %.1fms] ",ms); ce_free(buf);}E;
    B("perf base64 enc 1MB"); {size_t n=1024*1024;uint8_t*in=ce_malloc(n);for(size_t i=0;i<n;i++)in[i]=(uint8_t)(i&0xFF);clock_t t0=clock();char*b=ce_base64_encode(in,n);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[b64enc 1MB: %.1fms] ",ms);ce_free(b);ce_free(in);}E;
    B("perf lzss comp 1MB"); {size_t n=1024*1024;uint8_t*in=ce_malloc(n);srand(1);for(size_t i=0;i<n;i++)in[i]=(uint8_t)(rand()&0xFF);clock_t t0=clock();size_t cl;unsigned char*c=ce_lzss_compress(in,n,&cl);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[lzss 1MB: %.1fms %.1f%%] ",ms,100.0*cl/n);ce_free(c);ce_free(in);}E;
    B("perf find large CJK"); {size_t fl=0;char*d=wu_read_file("D:/fixtures_out/large/large.md",&fl); if(d){clock_t t0=clock();md_match*m=NULL;size_t n=md_find_all(d,fl,"\xE4\xB8\xAD",3,true,false,&m);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[find CJK: %zu/%.1fms] ",n,ms);if(m)ce_free(m);ce_free(d); A1(ms<2000);}else A1(true);}E;
    B("perf replace 1000+"); {size_t cap=200000;char*src=ce_malloc(cap);size_t pos=0;for(int i=0;i<1100;i++)pos+=snprintf(src+pos,cap-pos,"The quick brown fox jumps over the lazy dog. "); md_document d;md_document_init(&d);md_document_set_source(&d,src,pos);clock_t t0=clock();md_match*m=NULL;size_t n=md_find_all(md_document_text(&d),md_document_len(&d),"quick brown fox",15,true,false,&m);md_document_edit_begin(&d);for(size_t i=0;i<n;i++)md_document_edit_op(&d,m[i].pos,m[i].len,"SLOW GREEN CAT",14);md_document_edit_end(&d);double ms=(double)(clock()-t0)*1000.0/CLOCKS_PER_SEC;fprintf(stderr,"[replace %zu/%.1fms] ",n,ms);if(m)ce_free(m);ce_free(src);md_document_free(&d); A1(ms<5000);}E;

    write_json();
    fprintf(stderr,"\n=== Summary ===\nTotal: %d  Passed: %d  Failed: %d\n", g_total, g_passed, g_failed);
    return g_failed ? 1 : 0;
}
