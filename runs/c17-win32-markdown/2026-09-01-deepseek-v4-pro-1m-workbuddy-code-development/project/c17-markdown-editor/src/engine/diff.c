/* diff.c - Myers shortest-edit-script diff plus word/token refinement. */
#include "diff.h"
#include "ce_common.h"
#include "utf8.h"

typedef struct { size_t start, len; } span;

/* ----------------------------- Myers ----------------------------- */

typedef struct { int type; long a_idx; long b_idx; } edit; /* 0 equal,1 del,2 ins */

static bool span_eq(const char *sa, const span *a, const char *sb, const span *b){
    if(a->len != b->len) return false;
    return memcmp(sa + a->start, sb + b->start, a->len) == 0;
}

static edit *myers(const char *sa, const span *a, size_t na,
                   const char *sb, const span *b, size_t nb, size_t *nops){
    size_t max = na + nb;
    size_t width = 2 * max + 3;
    long offset = (long)max + 1;
    long *v = ce_calloc(width, sizeof(long));
    long **trace = ce_malloc((max + 1) * sizeof(long*));
    for(size_t i = 0; i <= max; i++) trace[i] = NULL;

    v[offset + 1] = 0;
    size_t D = 0;
    bool done = false;
    for(size_t d = 0; d <= max && !done; d++){
        D = d;
        trace[d] = ce_malloc(width * sizeof(long));
        memcpy(trace[d], v, width * sizeof(long));
        for(long k = -(long)d; k <= (long)d; k += 2){
            long x;
            if(k == -(long)d || (k != (long)d && v[offset + k - 1] < v[offset + k + 1]))
                x = v[offset + k + 1];
            else
                x = v[offset + k - 1] + 1;
            long y = x - k;
            while(x < (long)na && y < (long)nb && span_eq(sa, &a[x], sb, &b[y])){ x++; y++; }
            v[offset + k] = x;
            if(x >= (long)na && y >= (long)nb){ done = true; break; }
        }
    }

    /* backtrack */
    edit *edits = ce_malloc((max + 1) * sizeof(edit));
    size_t ne = 0;
    long x = (long)na, y = (long)nb;
    for(long d = (long)D; d >= 0; d--){
        long k = x - y;
        long prev_k;
        if(k == -d || (k != d && trace[d][offset + k - 1] < trace[d][offset + k + 1]))
            prev_k = k + 1;
        else
            prev_k = k - 1;
        long prev_x = trace[d][offset + prev_k];
        long prev_y = prev_x - prev_k;
        while(x > prev_x && y > prev_y){
            edits[ne].type = 0; edits[ne].a_idx = x - 1; edits[ne].b_idx = y - 1; ne++;
            x--; y--;
        }
        if(d > 0){
            if(x == prev_x){ edits[ne].type = 2; edits[ne].a_idx = -1; edits[ne].b_idx = y - 1; ne++; y--; }
            else { edits[ne].type = 1; edits[ne].a_idx = x - 1; edits[ne].b_idx = -1; ne++; x--; }
        }
    }

    /* reverse */
    for(size_t i = 0; i < ne / 2; i++){
        edit t = edits[i]; edits[i] = edits[ne - 1 - i]; edits[ne - 1 - i] = t;
    }

    for(size_t i = 0; i <= max; i++){ if(trace[i]) ce_free(trace[i]); }
    ce_free(trace);
    ce_free(v);

    *nops = ne;
    return edits;
}

/* ----------------------------- lines ----------------------------- */

static size_t split_lines(const char *s, size_t len, span **out){
    size_t cap = 64, n = 0;
    span *lines = ce_malloc(cap * sizeof(span));
    size_t i = 0;
    while(i <= len){
        size_t start = i;
        while(i < len && s[i] != '\n') i++;
        size_t end = i;
        if(end > start && s[end-1] == '\r') end--;
        if(n == cap){ cap *= 2; lines = ce_realloc(lines, cap * sizeof(span)); }
        lines[n].start = start; lines[n].len = end - start;
        n++;
        if(i >= len) break;
        i++;
    }
    *out = lines;
    return n;
}

/* ----------------------- word tokenization ----------------------- */

typedef struct { size_t start, len; } token;

static size_t tokenize(const char *s, size_t len, token **out){
    size_t cap = 16, n = 0;
    token *toks = ce_malloc(cap * sizeof(token));
    size_t i = 0;
    while(i < len){
        unsigned char c = (unsigned char)s[i];
        if(c < 0x80){
            if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'){
                size_t j = i;
                while(j < len){
                    unsigned char d = (unsigned char)s[j];
                    if((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || d == '_') j++;
                    else break;
                }
                if(n == cap){ cap *= 2; toks = ce_realloc(toks, cap * sizeof(token)); }
                toks[n].start = i; toks[n].len = j - i; n++;
                i = j;
            } else if(c == ' ' || c == '\t'){
                size_t j = i;
                while(j < len && (s[j] == ' ' || s[j] == '\t')) j++;
                if(n == cap){ cap *= 2; toks = ce_realloc(toks, cap * sizeof(token)); }
                toks[n].start = i; toks[n].len = j - i; n++;
                i = j;
            } else {
                /* ASCII punctuation run */
                size_t j = i;
                while(j < len){
                    unsigned char d = (unsigned char)s[j];
                    if(d >= 0x80) break;
                    if((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || d == '_') break;
                    if(d == ' ' || d == '\t') break;
                    j++;
                }
                if(n == cap){ cap *= 2; toks = ce_realloc(toks, cap * sizeof(token)); }
                toks[n].start = i; toks[n].len = j - i; n++;
                i = j;
            }
        } else {
            /* non-ASCII: one code point */
            size_t j = ce_utf8_next((const uint8_t*)s, len, i);
            if(n == cap){ cap *= 2; toks = ce_realloc(toks, cap * sizeof(token)); }
            toks[n].start = i; toks[n].len = j - i; n++;
            i = j;
        }
    }
    *out = toks;
    return n;
}

static void append_word(diff_hunk *h, int op, const char *text, size_t len){
    h->words = ce_realloc(h->words, (h->nwords + 1) * sizeof(h->words[0]));
    h->words[h->nwords].op = op;
    h->words[h->nwords].text = ce_strndup(text, len);
    h->nwords++;
}

size_t md_diff(const char *a, size_t alen, const char *b, size_t blen, diff_hunk **out){
    span *la = NULL, *lb = NULL;
    size_t na = split_lines(a, alen, &la);
    size_t nb = split_lines(b, blen, &lb);

    size_t nops = 0;
    edit *edits = myers(a, la, na, b, lb, nb, &nops);

    /* group edits into hunks */
    size_t cap = 16, nh = 0;
    diff_hunk *hunks = ce_malloc(cap * sizeof(diff_hunk));
    size_t i = 0;
    while(i < nops){
        int t = edits[i].type;
        if(t == 0){
            /* equal run */
            long a0 = edits[i].a_idx, b0 = edits[i].b_idx, c = 0;
            while(i < nops && edits[i].type == 0){ c++; i++; }
            if(nh == cap){ cap *= 2; hunks = ce_realloc(hunks, cap * sizeof(diff_hunk)); }
            memset(&hunks[nh], 0, sizeof(diff_hunk));
            hunks[nh].op = DIFF_EQUAL; hunks[nh].a_start = a0; hunks[nh].a_count = c;
            hunks[nh].b_start = b0; hunks[nh].b_count = c;
            nh++;
        } else {
            /* a change region: dels then adds */
            long a0 = -1, b0 = -1; long ad = 0, bd = 0;
            if(t == 1){ a0 = edits[i].a_idx; while(i < nops && edits[i].type == 1){ ad++; i++; } }
            if(i < nops && edits[i].type == 2){ b0 = edits[i].b_idx; while(i < nops && edits[i].type == 2){ bd++; i++; } }
            /* handle the case where adds come first (unlikely after grouping but safe) */
            if(a0 == -1 && b0 == -1) break;
            if(nh == cap){ cap *= 2; hunks = ce_realloc(hunks, cap * sizeof(diff_hunk)); }
            memset(&hunks[nh], 0, sizeof(diff_hunk));
            if(ad > 0 && bd > 0){
                hunks[nh].op = DIFF_DEL; hunks[nh].modified = true;
            } else if(ad > 0){
                hunks[nh].op = DIFF_DEL;
            } else {
                hunks[nh].op = DIFF_ADD;
            }
            hunks[nh].a_start = a0; hunks[nh].a_count = ad;
            hunks[nh].b_start = b0; hunks[nh].b_count = bd;
            /* word refinement for paired region */
            if(ad > 0 && bd > 0 && ad == bd){
                /* refine the single paired line */
                const span *al = &la[a0];
                const span *bl = &lb[b0];
                token *ta = NULL, *tb = NULL;
                size_t nt = tokenize(a + al->start, al->len, &ta);
                size_t ntb = tokenize(b + bl->start, bl->len, &tb);
                size_t ne = 0;
                edit *we = myers(a + al->start, (const span*)ta, nt, b + bl->start, (const span*)tb, ntb, &ne);
                for(size_t w = 0; w < ne; w++){
                    int op;
                    const char *txt; size_t tl;
                    if(we[w].type == 0){ op = DIFF_EQUAL; txt = a + al->start + ta[we[w].a_idx].start; tl = ta[we[w].a_idx].len; }
                    else if(we[w].type == 1){ op = DIFF_DEL; txt = a + al->start + ta[we[w].a_idx].start; tl = ta[we[w].a_idx].len; }
                    else { op = DIFF_ADD; txt = b + bl->start + tb[we[w].b_idx].start; tl = tb[we[w].b_idx].len; }
                    append_word(&hunks[nh], op, txt, tl);
                }
                ce_free(we); ce_free(ta); ce_free(tb);
            }
            nh++;
        }
    }

    ce_free(edits);
    ce_free(la);
    ce_free(lb);
    *out = hunks;
    return nh;
}

void md_diff_free(diff_hunk *h, size_t n){
    for(size_t i = 0; i < n; i++){
        for(size_t w = 0; w < h[i].nwords; w++) ce_free(h[i].words[w].text);
        if(h[i].words) ce_free(h[i].words);
    }
    ce_free(h);
}

size_t md_diff_script(const char *a, size_t alen, const char *b, size_t blen, md_edit **out){
    span *la = NULL, *lb = NULL;
    size_t na = split_lines(a, alen, &la);
    size_t nb = split_lines(b, blen, &lb);
    size_t n = 0;
    edit *ed = myers(a, la, na, b, lb, nb, &n);
    md_edit *r = ce_malloc((n ? n : 1) * sizeof(md_edit));
    for(size_t i = 0; i < n; i++){ r[i].type = ed[i].type; r[i].a_idx = ed[i].a_idx; r[i].b_idx = ed[i].b_idx; }
    ce_free(ed); ce_free(la); ce_free(lb);
    *out = r;
    return n;
}

size_t md_split_lines(const char *s, size_t len, size_t **starts, size_t **lens){
    span *ls = NULL;
    size_t n = split_lines(s, len, &ls);
    size_t *st = ce_malloc((n ? n : 1) * sizeof(size_t));
    size_t *ln = ce_malloc((n ? n : 1) * sizeof(size_t));
    for(size_t i = 0; i < n; i++){ st[i] = ls[i].start; ln[i] = ls[i].len; }
    ce_free(ls);
    *starts = st; *lens = ln;
    return n;
}
