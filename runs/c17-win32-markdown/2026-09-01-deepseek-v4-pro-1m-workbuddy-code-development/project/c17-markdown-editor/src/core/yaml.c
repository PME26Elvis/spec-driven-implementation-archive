/* yaml.c - minimal YAML subset -> ce_json DOM. */
#include "yaml.h"
#include "ce_common.h"

/* Trim leading/trailing ASCII whitespace. Returns trimmed span. */
static char *trim(char *s){
    while(*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while(n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) n--;
    s[n] = 0;
    return s;
}

/* Strip a trailing inline comment (# ...) not inside quotes. */
static void strip_comment(char *s){
    bool inq = false; char q = 0;
    for(char *p = s; *p; p++){
        if((*p == '"' || *p == '\'') && !inq){ inq = true; q = *p; }
        else if(inq && *p == q){ inq = false; }
        else if(!inq && *p == '#'){ *p = 0; return; }
    }
}

static char *unquote(char *s){
    size_t n = strlen(s);
    if(n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\''))){
        s[n-1] = 0;
        return s + 1;
    }
    return s;
}

static ce_json *scalar_to_json(ce_arena *a, const char *raw){
    char *s = ce_arena_strdup(a, raw);
    strip_comment(s);
    s = trim(s);
    s = unquote(s);
    if(strcmp(s, "true") == 0) return ce_json_new_bool(a, true);
    if(strcmp(s, "false") == 0) return ce_json_new_bool(a, false);
    if(strcmp(s, "null") == 0 || strcmp(s, "~") == 0) return ce_json_new_null(a);
    /* number? */
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if(end && *end == 0 && end != s && *s != '+' && s[0] != '0') return ce_json_new_int(a, (int64_t)v);
    if(end && *end == 0 && end != s && strcmp(s, "0") == 0) return ce_json_new_int(a, 0);
    return ce_json_new_str(a, s);
}

/* Parse inline [a, b, c] list. s points at '['. */
static ce_json *parse_inline_list(ce_arena *a, const char *s){
    ce_json *arr = ce_json_new_arr(a);
    const char *p = s + 1;
    while(*p){
        while(*p == ' ' || *p == '\t' || *p == ',') p++;
        if(*p == ']') return arr;
        if(*p == '"' || *p == '\''){
            char q = *p++;
            const char *start = p;
            while(*p && *p != q) p++;
            char *item = ce_arena_strndup(a, start, (size_t)(p - start));
            ce_json_arr_push(a, arr, ce_json_new_str(a, item));
            if(*p == q) p++;
        } else {
            const char *start = p;
            while(*p && *p != ',' && *p != ']') p++;
            char *item = ce_arena_strndup(a, start, (size_t)(p - start));
            item = trim(item);
            ce_json_arr_push(a, arr, scalar_to_json(a, item));
        }
    }
    return arr;
}

ce_json *ce_yaml_parse(ce_arena *a, const char *text, int *errline){
    ce_json *obj = ce_json_new_obj(a);
    /* copy into a mutable buffer so we can trim lines */
    size_t len = strlen(text);
    char *buf = ce_malloc(len + 1);
    memcpy(buf, text, len + 1);

    size_t pos = 0;
    int line = 0;
    while(pos < len){
        /* extract one line */
        size_t start = pos;
        while(pos < len && buf[pos] != '\n') pos++;
        size_t line_end = pos;
        if(pos < len) pos++; /* skip newline */
        line++;

        char *ln = buf + start;
        buf[line_end] = 0; /* null-terminate line in place */
        /* strip trailing \r */
        size_t lnlen = strlen(ln);
        if(lnlen && ln[lnlen-1] == '\r') ln[lnlen-1] = 0;

        char *t = trim(ln);
        if(*t == 0) continue;           /* blank */
        if(*t == '#') continue;         /* comment */

        /* must be a top-level "key: ..." */
        char *colon = strchr(t, ':');
        if(!colon){
            if(errline) *errline = line;
            ce_free(buf);
            return NULL;
        }
        *colon = 0;
        char *key = trim(t);
        char *rest = trim(colon + 1);
        if(*key == 0){
            if(errline) *errline = line;
            ce_free(buf);
            return NULL;
        }

        if(*rest == 0){
            /* block sequence: look ahead for indented "- item" lines */
            size_t save = pos;
            int save_line = line;
            ce_json *arr = ce_json_new_arr(a);
            for(;;){
                size_t p2 = pos;
                while(p2 < len && buf[p2] != '\n') p2++;
                size_t le = p2;
                /* peek line without consuming */
                char saved = buf[le];
                buf[le] = 0;
                char *pl = buf + pos;
                if(strlen(pl) && pl[strlen(pl)-1] == '\r') pl[strlen(pl)-1] = 0;
                char *pt = trim(pl);
                if(*pt == '-' && (pt[1] == ' ' || pt[1] == '\t' || pt[1] == 0)){
                    char *item = trim(pt + 1);
                    char *copy = ce_arena_strdup(a, item);
                    buf[le] = saved;   /* restore before advancing */
                    pos = (p2 < len) ? p2 + 1 : p2;
                    line++;
                    ce_json_arr_push(a, arr, scalar_to_json(a, copy));
                    continue;
                }
                buf[le] = saved;
                break;
            }
            if(arr->u.arr.count == 0){
                /* empty block -> empty array */
            }
            ce_json_obj_set(a, obj, key, arr);
            (void)save; (void)save_line;
        } else if(*rest == '['){
            ce_json_obj_set(a, obj, key, parse_inline_list(a, rest));
        } else {
            ce_json_obj_set(a, obj, key, scalar_to_json(a, rest));
        }
    }
    ce_free(buf);
    return obj;
}
