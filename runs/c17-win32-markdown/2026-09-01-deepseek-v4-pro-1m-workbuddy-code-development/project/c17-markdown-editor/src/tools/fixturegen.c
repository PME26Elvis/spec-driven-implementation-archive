/* fixturegen.c - deterministic Markdown/workspace fixture generator (Workstream A). */
#include "ce_common.h"
#include "buf.h"
#include "prng.h"
#include "sha256.h"
#include "base64.h"
#include "json.h"
#include "winutil.h"
#include "imgcodec.h"
#include <wchar.h>
#include <windows.h>

#define EXIT_OK 0
#define EXIT_USAGE 2
#define EXIT_READ 4
#define EXIT_WRITE 5
#define EXIT_MISMATCH 6

#define GEN_VERSION "1.0.0"
#define DEFAULT_SEED 0x9E3779B97F4A7C15ULL

typedef struct {
    char *rel;            /* '/' separators */
    unsigned char *data;
    size_t len;
    char *role;
} genfile;

typedef struct {
    genfile *files;
    size_t n, cap;
} flist;

static void add_buf(flist *fl, const char *rel, const void *data, size_t len, const char *role){
    if(fl->n == fl->cap){ fl->cap = fl->cap ? fl->cap * 2 : 64; fl->files = ce_realloc(fl->files, fl->cap * sizeof(genfile)); }
    genfile *g = &fl->files[fl->n++];
    g->rel = ce_strdup(rel);
    g->data = ce_malloc(len ? len : 1);
    memcpy(g->data, data, len);
    g->len = len;
    g->role = ce_strdup(role);
}

static void add_str(flist *fl, const char *rel, const char *s, const char *role){
    add_buf(fl, rel, s, strlen(s), role);
}

static void flist_free(flist *fl){
    for(size_t i = 0; i < fl->n; i++){
        ce_free(fl->files[i].rel);
        ce_free(fl->files[i].data);
        ce_free(fl->files[i].role);
    }
    if(fl->files) ce_free(fl->files);
    fl->files = NULL; fl->n = fl->cap = 0;
}

/* deterministic image bytes */
static void gen_image(ce_prng *rng, int w, int h, int fmt, unsigned char **out, size_t *out_len){
    uint8_t *rgba = ce_malloc((size_t)w * h * 4);
    for(int i = 0; i < w * h; i++){
        rgba[i*4+0] = (uint8_t)(ce_prng_next(rng) & 0xFF);
        rgba[i*4+1] = (uint8_t)(ce_prng_next(rng) & 0xFF);
        rgba[i*4+2] = (uint8_t)(ce_prng_next(rng) & 0xFF);
        rgba[i*4+3] = 0xFF;
    }
    *out = img_encode(rgba, w, h, fmt, out_len);
    ce_free(rgba);
}

static void add_image(flist *fl, ce_prng *rng, const char *rel, int fmt, const char *role){
    unsigned char *data = NULL; size_t len = 0;
    gen_image(rng, 24, 16, fmt, &data, &len);
    if(data){ add_buf(fl, rel, data, len, role); ce_free(data); }
}

/* ---------------- profile generators ---------------- */

static void gen_small(flist *fl, ce_prng *rng){
    add_str(fl, "README.md",
        "# Welcome\n\nThis is a small workspace fixture.\n\n## Features\n\n- Fast\n- Deterministic\n- Self-contained\n\nSee the [guide](docs/guide.md).\n",
        "markdown");
    add_str(fl, "docs/guide.md",
        "# Guide\n\nSome text with **bold**, *italic*, and `inline code`.\n\n```c\nint main(){ return 0; }\n```\n\n| A | B |\n|---|---|\n| 1 | 2 |\n\n![logo](../assets/logo.png)\n",
        "markdown");
    add_str(fl, "docs/notes.md",
        "# Notes\n\n- first\n- second\n  - nested\n\n1. one\n2. two\n",
        "markdown");
    add_image(fl, rng, "assets/logo.png", IMG_FMT_PNG, "image");
    /* base64-embedded image */
    {
        unsigned char *data = NULL; size_t len = 0;
        gen_image(rng, 20, 20, IMG_FMT_PNG, &data, &len);
        if(data){
            char *b64 = ce_base64_encode(data, len);
            ce_buf md; ce_buf_init(&md);
            ce_buf_append_fmt(&md, "# Embedded\n\n![dot](data:image/png;base64,%s)\n", b64);
            add_buf(fl, "docs/embedded.md", md.data, md.len, "markdown");
            ce_buf_free(&md);
            ce_free(b64);
            ce_free(data);
        }
    }
}

static void gen_unicode(flist *fl, ce_prng *rng){
    (void)rng;
    /* Traditional Chinese, English, mixed, accented, combining, emoji VS, ZWJ */
    const char *zh = "繁體中文測試文件，用於驗證編輯器的中文處理能力。\n\n";
    const char *en = "English paragraph with words and numbers 12345.\n\n";
    const char *mixed = "混合 Mixed 中英文 123 測試 Test 內容。\n\n";
    const char *accent = "Café résumé naïve façade coöperate.\n\n";
    const char *combining = "e\u0301 combining mark e\u0301 test.\n\n";
    const char *emoji_vs = "emoji \xE2\x9C\x88\xEF\xB8\x8F variation selector.\n\n";
    const char *zwj = "family \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7 sequence.\n\n";
    ce_buf b; ce_buf_init(&b);
    ce_buf_append_str(&b, "# 中文測試\n\n");
    ce_buf_append_str(&b, zh);
    ce_buf_append_str(&b, en);
    ce_buf_append_str(&b, mixed);
    ce_buf_append_str(&b, accent);
    ce_buf_append_str(&b, combining);
    ce_buf_append_str(&b, emoji_vs);
    ce_buf_append_str(&b, zwj);
    ce_buf_append_str(&b, "搜尋詞彙 搜尋詞彙 搜尋詞彙\n");
    add_buf(fl, "unicode/中文 測試.md", b.data, b.len, "markdown");
    ce_buf_free(&b);
}

static void gen_markdown_all(flist *fl, ce_prng *rng){
    (void)rng;
    ce_buf b; ce_buf_init(&b);
    ce_buf_append_str(&b, "# Heading 1\n\n## Heading 2\n\n### Heading 3\n\n#### Heading 4\n\n##### Heading 5\n\n###### Heading 6\n\n");
    ce_buf_append_str(&b, "Setext heading\n===============\n\nSetext two\n----------\n\n");
    ce_buf_append_str(&b, "Paragraph with **strong**, *emphasis*, ***both***, ~~strike~~, `code`, [link](https://example.com), and ![image](a.png).\n\n");
    ce_buf_append_str(&b, "Auto link <https://example.com> and escaped \\*literal\\*.\n\n");
    ce_buf_append_str(&b, "---\n\n");
    ce_buf_append_str(&b, "> blockquote\n> > nested\n\n");
    ce_buf_append_str(&b, "- item\n- item\n  - nested\n    - deep\n- [x] task done\n- [ ] task open\n\n");
    ce_buf_append_str(&b, "1. first\n2. second\n\n");
    ce_buf_append_str(&b, "    indented code\n\n");
    ce_buf_append_str(&b, "```python\nprint('hi')\n```\n\n");
    ce_buf_append_str(&b, "~~~\ntilde fence\n~~~\n\n");
    ce_buf_append_str(&b, "| H1 | H2 | H3 |\n|:---|:---:|---:|\n| a | b | c |\n| d | e | f |\n\n");
    ce_buf_append_str(&b, "Hard break  \nnext line\n\n");
    ce_buf_append_str(&b, "Malformed below:\n\n**unclosed bold\n\n[broken link](no-close\n\n```\nunclosed fence\n\n");
    add_buf(fl, "markdown-all.md", b.data, b.len, "markdown");
    ce_buf_free(&b);
}

static void gen_workspace(flist *fl, ce_prng *rng){
    add_str(fl, "index.md", "# Workspace\n\nRoot document.\n", "markdown");
    for(int i = 0; i < 24; i++){
        ce_buf p; ce_buf_init(&p);
        ce_buf_append_fmt(&p, "nested/level1/level2/level3/doc%02d.md", i);
        ce_buf m; ce_buf_init(&m);
        ce_buf_append_fmt(&m, "# Doc %02d\n\nContent with a [link](../doc%02d.md) and ![img](../../assets/pic.png).\n", i, (i+1)%24);
        add_buf(fl, p.data, m.data, m.len, "markdown");
        ce_buf_free(&p); ce_buf_free(&m);
    }
    add_str(fl, "nested/dup/README.md", "# Dup\n", "markdown");
    add_str(fl, "nested/other/README.md", "# Other dup\n", "markdown");
    add_str(fl, "with space/name with spaces.md", "# Spaces\n", "markdown");
    add_str(fl, "中文 目錄/中文檔案.md", "# 中文檔案\n", "markdown");
    add_str(fl, "nested/doc/./relative.md", "# Relative\n\n![x](./img.png) ![y](../img2.png)\n", "markdown");
    add_image(fl, rng, "assets/pic.png", IMG_FMT_PNG, "image");
    add_image(fl, rng, "nested/assets/pic.png", IMG_FMT_PNG, "image");
}

static void gen_medium(flist *fl, ce_prng *rng){
    ce_buf b; ce_buf_init(&b);
    ce_buf_append_str(&b, "# Medium Fixture\n\n");
    int headings = 0, lists = 0, tables = 0;
    int i = 0;
    while(headings < 300 || lists < 300 || tables < 30 || b.len < (1024u*1024u)){
        if(i % 200 == 0 && headings < 300){ ce_buf_append_fmt(&b, "## Section %d 中文章節\n\n", headings); headings++; }
        if(i % 50 == 0 && lists < 300){
            ce_buf_append_str(&b, "- list item 項目\n- another 項目\n");
            lists += 2;
        }
        if(i % 400 == 0 && tables < 30){
            ce_buf_append_str(&b, "| Col A 欄位 | Col B |\n|---|---|\n| value | data |\n");
            tables++;
        }
        ce_buf_append_fmt(&b, "Paragraph line %d with 中文 mixed English content for the fixture. 這是一段文字。\n", i);
        i++;
    }
    add_buf(fl, "medium.md", b.data, b.len, "markdown");
    ce_buf_free(&b);
    for(int k = 0; k < 20; k++){ char p[64]; snprintf(p, sizeof(p), "assets/img_medium_%d.png", k); add_image(fl, rng, p, IMG_FMT_PNG, "image"); }
}

static void gen_large(flist *fl, ce_prng *rng){
    ce_buf b; ce_buf_init(&b);
    ce_buf_append_str(&b, "# Large Fixture\n\n");
    long headings = 0, lists = 0, tables = 0;
    long i = 0;
    while(headings < 1000 || lists < 2000 || tables < 100 || b.len < (5u*1024u*1024u)){
        if(i % 500 == 0 && headings < 1000){ ce_buf_append_fmt(&b, "## Section %ld 大文件章節\n\n", headings); headings++; }
        if(i % 250 == 0 && lists < 2000){
            ce_buf_append_str(&b, "- 列表項目 list item\n- another 項目 item\n");
            lists += 2;
        }
        if(i % 600 == 0 && tables < 100){
            ce_buf_append_str(&b, "| 欄位 A | Col B | Col C |\n|---|---|---|\n| v | w | x |\n");
            tables++;
        }
        if(i % 3000 == 0){
            ce_buf_append_str(&b, "```c\n// code block\nint x = 0;\nfor(int j=0;j<10;j++) x += j;\n```\n");
        }
        ce_buf_append_fmt(&b, "Long paragraph %ld combining e\u0301 and emoji \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9 中文內容 mixed English content.\n", i);
        i++;
    }
    add_buf(fl, "large.md", b.data, b.len, "markdown");
    ce_buf_free(&b);
    for(int k = 0; k < 20; k++){ char p[64]; snprintf(p, sizeof(p), "assets/img_large_%d.png", k); add_image(fl, rng, p, IMG_FMT_PNG, "image"); }
}

static void gen_stress_long_line(flist *fl, ce_prng *rng){
    (void)rng;
    /* 1 MiB single logical line (valid UTF-8, no newline) */
    const char *word = "The quick brown fox jumps over the lazy dog 中文測試內容 ";
    size_t wl = strlen(word);
    size_t n = 1024u * 1024u;
    char *line = ce_malloc(n);
    for(size_t i = 0; i < n; i++) line[i] = word[i % wl];
    add_buf(fl, "stress-long-line.md", line, n, "markdown");
    ce_free(line);
}

static void gen_failure(flist *fl, ce_prng *rng){
    /* invalid UTF-8 */
    { unsigned char bad[] = { 'a', 'b', 0xFF, 0xFE, '\n', 'c' }; add_buf(fl, "failure/invalid-utf8.md", bad, sizeof(bad), "failure"); }
    /* malformed markdown */
    add_str(fl, "failure/malformed.md", "**unclosed\n\n[broken\n\n# heading without space is not a heading\n#ok\n", "failure");
    /* missing image reference */
    add_str(fl, "failure/missing-image.md", "# Missing\n\n![gone](does-not-exist.png)\n", "failure");
    /* corrupt png/jpeg/bmp */
    { unsigned char c[] = { 0x89, 'P','N','G', 0x0D, 0x0A, 0x1A, 0x0A, 0,1,2,3 }; add_buf(fl, "failure/corrupt.png", c, sizeof(c), "failure"); }
    { unsigned char c[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0, 1, 2 }; add_buf(fl, "failure/corrupt.jpg", c, sizeof(c), "failure"); }
    { unsigned char c[] = { 'B','M', 0,1,2,3 }; add_buf(fl, "failure/corrupt.bmp", c, sizeof(c), "failure"); }
    /* truncated JSON workspace state */
    add_str(fl, "failure/workspace-state.json", "{\"version\":1,\"tabs\":[", "failure");
    /* truncated history record */
    add_str(fl, "failure/history.bin", "MDHV01\x04\x00\x00\x00\x10\x00\x00\x00", "failure");
    /* truncated recovery record */
    add_str(fl, "failure/recovery.bin", "RCRD\x01\x00\x00\x00\xff", "failure");
    (void)rng;
}

/* ---------------- manifest + writing ---------------- */

static int write_tree(flist *fl, const char *outdir){
    /* create outdir */
    {
        wchar_t *w = wu_u8_to_native(outdir);
        if(w){
            CreateDirectoryW(w, NULL);
            ce_free(w);
        }
    }
    for(size_t i = 0; i < fl->n; i++){
        genfile *g = &fl->files[i];
        ce_buf full; ce_buf_init(&full);
        ce_buf_append_str(&full, outdir);
        ce_buf_append_c(&full, '\\');
        /* convert '/' to '\\' and append */
        for(const char *p = g->rel; *p; p++) ce_buf_append_c(&full, (*p == '/') ? '\\' : *p);
        /* create parent dirs */
        char *dir = ce_strdup(full.data);
        char *slash = strrchr(dir, '\\');
        if(slash){
            *slash = 0;
            ce_buf dp; ce_buf_init(&dp);
            ce_buf_append_str(&dp, dir);
            char *cur = ce_malloc(strlen(dir) + 1);
            strcpy(cur, dir);
            for(char *q = cur + 1; *q; q++){
                if(*q == '\\'){
                    *q = 0;
                    wchar_t *wq = wu_u8_to_w(cur);
                    if(wq){ CreateDirectoryW(wq, NULL); ce_free(wq); }
                    *q = '\\';
                }
            }
            wchar_t *wd = wu_u8_to_w(dir);
            if(wd){ CreateDirectoryW(wd, NULL); ce_free(wd); }
            ce_free(cur);
            ce_buf_free(&dp);
        }
        ce_free(dir);
        if(!wu_write_file(full.data, g->data, g->len)){
            ce_log_error("failed to write '%s'", full.data);
            ce_buf_free(&full);
            return EXIT_WRITE;
        }
        ce_buf_free(&full);
    }
    return EXIT_OK;
}

static int write_manifest(flist *fl, const char *outdir, const char *profile, uint64_t seed){
    ce_arena a; ce_arena_init(&a);
    ce_json *root = ce_json_new_obj(&a);
    ce_json_obj_set(&a, root, "schema_version", ce_json_new_int(&a, 1));
    ce_json_obj_set(&a, root, "generator_version", ce_json_new_str(&a, GEN_VERSION));
    ce_json_obj_set(&a, root, "profile", ce_json_new_str(&a, profile));
    {
        char seedbuf[32];
        snprintf(seedbuf, sizeof(seedbuf), "%llu", (unsigned long long)seed);
        ce_json_obj_set(&a, root, "seed", ce_json_new_str(&a, seedbuf));
    }
    ce_json *files = ce_json_new_arr(&a);
    for(size_t i = 0; i < fl->n; i++){
        genfile *g = &fl->files[i];
        uint8_t sha[32]; ce_sha256_hash(g->data, g->len, sha);
        char hex[65]; 
        for(int k = 0; k < 32; k++){ hex[k*2] = "0123456789abcdef"[sha[k]>>4]; hex[k*2+1] = "0123456789abcdef"[sha[k]&15]; }
        hex[64] = 0;
        ce_json *e = ce_json_new_obj(&a);
        ce_json_obj_set(&a, e, "path", ce_json_new_str(&a, g->rel));
        ce_json_obj_set(&a, e, "size", ce_json_new_int(&a, (int64_t)g->len));
        ce_json_obj_set(&a, e, "sha256", ce_json_new_str(&a, hex));
        ce_json_obj_set(&a, e, "role", ce_json_new_str(&a, g->role));
        ce_json_arr_push(&a, files, e);
    }
    ce_json_obj_set(&a, root, "files", files);
    char *json = ce_json_to_string(root);
    ce_buf mp; ce_buf_init(&mp);
    ce_buf_append_str(&mp, outdir);
    ce_buf_append_str(&mp, "\\fixture-manifest.json");
    bool ok = wu_write_file(mp.data, json, strlen(json));
    ce_buf_free(&mp);
    ce_free(json);
    ce_arena_free(&a);
    return ok ? EXIT_OK : EXIT_WRITE;
}

static int verify_dir(const char *dir){
    ce_buf mp; ce_buf_init(&mp);
    ce_buf_append_str(&mp, dir);
    ce_buf_append_str(&mp, "\\fixture-manifest.json");
    size_t mlen = 0;
    char *mdata = wu_read_file(mp.data, &mlen);
    ce_buf_free(&mp);
    if(!mdata){ ce_log_error("manifest not found in '%s'", dir); return EXIT_MISMATCH; }
    ce_arena a; ce_arena_init(&a);
    size_t errpos = 0;
    ce_json *root = ce_json_parse(&a, mdata, &errpos);
    ce_free(mdata);
    if(!root){ ce_log_error("manifest parse error at %zu", errpos); ce_arena_free(&a); return EXIT_MISMATCH; }
    ce_json *files = ce_json_obj_get(root, "files");
    size_t ok_count = 0, fail = 0;
    if(!files || files->type != CEJ_ARR){ ce_log_error("manifest missing 'files'"); ce_arena_free(&a); return EXIT_MISMATCH; }
    for(size_t i = 0; i < files->u.arr.count; i++){
        ce_json *e = files->u.arr.items[i];
        const char *path = ce_json_str(ce_json_obj_get(e, "path"));
        int64_t size = ce_json_int(ce_json_obj_get(e, "size"), -1);
        const char *sha = ce_json_str(ce_json_obj_get(e, "sha256"));
        ce_buf fp; ce_buf_init(&fp);
        ce_buf_append_str(&fp, dir);
        ce_buf_append_c(&fp, '\\');
        for(const char *p = path; *p; p++) ce_buf_append_c(&fp, (*p=='/')?'\\':*p);
        size_t flen = 0;
        char *fdata = wu_read_file(fp.data, &flen);
        ce_buf_free(&fp);
        if(!fdata){ printf("  MISSING %s\n", path); fail++; continue; }
        if(size >= 0 && (size_t)size != flen){ printf("  SIZE-MISMATCH %s (%lld != %zu)\n", path, (long long)size, flen); fail++; ce_free(fdata); continue; }
        uint8_t sha2[32]; ce_sha256_hash(fdata, flen, sha2);
        char hex[65];
        for(int k = 0; k < 32; k++){ hex[k*2] = "0123456789abcdef"[sha2[k]>>4]; hex[k*2+1] = "0123456789abcdef"[sha2[k]&15]; }
        hex[64] = 0;
        if(strcmp(hex, sha) != 0){ printf("  DIGEST-MISMATCH %s\n", path); fail++; ce_free(fdata); continue; }
        ok_count++; ce_free(fdata);
    }
    ce_arena_free(&a);
    printf("fixturegen verify: %zu ok, %zu failed\n", ok_count, fail);
    return fail ? EXIT_MISMATCH : EXIT_OK;
}

static const char *PROFILES[] = { "small", "unicode", "markdown-all", "workspace", "medium", "large", "stress-long-line", "failure" };

static void usage(void){
    fprintf(stderr, "usage: fixturegen --profile <name> --output <dir> [--seed <u64>]\n");
    fprintf(stderr, "       fixturegen --list-profiles\n");
    fprintf(stderr, "       fixturegen --verify <dir>\n");
}

int wmain(int argc, wchar_t **wargv){
    char **argv = ce_malloc((size_t)(argc + 1) * sizeof(char*));
    for(int i = 0; i < argc; i++) argv[i] = wu_w_to_u8(wargv[i]);
    argv[argc] = NULL;

    const char *profile = NULL, *out = NULL;
    uint64_t seed = DEFAULT_SEED;
    bool list = false;
    const char *verify = NULL;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--profile") == 0 && i + 1 < argc) profile = argv[++i];
        else if(strcmp(argv[i], "--output") == 0 && i + 1 < argc) out = argv[++i];
        else if(strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = strtoull(argv[++i], NULL, 0);
        else if(strcmp(argv[i], "--list-profiles") == 0) list = true;
        else if(strcmp(argv[i], "--verify") == 0 && i + 1 < argc) verify = argv[++i];
        else { usage(); return EXIT_USAGE; }
    }

    if(list){
        printf("profiles:\n");
        for(size_t i = 0; i < CE_ARRAY_LEN(PROFILES); i++) printf("  %s\n", PROFILES[i]);
        return EXIT_OK;
    }
    if(verify) return verify_dir(verify);
    if(!profile || !out){ usage(); return EXIT_USAGE; }

    bool known = false;
    for(size_t i = 0; i < CE_ARRAY_LEN(PROFILES); i++) if(strcmp(profile, PROFILES[i]) == 0) known = true;
    if(!known){ ce_log_error("unknown profile '%s'", profile); return EXIT_USAGE; }

    ce_prng rng; ce_prng_seed(&rng, seed);
    flist fl = {0};
    if(strcmp(profile, "small") == 0) gen_small(&fl, &rng);
    else if(strcmp(profile, "unicode") == 0) gen_unicode(&fl, &rng);
    else if(strcmp(profile, "markdown-all") == 0) gen_markdown_all(&fl, &rng);
    else if(strcmp(profile, "workspace") == 0) gen_workspace(&fl, &rng);
    else if(strcmp(profile, "medium") == 0) gen_medium(&fl, &rng);
    else if(strcmp(profile, "large") == 0) gen_large(&fl, &rng);
    else if(strcmp(profile, "stress-long-line") == 0) gen_stress_long_line(&fl, &rng);
    else if(strcmp(profile, "failure") == 0) gen_failure(&fl, &rng);

    int rc = write_tree(&fl, out);
    if(rc == EXIT_OK) rc = write_manifest(&fl, out, profile, seed);
    printf("fixturegen: profile=%s files=%zu seed=%llu\n", profile, fl.n, (unsigned long long)seed);

    flist_free(&fl);
    for(int i = 0; i < argc; i++) ce_free(argv[i]);
    ce_free(argv);
    return rc;
}
