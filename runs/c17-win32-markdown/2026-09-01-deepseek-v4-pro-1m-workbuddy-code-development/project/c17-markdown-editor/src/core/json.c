/* json.c - JSON DOM parser/serializer.
 * Nodes are allocated from an arena; growable array/object storage uses
 * malloc'd vectors (freed when the arena is freed via ce_json_free_storage). */
#include "json.h"
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"

typedef struct {
    ce_arena *a;
    const char *s;
    size_t pos;
    size_t len;
} parser;

static ce_json *parse_value(parser *p);

static void skip_ws(parser *p){
    while(p->pos < p->len){
        char c = p->s[p->pos];
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static int parse_hex4(parser *p, uint32_t *out){
    if(p->pos + 4 > p->len) return -1;
    uint32_t v = 0;
    for(int i = 0; i < 4; i++){
        char c = p->s[p->pos + i];
        int d;
        if(c >= '0' && c <= '9') d = c - '0';
        else if(c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if(c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = (v << 4) | (uint32_t)d;
    }
    p->pos += 4;
    *out = v;
    return 0;
}

static int append_cp(ce_buf *b, uint32_t cp){
    uint8_t enc[4];
    int n = ce_utf8_encode(cp, enc);
    ce_buf_append(b, enc, (size_t)n);
    return 0;
}

static ce_json *parse_string(parser *p){
    if(p->s[p->pos] != '"') return NULL;
    p->pos++;
    ce_buf b; ce_buf_init(&b);
    while(p->pos < p->len){
        unsigned char c = (unsigned char)p->s[p->pos];
        if(c == '"'){ p->pos++; ce_json *v = ce_json_new_strn(p->a, b.data ? b.data : "", b.len); ce_buf_free(&b); return v; }
        if(c == '\\'){
            p->pos++;
            if(p->pos >= p->len){ ce_buf_free(&b); return NULL; }
            char e = p->s[p->pos++];
            switch(e){
                case '"': ce_buf_append_c(&b, '"'); break;
                case '\\': ce_buf_append_c(&b, '\\'); break;
                case '/': ce_buf_append_c(&b, '/'); break;
                case 'b': ce_buf_append_c(&b, '\b'); break;
                case 'f': ce_buf_append_c(&b, '\f'); break;
                case 'n': ce_buf_append_c(&b, '\n'); break;
                case 'r': ce_buf_append_c(&b, '\r'); break;
                case 't': ce_buf_append_c(&b, '\t'); break;
                case 'u': {
                    uint32_t cp;
                    if(parse_hex4(p, &cp) != 0){ ce_buf_free(&b); return NULL; }
                    if(cp >= 0xD800 && cp <= 0xDBFF){
                        if(p->pos + 1 < p->len && p->s[p->pos] == '\\' && p->s[p->pos+1] == 'u'){
                            p->pos += 2;
                            uint32_t lo;
                            if(parse_hex4(p, &lo) != 0 || lo < 0xDC00 || lo > 0xDFFF){ ce_buf_free(&b); return NULL; }
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else { ce_buf_free(&b); return NULL; }
                    }
                    append_cp(&b, cp);
                    break;
                }
                default: ce_buf_free(&b); return NULL;
            }
        } else if(c < 0x20){ ce_buf_free(&b); return NULL; }
        else {
            ce_buf_append_c(&b, (char)c);
            p->pos++;
        }
    }
    ce_buf_free(&b);
    return NULL;
}

static ce_json *parse_number(parser *p){
    size_t start = p->pos;
    bool is_double = false;
    if(p->pos < p->len && p->s[p->pos] == '-') p->pos++;
    while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    if(p->pos < p->len && p->s[p->pos] == '.'){
        is_double = true; p->pos++;
        while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    if(p->pos < p->len && (p->s[p->pos] == 'e' || p->s[p->pos] == 'E')){
        is_double = true; p->pos++;
        if(p->pos < p->len && (p->s[p->pos] == '+' || p->s[p->pos] == '-')) p->pos++;
        while(p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    size_t n = p->pos - start;
    if(n == 0) return NULL;
    char tmp[64];
    if(n >= sizeof(tmp)) return NULL;
    memcpy(tmp, p->s + start, n); tmp[n] = 0;
    if(is_double) return ce_json_new_double(p->a, strtod(tmp, NULL));
    return ce_json_new_int(p->a, (int64_t)strtoll(tmp, NULL, 10));
}

static ce_json *parse_array(parser *p){
    p->pos++;
    ce_json *arr = ce_json_new_arr(p->a);
    skip_ws(p);
    if(p->pos < p->len && p->s[p->pos] == ']'){ p->pos++; return arr; }
    for(;;){
        skip_ws(p);
        ce_json *v = parse_value(p);
        if(!v) return NULL;
        ce_json_arr_push(p->a, arr, v);
        skip_ws(p);
        if(p->pos >= p->len) return NULL;
        if(p->s[p->pos] == ','){ p->pos++; continue; }
        if(p->s[p->pos] == ']'){ p->pos++; return arr; }
        return NULL;
    }
}

static ce_json *parse_object(parser *p){
    p->pos++;
    ce_json *obj = ce_json_new_obj(p->a);
    skip_ws(p);
    if(p->pos < p->len && p->s[p->pos] == '}'){ p->pos++; return obj; }
    for(;;){
        skip_ws(p);
        if(p->pos >= p->len || p->s[p->pos] != '"') return NULL;
        ce_json *k = parse_string(p);
        if(!k) return NULL;
        skip_ws(p);
        if(p->pos >= p->len || p->s[p->pos] != ':') return NULL;
        p->pos++;
        skip_ws(p);
        ce_json *v = parse_value(p);
        if(!v) return NULL;
        ce_json_obj_set(p->a, obj, k->u.str.s, v);
        skip_ws(p);
        if(p->pos >= p->len) return NULL;
        if(p->s[p->pos] == ','){ p->pos++; continue; }
        if(p->s[p->pos] == '}'){ p->pos++; return obj; }
        return NULL;
    }
}

static ce_json *parse_value(parser *p){
    skip_ws(p);
    if(p->pos >= p->len) return NULL;
    char c = p->s[p->pos];
    if(c == '"') return parse_string(p);
    if(c == '{') return parse_object(p);
    if(c == '[') return parse_array(p);
    if(c == 't' && !strncmp(p->s + p->pos, "true", 4)){ p->pos += 4; return ce_json_new_bool(p->a, true); }
    if(c == 'f' && !strncmp(p->s + p->pos, "false", 5)){ p->pos += 5; return ce_json_new_bool(p->a, false); }
    if(c == 'n' && !strncmp(p->s + p->pos, "null", 4)){ p->pos += 4; return ce_json_new_null(p->a); }
    if(c == '-' || (c >= '0' && c <= '9')) return parse_number(p);
    return NULL;
}

ce_json *ce_json_parse(ce_arena *a, const char *s, size_t *errpos){
    parser p; p.a = a; p.s = s; p.pos = 0; p.len = strlen(s);
    ce_json *v = parse_value(&p);
    skip_ws(&p);
    if(!v || p.pos != p.len){
        if(errpos) *errpos = p.pos;
        return NULL;
    }
    return v;
}

/* ---------------- builders ---------------- */

ce_json *ce_json_new_null(ce_arena *a){ ce_json *v = ce_arena_alloc(a, sizeof(*v)); memset(v, 0, sizeof(*v)); v->type = CEJ_NULL; return v; }
ce_json *ce_json_new_bool(ce_arena *a, bool b){ ce_json *v = ce_json_new_null(a); v->type = CEJ_BOOL; v->u.b = b; return v; }
ce_json *ce_json_new_int(ce_arena *a, int64_t i){ ce_json *v = ce_json_new_null(a); v->type = CEJ_INT; v->u.i = i; return v; }
ce_json *ce_json_new_double(ce_arena *a, double d){ ce_json *v = ce_json_new_null(a); v->type = CEJ_DOUBLE; v->u.d = d; return v; }
ce_json *ce_json_new_strn(ce_arena *a, const char *s, size_t n){ ce_json *v = ce_json_new_null(a); v->type = CEJ_STR; v->u.str.s = ce_arena_strndup(a, s, n); v->u.str.len = n; return v; }
ce_json *ce_json_new_str(ce_arena *a, const char *s){ return ce_json_new_strn(a, s, strlen(s)); }
ce_json *ce_json_new_arr(ce_arena *a){ ce_json *v = ce_json_new_null(a); v->type = CEJ_ARR; return v; }
ce_json *ce_json_new_obj(ce_arena *a){ ce_json *v = ce_json_new_null(a); v->type = CEJ_OBJ; return v; }

void ce_json_arr_push(ce_arena *a, ce_json *arr, ce_json *v){
    (void)a;
    if(arr->u.arr.count == 0){
        arr->u.arr.items = ce_malloc(8 * sizeof(ce_json*));
        /* store capacity implicitly via a hidden approach: we keep cap in a side field.
         * Since we can't add fields to the public struct here without changing header,
         * we use a fixed growth approach with realloc based on count. */
    }
    /* Grow using realloc; capacity tracked by a hidden allocation header is avoided
     * by using a power-of-two realloc heuristic based on count. */
    /* Reallocate whenever count is a power of two (plus initial). */
    size_t cnt = arr->u.arr.count;
    if(cnt != 0 && (cnt & (cnt - 1)) == 0){
        arr->u.arr.items = ce_realloc(arr->u.arr.items, cnt * 2 * sizeof(ce_json*));
    }
    arr->u.arr.items[cnt] = v;
    arr->u.arr.count = cnt + 1;
}

void ce_json_obj_set(ce_arena *a, ce_json *obj, const char *key, ce_json *v){
    /* check for existing key */
    for(size_t i = 0; i < obj->u.obj.count; i++){
        if(strcmp(obj->u.obj.keys[i], key) == 0){
            obj->u.obj.vals[i] = v;
            return;
        }
    }
    size_t cnt = obj->u.obj.count;
    if(cnt == 0){
        obj->u.obj.keys = ce_malloc(8 * sizeof(char*));
        obj->u.obj.vals = ce_malloc(8 * sizeof(ce_json*));
    } else if((cnt & (cnt - 1)) == 0){
        obj->u.obj.keys = ce_realloc(obj->u.obj.keys, cnt * 2 * sizeof(char*));
        obj->u.obj.vals = ce_realloc(obj->u.obj.vals, cnt * 2 * sizeof(ce_json*));
    }
    obj->u.obj.keys[cnt] = ce_arena_strdup(a, key);
    obj->u.obj.vals[cnt] = v;
    obj->u.obj.count = cnt + 1;
}

ce_json *ce_json_obj_get(const ce_json *obj, const char *key){
    if(!obj || obj->type != CEJ_OBJ) return NULL;
    for(size_t i = 0; i < obj->u.obj.count; i++)
        if(strcmp(obj->u.obj.keys[i], key) == 0) return obj->u.obj.vals[i];
    return NULL;
}

const char *ce_json_str(const ce_json *v){ return (v && v->type == CEJ_STR) ? v->u.str.s : ""; }
int64_t ce_json_int(const ce_json *v, int64_t def){
    if(!v) return def;
    if(v->type == CEJ_INT) return v->u.i;
    if(v->type == CEJ_DOUBLE) return (int64_t)v->u.d;
    return def;
}
bool ce_json_bool(const ce_json *v, bool def){ return (v && v->type == CEJ_BOOL) ? v->u.b : def; }

/* ---------------- serializer ---------------- */

static void write_escaped(const char *s, size_t n, void *ctx, void (*emit)(void*, const char*, size_t)){
    emit(ctx, "\"", 1);
    for(size_t i = 0; i < n; i++){
        unsigned char c = (unsigned char)s[i];
        switch(c){
            case '"': emit(ctx, "\\\"", 2); break;
            case '\\': emit(ctx, "\\\\", 2); break;
            case '\n': emit(ctx, "\\n", 2); break;
            case '\r': emit(ctx, "\\r", 2); break;
            case '\t': emit(ctx, "\\t", 2); break;
            case '\b': emit(ctx, "\\b", 2); break;
            case '\f': emit(ctx, "\\f", 2); break;
            default:
                if(c < 0x20){ char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); emit(ctx, b, 6); }
                else emit(ctx, (const char*)&c, 1);
        }
    }
    emit(ctx, "\"", 1);
}

void ce_json_write(ce_json *v, void *ctx, void (*emit)(void*, const char*, size_t)){
    if(!v){ emit(ctx, "null", 4); return; }
    char b[64];
    switch(v->type){
        case CEJ_NULL: emit(ctx, "null", 4); break;
        case CEJ_BOOL: emit(ctx, v->u.b ? "true" : "false", v->u.b ? 4 : 5); break;
        case CEJ_INT: snprintf(b, sizeof(b), "%lld", (long long)v->u.i); emit(ctx, b, strlen(b)); break;
        case CEJ_DOUBLE: snprintf(b, sizeof(b), "%.17g", v->u.d); emit(ctx, b, strlen(b)); break;
        case CEJ_STR: write_escaped(v->u.str.s, v->u.str.len, ctx, emit); break;
        case CEJ_ARR:
            emit(ctx, "[", 1);
            for(size_t i = 0; i < v->u.arr.count; i++){
                if(i) emit(ctx, ",", 1);
                ce_json_write(v->u.arr.items[i], ctx, emit);
            }
            emit(ctx, "]", 1);
            break;
        case CEJ_OBJ:
            emit(ctx, "{", 1);
            for(size_t i = 0; i < v->u.obj.count; i++){
                if(i) emit(ctx, ",", 1);
                write_escaped(v->u.obj.keys[i], strlen(v->u.obj.keys[i]), ctx, emit);
                emit(ctx, ":", 1);
                ce_json_write(v->u.obj.vals[i], ctx, emit);
            }
            emit(ctx, "}", 1);
            break;
    }
}

typedef struct { ce_buf *b; } emit_ctx;
static void emit_to_buf(void *ctx, const char *s, size_t n){ ce_buf_append(((emit_ctx*)ctx)->b, s, n); }

char *ce_json_to_string(ce_json *v){
    ce_buf b; ce_buf_init(&b);
    emit_ctx ec; ec.b = &b;
    ce_json_write(v, &ec, emit_to_buf);
    return ce_buf_detach(&b);
}
