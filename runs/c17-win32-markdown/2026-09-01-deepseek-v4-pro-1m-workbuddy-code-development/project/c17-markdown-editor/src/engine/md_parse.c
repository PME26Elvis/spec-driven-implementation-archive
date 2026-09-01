/* md_parse.c - Markdown block/inline parser producing a source-mapped tree. */
#include "md.h"
#include "ce_common.h"
#include "utf8.h"

/* ============================================================ inline list */

typedef struct {
    md_inline **items;
    size_t n, cap;
} IList;

static void il_push(IList *l, md_inline *inl){
    if(l->n == l->cap){ l->cap = l->cap ? l->cap * 2 : 8; l->items = ce_realloc(l->items, l->cap * sizeof(md_inline*)); }
    l->items[l->n++] = inl;
}

static md_inline *inl_new(int type, size_t start, size_t end){
    md_inline *inl = ce_calloc(1, sizeof(md_inline));
    inl->type = type; inl->start = start; inl->end = end;
    inl->cstart = start; inl->cend = end;
    return inl;
}

/* ============================================================ helpers */

static bool is_ws_char(char c){ return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static bool is_punct_char(char c){
    if(c >= '0' && c <= '9') return false;
    if(c >= 'a' && c <= 'z') return false;
    if(c >= 'A' && c <= 'Z') return false;
    if(c < 0) return true;           /* non-ASCII treated as non-punct for flanking */
    return (c >= 0x21 && c <= 0x7E) || c > 0x7E;
}
static bool is_alnum_char(char c){
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

typedef struct {
    md_doc *doc;
} PS;

static const char *src_at(const PS *ps){ return ps->doc->src; }

/* ============================================================ inline parse */

typedef struct {
    PS *ps;
    IList *list;
} IP;

static void add_text(IP *ip, size_t start, size_t end){
    if(start >= end) return;
    const char *s = src_at(ip->ps);
    md_inline *inl = inl_new(MD_INL_TEXT, start, end);
    inl->text = ce_strndup(s + start, end - start);
    il_push(ip->list, inl);
}

/* left/right flanking for '*'/'_' */
static bool left_flanking(const char *s, size_t pos){
    /* can open if next char is non-whitespace and (next not punct or prev is ws/punct) */
    unsigned char n = (unsigned char)s[pos + 1];
    if(n == 0 || is_ws_char((char)n)) return false;
    unsigned char p = (pos > 0) ? (unsigned char)s[pos - 1] : ' ';
    if(!is_punct_char((char)n)) return true;
    return is_ws_char((char)p) || is_punct_char((char)p);
}
static bool right_flanking(const char *s, size_t pos){
    unsigned char p = (pos > 0) ? (unsigned char)s[pos - 1] : ' ';
    if(is_ws_char((char)p)) return false;
    unsigned char n = (unsigned char)s[pos + 1];
    if(!is_punct_char((char)p)) return true;
    return n == 0 || is_ws_char((char)n) || is_punct_char((char)n);
}

static size_t run_len(const char *s, size_t pos, char c, size_t end){
    size_t n = 0;
    while(pos + n < end && s[pos + n] == c) n++;
    return n;
}

/* find a closing run of `c` of length >= need, right-flanking, after `from` */
static int find_close(const char *s, size_t from, size_t end, char c, int need, size_t *out){
    for(size_t i = from; i < end; i++){
        if(s[i] == c){
            size_t r = run_len(s, i, c, end);
            if((int)r >= need && right_flanking(s, i)){
                /* for '_' disallow intraword */
                if(c == '_'){
                    unsigned char prev = (i > 0) ? (unsigned char)s[i-1] : ' ';
                    unsigned char next = (i + (size_t)need < end) ? (unsigned char)s[i + need] : ' ';
                    if(is_alnum_char((char)prev) && is_alnum_char((char)next)) continue;
                }
                *out = i;
                return 0;
            }
            i += r - 1;
        }
    }
    return -1;
}

static void parse_inlines_rec(IP *ip, size_t start, size_t end);

static void parse_inlines_rec(IP *ip, size_t start, size_t end){
    const char *s = src_at(ip->ps);
    size_t i = start;
    size_t text_start = start;

    #define FLUSH_TEXT() do { add_text(ip, text_start, i); } while(0)

    while(i < end){
        unsigned char c = (unsigned char)s[i];
        if(c < 0x80){
            if(c == '\\'){
                if(i + 1 < end && is_punct_char(s[i+1])){ i += 2; continue; }
            } else if(c == '`'){
                size_t r = run_len(s, i, '`', end);
                /* find closing run of exactly r backticks */
                size_t j = i + r;
                bool found = false; size_t close = 0;
                while(j < end){
                    if(s[j] == '`'){
                        size_t rr = run_len(s, j, '`', end);
                        if(rr == r){ close = j; found = true; break; }
                        j += rr;
                    } else j++;
                }
                if(found){
                    FLUSH_TEXT();
                    md_inline *inl = inl_new(MD_INL_CODE, i, close + r);
                    inl->cstart = i + r; inl->cend = close;
                    inl->text = ce_strndup(s + i + r, close - (i + r));
                    il_push(ip->list, inl);
                    i = close + r; text_start = i; continue;
                } else {
                    i += r; continue;
                }
            } else if(c == '~' && i + 1 < end && s[i+1] == '~'){
                size_t r = run_len(s, i, '~', end);
                if(r >= 2){
                    /* strikethrough with 2 tildes */
                    size_t close = 0;
                    if(find_close(s, i + 2, end, '~', 2, &close) == 0){
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_STRIKE, i, close + 2);
                        inl->cstart = i + 2; inl->cend = close;
                        IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                        parse_inlines_rec(&sub, i + 2, close);
                        inl->children = sl.items; inl->nchildren = sl.n; inl->capchildren = sl.cap;
                        il_push(ip->list, inl);
                        i = close + 2; text_start = i; continue;
                    }
                }
                i += (r >= 2 ? 2 : 1); continue;
            } else if(c == '*' || c == '_'){
                size_t r = run_len(s, i, c, end);
                /* combined strong + emphasis (3+ delimiters) */
                if(r >= 3 && left_flanking(s, i)){
                    size_t close = 0;
                    if(find_close(s, i + 3, end, c, 3, &close) == 0){
                        FLUSH_TEXT();
                        md_inline *strong = inl_new(MD_INL_STRONG, i, close + 3);
                        strong->cstart = i + 3; strong->cend = close;
                        md_inline *emph = inl_new(MD_INL_EMPH, i + 2, close + 1);
                        emph->cstart = i + 3; emph->cend = close;
                        IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                        parse_inlines_rec(&sub, i + 3, close);
                        emph->children = sl.items; emph->nchildren = sl.n; emph->capchildren = sl.cap;
                        strong->children = ce_malloc(sizeof(md_inline*));
                        strong->children[0] = emph; strong->nchildren = 1; strong->capchildren = 1;
                        il_push(ip->list, strong);
                        i = close + 3; text_start = i; continue;
                    }
                }
                /* strong */
                if(r >= 2 && left_flanking(s, i)){
                    size_t close = 0;
                    if(find_close(s, i + 2, end, c, 2, &close) == 0){
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_STRONG, i, close + 2);
                        inl->cstart = i + 2; inl->cend = close;
                        IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                        parse_inlines_rec(&sub, i + 2, close);
                        inl->children = sl.items; inl->nchildren = sl.n; inl->capchildren = sl.cap;
                        il_push(ip->list, inl);
                        i = close + 2; text_start = i; continue;
                    }
                }
                if(left_flanking(s, i)){
                    size_t close = 0;
                    if(find_close(s, i + 1, end, c, 1, &close) == 0){
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_EMPH, i, close + 1);
                        inl->cstart = i + 1; inl->cend = close;
                        IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                        parse_inlines_rec(&sub, i + 1, close);
                        inl->children = sl.items; inl->nchildren = sl.n; inl->capchildren = sl.cap;
                        il_push(ip->list, inl);
                        i = close + 1; text_start = i; continue;
                    }
                }
                i += r; continue;
            } else if(c == '!' && i + 1 < end && s[i+1] == '['){
                /* image */
                size_t open = i + 1;
                size_t depth = 1; size_t j = open + 1;
                while(j < end && depth){
                    if(s[j] == '[') depth++;
                    else if(s[j] == ']') depth--;
                    j++;
                }
                if(depth == 0){
                    size_t label_end = j - 1;
                    if(j < end && s[j] == '('){
                        /* find closing paren */
                        size_t depth2 = 1; size_t k = j + 1;
                        while(k < end && depth2){
                            if(s[k] == '(') depth2++;
                            else if(s[k] == ')') depth2--;
                            k++;
                        }
                        if(depth2 == 0){
                            size_t close = k - 1;
                            FLUSH_TEXT();
                            md_inline *inl = inl_new(MD_INL_IMAGE, i, close + 1);
                            inl->cstart = open + 1; inl->cend = label_end;
                            /* parse dest */
                            const char *d = s + j + 1;
                            size_t dlen = close - (j + 1);
                            /* trim */
                            size_t a = 0, b = dlen;
                            while(a < b && is_ws_char(d[a])) a++;
                            while(b > a && is_ws_char(d[b-1])) b--;
                            if(a < b && d[a] == '<'){ a++; if(b > a && d[b-1] == '>') b--; }
                            size_t url_end = a;
                            while(url_end < b && !is_ws_char(d[url_end])) url_end++;
                            inl->url = ce_strndup(d + a, url_end - a);
                            /* title */
                            size_t t = url_end;
                            while(t < b && is_ws_char(d[t])) t++;
                            if(t < b){
                                if(d[t] == '"' || d[t] == '\''){
                                    char q = d[t++];
                                    size_t ts = t;
                                    while(t < b && d[t] != q) t++;
                                    inl->title = ce_strndup(d + ts, t - ts);
                                } else if(d[t] == '('){
                                    size_t ts = t + 1;
                                    size_t depth3 = 1; t++;
                                    while(t < b && depth3){ if(d[t]=='(') depth3++; else if(d[t]==')') depth3--; t++; }
                                    inl->title = ce_strndup(d + ts, t - 1 - ts);
                                }
                            }
                            IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                            parse_inlines_rec(&sub, open + 1, label_end);
                            inl->children = sl.items; inl->nchildren = sl.n; inl->capchildren = sl.cap;
                            il_push(ip->list, inl);
                            i = close + 1; text_start = i; continue;
                        }
                    }
                }
                i++; continue;
            } else if(c == '['){
                /* link */
                size_t depth = 1; size_t j = i + 1;
                while(j < end && depth){
                    if(s[j] == '[') depth++;
                    else if(s[j] == ']') depth--;
                    j++;
                }
                if(depth == 0 && j < end && s[j] == '('){
                    size_t label_end = j - 1;
                    size_t depth2 = 1; size_t k = j + 1;
                    while(k < end && depth2){
                        if(s[k] == '(') depth2++;
                        else if(s[k] == ')') depth2--;
                        k++;
                    }
                    if(depth2 == 0){
                        size_t close = k - 1;
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_LINK, i, close + 1);
                        inl->cstart = i + 1; inl->cend = label_end;
                        const char *d = s + j + 1;
                        size_t dlen = close - (j + 1);
                        size_t a = 0, b = dlen;
                        while(a < b && is_ws_char(d[a])) a++;
                        while(b > a && is_ws_char(d[b-1])) b--;
                        if(a < b && d[a] == '<'){ a++; if(b > a && d[b-1] == '>') b--; }
                        size_t url_end = a;
                        while(url_end < b && !is_ws_char(d[url_end])) url_end++;
                        inl->url = ce_strndup(d + a, url_end - a);
                        size_t t = url_end;
                        while(t < b && is_ws_char(d[t])) t++;
                        if(t < b && (d[t] == '"' || d[t] == '\'')){
                            char q = d[t++];
                            size_t ts = t;
                            while(t < b && d[t] != q) t++;
                            inl->title = ce_strndup(d + ts, t - ts);
                        } else if(t < b && d[t] == '('){
                            size_t ts = t + 1; size_t depth3 = 1; t++;
                            while(t < b && depth3){ if(d[t]=='(') depth3++; else if(d[t]==')') depth3--; t++; }
                            inl->title = ce_strndup(d + ts, t - 1 - ts);
                        }
                        IP sub; sub.ps = ip->ps; IList sl = {0,0,0}; sub.list = &sl;
                        parse_inlines_rec(&sub, i + 1, label_end);
                        inl->children = sl.items; inl->nchildren = sl.n; inl->capchildren = sl.cap;
                        il_push(ip->list, inl);
                        i = close + 1; text_start = i; continue;
                    }
                }
                i++; continue;
            } else if(c == '<'){
                /* autolink or html */
                size_t j = i + 1;
                while(j < end && s[j] != '>' && s[j] != ' ' && s[j] != '\n' && s[j] != '<') j++;
                if(j < end && s[j] == '>' && j > i + 1){
                    const char *inner = s + i + 1;
                    size_t ilen = j - (i + 1);
                    bool is_autolink = false;
                    /* scheme: */
                    for(size_t k = 0; k + 1 < ilen; k++){
                        if(inner[k] == ':'){
                            bool scheme_ok = is_alnum_char(inner[0]);
                            for(size_t m = 1; m < k; m++) if(!(is_alnum_char(inner[m]) || inner[m] == '+' || inner[m] == '.' || inner[m] == '-')) scheme_ok = false;
                            if(scheme_ok && k >= 1){ is_autolink = true; }
                            break;
                        }
                    }
                    if(!is_autolink && memchr(inner, '@', ilen)) is_autolink = true;
                    if(is_autolink){
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_AUTOLINK, i, j + 1);
                        inl->url = ce_strndup(inner, ilen);
                        il_push(ip->list, inl);
                        i = j + 1; text_start = i; continue;
                    }
                    /* html inline */
                    if(is_alnum_char(inner[0]) || inner[0] == '/'){
                        FLUSH_TEXT();
                        md_inline *inl = inl_new(MD_INL_HTML, i, j + 1);
                        inl->text = ce_strndup(s + i, j + 1 - i);
                        il_push(ip->list, inl);
                        i = j + 1; text_start = i; continue;
                    }
                }
                i++; continue;
            }
        }
        i++;
    }
    FLUSH_TEXT();
    #undef FLUSH_TEXT
}

/* ============================================================ block parse */

typedef struct {
    size_t start;   /* content start (after container/list prefixes) */
    size_t end;     /* content end (excluding newline) */
} Line;

typedef struct {
    md_doc *doc;
    Line *lines;
    size_t nlines;
} BS;

static const char *bsrc(const BS *bs){ return bs->doc->src; }


static bool line_blank(const BS *bs, const Line *l){
    const char *s = bsrc(bs) + l->start;
    for(size_t i = 0; i < l->end - l->start; i++) if(s[i] != ' ' && s[i] != '\t') return false;
    return true;
}

static size_t count_indent(const BS *bs, const Line *l){
    const char *s = bsrc(bs) + l->start;
    size_t n = 0;
    for(size_t i = 0; i < l->end - l->start; i++){
        if(s[i] == ' ') n++;
        else if(s[i] == '\t') n += 4 - (n % 4);
        else break;
    }
    return n;
}

static size_t skip_spaces(const BS *bs, size_t pos, size_t end){
    const char *s = bsrc(bs);
    while(pos < end && (s[pos] == ' ' || s[pos] == '\t')) pos++;
    return pos;
}

/* ATX heading: 1..6 '#' followed by space or end. Returns level or 0. */
static int atx_level(const BS *bs, const Line *l, size_t *content_start){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    while(i < len && s[i] == '#' ) i++;
    if(i >= 1 && i <= 6){
        if(i == len || s[i] == ' ' || s[i] == '\t'){
            size_t cs = skip_spaces(bs, l->start + i, l->end);
            *content_start = cs;
            return (int)i;
        }
    }
    return 0;
}

static bool setext_underline(const BS *bs, const Line *l, char *kind){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    while(i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if(i >= len) return false;
    char c = s[i];
    if(c != '=' && c != '-') return false;
    size_t j = i;
    while(j < len && s[j] == c) j++;
    while(j < len && (s[j] == ' ' || s[j] == '\t')) j++;
    if(j == len){ *kind = c; return true; }
    return false;
}

static bool thematic_break(const BS *bs, const Line *l){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    while(i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if(i >= len) return false;
    char c = s[i];
    if(c != '-' && c != '*' && c != '_') return false;
    size_t count = 0;
    while(i < len){
        if(s[i] == c) count++;
        else if(s[i] != ' ' && s[i] != '\t') return false;
        i++;
    }
    return count >= 3;
}

/* fence: 3+ backticks/tildes. Returns fence len or 0. */
static int fence_at(const BS *bs, const Line *l, char *fchar, size_t *info_start, size_t *info_end){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    while(i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if(i >= len) return 0;
    char c = s[i];
    if(c != '`' && c != '~') return 0;
    size_t n = 0;
    while(i < len && s[i] == c){ n++; i++; }
    if(n < 3) return 0;
    /* rest must be only spaces + optional info string (no backtick for backtick fences) */
    size_t info_s = i;
    while(i < len && s[i] != '`' && (c != '`' || s[i] != '`')) i++;
    /* for backtick fence, info string must not contain backtick */
    if(c == '`'){
        for(size_t k = info_s; k < len; k++) if(s[k] == '`') return 0;
    }
    /* trim trailing spaces of info */
    size_t info_e = i;
    while(info_e > info_s && (s[info_e-1] == ' ' || s[info_e-1] == '\t')) info_e--;
    *fchar = c; *info_start = info_s; *info_end = info_e;
    return (int)n;
}

static bool list_marker_at(const BS *bs, const Line *l, size_t indent, int *ordered, int *start_num, char *marker, size_t *content_start, size_t *marker_end, int *task, size_t *content_indent){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    (void)indent;
    if(i >= len) return false;
    char c = s[i];
    *task = -1;
    if(c == '-' || c == '+' || c == '*'){
        *ordered = 0; *start_num = 1; *marker = c;
        i = 1;
    } else if(c >= '0' && c <= '9'){
        size_t j = i;
        int num = 0; int digits = 0;
        while(j < len && s[j] >= '0' && s[j] <= '9'){ if(num <= 99999999) num = num * 10 + (s[j]-'0'); j++; digits++; }
        if(j >= len || (s[j] != '.' && s[j] != ')')) return false;
        if(digits > 9) return false;
        j++;
        *ordered = 1; *start_num = num; *marker = c;
        i = j;
    } else return false;
    /* require space after marker (or end of line) */
    size_t after = i;
    if(after < len && s[after] != ' ' && s[after] != '\t') return false;
    *marker_end = l->start + after;
    /* count padding spaces */
    size_t ci = after;
    size_t pad = 0;
    while(ci < len && s[ci] == ' ') { pad++; ci++; }
    while(ci < len && s[ci] == '\t') { pad += 4 - (pad % 4); ci++; }
    size_t marker_width = after; /* width of marker itself in columns */
    if(pad <= 4){
        *content_indent = marker_width + pad;
    } else {
        *content_indent = marker_width + 1;
        ci = after + 1; /* extra spaces become content */
    }
    /* task list */
    size_t tci = ci;
    if(tci + 2 < len && s[tci] == '[' && (s[tci+1] == ' ' || s[tci+1] == 'x' || s[tci+1] == 'X') && s[tci+2] == ']'){
        *task = (s[tci+1] == ' ') ? 0 : 1;
        ci = tci + 3;
        if(ci < len && s[ci] == ' ') ci++;
    }
    *content_start = l->start + ci;
    return true;
}

/* returns column count / detects separator line. Returns true and sets align array. */
static bool table_separator(const BS *bs, const Line *l, int **aligns, size_t *ncols){
    const char *s = bsrc(bs) + l->start;
    size_t len = l->end - l->start;
    size_t i = 0;
    while(i < len && (s[i] == ' ' || s[i] == '\t')) i++;
    if(i >= len) return false;
    bool has_pipe = false;
    for(size_t k = i; k < len; k++) if(s[k] == '|'){ has_pipe = true; break; }
    if(!has_pipe) return false;
    if(s[i] == '|') i++;
    size_t count = 0;
    size_t cap = 0;
    int *al = NULL;
    for(;;){
        size_t cs = i;
        while(i < len && s[i] != '|') i++;
        size_t ce = i;
        bool is_last = (i >= len);
        /* trim */
        size_t a = cs, b = ce;
        while(a < b && (s[a] == ' ' || s[a] == '\t')) a++;
        while(b > a && (s[b-1] == ' ' || s[b-1] == '\t')) b--;
        int left = 0, right = 0, dashes = 0;
        size_t j = a;
        if(j < b && s[j] == ':'){ left = 1; j++; }
        while(j < b && s[j] == '-'){ dashes++; j++; }
        if(j < b && s[j] == ':'){ right = 1; j++; }
        if(dashes >= 1 && j == b){
            int align = (left && right) ? 1 : (left ? 0 : (right ? 2 : -1));
            if(count == cap){ cap = cap ? cap*2 : 8; al = ce_realloc(al, cap * sizeof(int)); }
            al[count++] = align;
        } else if(dashes == 0 && a == b && is_last){
            /* trailing empty cell after a closing '|' — ignore */
        } else {
            if(al) ce_free(al);
            return false;
        }
        if(i >= len) break;
        i++; /* skip '|' */
    }
    if(count == 0){ if(al) ce_free(al); return false; }
    *aligns = al;
    *ncols = count;
    return true;
}

/* split a table row line into cells. Returns malloc'd array of strings. */
static char **split_table_row(const char *s, size_t start, size_t end, size_t *n){
    size_t cap = 8, count = 0;
    char **cells = ce_malloc(cap * sizeof(char*));
    size_t i = start;
    while(i < end && (s[i] == ' ' || s[i] == '\t')) i++;
    if(i < end && s[i] == '|') i++;
    for(;;){
        size_t cs = i;
        while(i < end && s[i] != '|') i++;
        size_t ce = i;
        bool is_last = (i >= end);
        size_t a = cs, b = ce;
        while(a < b && (s[a] == ' ' || s[a] == '\t')) a++;
        while(b > a && (s[b-1] == ' ' || s[b-1] == '\t')) b--;
        if(a == b && is_last){
            /* trailing empty cell after a closing '|' — drop */
            break;
        }
        if(count == cap){ cap *= 2; cells = ce_realloc(cells, cap * sizeof(char*)); }
        cells[count++] = ce_strndup(s + a, b - a);
        if(i >= end) break;
        i++;
    }
    *n = count;
    return cells;
}

/* ============================================================ block parse */

static void parse_blocks(BS *bs, size_t from, size_t to);

static void parse_blockquote(BS *bs, size_t from, size_t to, md_block *bq){
    /* collect contiguous blockquote lines, strip '>' prefix */
    size_t cap = 8, n = 0;
    Line *inner = ce_malloc(cap * sizeof(Line));
    size_t i = from;
    while(i < to){
        Line *l = &bs->lines[i];
        const char *s = bsrc(bs) + l->start;
        size_t len = l->end - l->start;
        /* find '>' after optional spaces */
        size_t p = 0;
        while(p < len && (s[p] == ' ' || s[p] == '\t')) p++;
        if(p >= len || s[p] != '>') break;
        p++;
        if(p < len && s[p] == ' ') p++;
        if(n == cap){ cap *= 2; inner = ce_realloc(inner, cap * sizeof(Line)); }
        inner[n].start = l->start + p;
        inner[n].end = l->end;
        n++;
        i++;
    }
    /* recursively parse inner lines as blocks (temporarily swap lines) */
    Line *saved = bs->lines; size_t saved_n = bs->nlines;
    bs->lines = inner; bs->nlines = n;
    /* create a temporary doc context: we parse blocks into bq->children.
     * Easiest: create a sub-document and move blocks. */
    md_doc *sub = ce_calloc(1, sizeof(md_doc));
    sub->src = bs->doc->src; sub->len = bs->doc->len;
    BS subbs = *bs; subbs.doc = sub; subbs.lines = inner; subbs.nlines = n;
    parse_blocks(&subbs, 0, n);
    for(size_t k = 0; k < sub->nblocks; k++) md_block_add_child(bq, sub->blocks[k]);
    ce_free(sub->blocks);
    ce_free(sub);
    bs->lines = saved; bs->nlines = saved_n;
    ce_free(inner);
}

static void parse_fenced_code(BS *bs, size_t from, size_t to, size_t line_idx, md_block *blk){
    /* line_idx is the opening fence; blk already created with info */
    char fchar = (char)blk->fence_char;
    int flen = blk->fence_len;
    size_t i = line_idx + 1;
    size_t content_start = bs->lines[line_idx].end; /* after opening line; need newline */
    /* find closing fence */
    size_t j = i;
    while(j < to){
        Line *l = &bs->lines[j];
        const char *s = bsrc(bs) + l->start;
        size_t len = l->end - l->start;
        size_t p = 0;
        while(p < len && (s[p] == ' ' || s[p] == '\t')) p++;
        if(p < len && s[p] == fchar){
            size_t k = p;
            while(k < len && s[k] == fchar) k++;
            size_t rest = k;
            while(rest < len && (s[rest] == ' ' || s[rest] == '\t')) rest++;
            if(k - p >= (size_t)flen && rest == len){
                /* closing fence */
                blk->end = bs->lines[j].end;
                (void)content_start;
                return;
            }
        }
        j++;
    }
    blk->end = (to > 0) ? bs->lines[to-1].end : bs->lines[line_idx].end;
}


/* parse list items starting at `from` for list `list` (already created). */
static size_t parse_list_items(BS *bs, size_t from, size_t to, md_block *list){
    size_t i = from;
    while(i < to){
        Line *l = &bs->lines[i];
        int ordered, start_num, task; char marker;
        size_t content_start, marker_end, content_indent;
        if(!list_marker_at(bs, l, 0, &ordered, &start_num, &marker, &content_start, &marker_end, &task, &content_indent)) break;
        /* item content lines: from marker line (content) through continuation */
        md_block *item = md_new_block(MD_BLOCK_LIST_ITEM, l->start, l->end);
        item->list_ordered = ordered; item->list_start = start_num; item->list_marker = marker; item->task = task;
        md_block_add_child(list, item);

        /* collect item content lines (recursively parsed) */
        size_t cap = 8, n = 0;
        Line *inner = ce_malloc(cap * sizeof(Line));
        if(n == cap){ cap *= 2; inner = ce_realloc(inner, cap * sizeof(Line)); }
        inner[n].start = content_start; inner[n].end = l->end; n++;

        size_t j = i + 1;
        while(j < to){
            Line *lj = &bs->lines[j];
            if(line_blank(bs, lj)){
                /* blank line: include if followed by indented content; else break */
                size_t k = j + 1;
                bool cont = false;
                while(k < to && line_blank(bs, &bs->lines[k])) k++;
                if(k < to){
                    size_t ind = count_indent(bs, &bs->lines[k]);
                    if(ind >= content_indent && content_indent > 0) cont = true;
                    else {
                        /* check if next is a new list marker at same level */
                        int o2, s2, t2; char m2; size_t cs2, me2, ci2;
                        if(!list_marker_at(bs, &bs->lines[k], 0, &o2, &s2, &m2, &cs2, &me2, &t2, &ci2)) cont = false;
                    }
                }
                if(cont){ /* include blank line(s) and continue */ 
                    while(j < to && line_blank(bs, &bs->lines[j])){ 
                        if(n == cap){ cap *= 2; inner = ce_realloc(inner, cap*sizeof(Line)); }
                        inner[n].start = bs->lines[j].start; inner[n].end = bs->lines[j].end; n++;
                        j++;
                    }
                    continue;
                }
                break;
            }
            size_t ind = count_indent(bs, lj);
            if(ind >= content_indent && content_indent > 0){
                if(n == cap){ cap *= 2; inner = ce_realloc(inner, cap*sizeof(Line)); }
                inner[n].start = lj->start + content_indent; inner[n].end = lj->end; n++;
                j++;
                continue;
            }
            /* lazy continuation: a non-blank line that is not a new block/list marker */
            int o2, s2, t2; char m2; size_t cs2, me2, ci2;
            if(list_marker_at(bs, lj, 0, &o2, &s2, &m2, &cs2, &me2, &t2, &ci2)) break;
            if(ind >= content_indent || content_indent == 0){
                /* treat as continuation if it's paragraph text */
                if(n == cap){ cap *= 2; inner = ce_realloc(inner, cap*sizeof(Line)); }
                inner[n].start = lj->start + (content_indent > 0 ? content_indent : 0); inner[n].end = lj->end; n++;
                j++;
                continue;
            }
            break;
        }

        /* parse item content recursively */
        Line *saved = bs->lines; size_t saved_n = bs->nlines;
        bs->lines = inner; bs->nlines = n;
        md_doc *sub = ce_calloc(1, sizeof(md_doc));
        sub->src = bs->doc->src; sub->len = bs->doc->len;
        BS subbs = *bs; subbs.doc = sub; subbs.lines = inner; subbs.nlines = n;
        parse_blocks(&subbs, 0, n);
        for(size_t k = 0; k < sub->nblocks; k++) md_block_add_child(item, sub->blocks[k]);
        ce_free(sub->blocks);
        ce_free(sub);
        bs->lines = saved; bs->nlines = saved_n;
        ce_free(inner);

        /* update item end to last consumed line */
        if(j > i) item->end = bs->lines[j-1].end;
        i = j;
    }
    return i;
}

static void parse_blocks(BS *bs, size_t from, size_t to){
    size_t i = from;
    while(i < to){
        Line *l = &bs->lines[i];
        if(line_blank(bs, l)){ i++; continue; }
        size_t len = l->end - l->start;

        /* indented code block (4 spaces) */
        size_t indent = count_indent(bs, l);
        if(indent >= 4){
            size_t cs = skip_spaces(bs, l->start, l->end);
            md_block *code = md_add_block(bs->doc, MD_BLOCK_CODE, cs, l->end);
            code->fence_char = ' '; code->fence_len = 4;
            size_t j = i + 1;
            while(j < to && (line_blank(bs, &bs->lines[j]) || count_indent(bs, &bs->lines[j]) >= 4)){
                code->end = bs->lines[j].end;
                j++;
            }
            i = j;
            continue;
        }

        /* fenced code */
        char fchar; size_t info_s, info_e;
        int flen = fence_at(bs, l, &fchar, &info_s, &info_e);
        if(flen){
            md_block *code = md_add_block(bs->doc, MD_BLOCK_CODE, l->start, l->end);
            code->fence_char = fchar; code->fence_len = flen;
            if(info_e > info_s) code->info = ce_strndup(bsrc(bs) + info_s, info_e - info_s);
            parse_fenced_code(bs, from, to, i, code);
            /* find closing index to skip */
            size_t j = i + 1;
            while(j < to){
                const char *ss = bsrc(bs) + bs->lines[j].start;
                size_t ll = bs->lines[j].end - bs->lines[j].start;
                size_t p = 0;
                while(p < ll && (ss[p] == ' ' || ss[p] == '\t')) p++;
                if(p < ll && ss[p] == fchar){
                    size_t k = p; while(k < ll && ss[k] == fchar) k++;
                    size_t r = k; while(r < ll && (ss[r]==' '||ss[r]=='\t')) r++;
                    if(k - p >= (size_t)flen && r == ll){ j++; break; }
                }
                j++;
            }
            i = j;
            continue;
        }

        /* ATX heading */
        size_t hcs;
        int hl = atx_level(bs, l, &hcs);
        if(hl){
            md_block *h = md_add_block(bs->doc, MD_BLOCK_HEADING, hcs, l->end);
            h->level = hl;
            /* strip trailing '#' */
            size_t he = l->end;
            const char *hs = bsrc(bs);
            while(he > hcs && (hs[he-1] == ' ' || hs[he-1] == '\t')) he--;
            if(he > hcs && hs[he-1] == '#'){
                size_t t = he;
                while(t > hcs && hs[t-1] == '#') t--;
                size_t before = t;
                while(before > hcs && (hs[before-1]==' '||hs[before-1]=='\t')) before--;
                if(t > hcs && (before == hcs || hs[t-1]==' ')) he = before;
            }
            h->end = he;
            IP ip; ip.ps = (PS*)&(PS){ .doc = bs->doc }; IList il = {0,0,0}; ip.list = &il;
            parse_inlines_rec(&ip, hcs, he);
            h->inlines = il.items; h->ninlines = il.n; h->capinlines = il.cap;
            i++;
            continue;
        }

        /* thematic break */
        if(thematic_break(bs, l)){
            md_block *tb = md_add_block(bs->doc, MD_BLOCK_THEMATIC_BREAK, l->start, l->end);
            (void)tb;
            i++;
            continue;
        }

        /* blockquote */
        {
            const char *ss = bsrc(bs) + l->start;
            size_t p = 0;
            while(p < len && (ss[p] == ' ' || ss[p] == '\t')) p++;
            if(p < len && ss[p] == '>'){
                md_block *bq = md_add_block(bs->doc, MD_BLOCK_BLOCKQUOTE, l->start, l->end);
                size_t j = i;
                while(j < to){
                    const char *s2 = bsrc(bs) + bs->lines[j].start;
                    size_t l2 = bs->lines[j].end - bs->lines[j].start;
                    size_t q = 0;
                    while(q < l2 && (s2[q]==' '||s2[q]=='\t')) q++;
                    if(q < l2 && s2[q] == '>'){ bq->end = bs->lines[j].end; j++; }
                    else if(line_blank(bs, &bs->lines[j])){ j++; }
                    else break;
                }
                /* parse inner quote lines */
                parse_blockquote(bs, i, j, bq);
                i = j;
                continue;
            }
        }

        /* list */
        {
            int ordered, start_num, task; char marker;
            size_t content_start, marker_end, ci_dummy;
            if(list_marker_at(bs, l, 0, &ordered, &start_num, &marker, &content_start, &marker_end, &task, &ci_dummy)){
                md_block *list = md_add_block(bs->doc, MD_BLOCK_LIST, l->start, l->end);
                list->list_ordered = ordered;
                list->list_start = start_num;
                size_t j = parse_list_items(bs, i, to, list);
                list->end = (j > i) ? bs->lines[j-1].end : l->end;
                i = j;
                continue;
            }
        }

        /* table: header line followed by separator */
        if(memchr(bsrc(bs) + l->start, '|', len) != NULL && i + 1 < to){
            int *aligns = NULL; size_t ncols = 0;
            if(table_separator(bs, &bs->lines[i+1], &aligns, &ncols)){
                md_block *tbl = md_add_block(bs->doc, MD_BLOCK_TABLE, l->start, bs->lines[i+1].end);
                tbl->ncols = ncols;
                tbl->cols = ce_malloc(ncols * sizeof(md_col));
                for(size_t c = 0; c < ncols; c++){ tbl->cols[c].align = aligns[c]; }
                ce_free(aligns);
                /* header row */
                size_t hn;
                char **hdr = split_table_row(bsrc(bs), l->start, l->end, &hn);
                /* pad header cells to ncols */
                tbl->cells = ce_malloc(sizeof(char**));
                tbl->cells[0] = ce_malloc(ncols * sizeof(char*));
                tbl->row_src = ce_malloc(sizeof(size_t));
                tbl->row_src[0] = l->start;
                for(size_t c = 0; c < ncols; c++){
                    tbl->cells[0][c] = (c < hn) ? hdr[c] : ce_strdup("");
                }
                if(hn > ncols){ for(size_t c = ncols; c < hn; c++) ce_free(hdr[c]); }
                ce_free(hdr);
                tbl->nrows = 1;
                tbl->header_row_present = 1;
                /* body rows */
                size_t j = i + 2;
                while(j < to && !line_blank(bs, &bs->lines[j])){
                    Line *bl = &bs->lines[j];
                    if(memchr(bsrc(bs) + bl->start, '|', bl->end - bl->start) == NULL) break;
                    size_t bn;
                    char **cells = split_table_row(bsrc(bs), bl->start, bl->end, &bn);
                    tbl->cells = ce_realloc(tbl->cells, (tbl->nrows + 1) * sizeof(char**));
                    tbl->row_src = ce_realloc(tbl->row_src, (tbl->nrows + 1) * sizeof(size_t));
                    tbl->cells[tbl->nrows] = ce_malloc(ncols * sizeof(char*));
                    tbl->row_src[tbl->nrows] = bl->start;
                    for(size_t c = 0; c < ncols; c++){
                        tbl->cells[tbl->nrows][c] = (c < bn) ? cells[c] : ce_strdup("");
                    }
                    if(bn > ncols){ for(size_t c = ncols; c < bn; c++) ce_free(cells[c]); }
                    ce_free(cells);
                    tbl->nrows++;
                    tbl->end = bl->end;
                    j++;
                }
                i = j;
                continue;
            }
            if(aligns) ce_free(aligns);
        }

        /* HTML block */
        {
            const char *ss = bsrc(bs) + l->start;
            size_t p = 0;
            while(p < len && (ss[p] == ' ' || ss[p] == '\t')) p++;
            if(p < len && ss[p] == '<'){
                bool is_tag = false;
                if(p+1 < len && (is_alnum_char(ss[p+1]) || ss[p+1] == '/' || ss[p+1] == '!')){
                    /* check for block-level tag names */
                    const char *name = ss + p + 1;
                    size_t nm = 0;
                    while(nm < len - p - 1 && (is_alnum_char(name[nm]) || name[nm]=='-')) nm++;
                    static const char *blocktags[] = {"div","table","tr","td","th","ul","ol","li","blockquote","pre","p","h1","h2","h3","h4","h5","h6","section","article","header","footer","nav","aside","hr","figure","figcaption","details","summary","script","style","html","head","body","!--","!DOCTYPE"};
                    for(size_t t = 0; t < CE_ARRAY_LEN(blocktags); t++){
                        if(nm == strlen(blocktags[t]) && strncasecmp(name, blocktags[t], nm) == 0){ is_tag = true; break; }
                    }
                }
                if(is_tag){
                    md_block *hb = md_add_block(bs->doc, MD_BLOCK_HTML, l->start, l->end);
                    size_t j = i + 1;
                    bool closed = strstr(ss, "</") != NULL;
                    while(j < to && !closed && !line_blank(bs, &bs->lines[j])){ hb->end = bs->lines[j].end; j++; }
                    if(!closed) { /* skip until blank */ }
                    else hb->end = l->end;
                    i = j;
                    continue;
                }
            }
        }

        /* setext heading: current line + next line is underline */
        {
            char kind;
            if(i + 1 < to && setext_underline(bs, &bs->lines[i+1], &kind)){
                md_block *h = md_add_block(bs->doc, MD_BLOCK_HEADING, l->start, l->end);
                h->level = (kind == '=') ? 1 : 2;
                IP ip; ip.ps = (PS*)&(PS){ .doc = bs->doc }; IList il = {0,0,0}; ip.list = &il;
                parse_inlines_rec(&ip, l->start, l->end);
                h->inlines = il.items; h->ninlines = il.n; h->capinlines = il.cap;
                i += 2;
                continue;
            }
        }

        /* paragraph: collect until blank or new block */
        {
            md_block *p = md_add_block(bs->doc, MD_BLOCK_PARAGRAPH, l->start, l->end);
            size_t j = i;
            while(j < to && !line_blank(bs, &bs->lines[j])){
                /* check if line j is a block starter (only for j > i) */
                if(j > i){
                    Line *lj = &bs->lines[j];
                    /* fenced, heading, thematic, blockquote, list, indented code interrupt paragraph? */
                    char fchar2; size_t is2, ie2;
                    size_t ind = count_indent(bs, lj);
                    if(ind >= 4) break;
                    if(fence_at(bs, lj, &fchar2, &is2, &ie2)) break;
                    size_t hcs2; int hl2 = atx_level(bs, lj, &hcs2); if(hl2) break;
                    if(thematic_break(bs, lj)) break;
                    int o2, s2, t2; char m2; size_t cs2, me2, ci2;
                    if(list_marker_at(bs, lj, 0, &o2, &s2, &m2, &cs2, &me2, &t2, &ci2)) break;
                    const char *ss = bsrc(bs) + lj->start;
                    size_t q = 0; while(q < lj->end - lj->start && (ss[q]==' '||ss[q]=='\t')) q++;
                    if(q < lj->end - lj->start && ss[q] == '>') break;
                    if(memchr(bsrc(bs) + lj->start, '|', lj->end - lj->start) != NULL && j + 1 < to){
                        int *al2 = NULL; size_t nc2 = 0;
                        if(table_separator(bs, &bs->lines[j+1], &al2, &nc2)){ if(al2) ce_free(al2); break; }
                    }
                }
                p->end = bs->lines[j].end;
                j++;
            }
            /* parse inlines across the paragraph lines */
            PS psctx; psctx.doc = bs->doc;
            for(size_t k = i; k < j; k++){
                Line *lk = &bs->lines[k];
                IList line_il = {0,0,0};
                IP lip; lip.ps = &psctx; lip.list = &line_il;
                parse_inlines_rec(&lip, lk->start, lk->end);
                for(size_t m = 0; m < line_il.n; m++) md_block_add_inline(p, line_il.items[m]);
                ce_free(line_il.items);
                if(k + 1 < j){
                    /* hard break if line ends with 2+ spaces or backslash */
                    const char *ls = bsrc(bs) + lk->start;
                    size_t ll = lk->end - lk->start;
                    bool hard = false;
                    if(ll >= 2 && ls[ll-1]==' ' && ls[ll-2]==' ') hard = true;
                    else if(ll >= 1 && ls[ll-1] == '\\') hard = true;
                    md_inline *brk = inl_new(hard ? MD_INL_HARDBREAK : MD_INL_SOFTBREAK, lk->end, bs->lines[k+1].start);
                    md_block_add_inline(p, brk);
                }
            }
            i = j;
            continue;
        }
    }
}

/* ============================================================ public API */

md_doc *md_parse(const char *src, size_t len){
    md_doc *d = ce_calloc(1, sizeof(md_doc));
    d->src = ce_malloc(len + 1);
    memcpy(d->src, src, len);
    d->src[len] = 0;
    d->len = len;

    /* split lines */
    size_t cap = 64, n = 0;
    Line *lines = ce_malloc(cap * sizeof(Line));
    size_t i = 0;
    while(i <= len){
        size_t start = i;
        while(i < len && d->src[i] != '\n') i++;
        size_t end = i;
        if(end > start && d->src[end-1] == '\r') end--;
        if(n == cap){ cap *= 2; lines = ce_realloc(lines, cap * sizeof(Line)); }
        lines[n].start = start; lines[n].end = end;
        n++;
        if(i >= len) break;
        i++; /* skip newline */
    }

    BS bs; bs.doc = d; bs.lines = lines; bs.nlines = n;
    parse_blocks(&bs, 0, n);
    ce_free(lines);
    return d;
}

void md_free(md_doc *d){
    if(!d) return;
    for(size_t i = 0; i < d->nblocks; i++) md_block_free(d->blocks[i]);
    ce_free(d->blocks);
    ce_free(d->src);
    ce_free(d);
}
