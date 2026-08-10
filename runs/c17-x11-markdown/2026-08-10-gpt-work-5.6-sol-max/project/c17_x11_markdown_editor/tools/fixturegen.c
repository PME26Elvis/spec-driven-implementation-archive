#include "mdedit/core.h"
#include "mdedit/image.h"
#include "mdedit/json.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { EXIT_USAGE=2,EXIT_CONFIG=3,EXIT_INPUT=4,EXIT_OUTPUT=5,EXIT_VERIFY=6,EXIT_INTERNAL=7 };

typedef struct { char *path; char *role; } GenFile;
typedef struct { GenFile *items; size_t len; size_t cap; } GenFiles;

static const char *profiles[]={"small","unicode","markdown-all","workspace","medium","large","stress-long-line","failure"};

static void generated_free(GenFiles *files) {
    for (size_t i=0U;i<files->len;++i) { free(files->items[i].path); free(files->items[i].role); }
    free(files->items); memset(files,0,sizeof(*files));
}

static bool generated_add(GenFiles *files,const char *path,const char *role) {
    if (files->len==files->cap) {
        size_t next=files->cap==0U?32U:files->cap*2U; GenFile *items=realloc(files->items,next*sizeof(*items));
        if (items==NULL) return false;
        files->items=items;
        files->cap=next;
    }
    files->items[files->len].path=md_strdup(path); files->items[files->len].role=md_strdup(role);
    if (files->items[files->len].path==NULL||files->items[files->len].role==NULL) return false;
    ++files->len; return true;
}

static int generated_compare(const void *left,const void *right) {
    return strcmp(((const GenFile *)left)->path,((const GenFile *)right)->path);
}

static bool write_generated(const char *root,const char *relative,const void *data,size_t len,
                            const char *role,GenFiles *files) {
    char normalized[MD_PATH_MAX],path[MD_PATH_MAX],dir[MD_PATH_MAX],error[256];
    if (!md_path_normalize_relative(relative,normalized)||!md_path_join(path,root,normalized)||
        !md_path_dirname(dir,path)||!md_mkdirs(dir,0755,error,sizeof(error))) {
        fprintf(stderr,"fixturegen: cannot prepare %s: %s\n",relative,error); return false;
    }
    if (!md_write_file_atomic(path,data,len,error,sizeof(error))) { fprintf(stderr,"fixturegen: %s\n",error); return false; }
    return generated_add(files,normalized,role);
}

static void bmp_u16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); }
static void bmp_u32(uint8_t *p,uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); p[2]=(uint8_t)(v>>16U); p[3]=(uint8_t)(v>>24U); }

static bool make_bmp(uint8_t red,uint8_t green,uint8_t blue,MdBytes *out) {
    uint8_t bmp[70]; memset(bmp,0,sizeof(bmp)); bmp[0]='B'; bmp[1]='M'; bmp_u32(bmp+2U,sizeof(bmp));
    bmp_u32(bmp+10U,54U); bmp_u32(bmp+14U,40U); bmp_u32(bmp+18U,2U); bmp_u32(bmp+22U,2U);
    bmp_u16(bmp+26U,1U); bmp_u16(bmp+28U,24U); bmp_u32(bmp+34U,16U);
    for (size_t y=0U;y<2U;++y) for (size_t x=0U;x<2U;++x) {
        size_t at=54U+y*8U+x*3U; bmp[at]=blue; bmp[at+1U]=green; bmp[at+2U]=red;
    }
    return md_bytes_append(out,bmp,sizeof(bmp));
}

static bool write_bmp_asset(const char *root,const char *relative,uint8_t r,uint8_t g,uint8_t b,GenFiles *files) {
    MdBytes bmp; md_bytes_init(&bmp); bool ok=make_bmp(r,g,b,&bmp)&&write_generated(root,relative,bmp.data,bmp.len,"image-asset",files);
    md_bytes_free(&bmp); return ok;
}

static bool write_codec_asset(const char *root,const char *relative,bool jpeg,GenFiles *files) {
    char normalized[MD_PATH_MAX],path[MD_PATH_MAX],dir[MD_PATH_MAX],error[256];
    if (!md_path_normalize_relative(relative,normalized)||!md_path_join(path,root,normalized)||
        !md_path_dirname(dir,path)||!md_mkdirs(dir,0755,error,sizeof(error))) return false;
    uint8_t rgba[8U*8U*4U];
    for (size_t y=0U;y<8U;++y) for (size_t x=0U;x<8U;++x) {
        size_t at=(y*8U+x)*4U; rgba[at]=(uint8_t)(32U+x*27U); rgba[at+1U]=(uint8_t)(48U+y*23U);
        rgba[at+2U]=(uint8_t)(220U-(x+y)*9U); rgba[at+3U]=255U;
    }
    bool ok=jpeg?md_image_write_jpeg(path,rgba,8U,8U,90,error,sizeof(error)):
                 md_image_write_png(path,rgba,8U,8U,error,sizeof(error));
    if (!ok) { fprintf(stderr,"fixturegen: %s\n",error); return false; }
    return generated_add(files,normalized,"image-asset");
}

static bool profile_small(const char *root,uint64_t seed,GenFiles *files) {
    (void)seed; MdBytes bmp; md_bytes_init(&bmp);
    static const char guide[]="# Guide\n\nA compact nested document.\n";
    static const char checklist[]="# Checklist\n\n- [x] generated\n";
    if (!make_bmp(67U,120U,245U,&bmp)) return false;
    MdBuf embedded; md_buf_init(&embedded);
    if (!md_image_make_data_uri(MD_IMAGE_BMP,bmp.data,bmp.len,&embedded)) { md_bytes_free(&bmp); return false; }
    MdBuf readme; md_buf_init(&readme);
    bool ok=md_buf_appendf(&readme,"# Small Fixture\n\n繁體中文與 English 123。\n\n- first\n- [ ] task\n\n[link](notes/guide.md) · [external](https://example.com)\n\n```c\nint main(void) { return 0; }\n```\n\n| Name | Value |\n| :--- | ---: |\n| alpha | 1 |\n\n![BMP](assets/blue.bmp)\n\n![PNG](assets/gradient.png)\n\n![JPEG](assets/gradient.jpg)\n\n![embedded](%s)\n",embedded.data)&&
        write_generated(root,"README.md",readme.data,readme.len,"markdown",files)&&
        write_generated(root,"notes/guide.md",guide,sizeof(guide)-1U,"markdown",files)&&
        write_generated(root,"notes/checklist.md",checklist,sizeof(checklist)-1U,"markdown",files)&&
        write_generated(root,"assets/blue.bmp",bmp.data,bmp.len,"image-asset",files)&&
        write_codec_asset(root,"assets/gradient.png",false,files)&&
        write_codec_asset(root,"assets/gradient.jpg",true,files);
    md_buf_free(&readme); md_buf_free(&embedded); md_bytes_free(&bmp); return ok;
}

static bool profile_unicode(const char *root,uint64_t seed,GenFiles *files) {
    (void)seed;
    static const char text[]=
        "# Unicode 正規測試\n\n"
        "繁體中文段落：搜尋短語會重複出現。搜尋短語。\n\n"
        "English words and numbers 12345. Café naïve coöperate.\n\n"
        "混合 Chinese/English 2026：搜尋短語。\n\n"
        "Combining: e\xCC\x81 a\xCC\x88 n\xCC\x83\n\n"
        "Variation: \xE2\x9C\x88\xEF\xB8\x8F\n\n"
        "ZWJ family: \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6\n"
        "ZWJ technologist: \xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB\n";
    return write_generated(root,"中文 路徑/Unicode 測試.md",text,sizeof(text)-1U,"unicode-markdown",files)&&
           write_generated(root,"path with spaces/搜尋 #1.md",text,sizeof(text)-1U,"unicode-markdown",files);
}

static bool profile_markdown_all(const char *root,uint64_t seed,GenFiles *files) {
    (void)seed;
    static const char text[]=
        "# H1\n## H2\n### H3\n#### H4\n##### H5\n###### H6\n\n"
        "Setext H1\n=========\n\nSetext H2\n---------\n\n---\n\n"
        "Paragraph with *emphasis*, **strong**, ***both***, ~~strike~~, `inline *code*`,  \n"
        "hard break and [link](https://example.com \"title\") plus <https://example.com>.\n\n"
        "> quote\n>> nested quote\n\n- unordered\n  - nested\n1. ordered\n10. multi digit\n- [ ] open task\n- [x] done task\n\n"
        "    indented code *literal*\n\n```c\n# not a heading\n```\n\n~~~text\n* literal *\n~~~\n\n"
        "| left | center | right | default |\n| :--- | :---: | ---: | --- |\n| **bold** | `a|b` | escaped \\| | ![alt](asset.bmp) |\n\n"
        "<span>preserved raw HTML</span>\n\n![broken](missing.png)\n\n"
        "Incomplete **emphasis\n[broken link](\n```unterminated\n";
    return write_generated(root,"markdown-all.md",text,sizeof(text)-1U,"markdown-corpus",files)&&
           write_bmp_asset(root,"asset.bmp",240U,160U,40U,files);
}

static bool profile_workspace(const char *root,uint64_t seed,GenFiles *files) {
    (void)seed;
    if (!write_bmp_asset(root,"assets/workspace.bmp",20U,180U,100U,files)) return false;
    for (unsigned i=0U;i<20U;++i) {
        char path[MD_PATH_MAX];
        if (i==0U) strcpy(path,"level one/level two/level three/level four/deep.md");
        else if (i==1U) strcpy(path,"duplicate/a/note.md");
        else if (i==2U) strcpy(path,"duplicate/b/note.md");
        else if (i==3U) strcpy(path,"包含 空格/繁體中文.md");
        else (void)snprintf(path,sizeof(path),"documents/group-%u/document-%02u.md",i%4U,i);
        MdBuf text; md_buf_init(&text);
        bool ok=md_buf_appendf(&text,"# Workspace Document %u\n\nRelative [next](../document-%02u.md).\n\n![asset](%s)\n",
            i,(i+1U)%20U,i%2U==0U?"../../assets/workspace.bmp":"./missing-relative.bmp")&&
            write_generated(root,path,text.data,text.len,"workspace-markdown",files);
        md_buf_free(&text); if (!ok) return false;
    }
    return true;
}

static bool profile_performance(const char *root,uint64_t seed,bool large,GenFiles *files) {
    size_t total_lines=large?50000U:10000U;
    size_t heading_count=large?1000U:300U,list_count=large?2000U:300U;
    size_t table_count=large?100U:30U,image_count=large?50U:20U,unique_assets=large?20U:20U;
    for (size_t i=0U;i<unique_assets;++i) {
        char path[128]; (void)snprintf(path,sizeof(path),"assets/generated-%02zu.bmp",i);
        if (!write_bmp_asset(root,path,(uint8_t)(20U+i*7U),(uint8_t)(80U+i*5U),(uint8_t)(180U-i*3U),files)) return false;
    }
    MdPrng prng; md_prng_seed(&prng,seed); MdBuf doc; md_buf_init(&doc); size_t line=0U;
    for (size_t i=0U;i<heading_count&&line<total_lines;++i,++line)
        if (!md_buf_appendf(&doc,"## Heading %zu — 效能標題 %llu\n",i,(unsigned long long)(md_prng_next(&prng)%100000U))) goto oom;
    for (size_t i=0U;i<list_count&&line<total_lines;++i,++line)
        if (!md_buf_appendf(&doc,"- List item %zu：繁體中文與 English token_%zu 搜尋短語\n",i,i)) goto oom;
    for (size_t t=0U;t<table_count&&line+3U<=total_lines;++t) {
        if (!md_buf_appendf(&doc,"| Table %zu | Value | 狀態 |\n| :--- | ---: | :---: |\n| row | %zu | 完成 |\n",t,t)) goto oom;
        line+=3U;
    }
    for (size_t i=0U;i<image_count&&line<total_lines;++i,++line)
        if (!md_buf_appendf(&doc,"![performance image %zu](assets/generated-%02zu.bmp)\n",i,i%unique_assets)) goto oom;
    if (line+4U<total_lines) {
        if (!md_buf_append_cstr(&doc,"```text\nMarkdown-looking **code** [link](x) 繁體中文\nsecond code line\n```\n")) goto oom;
        line+=4U;
    }
    while (line<total_lines) {
        uint64_t value=md_prng_next(&prng);
        if (!md_buf_appendf(&doc,"Paragraph %zu: deterministic %016llx — 繁體中文 mixed English accented café combining e\xCC\x81 emoji \xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB 搜尋短語 searchable_token. Additional text keeps the fixture realistically wide and exceeds its byte-size gate.\n",
            line,(unsigned long long)value)) goto oom;
        ++line;
    }
    {
        const size_t minimum=large?5U*1024U*1024U:1024U*1024U;
        if (doc.len<minimum) { fprintf(stderr,"fixturegen: internal fixture size gate missed\n"); md_buf_free(&doc); return false; }
        bool ok=write_generated(root,large?"large.md":"medium.md",doc.data,doc.len,large?"large-performance":"medium-performance",files);
        md_buf_free(&doc); return ok;
    }
oom:
    md_buf_free(&doc); return false;
}

static bool profile_stress(const char *root,GenFiles *files) {
    size_t len=1024U*1024U; char *line=malloc(len); if (line==NULL) return false;
    static const char pattern[]="LongLine繁體中文0123456789_"; size_t p_len=sizeof(pattern)-1U;
    size_t at=0U;
    while (at+p_len<=len) { memcpy(line+at,pattern,p_len); at+=p_len; }
    while (at<len) line[at++]='x';
    bool ok=write_generated(root,"stress-long-line.md",line,len,"stress-long-line",files); free(line); return ok;
}

static bool profile_failure(const char *root,GenFiles *files) {
    static const uint8_t invalid_utf8[]={0x66U,0x6fU,0x80U,0x6fU};
    static const char malformed[]="**unterminated\n```\nfence\n[link](\n| incomplete |\n";
    static const uint8_t corrupt_png[]={137U,80U,78U,71U,13U,10U,26U,10U,0U};
    static const uint8_t corrupt_jpeg[]={0xffU,0xd8U,0xffU,0xe0U,0U};
    static const uint8_t corrupt_bmp[]={'B','M',0U,0U};
    static const char missing_image[]="![missing](does-not-exist.png)\n";
    static const char truncated_session[]="{\"tabs\":[";
    static const char special_path[]="safe path content\n";
    static const char read_only_source[]="read only\n";
    bool ok=write_generated(root,"invalid-utf8.md",invalid_utf8,sizeof(invalid_utf8),"invalid-utf8",files)&&
        write_generated(root,"malformed.md",malformed,sizeof(malformed)-1U,"malformed-markdown",files)&&
        write_generated(root,"missing-image.md",missing_image,sizeof(missing_image)-1U,"missing-image",files)&&
        write_generated(root,"corrupt.png",corrupt_png,sizeof(corrupt_png),"corrupt-image",files)&&
        write_generated(root,"corrupt.jpg",corrupt_jpeg,sizeof(corrupt_jpeg),"corrupt-image",files)&&
        write_generated(root,"corrupt.bmp",corrupt_bmp,sizeof(corrupt_bmp),"corrupt-image",files)&&
        write_generated(root,"truncated-session.json",truncated_session,sizeof(truncated_session)-1U,"corrupt-state",files)&&
        write_generated(root,"truncated-history.mhv","MDH1",4U,"corrupt-history",files)&&
        write_generated(root,"truncated-recovery.mrec","MDR1",4U,"corrupt-recovery",files)&&
        write_generated(root,"special path/繁體 #$& file.md",special_path,sizeof(special_path)-1U,"special-path",files);
    if (!ok) return false;
    char read_only[MD_PATH_MAX]; if (!md_path_join(read_only,root,"read-only.md")) return false;
    if (!write_generated(root,"read-only.md",read_only_source,sizeof(read_only_source)-1U,"read-only",files)) return false;
    (void)chmod(read_only,0444);
    char a[MD_PATH_MAX],b[MD_PATH_MAX],error[128];
    if (!md_path_join(a,root,"symlink-cycle/a")||!md_path_join(b,root,"symlink-cycle/a/back")||
        !md_mkdirs(a,0755,error,sizeof(error))) return false;
    if (symlink("..",b)!=0&&errno!=EEXIST) return false;
    return true;
}

static bool write_manifest(const char *root,const char *profile,uint64_t seed,GenFiles *files) {
    if (files->len>1U) qsort(files->items,files->len,sizeof(*files->items),generated_compare);
    MdBuf json; md_buf_init(&json);
    if (!md_buf_appendf(&json,"{\n  \"schema_version\": 1,\n  \"generator_version\": \"1.0.0\",\n  \"profile\": \"%s\",\n  \"seed\": %llu,\n  \"files\": [\n",profile,(unsigned long long)seed)) goto oom;
    for (size_t i=0U;i<files->len;++i) {
        char path[MD_PATH_MAX]; if (!md_path_join(path,root,files->items[i].path)) goto oom;
        MdFileDigest digest; char error[256]; if (!md_file_digest(path,&digest,error,sizeof(error))) { fprintf(stderr,"fixturegen: %s\n",error); md_buf_free(&json); return false; }
        char hex[65]; md_hex_encode(digest.sha256,32U,hex);
        if (!md_buf_append_cstr(&json,"    {\"path\": ")||!md_json_write_escaped(&json,files->items[i].path,strlen(files->items[i].path))||
            !md_buf_append_cstr(&json,", \"role\": ")||!md_json_write_escaped(&json,files->items[i].role,strlen(files->items[i].role))||
            !md_buf_appendf(&json,", \"size\": %llu, \"sha256\": \"%s\"}%s\n",
                (unsigned long long)digest.size,hex,i+1U<files->len?",":"")) goto oom;
    }
    if (!md_buf_append_cstr(&json,"  ]\n}\n")) goto oom;
    char path[MD_PATH_MAX],error[256]; if (!md_path_join(path,root,"fixture-manifest.json")) goto oom;
    {
        bool ok=md_write_file_atomic(path,json.data,json.len,error,sizeof(error));
        if (!ok) fprintf(stderr,"fixturegen: %s\n",error);
        md_buf_free(&json);
        return ok;
    }
oom:
    md_buf_free(&json); fprintf(stderr,"fixturegen: out of memory writing manifest\n"); return false;
}

static bool generate(const char *profile,const char *root,uint64_t seed) {
    char error[256]; if (!md_mkdirs(root,0755,error,sizeof(error))) { fprintf(stderr,"fixturegen: %s\n",error); return false; }
    GenFiles files={0}; bool ok=false;
    if (strcmp(profile,"small")==0) ok=profile_small(root,seed,&files);
    else if (strcmp(profile,"unicode")==0) ok=profile_unicode(root,seed,&files);
    else if (strcmp(profile,"markdown-all")==0) ok=profile_markdown_all(root,seed,&files);
    else if (strcmp(profile,"workspace")==0) ok=profile_workspace(root,seed,&files);
    else if (strcmp(profile,"medium")==0) ok=profile_performance(root,seed,false,&files);
    else if (strcmp(profile,"large")==0) ok=profile_performance(root,seed,true,&files);
    else if (strcmp(profile,"stress-long-line")==0) ok=profile_stress(root,&files);
    else if (strcmp(profile,"failure")==0) ok=profile_failure(root,&files);
    if (ok) ok=write_manifest(root,profile,seed,&files);
    if (ok) printf("fixturegen PASS profile=%s seed=%llu files=%zu\n",profile,(unsigned long long)seed,files.len);
    generated_free(&files); return ok;
}

static bool safe_manifest_path(const char *path) {
    char normalized[MD_PATH_MAX]; return path[0]!='/'&&md_path_normalize_relative(path,normalized)&&strcmp(normalized,path)==0;
}

static int verify(const char *root) {
    char manifest_path[MD_PATH_MAX]; if (!md_path_join(manifest_path,root,"fixture-manifest.json")) return EXIT_INPUT;
    MdBytes bytes; md_bytes_init(&bytes); char error[256];
    if (!md_read_file(manifest_path,&bytes,error,sizeof(error))) { fprintf(stderr,"fixturegen: %s\n",error); return EXIT_INPUT; }
    MdJsonError parse; MdJson *manifest=md_json_parse((const char *)bytes.data,bytes.len,&parse); md_bytes_free(&bytes);
    if (manifest==NULL) { fprintf(stderr,"fixturegen: invalid manifest at %zu:%zu: %s\n",parse.line,parse.column,parse.message); return EXIT_CONFIG; }
    const MdJson *entries=md_json_get(manifest,"files");
    if (manifest->type!=MD_JSON_OBJECT||entries==NULL||entries->type!=MD_JSON_ARRAY) { md_json_free(manifest); return EXIT_CONFIG; }
    size_t checked=0U;
    for (size_t i=0U;i<entries->as.array.len;++i) {
        const MdJson *entry=entries->as.array.items[i]; const char *relative=md_json_string(md_json_get(entry,"path"));
        const char *expected_sha=md_json_string(md_json_get(entry,"sha256")); uint64_t expected_size=0U;
        if (entry->type!=MD_JSON_OBJECT||relative==NULL||expected_sha==NULL||strlen(expected_sha)!=64U||
            !md_json_u64(md_json_get(entry,"size"),&expected_size)||!safe_manifest_path(relative)) {
            fprintf(stderr,"fixturegen: invalid manifest file entry %zu\n",i); md_json_free(manifest); return EXIT_CONFIG;
        }
        char path[MD_PATH_MAX]; if (!md_path_join(path,root,relative)) { md_json_free(manifest); return EXIT_CONFIG; }
        MdFileDigest digest; if (!md_file_digest(path,&digest,error,sizeof(error))) { fprintf(stderr,"fixturegen: %s\n",error); md_json_free(manifest); return EXIT_VERIFY; }
        char actual[65]; md_hex_encode(digest.sha256,32U,actual);
        if (digest.size!=expected_size||strcmp(actual,expected_sha)!=0) {
            fprintf(stderr,"fixturegen: mismatch %s\n",relative); md_json_free(manifest); return EXIT_VERIFY;
        }
        ++checked;
    }
    printf("fixturegen verify PASS files=%zu\n",checked); md_json_free(manifest); return 0;
}

static void usage(void) {
    fputs("usage: fixturegen --profile NAME --output DIR [--seed UINT64]\n       fixturegen --verify DIR\n       fixturegen --list-profiles\n",stderr);
}

int main(int argc,char **argv) {
    const char *profile=NULL,*output=NULL,*verify_dir=NULL; uint64_t seed=424242U;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--list-profiles")==0) { for (size_t p=0U;p<MD_ARRAY_LEN(profiles);++p) puts(profiles[p]); return 0; }
        if (strcmp(argv[i],"--profile")==0&&i+1<argc) profile=argv[++i];
        else if (strcmp(argv[i],"--output")==0&&i+1<argc) output=argv[++i];
        else if (strcmp(argv[i],"--seed")==0&&i+1<argc) { char *end=NULL; errno=0; unsigned long long value=strtoull(argv[++i],&end,10); if (errno!=0||end==NULL||*end!='\0') { usage(); return EXIT_USAGE; } seed=(uint64_t)value; }
        else if (strcmp(argv[i],"--verify")==0&&i+1<argc) verify_dir=argv[++i];
        else { usage(); return EXIT_USAGE; }
    }
    if (verify_dir!=NULL) return profile==NULL&&output==NULL?verify(verify_dir):EXIT_USAGE;
    if (profile==NULL||output==NULL) { usage(); return EXIT_USAGE; }
    bool known=false; for (size_t i=0U;i<MD_ARRAY_LEN(profiles);++i) if (strcmp(profile,profiles[i])==0) known=true;
    if (!known) { fprintf(stderr,"fixturegen: unknown profile %s\n",profile); return EXIT_USAGE; }
    return generate(profile,output,seed)?0:EXIT_OUTPUT;
}
