/* locscan.c - repository inventory / line-count utility (Workstream A). */
#include "ce_common.h"
#include "buf.h"
#include "json.h"
#include "yaml.h"
#include "match.h"
#include "winutil.h"
#include <wchar.h>

#define EXIT_OK 0
#define EXIT_USAGE 2
#define EXIT_CONFIG 3
#define EXIT_READ 4
#define EXIT_WRITE 5

typedef struct {
    char **include_ext;    size_t n_include;
    char **source_ext;     size_t n_source;
    char **test_ext;       size_t n_test;
    char **doc_ext;        size_t n_doc;
    char **config_ext;     size_t n_config;
    char **exclude_dirs;   size_t n_exdirs;
    char **exclude_paths;  size_t n_expaths;
    char **generated;      size_t n_generated;
    char **overrides;      size_t n_overrides;
    bool follow_reparse;
} lc_config;

typedef struct {
    size_t files, lines;
} lc_cat; /* one per category */

static const char *CAT_SOURCE = "source";
static const char *CAT_TEST = "test";
static const char *CAT_DOC = "documentation";
static const char *CAT_CONFIG = "config";
static const char *CAT_OTHER = "other";

static void add_str(char ***arr, size_t *n, const char *s){
    *arr = ce_realloc(*arr, (*n + 1) * sizeof(char*));
    (*arr)[(*n)++] = ce_strdup(s);
}

static const char *ext_of(const char *p){
    const char *dot = strrchr(p, '.');
    if(!dot) return "";
    /* dot must be after last slash */
    const char *slash = strrchr(p, '/');
    if(slash && dot < slash) return "";
    return dot; /* includes '.' */
}

static int ext_matches(const char *ext, char **list, size_t n, bool fold){
    for(size_t i = 0; i < n; i++){
        if(ce_strcasecmp(ext, list[i]) == 0) return 1;
    }
    (void)fold;
    return 0;
}

/* convert a JSON/YAML value to a string-array config field */
static void cfg_from_json(lc_config *cfg, ce_json *obj){
    ce_json *v;
    #define ARR(key, mem, cnt) do{ v = ce_json_obj_get(obj, key); if(v && v->type == CEJ_ARR){ for(size_t i=0;i<v->u.arr.count;i++){ if(v->u.arr.items[i]->type==CEJ_STR) add_str(&mem, &cnt, v->u.arr.items[i]->u.str.s); } } }while(0)
    ARR("include_extensions", cfg->include_ext, cfg->n_include);
    ARR("source_extensions", cfg->source_ext, cfg->n_source);
    ARR("test_extensions", cfg->test_ext, cfg->n_test);
    ARR("documentation_extensions", cfg->doc_ext, cfg->n_doc);
    ARR("config_build_extensions", cfg->config_ext, cfg->n_config);
    ARR("exclude_dirs", cfg->exclude_dirs, cfg->n_exdirs);
    ARR("exclude_paths", cfg->exclude_paths, cfg->n_expaths);
    ARR("generated_paths", cfg->generated, cfg->n_generated);
    ARR("include_overrides", cfg->overrides, cfg->n_overrides);
    #undef ARR
    v = ce_json_obj_get(obj, "follow_directory_reparse_points");
    if(v && v->type == CEJ_BOOL) cfg->follow_reparse = v->u.b;
}

static int load_config(const char *path, lc_config *cfg){
    memset(cfg, 0, sizeof(*cfg));
    size_t len = 0;
    char *data = wu_read_file(path, &len);
    if(!data){ ce_log_error("cannot read config '%s'", path); return EXIT_READ; }
    ce_arena a; ce_arena_init(&a);
    bool is_yaml = ce_ends_with(path, ".yaml") || ce_ends_with(path, ".yml");
    ce_json *obj;
    if(is_yaml){
        int errline = 0;
        obj = ce_yaml_parse(&a, data, &errline);
        if(!obj){ ce_log_error("malformed YAML config '%s' near line %d", path, errline); ce_free(data); ce_arena_free(&a); return EXIT_CONFIG; }
    } else {
        size_t errpos = 0;
        obj = ce_json_parse(&a, data, &errpos);
        if(!obj){ ce_log_error("malformed JSON config '%s' at offset %zu", path, errpos); ce_free(data); ce_arena_free(&a); return EXIT_CONFIG; }
    }
    cfg_from_json(cfg, obj);
    ce_free(data);
    ce_arena_free(&a);
    return EXIT_OK;
}

typedef struct {
    char *root;
    lc_config *cfg;
    lc_cat source, test, doc, config, other;
    size_t total_files, total_lines;
    /* detail entries */
    ce_json *detail;      /* array */
    ce_arena arena;
    ce_json *json_root;
} scan_ctx;

/* classification: returns category string, or NULL if excluded/not counted */
static const char *classify(scan_ctx *s, const char *rel){
    const char *ext = ext_of(rel);
    const char *cat = NULL;
    if(ext_matches(ext, s->cfg->source_ext, s->cfg->n_source, true)) cat = CAT_SOURCE;
    else if(ext_matches(ext, s->cfg->test_ext, s->cfg->n_test, true)) cat = CAT_TEST;
    else if(ext_matches(ext, s->cfg->doc_ext, s->cfg->n_doc, true)) cat = CAT_DOC;
    else if(ext_matches(ext, s->cfg->config_ext, s->cfg->n_config, true)) cat = CAT_CONFIG;
    else if(ext_matches(ext, s->cfg->include_ext, s->cfg->n_include, true)) cat = CAT_OTHER;
    else {
        /* not in any include list: not counted */
        return NULL;
    }
    return cat;
}

static int walk_cb(void *ctx, const char *full, int is_dir){
    scan_ctx *s = ctx;
    size_t rl = strlen(s->root);
    const char *rel = full + rl;
    while(*rel == '\\' || *rel == '/') rel++;
    /* normalize to '/' */
    char *reln = ce_strdup(rel);
    for(char *q = reln; *q; q++) if(*q == '\\') *q = '/';

    if(is_dir){
        /* check exclude_dirs (subtree) */
        for(size_t i = 0; i < s->cfg->n_exdirs; i++){
            if(ce_path_match(s->cfg->exclude_dirs[i], reln, true)){
                ce_free(reln);
                return 0;
            }
        }
        ce_free(reln);
        return 0;
    }
    /* file */
    bool excluded = false, generated = false;
    for(size_t i = 0; i < s->cfg->n_expaths; i++) if(ce_path_match(s->cfg->exclude_paths[i], reln, true)){ excluded = true; break; }
    for(size_t i = 0; i < s->cfg->n_generated; i++) if(ce_path_match(s->cfg->generated[i], reln, true)){ generated = true; break; }
    if(excluded || generated){
        /* include_overrides can re-include */
        bool reinclude = false;
        for(size_t i = 0; i < s->cfg->n_overrides; i++) if(ce_path_match(s->cfg->overrides[i], reln, true)){ reinclude = true; break; }
        if(!reinclude){ ce_free(reln); return 0; }
    }
    const char *cat = classify(s, reln);
    if(!cat){ ce_free(reln); return 0; }

    /* read and count lines */
    size_t flen = 0;
    char *data = wu_read_file(full, &flen);
    size_t lines = 0;
    bool binary = false;
    if(data){
        for(size_t i = 0; i < flen; i++) if(data[i] == '\0'){ binary = true; break; }
        if(!binary){
            lines = 0;
            for(size_t i = 0; i < flen; i++) if(data[i] == '\n') lines++;
            if(flen > 0 && data[flen-1] != '\n') lines++;
        }
        ce_free(data);
    } else {
        /* unreadable file: surface in report */
        ce_json *e = ce_json_new_obj(&s->arena);
        ce_json_obj_set(&s->arena, e, "path", ce_json_new_str(&s->arena, reln));
        ce_json_obj_set(&s->arena, e, "category", ce_json_new_str(&s->arena, cat));
        ce_json_obj_set(&s->arena, e, "unreadable", ce_json_new_bool(&s->arena, true));
        ce_json_arr_push(&s->arena, s->detail, e);
        ce_free(reln);
        return 0;
    }
    if(binary){ ce_free(reln); return 0; }

    lc_cat *c = NULL;
    if(cat == CAT_SOURCE) c = &s->source;
    else if(cat == CAT_TEST) c = &s->test;
    else if(cat == CAT_DOC) c = &s->doc;
    else if(cat == CAT_CONFIG) c = &s->config;
    else c = &s->other;
    c->files++; c->lines += lines;
    s->total_files++; s->total_lines += lines;

    /* detail entry */
    {
        ce_json *e = ce_json_new_obj(&s->arena);
        ce_json_obj_set(&s->arena, e, "path", ce_json_new_str(&s->arena, reln));
        ce_json_obj_set(&s->arena, e, "category", ce_json_new_str(&s->arena, cat));
        ce_json_obj_set(&s->arena, e, "lines", ce_json_new_int(&s->arena, (int64_t)lines));
        ce_json_arr_push(&s->arena, s->detail, e);
    }
    ce_free(reln);
    return 0;
}

static void usage(const char *prog){
    fprintf(stderr, "usage: %s <root> <config> [--detail] [--output <json>]\n", prog);
}

int wmain(int argc, wchar_t **wargv){
    char **argv = ce_malloc((size_t)(argc + 1) * sizeof(char*));
    for(int i = 0; i < argc; i++) argv[i] = wu_w_to_u8(wargv[i]);
    argv[argc] = NULL;

    bool detail = false;
    const char *out = NULL;
    const char *root = NULL, *cfg = NULL;
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "--detail") == 0) detail = true;
        else if(strcmp(argv[i], "--output") == 0 && i + 1 < argc) out = argv[++i];
        else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0){ usage(argv[0]); return EXIT_OK; }
        else if(!root) root = argv[i];
        else if(!cfg) cfg = argv[i];
        else { usage(argv[0]); return EXIT_USAGE; }
    }
    if(!root || !cfg){ usage(argv[0]); return EXIT_USAGE; }

    if(!wu_is_dir(root)){ ce_log_error("root directory not found: %s", root); return EXIT_READ; }

    lc_config lcfg;
    int rc = load_config(cfg, &lcfg);
    if(rc != EXIT_OK) return rc;

    scan_ctx s; memset(&s, 0, sizeof(s));
    s.root = wu_norm_sep(root);
    s.cfg = &lcfg;
    ce_arena_init(&s.arena);
    s.json_root = ce_json_new_obj(&s.arena);
    s.detail = ce_json_new_arr(&s.arena);

    wu_walk_dir(s.root, lcfg.follow_reparse, walk_cb, &s);

    /* build JSON report */
    ce_json_obj_set(&s.arena, s.json_root, "tool", ce_json_new_str(&s.arena, "locscan"));
    ce_json_obj_set(&s.arena, s.json_root, "root", ce_json_new_str(&s.arena, s.root));
    ce_json_obj_set(&s.arena, s.json_root, "total_files", ce_json_new_int(&s.arena, (int64_t)s.total_files));
    ce_json_obj_set(&s.arena, s.json_root, "total_lines", ce_json_new_int(&s.arena, (int64_t)s.total_lines));
    ce_json_obj_set(&s.arena, s.json_root, "documentation_lines", ce_json_new_int(&s.arena, (int64_t)s.doc.lines));

    ce_json *cats = ce_json_new_obj(&s.arena);
    #define ADDCAT(name, cat) do { ce_json *o = ce_json_new_obj(&s.arena); \
        ce_json_obj_set(&s.arena, o, "files", ce_json_new_int(&s.arena, (int64_t)(cat).files)); \
        ce_json_obj_set(&s.arena, o, "lines", ce_json_new_int(&s.arena, (int64_t)(cat).lines)); \
        ce_json_obj_set(&s.arena, cats, name, o); }while(0)
    ADDCAT("source", s.source);
    ADDCAT("test", s.test);
    ADDCAT("documentation", s.doc);
    ADDCAT("config", s.config);
    ADDCAT("other", s.other);
    #undef ADDCAT
    ce_json_obj_set(&s.arena, s.json_root, "categories", cats);
    if(detail) ce_json_obj_set(&s.arena, s.json_root, "files", s.detail);

    char *json = ce_json_to_string(s.json_root);

    /* human summary to stdout */
    printf("locscan: %zu files, %zu lines total\n", s.total_files, s.total_lines);
    printf("  source:        %zu files, %zu lines\n", s.source.files, s.source.lines);
    printf("  test:          %zu files, %zu lines\n", s.test.files, s.test.lines);
    printf("  documentation: %zu files, %zu lines\n", s.doc.files, s.doc.lines);
    printf("  config:        %zu files, %zu lines\n", s.config.files, s.config.lines);
    printf("  other:         %zu files, %zu lines\n", s.other.files, s.other.lines);

    if(out){
        FILE *f = fopen(out, "wb");
        if(!f){ ce_log_error("cannot write output '%s'", out); rc = EXIT_WRITE; }
        else { fwrite(json, 1, strlen(json), f); fclose(f); }
    } else {
        printf("%s\n", json);
    }

    ce_free(json);
    ce_free(s.root);
    ce_arena_free(&s.arena);
    for(int i = 0; i < argc; i++) ce_free(argv[i]);
    ce_free(argv);
    return rc;
}
