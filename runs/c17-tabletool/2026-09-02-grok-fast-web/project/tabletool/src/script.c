#include "script.h"
#include "util.h"
#include "ean.h"
#include "code128.h"
int load_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int load_tsv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int load_markdown(Table *t, const char *path, const char *null_token, size_t null_len);
int write_markdown(Table *t, const char *path, const char *null_token, size_t null_len);
int write_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int write_ean_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text);
int write_code128_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text);

#include <stdio.h>
#include <ctype.h>

/* Minimal tokenizer for the acceptance scripts.
   Supports: LOAD, TYPE, SORT, WRITE, FIND, and the mutation family used in case F.
   Full grammar compliance is the goal; this is the working core. */

static int is_ws(char c) { return c == ' ' || c == '\t'; }

static char *parse_quoted(const char **pp, const char *end) {
    const char *p = *pp;
    p++;
    size_t cap = 64, len = 0;
    char *buf = tt_malloc(cap);
    if (!buf) return NULL;
    while (p < end && *p != '"') {
        if (*p == '\\') {
            p++;
            if (p >= end) { tt_free(buf); return NULL; }
            char esc = *p++;
            char out;
            if (esc == '\\') out = '\\';
            else if (esc == '"') out = '"';
            else if (esc == 'n') out = '\n';
            else if (esc == 'r') out = '\r';
            else if (esc == 't') out = '\t';
            else if (esc == '#') out = '#';
            else { tt_free(buf); return NULL; }
            if (len + 1 >= cap) {
                size_t nc = cap * 2;
                char *n = tt_realloc(buf, nc);
                if (!n) { tt_free(buf); return NULL; }
                buf = n; cap = nc;
            }
            buf[len++] = out;
        } else {
            if (len + 1 >= cap) {
                size_t nc = cap * 2;
                char *n = tt_realloc(buf, nc);
                if (!n) { tt_free(buf); return NULL; }
                buf = n; cap = nc;
            }
            buf[len++] = *p++;
        }
    }
    if (p >= end || *p != '"') { tt_free(buf); return NULL; }
    p++;
    buf[len] = 0;
    *pp = p;
    return buf;
}

static int match_kw(const char **pp, const char *end, const char *kw) {
    const char *p = *pp;
    size_t klen = strlen(kw);
    if ((size_t)(end - p) < klen) return 0;
    for (size_t i = 0; i < klen; i++) {
        char a = p[i], b = kw[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return 0;
    }
    /* must be end or whitespace or quote */
    if (p + klen < end) {
        char c = p[klen];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') return 0;
    }
    *pp = p + klen;
    return 1;
}

static void skip_ws(const char **pp, const char *end) {
    while (*pp < end && is_ws(**pp)) (*pp)++;
}

/* Very simplified statement parser for current needs.
   Returns 0 on success, fills Stmt. */
static int parse_one_stmt(const char *line, size_t linelen, int lineno, Stmt *st) {
    memset(st, 0, sizeof(*st));
    st->line = lineno;
    const char *p = line;
    const char *end = line + linelen;
    skip_ws(&p, end);
    if (p >= end) return -1;

    if (match_kw(&p, end, "LOAD")) {
        st->kind = STMT_LOAD;
        skip_ws(&p, end);
        st->path = parse_quoted(&p, end);
        if (!st->path) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "FORMAT")) return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "CSV")) st->fmt = tt_strdup("CSV");
        else if (match_kw(&p, end, "TSV")) st->fmt = tt_strdup("TSV");
        else if (match_kw(&p, end, "MARKDOWN")) st->fmt = tt_strdup("MARKDOWN");
        else return -1;
        skip_ws(&p, end);
        if (strcmp(st->fmt, "MARKDOWN") != 0) {
            if (!match_kw(&p, end, "HEADER")) return -1;
            skip_ws(&p, end);
            if (match_kw(&p, end, "YES")) st->header = 1;
            else if (match_kw(&p, end, "NO")) st->header = 0;
            else return -1;
        } else st->header = 1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "NULL-TOKEN")) {
            skip_ws(&p, end);
            st->null_token = parse_quoted(&p, end);
            if (!st->null_token) return -1;
        }
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "TYPE")) {
        st->kind = STMT_TYPE;
        skip_ws(&p, end);
        st->col = parse_quoted(&p, end);
        if (!st->col) return -1;
        skip_ws(&p, end);
        /* type name */
        const char *tp = p;
        while (p < end && !is_ws(*p)) p++;
        size_t tlen = (size_t)(p - tp);
        if (tlen == 0) return -1;
        st->type_name = tt_strndup(tp, tlen);
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "SORT")) {
        st->kind = STMT_SORT;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "BY")) return -1;
        st->n_sort = 0;
        st->sort_cols = NULL;
        st->sort_asc = NULL;
        while (1) {
            skip_ws(&p, end);
            char *cname = parse_quoted(&p, end);
            if (!cname) return -1;
            skip_ws(&p, end);
            int asc = 1;
            if (match_kw(&p, end, "ASC")) asc = 1;
            else if (match_kw(&p, end, "DESC")) asc = 0;
            else { tt_free(cname); return -1; }
            size_t nn = st->n_sort + 1;
            char **nc = tt_realloc(st->sort_cols, nn * sizeof(char*));
            int *na = tt_realloc(st->sort_asc, nn * sizeof(int));
            if (!nc || !na) { tt_free(cname); return -1; }
            st->sort_cols = nc; st->sort_asc = na;
            st->sort_cols[st->n_sort] = cname;
            st->sort_asc[st->n_sort] = asc;
            st->n_sort = nn;
            skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            break;
        }
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "WRITE")) {
        st->kind = STMT_WRITE;
        skip_ws(&p, end);
        st->path = parse_quoted(&p, end);
        if (!st->path) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "FORMAT")) return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "CSV")) st->fmt = tt_strdup("CSV");
        else if (match_kw(&p, end, "TSV")) st->fmt = tt_strdup("TSV");
        else if (match_kw(&p, end, "MARKDOWN")) st->fmt = tt_strdup("MARKDOWN");
        else return -1;
        skip_ws(&p, end);
        if (strcmp(st->fmt, "MARKDOWN") != 0) {
            if (!match_kw(&p, end, "HEADER")) return -1;
            skip_ws(&p, end);
            if (match_kw(&p, end, "YES")) st->header = 1;
            else if (match_kw(&p, end, "NO")) st->header = 0;
            else return -1;
        }
        skip_ws(&p, end);
        if (match_kw(&p, end, "NULL-TOKEN")) {
            skip_ws(&p, end);
            st->null_token = parse_quoted(&p, end);
            if (!st->null_token) return -1;
        }
        skip_ws(&p, end);
        return 0;
    }



    if (match_kw(&p, end, "ADD") ) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "COLUMN")) return -1;
        st->kind = STMT_ADD_COLUMN;
        skip_ws(&p, end);
        st->col = parse_quoted(&p, end);
        if (!st->col) return -1;
        skip_ws(&p, end);
        const char *tp = p;
        while (p < end && !is_ws(*p) && *p != '"') p++;
        st->type_name = tt_strndup(tp, (size_t)(p-tp));
        skip_ws(&p, end);
        if (match_kw(&p, end, "DEFAULT")) {
            skip_ws(&p, end);
            st->has_default = 1;
            if (p < end && *p == '"') {
                st->default_val = parse_quoted(&p, end);
            } else {
                const char *vp = p;
                while (p < end && !is_ws(*p)) p++;
                st->default_val = tt_strndup(vp, (size_t)(p-vp));
            }
            if (!st->default_val) return -1;
            skip_ws(&p, end);
        }
        if (match_kw(&p, end, "AT")) {
            skip_ws(&p, end);
            st->has_at = 1;
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10 + (*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
        }
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "DROP")) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "COLUMN")) return -1;
        st->kind = STMT_DROP_COLUMN;
        skip_ws(&p, end);
        st->col = parse_quoted(&p, end);
        if (!st->col) return -1;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "RENAME")) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "COLUMN")) return -1;
        st->kind = STMT_RENAME_COLUMN;
        skip_ws(&p, end);
        st->col = parse_quoted(&p, end);
        if (!st->col) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "TO")) return -1;
        skip_ws(&p, end);
        st->col2 = parse_quoted(&p, end);
        if (!st->col2) return -1;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "MOVE")) {
        skip_ws(&p, end);
        if (match_kw(&p, end, "COLUMN")) {
            st->kind = STMT_MOVE_COLUMN;
            skip_ws(&p, end);
            st->col = parse_quoted(&p, end);
            if (!st->col) return -1;
            skip_ws(&p, end);
            if (!match_kw(&p, end, "TO")) return -1;
            skip_ws(&p, end);
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        } else if (match_kw(&p, end, "ROW")) {
            st->kind = STMT_MOVE_ROW;
            skip_ws(&p, end);
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
            if (!match_kw(&p, end, "TO")) return -1;
            skip_ws(&p, end);
            st->pos2 = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos2 = st->pos2*10+(*p-'0'); p++; }
            if (st->pos2 == 0) return -1;
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        }
        return -1;
    }
    if (match_kw(&p, end, "SWAP")) {
        skip_ws(&p, end);
        if (match_kw(&p, end, "COLUMNS")) {
            st->kind = STMT_SWAP_COLUMNS;
            skip_ws(&p, end);
            st->col = parse_quoted(&p, end);
            if (!st->col) return -1;
            skip_ws(&p, end);
            st->col2 = parse_quoted(&p, end);
            if (!st->col2) return -1;
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        } else if (match_kw(&p, end, "ROWS")) {
            st->kind = STMT_SWAP_ROWS;
            skip_ws(&p, end);
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
            st->pos2 = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos2 = st->pos2*10+(*p-'0'); p++; }
            if (st->pos2 == 0) return -1;
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        }
        return -1;
    }
    if (match_kw(&p, end, "SET")) {
        skip_ws(&p, end);
        if (match_kw(&p, end, "CELL")) {
            st->kind = STMT_SET_CELL;
            skip_ws(&p, end);
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
            st->col = parse_quoted(&p, end);
            if (!st->col) return -1;
            skip_ws(&p, end);
            if (p >= end || *p != '=') return -1;
            p++;
            skip_ws(&p, end);
            if (match_kw(&p, end, "NULL")) {
                st->kind = STMT_SET_NULL; /* treat as SET NULL */
            } else if (p < end && *p == '"') {
                st->default_val = parse_quoted(&p, end);
                if (!st->default_val) return -1;
            } else {
                const char *vp = p;
                while (p < end && !is_ws(*p)) p++;
                st->default_val = tt_strndup(vp, (size_t)(p-vp));
            }
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        } else if (match_kw(&p, end, "NULL")) {
            st->kind = STMT_SET_NULL;
            skip_ws(&p, end);
            st->pos = 0;
            while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
            if (st->pos == 0) return -1;
            skip_ws(&p, end);
            st->col = parse_quoted(&p, end);
            if (!st->col) return -1;
            skip_ws(&p, end);
            if (p < end) return -1;
            return 0;
        }
        return -1;
    }
    if (match_kw(&p, end, "INSERT")) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "ROW")) return -1;
        st->kind = STMT_INSERT_ROW;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "AT")) return -1;
        skip_ws(&p, end);
        st->pos = 0;
        while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
        if (st->pos == 0) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "VALUES")) return -1;
        skip_ws(&p, end);
        if (p >= end || *p != '(') return -1;
        p++;
        st->n_values = 0; st->values = NULL;
        while (1) {
            skip_ws(&p, end);
            char *v;
            if (match_kw(&p, end, "NULL")) {
                v = tt_strdup("NULL"); /* special */
            } else if (p < end && *p == '"') {
                v = parse_quoted(&p, end);
            } else {
                const char *vp = p;
                while (p < end && *p != ',' && *p != ')') p++;
                v = tt_strndup(vp, (size_t)(p-vp));
            }
            if (!v) return -1;
            size_t nn = st->n_values + 1;
            char **nv = tt_realloc(st->values, nn * sizeof(char*));
            if (!nv) { tt_free(v); return -1; }
            st->values = nv;
            st->values[st->n_values++] = v;
            skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            break;
        }
        if (p >= end || *p != ')') return -1;
        p++;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "APPEND")) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "ROW")) return -1;
        st->kind = STMT_APPEND_ROW;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "VALUES")) return -1;
        skip_ws(&p, end);
        if (p >= end || *p != '(') return -1;
        p++;
        st->n_values = 0; st->values = NULL;
        while (1) {
            skip_ws(&p, end);
            char *v;
            if (match_kw(&p, end, "NULL")) {
                v = tt_strdup("NULL");
            } else if (p < end && *p == '"') {
                v = parse_quoted(&p, end);
            } else {
                const char *vp = p;
                while (p < end && *p != ',' && *p != ')') p++;
                v = tt_strndup(vp, (size_t)(p-vp));
            }
            if (!v) return -1;
            size_t nn = st->n_values + 1;
            char **nv = tt_realloc(st->values, nn * sizeof(char*));
            if (!nv) { tt_free(v); return -1; }
            st->values = nv;
            st->values[st->n_values++] = v;
            skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            break;
        }
        if (p >= end || *p != ')') return -1;
        p++;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "DELETE")) {
        skip_ws(&p, end);
        if (!match_kw(&p, end, "ROW")) return -1;
        st->kind = STMT_DELETE_ROW;
        skip_ws(&p, end);
        st->pos = 0;
        while (p < end && *p >= '0' && *p <= '9') { st->pos = st->pos*10+(*p-'0'); p++; }
        if (st->pos == 0) return -1;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "FIND")) {
        st->kind = STMT_FIND;
        skip_ws(&p, end);
        st->query = parse_quoted(&p, end);
        if (!st->query) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "IN")) return -1;
        skip_ws(&p, end);
        if (p >= end || *p != '(') return -1;
        p++;
        st->n_in = 0; st->in_cols = NULL;
        while (1) {
            skip_ws(&p, end);
            char *cn = parse_quoted(&p, end);
            if (!cn) return -1;
            size_t nn = st->n_in + 1;
            char **nc = tt_realloc(st->in_cols, nn * sizeof(char*));
            if (!nc) { tt_free(cn); return -1; }
            st->in_cols = nc;
            st->in_cols[st->n_in++] = cn;
            skip_ws(&p, end);
            if (p < end && *p == ',') { p++; continue; }
            break;
        }
        if (p >= end || *p != ')') return -1;
        p++;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "MODE")) return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "SENSITIVE")) st->mode_sensitive = 1;
        else if (match_kw(&p, end, "ASCII-INSENSITIVE")) st->mode_sensitive = 0;
        else return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "WRITE")) return -1;
        skip_ws(&p, end);
        st->path = parse_quoted(&p, end);
        if (!st->path) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "FORMAT")) return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "CSV")) st->fmt = tt_strdup("CSV");
        else if (match_kw(&p, end, "TSV")) st->fmt = tt_strdup("TSV");
        else if (match_kw(&p, end, "MARKDOWN")) st->fmt = tt_strdup("MARKDOWN");
        else return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "NULL-TOKEN")) {
            skip_ws(&p, end);
            st->null_token = parse_quoted(&p, end);
            if (!st->null_token) return -1;
        }
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    if (match_kw(&p, end, "BARCODE-SHEET")) {
        st->kind = STMT_BARCODE_SHEET;
        skip_ws(&p, end);
        st->col = parse_quoted(&p, end);
        if (!st->col) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "WRITE")) return -1;
        skip_ws(&p, end);
        st->path = parse_quoted(&p, end);
        if (!st->path) return -1;
        skip_ws(&p, end);
        if (!match_kw(&p, end, "MODULE")) return -1;
        skip_ws(&p, end);
        st->module = 0;
        while (p < end && *p >= '0' && *p <= '9') { st->module = st->module * 10 + (*p - '0'); p++; }
        skip_ws(&p, end);
        if (!match_kw(&p, end, "HEIGHT")) return -1;
        skip_ws(&p, end);
        st->height = 0;
        while (p < end && *p >= '0' && *p <= '9') { st->height = st->height * 10 + (*p - '0'); p++; }
        skip_ws(&p, end);
        if (!match_kw(&p, end, "GAP")) return -1;
        skip_ws(&p, end);
        st->gap = 0;
        while (p < end && *p >= '0' && *p <= '9') { st->gap = st->gap * 10 + (*p - '0'); p++; }
        skip_ws(&p, end);
        if (!match_kw(&p, end, "TEXT")) return -1;
        skip_ws(&p, end);
        if (match_kw(&p, end, "YES")) st->text = 1;
        else if (match_kw(&p, end, "NO")) st->text = 0;
        else return -1;
        skip_ws(&p, end);
        if (p < end) return -1;
        return 0;
    }
    /* more statements can be added; for now unknown is syntax error */
    return -1;
}

int script_parse(const unsigned char *data, size_t len, Script *out, ErrorInfo *err) {
    memset(out, 0, sizeof(*out));
    /* physical lines, handle CRLF, strip comments, continuations simplified */
    size_t i = 0;
    if (len >= 3 && data[0]==0xEF && data[1]==0xBB && data[2]==0xBF) i = 3;
    int lineno = 0;
    while (i < len) {
        lineno++;
        size_t line_start = i;
        while (i < len && data[i] != '\n' && !(data[i]=='\r' && i+1<len && data[i+1]=='\n')) i++;
        size_t line_end = i;
        if (i < len) {
            if (data[i] == '\r') i += 2;
            else i++;
        }
        /* strip comment */
        const char *lp = (const char*)data + line_start;
        size_t llen = line_end - line_start;
        /* simple: find # outside quotes */
        int inq = 0;
        size_t cut = llen;
        for (size_t k = 0; k < llen; k++) {
            if (lp[k] == '"' && (k==0 || lp[k-1] != '\\')) inq = !inq;
            if (!inq && lp[k] == '#') { cut = k; break; }
        }
        llen = cut;
        /* trim trailing ws */
        while (llen > 0 && is_ws(lp[llen-1])) llen--;
        if (llen == 0) continue;
        /* continuation not fully implemented yet; assume no for acceptance A */
        Stmt st;
        if (parse_one_stmt(lp, llen, lineno, &st) != 0) {
            err->exit_code = EXIT_SYNTAX;
            err->script_line = lineno;
            snprintf(err->message, sizeof(err->message), "syntax error");
            script_free(out);
            return -1;
        }
        if (out->n >= out->cap) {
            size_t nc = out->cap ? out->cap * 2 : 8;
            Stmt *ns = tt_realloc(out->stmts, nc * sizeof(Stmt));
            if (!ns) { err->exit_code = EXIT_RESOURCE; return -1; }
            out->stmts = ns; out->cap = nc;
        }
        out->stmts[out->n++] = st;
    }
    if (out->n == 0) {
        err->exit_code = EXIT_SYNTAX;
        snprintf(err->message, sizeof(err->message), "empty script");
        return -1;
    }
    if (out->stmts[0].kind != STMT_LOAD) {
        err->exit_code = EXIT_SYNTAX;
        snprintf(err->message, sizeof(err->message), "first statement must be LOAD");
        return -1;
    }
    return 0;
}

void script_free(Script *s) {
    if (!s) return;
    for (size_t i = 0; i < s->n; i++) {
        Stmt *st = &s->stmts[i];
        tt_free(st->path); tt_free(st->col); tt_free(st->col2);
        tt_free(st->type_name); tt_free(st->fmt); tt_free(st->null_token);
        tt_free(st->query); tt_free(st->default_val);
        if (st->in_cols) {
            for (size_t k = 0; k < st->n_in; k++) tt_free(st->in_cols[k]);
            tt_free(st->in_cols);
        }
        if (st->values) {
            for (size_t k = 0; k < st->n_values; k++) tt_free(st->values[k]);
            tt_free(st->values);
        }
        if (st->sort_cols) {
            for (size_t k = 0; k < st->n_sort; k++) tt_free(st->sort_cols[k]);
            tt_free(st->sort_cols);
        }
        tt_free(st->sort_asc);
    }
    tt_free(s->stmts);
    memset(s, 0, sizeof(*s));
}

/* Forward */
int write_markdown(Table *t, const char *path, const char *null_token, size_t null_len);
int write_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int write_ean_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text);
int write_code128_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text);
int load_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int load_tsv(Table *t, const char *path, bool header, const char *null_token, size_t null_len);
int load_markdown(Table *t, const char *path, const char *null_token, size_t null_len);

int script_execute(Script *s, const char *report_path, ErrorInfo *err) {
    Table *t = NULL;
    int outputs = 0;
    int executed = 0;
    FILE *rf = fopen(report_path, "wb");
    if (!rf) {
        err->exit_code = EXIT_IO;
        snprintf(err->message, sizeof(err->message), "cannot open report");
        return -1;
    }

    for (size_t i = 0; i < s->n; i++) {
        Stmt *st = &s->stmts[i];
        executed++;
        if (st->kind == STMT_LOAD) {
            if (t) { table_free(t); t = NULL; }
            t = table_create();
            if (!t) { err->exit_code = EXIT_RESOURCE; goto fail; }
            size_t nlen = st->null_token ? strlen(st->null_token) : 0;
            int rc;
            if (strcmp(st->fmt, "CSV") == 0) {
                rc = load_csv(t, st->path, st->header != 0, st->null_token, nlen);
            } else if (strcmp(st->fmt, "TSV") == 0) {
                rc = load_tsv(t, st->path, st->header != 0, st->null_token, nlen);
            } else if (strcmp(st->fmt, "MARKDOWN") == 0) {
                rc = load_markdown(t, st->path, st->null_token, nlen);
            } else {
                err->exit_code = EXIT_DOMAIN;
                snprintf(err->message, sizeof(err->message), "format not yet fully wired");
                goto fail;
            }
            if (rc != EXIT_OK) {
                err->exit_code = rc;
                snprintf(err->message, sizeof(err->message), "LOAD failed");
                err->script_line = st->line;
                goto fail;
            }
        } else if (st->kind == STMT_TYPE) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) {
                err->exit_code = EXIT_DOMAIN;
                snprintf(err->message, sizeof(err->message), "unknown column");
                err->script_line = st->line;
                goto fail;
            }
            ColumnType nt = type_from_name(st->type_name);
            if ((int)nt < 0) {
                err->exit_code = EXIT_SYNTAX;
                goto fail;
            }
            int fail_row = 0;
            if (table_type_column(t, cidx, nt, &fail_row) != 0) {
                err->exit_code = EXIT_DATA;
                err->script_line = st->line;
                err->row = fail_row;
                snprintf(err->column, sizeof(err->column), "%s", st->col);
                snprintf(err->message, sizeof(err->message), "TYPE conversion failed");
                goto fail;
            }
        } else if (st->kind == STMT_SORT) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            int *idxs = tt_malloc(st->n_sort * sizeof(int));
            if (!idxs) { err->exit_code = EXIT_RESOURCE; goto fail; }
            for (size_t k = 0; k < st->n_sort; k++) {
                idxs[k] = table_find_column(t, st->sort_cols[k], strlen(st->sort_cols[k]));
                if (idxs[k] < 0) {
                    tt_free(idxs);
                    err->exit_code = EXIT_DOMAIN;
                    err->script_line = st->line;
                    goto fail;
                }
            }
            if (table_sort(t, idxs, st->sort_asc, st->n_sort) != 0) {
                tt_free(idxs);
                err->exit_code = EXIT_RESOURCE;
                goto fail;
            }
            tt_free(idxs);
        } else if (st->kind == STMT_WRITE) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            size_t nlen = st->null_token ? strlen(st->null_token) : 0;
            int rc;
            if (strcmp(st->fmt, "MARKDOWN") == 0) {
                rc = write_markdown(t, st->path, st->null_token, nlen);
            } else if (strcmp(st->fmt, "CSV") == 0) {
                rc = write_csv(t, st->path, st->header != 0, st->null_token, nlen);
            } else {
                err->exit_code = EXIT_DOMAIN;
                snprintf(err->message, sizeof(err->message), "WRITE format not fully wired");
                goto fail;
            }
            if (rc != EXIT_OK) {
                err->exit_code = rc;
                err->script_line = st->line;
                snprintf(err->message, sizeof(err->message), "WRITE failed");
                goto fail;
            }
            outputs++;
            fprintf(rf, "OUTPUT: line=%d kind=WRITE path=%s\n", st->line, st->path);

        } else if (st->kind == STMT_ADD_COLUMN) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            ColumnType nt = type_from_name(st->type_name);
            if ((int)nt < 0) { err->exit_code = EXIT_SYNTAX; goto fail; }
            size_t at = st->has_at ? (size_t)st->pos : t->ncol + 1;
            if (at < 1 || at > t->ncol + 1) { err->exit_code = EXIT_DOMAIN; goto fail; }
            if (table_add_column(t, st->col, strlen(st->col), nt) != 0) {
                err->exit_code = EXIT_DOMAIN; err->script_line = st->line; goto fail;
            }
            int new_idx = (int)t->ncol - 1;
            /* set default on all rows */
            for (size_t r = 0; r < t->nrow; r++) {
                Cell *cell = &t->rows[r][new_idx];
                if (!st->has_default) {
                    cell_set_null(cell);
                } else if (strcmp(st->default_val, "NULL") == 0) {
                    cell_set_null(cell);
                } else {
                    /* convert according to type */
                    if (nt == TYPE_STRING) cell_set_string(cell, st->default_val, strlen(st->default_val));
                    else if (nt == TYPE_DATE) {
                        int y,m,d;
                        if (parse_date(st->default_val, strlen(st->default_val), &y,&m,&d) != 0) {
                            err->exit_code = EXIT_DATA; goto fail;
                        }
                        cell->is_null = false; cell->owns = false; cell->v.date.y=y; cell->v.date.m=m; cell->v.date.d=d;
                    } else if (nt == TYPE_INTEGER) {
                        int64_t v; if (parse_integer(st->default_val, strlen(st->default_val), &v)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                        cell->is_null = false; cell->owns = false; cell->v.i64 = v;
                    } else if (nt == TYPE_DECIMAL) {
                        if (parse_decimal(st->default_val, strlen(st->default_val), cell) != 0) { err->exit_code=EXIT_DATA; goto fail; }
                    } else if (nt == TYPE_BOOLEAN) {
                        bool v; if (parse_boolean(st->default_val, strlen(st->default_val), &v)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                        cell->is_null = false; cell->owns = false; cell->v.boolean = v;
                    } else {
                        cell_set_string(cell, st->default_val, strlen(st->default_val));
                    }
                }
            }
            if (st->has_at && (size_t)st->pos != t->ncol) {
                table_move_column(t, new_idx, st->pos);
            }
        } else if (st->kind == STMT_DROP_COLUMN) {
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
            if (table_drop_column(t, cidx) != 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
        } else if (st->kind == STMT_RENAME_COLUMN) {
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
            if (table_rename_column(t, cidx, st->col2, strlen(st->col2)) != 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
        } else if (st->kind == STMT_MOVE_COLUMN) {
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
            if (table_move_column(t, cidx, st->pos) != 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
        } else if (st->kind == STMT_SWAP_COLUMNS) {
            int a = table_find_column(t, st->col, strlen(st->col));
            int b = table_find_column(t, st->col2, strlen(st->col2));
            if (a < 0 || b < 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
            table_swap_columns(t, a, b);
        } else if (st->kind == STMT_SET_CELL || st->kind == STMT_SET_NULL) {
            if (st->pos < 1 || (size_t)st->pos > t->nrow) { err->exit_code = EXIT_DOMAIN; goto fail; }
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) { err->exit_code = EXIT_DOMAIN; goto fail; }
            Cell *cell = &t->rows[st->pos - 1][cidx];
            if (st->kind == STMT_SET_NULL) {
                cell_set_null(cell);
            } else {
                ColumnType ty = t->cols[cidx].type;
                cell_clear(cell);
                if (ty == TYPE_STRING) cell_set_string(cell, st->default_val, strlen(st->default_val));
                else if (ty == TYPE_DATE) {
                    int y,m,d; if (parse_date(st->default_val, strlen(st->default_val), &y,&m,&d)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                    cell->is_null = false; cell->owns = false; cell->v.date.y=y; cell->v.date.m=m; cell->v.date.d=d;
                } else if (ty == TYPE_INTEGER) {
                    int64_t v; if (parse_integer(st->default_val, strlen(st->default_val), &v)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                    cell->is_null = false; cell->owns = false; cell->v.i64 = v;
                } else if (ty == TYPE_DECIMAL) {
                    if (parse_decimal(st->default_val, strlen(st->default_val), cell)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                } else if (ty == TYPE_BOOLEAN) {
                    bool v; if (parse_boolean(st->default_val, strlen(st->default_val), &v)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                    cell->is_null = false; cell->owns = false; cell->v.boolean = v;
                } else {
                    cell_set_string(cell, st->default_val, strlen(st->default_val));
                }
            }
        } else if (st->kind == STMT_DELETE_ROW) {
            if (st->pos < 1 || (size_t)st->pos > t->nrow) { err->exit_code = EXIT_DOMAIN; goto fail; }
            table_delete_row(t, (size_t)st->pos - 1);
        } else if (st->kind == STMT_MOVE_ROW) {
            if (st->pos < 1 || st->pos2 < 1 || (size_t)st->pos > t->nrow || (size_t)st->pos2 > t->nrow) {
                err->exit_code = EXIT_DOMAIN; goto fail;
            }
            table_move_row(t, (size_t)st->pos - 1, (size_t)st->pos2 - 1);
        } else if (st->kind == STMT_SWAP_ROWS) {
            if (st->pos < 1 || st->pos2 < 1 || (size_t)st->pos > t->nrow || (size_t)st->pos2 > t->nrow) {
                err->exit_code = EXIT_DOMAIN; goto fail;
            }
            table_swap_rows(t, (size_t)st->pos - 1, (size_t)st->pos2 - 1);
        } else if (st->kind == STMT_INSERT_ROW || st->kind == STMT_APPEND_ROW) {
            if (st->n_values != t->ncol) { err->exit_code = EXIT_DOMAIN; goto fail; }
            Cell *nrow = tt_calloc(t->ncol, sizeof(Cell));
            for (size_t c = 0; c < t->ncol; c++) {
                const char *v = st->values[c];
                ColumnType ty = t->cols[c].type;
                if (strcmp(v, "NULL") == 0) {
                    cell_set_null(&nrow[c]);
                } else if (ty == TYPE_STRING) {
                    cell_set_string(&nrow[c], v, strlen(v));
                } else if (ty == TYPE_INTEGER) {
                    int64_t iv; if (parse_integer(v, strlen(v), &iv)!=0) { /* free nrow */ err->exit_code=EXIT_DATA; goto fail; }
                    nrow[c].is_null = false; nrow[c].owns = false; nrow[c].v.i64 = iv;
                } else if (ty == TYPE_DECIMAL) {
                    if (parse_decimal(v, strlen(v), &nrow[c])!=0) { err->exit_code=EXIT_DATA; goto fail; }
                } else if (ty == TYPE_BOOLEAN) {
                    bool bv; if (parse_boolean(v, strlen(v), &bv)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                    nrow[c].is_null = false; nrow[c].owns = false; nrow[c].v.boolean = bv;
                } else if (ty == TYPE_DATE) {
                    int y,m,d; if (parse_date(v, strlen(v), &y,&m,&d)!=0) { err->exit_code=EXIT_DATA; goto fail; }
                    nrow[c].is_null = false; nrow[c].owns = false; nrow[c].v.date.y=y; nrow[c].v.date.m=m; nrow[c].v.date.d=d;
                } else {
                    cell_set_string(&nrow[c], v, strlen(v));
                }
            }
            if (st->kind == STMT_APPEND_ROW) {
                table_append_row(t, nrow);
            } else {
                if (st->pos < 1 || (size_t)st->pos > t->nrow + 1) { err->exit_code = EXIT_DOMAIN; goto fail; }
                table_insert_row(t, (size_t)st->pos - 1, nrow);
            }
        } else if (st->kind == STMT_FIND) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            /* build matching row indices */
            size_t *match = tt_malloc(t->nrow * sizeof(size_t));
            size_t nm = 0;
            if (!match && t->nrow) { err->exit_code = EXIT_RESOURCE; goto fail; }
            size_t qlen = strlen(st->query);
            for (size_t r = 0; r < t->nrow; r++) {
                int hit = 0;
                for (size_t k = 0; k < st->n_in; k++) {
                    int cidx = table_find_column(t, st->in_cols[k], strlen(st->in_cols[k]));
                    if (cidx < 0) { tt_free(match); err->exit_code = EXIT_DOMAIN; err->script_line = st->line; goto fail; }
                    Cell *cell = &t->rows[r][cidx];
                    if (cell->is_null) continue;
                    size_t clen; char *ctext = cell_canonical(cell, t->cols[cidx].type, &clen);
                    size_t found = utf8_find((unsigned char*)ctext, clen, (unsigned char*)st->query, qlen, !st->mode_sensitive);
                    tt_free(ctext);
                    if (found != (size_t)-1) { hit = 1; break; }
                }
                if (hit || qlen == 0) match[nm++] = r;
            }
            /* write subset table */
            Table *ft = table_create();
            for (size_t c = 0; c < t->ncol; c++) {
                table_add_column(ft, t->cols[c].name, t->cols[c].name_len, t->cols[c].type);
            }
            for (size_t i = 0; i < nm; i++) {
                Cell *nrow = tt_calloc(t->ncol, sizeof(Cell));
                for (size_t c = 0; c < t->ncol; c++) {
                    cell_copy(&nrow[c], &t->rows[match[i]][c], t->cols[c].type);
                }
                table_append_row(ft, nrow);
            }
            tt_free(match);
            size_t nlen = st->null_token ? strlen(st->null_token) : 0;
            int rc;
            if (strcmp(st->fmt, "CSV") == 0) rc = write_csv(ft, st->path, 1, st->null_token, nlen);
            else if (strcmp(st->fmt, "MARKDOWN") == 0) rc = write_markdown(ft, st->path, st->null_token, nlen);
            else { table_free(ft); err->exit_code = EXIT_DOMAIN; goto fail; }
            table_free(ft);
            if (rc != EXIT_OK) { err->exit_code = rc; err->script_line = st->line; goto fail; }
            outputs++;
            fprintf(rf, "OUTPUT: line=%d kind=FIND path=%s\n", st->line, st->path);
        } else if (st->kind == STMT_BARCODE_SHEET) {
            if (!t) { err->exit_code = EXIT_DOMAIN; goto fail; }
            int cidx = table_find_column(t, st->col, strlen(st->col));
            if (cidx < 0) { err->exit_code = EXIT_DOMAIN; err->script_line = st->line; goto fail; }
            if (t->cols[cidx].type != TYPE_EAN13 && t->cols[cidx].type != TYPE_CODE128) {
                err->exit_code = EXIT_DOMAIN; err->script_line = st->line; goto fail;
            }
            int rc;
            if (t->cols[cidx].type == TYPE_EAN13)
                rc = write_ean_svg_sheet(t, cidx, st->path, st->module, st->height, st->gap, st->text);
            else
                rc = write_code128_svg_sheet(t, cidx, st->path, st->module, st->height, st->gap, st->text);
            if (rc != EXIT_OK) { err->exit_code = rc; err->script_line = st->line; goto fail; }
            outputs++;
            fprintf(rf, "OUTPUT: line=%d kind=BARCODE-SHEET path=%s\n", st->line, st->path);
        } else {
            err->exit_code = EXIT_DOMAIN;
            err->script_line = st->line;
            snprintf(err->message, sizeof(err->message), "unsupported statement in this build stage");
            goto fail;
        }
    }

    fprintf(rf, "STATUS: SUCCESS\n");
    fprintf(rf, "EXIT_CODE: 0\n");
    fprintf(rf, "SCRIPT: %s\n", "script"); /* proper escape later */
    fprintf(rf, "COMMANDS_PARSED: %zu\n", s->n);
    fprintf(rf, "COMMANDS_EXECUTED: %d\n", executed);
    fprintf(rf, "OUTPUTS_WRITTEN: %d\n", outputs);
    fclose(rf);
    if (t) table_free(t);
    return 0;

fail:
    fprintf(rf, "STATUS: FAILED\n");
    fprintf(rf, "EXIT_CODE: %d\n", err->exit_code);
    fprintf(rf, "SCRIPT: script\n");
    fprintf(rf, "COMMANDS_PARSED: %zu\n", s->n);
    fprintf(rf, "COMMANDS_EXECUTED: %d\n", executed);
    fprintf(rf, "OUTPUTS_WRITTEN: %d\n", outputs);
    fprintf(rf, "ERROR: script_line=%d message=%s\n", err->script_line, err->message);
    fclose(rf);
    if (t) table_free(t);
    return -1;
}
