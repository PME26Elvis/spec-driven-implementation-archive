#include "table.h"
#include "ean.h"
#include "url.h"
#include <stdio.h>

static int tt_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}


Table *table_create(void) {
    Table *t = tt_calloc(1, sizeof(Table));
    return t;
}

void cell_clear(Cell *c) {
    if (!c) return;
    if (c->owns && c->v.str.data) {
        tt_free(c->v.str.data);
    }
    memset(c, 0, sizeof(*c));
    c->is_null = true;
    c->owns = false;
}

void table_free(Table *t) {
    if (!t) return;
    for (size_t r = 0; r < t->nrow; r++) {
        if (t->rows[r]) {
            for (size_t c = 0; c < t->ncol; c++) {
                cell_clear(&t->rows[r][c]);
            }
            tt_free(t->rows[r]);
        }
    }
    tt_free(t->rows);
    for (size_t c = 0; c < t->ncol; c++) {
        tt_free(t->cols[c].name);
    }
    tt_free(t->cols);
    tt_free(t);
}

int table_find_column(const Table *t, const char *name, size_t name_len) {
    for (size_t i = 0; i < t->ncol; i++) {
        if (t->cols[i].name_len == name_len &&
            memcmp(t->cols[i].name, name, name_len) == 0)
            return (int)i;
    }
    return -1;
}

int table_add_column(Table *t, const char *name, size_t name_len, ColumnType type) {
    if (table_find_column(t, name, name_len) >= 0) return -1;
    if (t->ncol >= t->col_cap) {
        size_t nc = t->col_cap ? t->col_cap * 2 : 8;
        Column *n = tt_realloc(t->cols, nc * sizeof(Column));
        if (!n) return -1;
        t->cols = n;
        t->col_cap = nc;
    }
    t->cols[t->ncol].name = tt_strndup(name, name_len);
    if (!t->cols[t->ncol].name) return -1;
    t->cols[t->ncol].name_len = name_len;
    t->cols[t->ncol].type = type;
    /* extend existing rows */
    for (size_t r = 0; r < t->nrow; r++) {
        Cell *nrow = tt_realloc(t->rows[r], (t->ncol + 1) * sizeof(Cell));
        if (!nrow) return -1;
        t->rows[r] = nrow;
        t->rows[r][t->ncol].is_null = true;
        t->rows[r][t->ncol].v.str.data = NULL;
        t->rows[r][t->ncol].v.str.len = 0;
    }
    t->ncol++;
    return 0;
}

void cell_set_null(Cell *c) {
    cell_clear(c);
    c->is_null = true;
}

int cell_set_string(Cell *c, const char *s, size_t n) {
    cell_clear(c);
    char *p = tt_strndup(s, n);
    if (!p) return -1;
    c->is_null = false;
    c->owns = true;
    c->v.str.data = p;
    c->v.str.len = n;
    return 0;
}

int cell_set_string_owned(Cell *c, char *s, size_t n) {
    cell_clear(c);
    c->is_null = false;
    c->owns = true;
    c->v.str.data = s;
    c->v.str.len = n;
    return 0;
}

ColumnType type_from_name(const char *s) {
    if (tt_strcasecmp(s, "STRING") == 0) return TYPE_STRING;
    if (tt_strcasecmp(s, "INTEGER") == 0) return TYPE_INTEGER;
    if (tt_strcasecmp(s, "DECIMAL") == 0) return TYPE_DECIMAL;
    if (tt_strcasecmp(s, "BOOLEAN") == 0) return TYPE_BOOLEAN;
    if (tt_strcasecmp(s, "DATE") == 0) return TYPE_DATE;
    if (tt_strcasecmp(s, "URL") == 0) return TYPE_URL;
    if (tt_strcasecmp(s, "EAN13") == 0) return TYPE_EAN13;
    if (tt_strcasecmp(s, "CODE128") == 0) return TYPE_CODE128;
    return (ColumnType)-1;
}

const char *type_name(ColumnType t) {
    switch (t) {
    case TYPE_STRING: return "STRING";
    case TYPE_INTEGER: return "INTEGER";
    case TYPE_DECIMAL: return "DECIMAL";
    case TYPE_BOOLEAN: return "BOOLEAN";
    case TYPE_DATE: return "DATE";
    case TYPE_URL: return "URL";
    case TYPE_EAN13: return "EAN13";
    case TYPE_CODE128: return "CODE128";
    }
    return "?";
}

int parse_integer(const char *s, size_t n, int64_t *out) {
    if (n == 0) return -1;
    size_t i = 0;
    int sign = 1;
    if (s[0] == '+') { i = 1; }
    else if (s[0] == '-') { sign = -1; i = 1; }
    if (i >= n) return -1;
    uint64_t val = 0;
    bool any = false;
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        any = true;
        int d = s[i] - '0';
        if (val > (uint64_t)INT64_MAX / 10) return -1;
        val = val * 10 + (uint64_t)d;
    }
    if (!any) return -1;
    if (sign > 0) {
        if (val > (uint64_t)INT64_MAX) return -1;
        *out = (int64_t)val;
    } else {
        if (val > (uint64_t)INT64_MAX + 1) return -1; /* for INT64_MIN */
        if (val == (uint64_t)INT64_MAX + 1) *out = INT64_MIN;
        else *out = -(int64_t)val;
    }
    return 0;
}

int parse_boolean(const char *s, size_t n, bool *out) {
    if (n == 1 && s[0] == '1') { *out = true; return 0; }
    if (n == 1 && s[0] == '0') { *out = false; return 0; }
    if (n == 4 && (s[0]=='t'||s[0]=='T') && (s[1]=='r'||s[1]=='R') &&
        (s[2]=='u'||s[2]=='U') && (s[3]=='e'||s[3]=='E')) { *out = true; return 0; }
    if (n == 5 && (s[0]=='f'||s[0]=='F') && (s[1]=='a'||s[1]=='A') &&
        (s[2]=='l'||s[2]=='L') && (s[3]=='s'||s[3]=='S') &&
        (s[4]=='e'||s[4]=='E')) { *out = false; return 0; }
    return -1;
}

static int is_leap(int y) {
    if (y % 4 != 0) return 0;
    if (y % 100 != 0) return 1;
    return (y % 400 == 0);
}

int parse_date(const char *s, size_t n, int *y, int *m, int *d) {
    if (n != 10) return -1;
    if (s[4] != '-' || s[7] != '-') return -1;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (s[i] < '0' || s[i] > '9') return -1;
    }
    int yy = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    int mm = (s[5]-'0')*10 + (s[6]-'0');
    int dd = (s[8]-'0')*10 + (s[9]-'0');
    if (yy < 1 || yy > 9999) return -1;
    if (mm < 1 || mm > 12) return -1;
    int mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mm == 2 && is_leap(yy)) mdays[2] = 29;
    if (dd < 1 || dd > mdays[mm]) return -1;
    *y = yy; *m = mm; *d = dd;
    return 0;
}

/* Simplified DECIMAL: store as string canonical for now; full exact later */
int parse_decimal(const char *s, size_t n, Cell *out) {
    /* Basic validation and canonicalization */
    if (n == 0) return -1;
    size_t i = 0;
    int sign = 1;
    if (s[0] == '+') i = 1;
    else if (s[0] == '-') { sign = -1; i = 1; }
    if (i >= n) return -1;
    size_t int_start = i;
    while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    size_t int_end = i;
    size_t frac_start = 0, frac_end = 0;
    if (i < n && s[i] == '.') {
        i++;
        frac_start = i;
        while (i < n && s[i] >= '0' && s[i] <= '9') i++;
        frac_end = i;
        if (frac_end == frac_start) return -1; /* no digit after . */
    }
    if (i != n) return -1;
    if (int_end == int_start && frac_end == frac_start) return -1;
    int scale = (int)(frac_end - frac_start);
    if (scale > 18) return -1;
    /* precision calculation */
    /* strip trailing frac zeros for precision */
    size_t fend = frac_end;
    while (fend > frac_start && s[fend-1] == '0') fend--;
    /* combined digits */
    size_t prec = 0;
    size_t is = int_start;
    while (is < int_end && s[is] == '0') is++;
    prec += (int_end - is);
    prec += (fend - frac_start);
    if (prec == 0) prec = 1;
    if (prec > 38) return -1;
    /* store as string for simplicity in this version; real exact later */
    /* Build canonical */
    char buf[128];
    size_t bp = 0;
    if (sign < 0) buf[bp++] = '-';
    /* integer part without leading zeros */
    if (is == int_end) {
        buf[bp++] = '0';
    } else {
        for (size_t k = is; k < int_end; k++) buf[bp++] = s[k];
    }
    if (fend > frac_start) {
        buf[bp++] = '.';
        for (size_t k = frac_start; k < fend; k++) buf[bp++] = s[k];
    }
    /* special zero */
    if ((bp == 1 && buf[0] == '0') || (bp == 2 && buf[0] == '-' && buf[1] == '0')) {
        buf[0] = '0'; bp = 1;
    }
    buf[bp] = '\0';
    return cell_set_string(out, buf, bp);
}

char *cell_canonical(const Cell *c, ColumnType type, size_t *out_len) {
    if (c->is_null) {
        *out_len = 0;
        return tt_strdup("");
    }
    char buf[64];
    switch (type) {
    case TYPE_STRING:
    case TYPE_URL:
    case TYPE_EAN13:
    case TYPE_CODE128:
    case TYPE_DECIMAL: /* stored as str */
        *out_len = c->v.str.len;
        return tt_strndup(c->v.str.data, c->v.str.len);
    case TYPE_INTEGER: {
        int n = snprintf(buf, sizeof(buf), "%" PRId64, c->v.i64);
        *out_len = (size_t)n;
        return tt_strndup(buf, (size_t)n);
    }
    case TYPE_BOOLEAN: {
        const char *s = c->v.boolean ? "true" : "false";
        *out_len = strlen(s);
        return tt_strdup(s);
    }
    case TYPE_DATE: {
        int n = snprintf(buf, sizeof(buf), "%04d-%02d-%02d", c->v.date.y, c->v.date.m, c->v.date.d);
        *out_len = (size_t)n;
        return tt_strndup(buf, (size_t)n);
    }
    }
    *out_len = 0;
    return tt_strdup("");
}

int cell_copy(Cell *dst, const Cell *src, ColumnType type) {
    cell_clear(dst);
    if (src->is_null) {
        dst->is_null = true;
        return 0;
    }
    switch (type) {
    case TYPE_STRING:
    case TYPE_URL:
    case TYPE_EAN13:
    case TYPE_CODE128:
    case TYPE_DECIMAL:
        return cell_set_string(dst, src->v.str.data, src->v.str.len);
    case TYPE_INTEGER:
        dst->is_null = false;
        dst->v.i64 = src->v.i64;
        return 0;
    case TYPE_BOOLEAN:
        dst->is_null = false;
        dst->v.boolean = src->v.boolean;
        return 0;
    case TYPE_DATE:
        dst->is_null = false;
        dst->v.date = src->v.date;
        return 0;
    }
    return -1;
}


/* Basic stable sort using indices - insertion for small, or merge later */
int table_sort(Table *t, int *col_idxs, int *asc, size_t nkeys) {
    if (t->nrow <= 1) return 0;
    size_t *idx = tt_malloc(t->nrow * sizeof(size_t));
    if (!idx) return -1;
    for (size_t i = 0; i < t->nrow; i++) idx[i] = i;
    /* simple insertion sort for stability and small n */
    for (size_t i = 1; i < t->nrow; i++) {
        size_t key = idx[i];
        size_t j = i;
        while (j > 0) {
            int cmp = 0;
            for (size_t k = 0; k < nkeys; k++) {
                size_t c = (size_t)col_idxs[k];
                Cell *a = &t->rows[idx[j-1]][c];
                Cell *b = &t->rows[key][c];
                ColumnType ty = t->cols[c].type;
                if (a->is_null && b->is_null) continue;
                if (a->is_null) { cmp = 1; break; } /* NULL last */
                if (b->is_null) { cmp = -1; break; }
                /* compare non-null */
                if (ty == TYPE_INTEGER) {
                    if (a->v.i64 < b->v.i64) cmp = -1;
                    else if (a->v.i64 > b->v.i64) cmp = 1;
                } else if (ty == TYPE_BOOLEAN) {
                    if (a->v.boolean != b->v.boolean) cmp = a->v.boolean ? 1 : -1;
                } else if (ty == TYPE_DATE) {
                    if (a->v.date.y != b->v.date.y) cmp = a->v.date.y < b->v.date.y ? -1 : 1;
                    else if (a->v.date.m != b->v.date.m) cmp = a->v.date.m < b->v.date.m ? -1 : 1;
                    else if (a->v.date.d != b->v.date.d) cmp = a->v.date.d < b->v.date.d ? -1 : 1;
                } else {
                    /* string-like */
                    size_t al, bl;
                    char *as = cell_canonical(a, ty, &al);
                    char *bs = cell_canonical(b, ty, &bl);
                    cmp = utf8_strcmp((unsigned char*)as, al, (unsigned char*)bs, bl);
                    tt_free(as); tt_free(bs);
                }
                if (cmp != 0) {
                    if (!asc[k]) cmp = -cmp;
                    break;
                }
            }
            if (cmp <= 0) break;
            idx[j] = idx[j-1];
            j--;
        }
        idx[j] = key;
    }
    /* apply permutation */
    Cell **new_rows = tt_malloc(t->nrow * sizeof(Cell*));
    if (!new_rows) { tt_free(idx); return -1; }
    for (size_t i = 0; i < t->nrow; i++) new_rows[i] = t->rows[idx[i]];
    tt_free(t->rows);
    t->rows = new_rows;
    tt_free(idx);
    return 0;
}

int table_drop_column(Table *t, int col_idx) {
    if (col_idx < 0 || (size_t)col_idx >= t->ncol) return -1;
    if (t->ncol <= 1) return -1; /* cannot drop last */
    for (size_t r = 0; r < t->nrow; r++) {
        cell_clear(&t->rows[r][col_idx]);
        for (size_t c = (size_t)col_idx; c + 1 < t->ncol; c++) {
            t->rows[r][c] = t->rows[r][c+1];
        }
        /* last cell is now duplicate; clear it conceptually by shrinking */
    }
    tt_free(t->cols[col_idx].name);
    for (size_t c = (size_t)col_idx; c + 1 < t->ncol; c++) {
        t->cols[c] = t->cols[c+1];
    }
    t->ncol--;
    return 0;
}

int table_rename_column(Table *t, int col_idx, const char *new_name, size_t new_len) {
    if (col_idx < 0 || (size_t)col_idx >= t->ncol) return -1;
    if (new_len == 0) return -1;
    int other = table_find_column(t, new_name, new_len);
    if (other >= 0 && other != col_idx) return -1;
    char *n = tt_strndup(new_name, new_len);
    if (!n) return -1;
    tt_free(t->cols[col_idx].name);
    t->cols[col_idx].name = n;
    t->cols[col_idx].name_len = new_len;
    return 0;
}

int table_move_column(Table *t, int from, int to_pos) {
    /* to_pos is 1-based final position */
    if (from < 0 || (size_t)from >= t->ncol) return -1;
    if (to_pos < 1 || (size_t)to_pos > t->ncol) return -1;
    int to = to_pos - 1;
    if (from == to) return 0;
    Column col = t->cols[from];
    if (from < to) {
        for (int i = from; i < to; i++) t->cols[i] = t->cols[i+1];
    } else {
        for (int i = from; i > to; i--) t->cols[i] = t->cols[i-1];
    }
    t->cols[to] = col;
    for (size_t r = 0; r < t->nrow; r++) {
        Cell cell = t->rows[r][from];
        if (from < to) {
            for (int i = from; i < to; i++) t->rows[r][i] = t->rows[r][i+1];
        } else {
            for (int i = from; i > to; i--) t->rows[r][i] = t->rows[r][i-1];
        }
        t->rows[r][to] = cell;
    }
    return 0;
}

int table_swap_columns(Table *t, int a, int b) {
    if (a < 0 || b < 0 || (size_t)a >= t->ncol || (size_t)b >= t->ncol) return -1;
    if (a == b) return 0;
    Column tmpc = t->cols[a]; t->cols[a] = t->cols[b]; t->cols[b] = tmpc;
    for (size_t r = 0; r < t->nrow; r++) {
        Cell tmp = t->rows[r][a]; t->rows[r][a] = t->rows[r][b]; t->rows[r][b] = tmp;
    }
    return 0;
}

int table_append_row(Table *t, Cell *cells) {
    if (t->nrow >= t->row_cap) {
        size_t nc = t->row_cap ? t->row_cap * 2 : 8;
        Cell **nr = tt_realloc(t->rows, nc * sizeof(Cell*));
        if (!nr) return -1;
        t->rows = nr; t->row_cap = nc;
    }
    t->rows[t->nrow++] = cells;
    return 0;
}

int table_insert_row(Table *t, size_t at, Cell *cells) {
    if (at > t->nrow) return -1;
    if (table_append_row(t, cells) != 0) return -1;
    if (at == t->nrow - 1) return 0;
    /* shift */
    Cell *tmp = t->rows[t->nrow - 1];
    for (size_t i = t->nrow - 1; i > at; i--) t->rows[i] = t->rows[i-1];
    t->rows[at] = tmp;
    return 0;
}

int table_delete_row(Table *t, size_t row) {
    if (row >= t->nrow) return -1;
    for (size_t c = 0; c < t->ncol; c++) cell_clear(&t->rows[row][c]);
    tt_free(t->rows[row]);
    for (size_t i = row; i + 1 < t->nrow; i++) t->rows[i] = t->rows[i+1];
    t->nrow--;
    return 0;
}

int table_move_row(Table *t, size_t from, size_t to) {
    if (from >= t->nrow || to >= t->nrow) return -1;
    if (from == to) return 0;
    Cell *row = t->rows[from];
    if (from < to) {
        for (size_t i = from; i < to; i++) t->rows[i] = t->rows[i+1];
    } else {
        for (size_t i = from; i > to; i--) t->rows[i] = t->rows[i-1];
    }
    t->rows[to] = row;
    return 0;
}

int table_swap_rows(Table *t, size_t a, size_t b) {
    if (a >= t->nrow || b >= t->nrow) return -1;
    if (a == b) return 0;
    Cell *tmp = t->rows[a]; t->rows[a] = t->rows[b]; t->rows[b] = tmp;
    return 0;
}

int table_type_column(Table *t, int col_idx, ColumnType new_type, int *fail_row) {
    if (col_idx < 0 || (size_t)col_idx >= t->ncol) return -1;
    ColumnType old = t->cols[col_idx].type;
    if (old == new_type) return 0;
    /* first validate all */
    for (size_t r = 0; r < t->nrow; r++) {
        Cell *c = &t->rows[r][col_idx];
        if (c->is_null) continue;
        size_t clen;
        char *ctext = cell_canonical(c, old, &clen);
        if (!ctext) { *fail_row = (int)(r+1); return -1; }
        int ok = 0;
        Cell tmp; memset(&tmp, 0, sizeof(tmp)); tmp.is_null = true;
        if (new_type == TYPE_STRING) {
            ok = cell_set_string(&tmp, ctext, clen) == 0;
        } else if (new_type == TYPE_INTEGER) {
            int64_t v;
            ok = parse_integer(ctext, clen, &v) == 0;
            if (ok) { tmp.is_null = false; tmp.v.i64 = v; }
        } else if (new_type == TYPE_DECIMAL) {
            ok = parse_decimal(ctext, clen, &tmp) == 0;
        } else if (new_type == TYPE_BOOLEAN) {
            bool v;
            ok = parse_boolean(ctext, clen, &v) == 0;
            if (ok) { tmp.is_null = false; tmp.v.boolean = v; }
        } else if (new_type == TYPE_DATE) {
            int y,m,d;
            ok = parse_date(ctext, clen, &y, &m, &d) == 0;
            if (ok) { tmp.is_null = false; tmp.v.date.y=y; tmp.v.date.m=m; tmp.v.date.d=d; }
        } else if (new_type == TYPE_EAN13) {
            char out[14];
            ok = ean13_canonicalize(ctext, clen, out) == 0; /* need extern */
            if (ok) ok = cell_set_string(&tmp, out, 13) == 0;
        } else if (new_type == TYPE_URL) {
            char *uout = NULL; size_t ulen = 0;
            ok = url_canonicalize(ctext, clen, &uout, &ulen) == 0;
            if (ok) { ok = cell_set_string_owned(&tmp, uout, ulen) == 0; if (!ok) tt_free(uout); }
        } else if (new_type == TYPE_CODE128) {
            ok = 1;
            if (clen == 0 || clen > 256) ok = 0;
            for (size_t i = 0; i < clen && ok; i++) {
                unsigned char ch = (unsigned char)ctext[i];
                if (ch < 32 || ch > 126) ok = 0;
            }
            if (ok) ok = cell_set_string(&tmp, ctext, clen) == 0;
        }
        tt_free(ctext);
        cell_clear(&tmp);
        if (!ok) { *fail_row = (int)(r+1); return -1; }
    }
    /* apply */
    for (size_t r = 0; r < t->nrow; r++) {
        Cell *c = &t->rows[r][col_idx];
        if (c->is_null) continue;
        size_t clen;
        char *ctext = cell_canonical(c, old, &clen);
        cell_clear(c);
        if (new_type == TYPE_STRING || new_type == TYPE_CODE128) {
            cell_set_string(c, ctext, clen);
        } else if (new_type == TYPE_INTEGER) {
            int64_t v; parse_integer(ctext, clen, &v);
            c->is_null = false; c->owns = false; c->v.i64 = v;
        } else if (new_type == TYPE_DECIMAL) {
            parse_decimal(ctext, clen, c);
        } else if (new_type == TYPE_BOOLEAN) {
            bool v; parse_boolean(ctext, clen, &v);
            c->is_null = false; c->owns = false; c->v.boolean = v;
        } else if (new_type == TYPE_DATE) {
            int y,m,d; parse_date(ctext, clen, &y,&m,&d);
            c->is_null = false; c->owns = false; c->v.date.y=y; c->v.date.m=m; c->v.date.d=d;
        } else if (new_type == TYPE_EAN13) {
            char out[14]; ean13_canonicalize(ctext, clen, out);
            cell_set_string(c, out, 13);
        } else if (new_type == TYPE_URL) {
            char *uout = NULL; size_t ulen = 0;
            url_canonicalize(ctext, clen, &uout, &ulen);
            cell_set_string_owned(c, uout, ulen);
        }
        tt_free(ctext);
    }
    t->cols[col_idx].type = new_type;
    return 0;
}
