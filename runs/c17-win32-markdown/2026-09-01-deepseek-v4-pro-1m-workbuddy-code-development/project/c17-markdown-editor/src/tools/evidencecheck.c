/* evidencecheck.c - evidence-manifest completeness/integrity validator (Workstream A). */
#include "ce_common.h"
#include "buf.h"
#include "json.h"
#include "sha256.h"
#include "winutil.h"
#include "imgcodec.h"
#include <wchar.h>

#define EXIT_OK 0
#define EXIT_USAGE 2
#define EXIT_PARSE 3
#define EXIT_READ 4
#define EXIT_WRITE 5
#define EXIT_MISMATCH 6
#define EXIT_INTERNAL 7

static const char *REQUIRED_SCREENSHOTS[] = {
    "UI-EMPTY-LIGHT","UI-EMPTY-DARK","UI-WORKSPACE-MULTITAB","UI-SOURCE","UI-SPLIT",
    "UI-PREVIEW","UI-RENDERED-EDIT","UI-MARKDOWN-ALL","UI-IMAGE-SELECTED","UI-IMAGE-RESIZE",
    "UI-TABLE-EDIT","UI-OUTLINE","UI-COMMAND-PALETTE","UI-STATISTICS","UI-VERSION-HISTORY",
    "UI-DIFF-SIDE-BY-SIDE","UI-DIFF-INLINE","UI-MODAL-BLUR","UI-FROSTED-SCROLLED",
    "UI-EXTERNAL-CONFLICT","UI-RECOVERY-CENTER","UI-ERROR-SAVE","UI-DPI-SCALED"
};

static const char *REQUIRED_CATEGORIES[] = { "unit","integration","e2e","performance","failure","regression" };

static const char *REQUIRED_TOP[] = {
    "schema_version","product_version","build_id","source_revision","generated_at",
    "test_summary","test_runs","screenshots","fixtures","performance_runs","failure_runs","artifacts"
};

static int path_insecure(const char *p){
    if(!p) return 1;
    if(p[0] == '/' || p[0] == '\\') return 1;                 /* rooted / UNC / \\?\ */
    if(((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') return 1; /* drive letter */
    const char *q = p;
    while(*q){
        if(q[0] == '.' && q[1] == '.' && (q[2] == 0 || q[2] == '/' || q[2] == '\\')) return 1; /* '..' */
        q++;
    }
    return 0;
}

typedef struct { int errors; int checks; } ec_result;

static int check_path(ec_result *r, const char *root, const char *rel, char **out_abs){
    *out_abs = NULL;
    if(path_insecure(rel)){
        printf("  SECURITY: rejected path '%s'\n", rel);
        r->errors++;
        return 1;
    }
    ce_buf abs; ce_buf_init(&abs);
    ce_buf_append_str(&abs, root);
    ce_buf_append_c(&abs, '\\');
    for(const char *p = rel; *p; p++) ce_buf_append_c(&abs, (*p == '/') ? '\\' : *p);
    *out_abs = ce_strdup(abs.data);
    ce_buf_free(&abs);
    return 0;
}

static void sha_hex(const unsigned char *d, char *hex){
    for(int i = 0; i < 32; i++){ hex[i*2] = "0123456789abcdef"[d[i]>>4]; hex[i*2+1] = "0123456789abcdef"[d[i]&15]; }
    hex[64] = 0;
}

int wmain(int argc, wchar_t **wargv){
    char **argv = ce_malloc((size_t)(argc + 1) * sizeof(char*));
    for(int i = 0; i < argc; i++) argv[i] = wu_w_to_u8(wargv[i]);
    argv[argc] = NULL;

    const char *root = NULL;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--help") == 0){ printf("usage: evidencecheck <evidence-root>\n"); return EXIT_OK; }
        else if(!root) root = argv[i];
        else { fprintf(stderr, "usage: evidencecheck <evidence-root>\n"); return EXIT_USAGE; }
    }
    if(!root){ fprintf(stderr, "usage: evidencecheck <evidence-root>\n"); return EXIT_USAGE; }

    ce_buf mp; ce_buf_init(&mp);
    ce_buf_append_str(&mp, root);
    ce_buf_append_str(&mp, "\\manifest.json");
    size_t mlen = 0;
    char *mdata = wu_read_file(mp.data, &mlen);
    ce_buf_free(&mp);
    if(!mdata){ ce_log_error("cannot read manifest.json under '%s'", root); return EXIT_READ; }

    ce_arena a; ce_arena_init(&a);
    size_t errpos = 0;
    ce_json *m = ce_json_parse(&a, mdata, &errpos);
    ce_free(mdata);
    if(!m || m->type != CEJ_OBJ){ ce_log_error("manifest.json parse error at %zu", errpos); ce_arena_free(&a); return EXIT_PARSE; }

    ec_result r = {0, 0};

    /* top-level fields */
    for(size_t i = 0; i < CE_ARRAY_LEN(REQUIRED_TOP); i++){
        if(!ce_json_obj_get(m, REQUIRED_TOP[i])){ printf("  MISSING top-level field '%s'\n", REQUIRED_TOP[i]); r.errors++; }
    }

    /* test summary */
    ce_json *ts = ce_json_obj_get(m, "test_summary");
    if(ts){
        int64_t failed = ce_json_int(ce_json_obj_get(ts, "failed"), -1);
        int64_t skipped = ce_json_int(ce_json_obj_get(ts, "skipped"), -1);
        if(failed != 0){ printf("  FAIL: test_summary.failed = %lld (must be 0)\n", (long long)failed); r.errors++; }
        if(skipped != 0){ printf("  FAIL: test_summary.skipped = %lld (must be 0)\n", (long long)skipped); r.errors++; }
        int64_t total = ce_json_int(ce_json_obj_get(ts, "total"), 0);
        int64_t passed = ce_json_int(ce_json_obj_get(ts, "passed"), 0);
        if(passed < 0 || total < passed){ r.errors++; }
    } else { r.errors++; }

    /* test runs: categories + no skipped + logs */
    {
        ce_json *runs = ce_json_obj_get(m, "test_runs");
        bool cat[6] = {0};
        if(runs && runs->type == CEJ_ARR){
            for(size_t i = 0; i < runs->u.arr.count; i++){
                ce_json *e = runs->u.arr.items[i];
                const char *catname = ce_json_str(ce_json_obj_get(e, "category"));
                for(size_t c = 0; c < CE_ARRAY_LEN(REQUIRED_CATEGORIES); c++)
                    if(strcmp(catname, REQUIRED_CATEGORIES[c]) == 0) cat[c] = true;
                const char *result = ce_json_str(ce_json_obj_get(e, "result"));
                if(strcmp(result, "skipped") == 0){ printf("  FAIL: mandatory test run '%s' skipped\n", ce_json_str(ce_json_obj_get(e, "id"))); r.errors++; }
                /* verify log */
                const char *log = ce_json_str(ce_json_obj_get(e, "log"));
                if(log && *log){
                    char *abs = NULL;
                    if(check_path(&r, root, log, &abs) == 0){
                        size_t llen = 0;
                        char *ld = wu_read_file(abs, &llen);
                        if(!ld){ printf("  MISSING log '%s'\n", log); r.errors++; }
                        else {
                            uint8_t sha[32]; ce_sha256_hash(ld, llen, sha);
                            char hex[65]; sha_hex(sha, hex);
                            const char *expect = ce_json_str(ce_json_obj_get(e, "log_sha256"));
                            if(expect && *expect && strcmp(hex, expect) != 0){ printf("  DIGEST-MISMATCH log '%s'\n", log); r.errors++; }
                            ce_free(ld);
                        }
                        ce_free(abs);
                    }
                }
                r.checks++;
            }
        }
        for(size_t c = 0; c < CE_ARRAY_LEN(REQUIRED_CATEGORIES); c++)
            if(!cat[c]){ printf("  MISSING test category '%s'\n", REQUIRED_CATEGORIES[c]); r.errors++; }
    }

    /* screenshots */
    {
        ce_json *shots = ce_json_obj_get(m, "screenshots");
        bool seen[CE_ARRAY_LEN(REQUIRED_SCREENSHOTS)] = {0};
        if(shots && shots->type == CEJ_ARR){
            for(size_t i = 0; i < shots->u.arr.count; i++){
                ce_json *e = shots->u.arr.items[i];
                const char *id = ce_json_str(ce_json_obj_get(e, "id"));
                for(size_t s = 0; s < CE_ARRAY_LEN(REQUIRED_SCREENSHOTS); s++)
                    if(strcmp(id, REQUIRED_SCREENSHOTS[s]) == 0) seen[s] = true;
                const char *path = ce_json_str(ce_json_obj_get(e, "path"));
                char *abs = NULL;
                if(check_path(&r, root, path, &abs) == 0){
                    size_t ilen = 0;
                    char *idat = wu_read_file(abs, &ilen);
                    if(!idat){ printf("  MISSING screenshot '%s'\n", path); r.errors++; }
                    else {
                        uint8_t sha[32]; ce_sha256_hash(idat, ilen, sha);
                        char hex[65]; sha_hex(sha, hex);
                        const char *expect = ce_json_str(ce_json_obj_get(e, "sha256"));
                        if(expect && *expect && strcmp(hex, expect) != 0){ printf("  DIGEST-MISMATCH screenshot '%s'\n", path); r.errors++; }
                        /* image header for width/height */
                        int w = 0, h = 0;
                        uint8_t *px = img_decode((unsigned char*)idat, ilen, &w, &h);
                        if(!px){ printf("  NOT-IMAGE screenshot '%s'\n", path); r.errors++; }
                        else {
                            ce_free(px);
                            int64_t mw = ce_json_int(ce_json_obj_get(e, "width"), -1);
                            int64_t mh = ce_json_int(ce_json_obj_get(e, "height"), -1);
                            if(mw != w || mh != h){ printf("  SIZE-MISMATCH screenshot '%s' (%dx%d vs %lldx%lld)\n", path, w, h, (long long)mw, (long long)mh); r.errors++; }
                        }
                        ce_free(idat);
                    }
                    ce_free(abs);
                }
                r.checks++;
            }
        }
        for(size_t s = 0; s < CE_ARRAY_LEN(REQUIRED_SCREENSHOTS); s++)
            if(!seen[s]){ printf("  MISSING required screenshot '%s'\n", REQUIRED_SCREENSHOTS[s]); r.errors++; }
    }

    /* artifacts */
    {
        ce_json *arts = ce_json_obj_get(m, "artifacts");
        if(arts && arts->type == CEJ_ARR){
            for(size_t i = 0; i < arts->u.arr.count; i++){
                ce_json *e = arts->u.arr.items[i];
                const char *path = ce_json_str(ce_json_obj_get(e, "path"));
                char *abs = NULL;
                if(check_path(&r, root, path, &abs) == 0){
                    size_t alen = 0;
                    char *ad = wu_read_file(abs, &alen);
                    if(!ad){ printf("  MISSING artifact '%s'\n", path); r.errors++; }
                    else {
                        uint8_t sha[32]; ce_sha256_hash(ad, alen, sha);
                        char hex[65]; sha_hex(sha, hex);
                        const char *expect = ce_json_str(ce_json_obj_get(e, "sha256"));
                        if(expect && *expect && strcmp(hex, expect) != 0){ printf("  DIGEST-MISMATCH artifact '%s'\n", path); r.errors++; }
                        int64_t esz = ce_json_int(ce_json_obj_get(e, "size"), -1);
                        if(esz >= 0 && (size_t)esz != alen){ printf("  SIZE-MISMATCH artifact '%s'\n", path); r.errors++; }
                        ce_free(ad);
                    }
                    ce_free(abs);
                }
                r.checks++;
            }
        }
    }

    /* fixtures: verify referenced fixture manifests */
    {
        ce_json *fix = ce_json_obj_get(m, "fixtures");
        if(fix && fix->type == CEJ_ARR){
            for(size_t i = 0; i < fix->u.arr.count; i++){
                ce_json *e = fix->u.arr.items[i];
                const char *path = ce_json_str(ce_json_obj_get(e, "path"));
                char *abs = NULL;
                if(check_path(&r, root, path, &abs) == 0){
                    size_t flen = 0;
                    char *fd = wu_read_file(abs, &flen);
                    if(!fd){ printf("  MISSING fixture manifest '%s'\n", path); r.errors++; }
                    else {
                        uint8_t sha[32]; ce_sha256_hash(fd, flen, sha);
                        char hex[65]; sha_hex(sha, hex);
                        const char *expect = ce_json_str(ce_json_obj_get(e, "sha256"));
                        if(expect && *expect && strcmp(hex, expect) != 0){ printf("  DIGEST-MISMATCH fixture '%s'\n", path); r.errors++; }
                        ce_free(fd);
                    }
                    ce_free(abs);
                }
                r.checks++;
            }
        }
    }

    /* performance + failure runs present */
    {
        ce_json *p = ce_json_obj_get(m, "performance_runs");
        ce_json *f = ce_json_obj_get(m, "failure_runs");
        if(!p || p->type != CEJ_ARR || p->u.arr.count == 0){ printf("  MISSING performance_runs\n"); r.errors++; }
        if(!f || f->type != CEJ_ARR || f->u.arr.count == 0){ printf("  MISSING failure_runs\n"); r.errors++; }
    }

    ce_arena_free(&a);

    printf("evidencecheck: %d checks, %d errors\n", r.checks, r.errors);
    if(r.errors == 0) printf("PASS\n");
    else printf("FAIL\n");

    for(int i = 0; i < argc; i++) ce_free(argv[i]);
    ce_free(argv);
    return r.errors ? EXIT_MISMATCH : EXIT_OK;
}
