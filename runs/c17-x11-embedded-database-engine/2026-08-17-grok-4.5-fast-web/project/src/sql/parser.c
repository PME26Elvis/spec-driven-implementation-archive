#include "edb/sql_parser.h"
#include "edb/byteorder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    edb_lexer lex;
    edb_error *err;
} parser_t;

static void error_at(parser_t *p, const char *msg) {
    edb_error_set(p->err, EDB_SQL_PARSE, 0, msg);
}

static bool accept(parser_t *p, edb_token_kind k) {
    if (p->lex.current.kind == k) {
        edb_lexer_next(&p->lex);
        return true;
    }
    return false;
}

static bool expect(parser_t *p, edb_token_kind k, const char *what) {
    if (accept(p, k)) return true;
    char buf[128];
    snprintf(buf, sizeof buf, "expected %s", what);
    error_at(p, buf);
    return false;
}

static void copy_tok(char *dst, size_t dstsz, const edb_token *t) {
    size_t n = t->len < dstsz - 1 ? t->len : dstsz - 1;
    memcpy(dst, t->start, n);
    dst[n] = 0;
}

static edb_val_type parse_type(parser_t *p) {
    if (accept(p, EDB_TOK_KW_INTEGER_TYPE)) return EDB_VAL_INTEGER;
    if (accept(p, EDB_TOK_KW_REAL_TYPE)) return EDB_VAL_REAL;
    if (accept(p, EDB_TOK_KW_TEXT_TYPE)) return EDB_VAL_TEXT;
    if (accept(p, EDB_TOK_KW_BLOB_TYPE)) return EDB_VAL_BLOB;
    error_at(p, "expected type");
    return EDB_VAL_NULL;
}

static bool parse_coldef(parser_t *p, edb_ast_coldef *c) {
    memset(c, 0, sizeof(*c));
    if (p->lex.current.kind != EDB_TOK_ID) {
        error_at(p, "expected column name");
        return false;
    }
    copy_tok(c->name, sizeof c->name, &p->lex.current);
    edb_lexer_next(&p->lex);
    c->type = parse_type(p);
    if (p->err->cat != EDB_OK) return false;
    while (p->lex.current.kind == EDB_TOK_KW_NOT ||
           p->lex.current.kind == EDB_TOK_KW_PRIMARY ||
           p->lex.current.kind == EDB_TOK_KW_DEFAULT) {
        if (accept(p, EDB_TOK_KW_NOT)) {
            if (!expect(p, EDB_TOK_NULL, "NULL")) return false;
            c->not_null = true;
        } else if (accept(p, EDB_TOK_KW_PRIMARY)) {
            if (!expect(p, EDB_TOK_KW_KEY, "KEY")) return false;
            c->primary_key = true;
            c->not_null = true;
        } else if (accept(p, EDB_TOK_KW_DEFAULT)) {
            c->has_default = true;
            if (p->lex.current.kind == EDB_TOK_INTEGER) {
                c->default_val.type = EDB_VAL_INTEGER;
                c->default_val.u.i64 = p->lex.current.int_val;
                edb_lexer_next(&p->lex);
            } else if (p->lex.current.kind == EDB_TOK_NULL) {
                c->default_val.type = EDB_VAL_NULL;
                edb_lexer_next(&p->lex);
            } else if (p->lex.current.kind == EDB_TOK_STRING) {
                c->default_val.type = EDB_VAL_TEXT;
                /* note: points into original SQL; caller must keep SQL alive or copy */
                c->default_val.u.bin.p = (const uint8_t*)p->lex.current.start;
                c->default_val.u.bin.len = (uint32_t)p->lex.current.len;
                edb_lexer_next(&p->lex);
            } else {
                error_at(p, "unsupported default");
                return false;
            }
        }
    }
    return true;
}

static edb_ast *parse_create_table(parser_t *p) {
    edb_ast *a = calloc(1, sizeof(*a));
    if (!a) { error_at(p, "oom"); return NULL; }
    a->kind = EDB_AST_CREATE_TABLE;
    if (!expect(p, EDB_TOK_KW_TABLE, "TABLE")) { free(a); return NULL; }
    if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected table name"); free(a); return NULL; }
    copy_tok(a->name, sizeof a->name, &p->lex.current);
    edb_lexer_next(&p->lex);
    if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
    do {
        if (a->col_count >= EDB_MAX_COLS) { error_at(p, "too many columns"); free(a); return NULL; }
        if (!parse_coldef(p, &a->cols[a->col_count])) { free(a); return NULL; }
        a->col_count++;
    } while (accept(p, EDB_TOK_COMMA));
    if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    accept(p, EDB_TOK_SEMI);
    return a;
}

static edb_ast *parse_create_index(parser_t *p) {
    edb_ast *a = calloc(1, sizeof(*a));
    if (!a) { error_at(p, "oom"); return NULL; }
    a->kind = EDB_AST_CREATE_INDEX;
    if (accept(p, EDB_TOK_KW_UNIQUE)) a->unique_index = true;
    if (!expect(p, EDB_TOK_KW_INDEX, "INDEX")) { free(a); return NULL; }
    if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected index name"); free(a); return NULL; }
    copy_tok(a->name, sizeof a->name, &p->lex.current);
    edb_lexer_next(&p->lex);
    if (!expect(p, EDB_TOK_KW_ON, "ON")) { free(a); return NULL; }
    if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected table name"); free(a); return NULL; }
    copy_tok(a->table, sizeof a->table, &p->lex.current);
    edb_lexer_next(&p->lex);
    if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
    do {
        if (a->idx_col_count >= EDB_MAX_IDX_COLS) { error_at(p, "too many index columns"); free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        copy_tok(a->idx_cols[a->idx_col_count], sizeof a->idx_cols[0], &p->lex.current);
        a->idx_col_count++;
        edb_lexer_next(&p->lex);
    } while (accept(p, EDB_TOK_COMMA));
    if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    accept(p, EDB_TOK_SEMI);
    return a;
}

static bool parse_literal(parser_t *p, edb_value *v) {
    memset(v, 0, sizeof(*v));
    if (accept(p, EDB_TOK_NULL)) { v->type = EDB_VAL_NULL; return true; }
    if (p->lex.current.kind == EDB_TOK_INTEGER) {
        v->type = EDB_VAL_INTEGER; v->u.i64 = p->lex.current.int_val;
        edb_lexer_next(&p->lex); return true;
    }
    if (p->lex.current.kind == EDB_TOK_REAL) {
        v->type = EDB_VAL_REAL; v->u.real = p->lex.current.real_val;
        edb_lexer_next(&p->lex); return true;
    }
    if (p->lex.current.kind == EDB_TOK_STRING) {
        v->type = EDB_VAL_TEXT;
        v->u.bin.p = (const uint8_t*)p->lex.current.start;
        v->u.bin.len = (uint32_t)p->lex.current.len;
        edb_lexer_next(&p->lex); return true;
    }
    error_at(p, "expected literal");
    return false;
}

static edb_ast *parse_insert(parser_t *p) {
    edb_ast *a = calloc(1, sizeof(*a));
    if (!a) { error_at(p, "oom"); return NULL; }
    a->kind = EDB_AST_INSERT;
    if (!expect(p, EDB_TOK_KW_INTO, "INTO")) { free(a); return NULL; }
    if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected table"); free(a); return NULL; }
    copy_tok(a->table, sizeof a->table, &p->lex.current);
    edb_lexer_next(&p->lex);
    /* optional column list skipped for minimal parser */
    if (!expect(p, EDB_TOK_KW_VALUES, "VALUES")) { free(a); return NULL; }
    if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
    int cols = 0;
    do {
        if (a->value_count >= EDB_MAX_COLS * 8) { error_at(p, "too many values"); free(a); return NULL; }
        if (!parse_literal(p, &a->values[a->value_count])) { free(a); return NULL; }
        a->value_count++;
        cols++;
    } while (accept(p, EDB_TOK_COMMA));
    if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    a->insert_nrows = 1;
    int col_count = cols;
    while (accept(p, EDB_TOK_COMMA)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        int c = 0;
        do {
            if (a->value_count >= EDB_MAX_COLS * 8) { error_at(p, "too many values"); free(a); return NULL; }
            if (!parse_literal(p, &a->values[a->value_count])) { free(a); return NULL; }
            a->value_count++;
            c++;
        } while (accept(p, EDB_TOK_COMMA));
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
        if (c != col_count) { error_at(p, "row width mismatch"); free(a); return NULL; }
        a->insert_nrows++;
    }
    a->value_count = col_count; /* per-row width */
    accept(p, EDB_TOK_SEMI);
    return a;
}

static edb_ast *parse_select(parser_t *p) {
    edb_ast *a = calloc(1, sizeof(*a));
    if (!a) { error_at(p, "oom"); return NULL; }
    a->kind = EDB_AST_SELECT;
    if (accept(p, EDB_TOK_KW_DISTINCT)) a->is_distinct = true;
    if (accept(p, EDB_TOK_KW_COUNT)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        if (!expect(p, EDB_TOK_STAR, "*")) { free(a); return NULL; }
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
        a->is_count_star = true;
        a->select_star = false;
    } else if (accept(p, EDB_TOK_KW_SUM)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->is_sum = true;
        copy_tok(a->sum_col, sizeof a->sum_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    } else if (accept(p, EDB_TOK_KW_AVG)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->is_avg = true;
        copy_tok(a->sum_col, sizeof a->sum_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    } else if (accept(p, EDB_TOK_KW_MIN)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->is_min = true;
        copy_tok(a->sum_col, sizeof a->sum_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    } else if (accept(p, EDB_TOK_KW_MAX)) {
        if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->is_max = true;
        copy_tok(a->sum_col, sizeof a->sum_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
    } else if (accept(p, EDB_TOK_STAR)) {
        a->select_star = true;
    } else {
        do {
            if (a->select_col_count >= EDB_MAX_COLS) { error_at(p, "too many columns"); free(a); return NULL; }
            if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
            copy_tok(a->select_cols[a->select_col_count], sizeof a->select_cols[0], &p->lex.current);
            a->select_col_count++;
            edb_lexer_next(&p->lex);
        } while (accept(p, EDB_TOK_COMMA));
    }
    if (!expect(p, EDB_TOK_KW_FROM, "FROM")) { free(a); return NULL; }
    if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected table"); free(a); return NULL; }
    copy_tok(a->table, sizeof a->table, &p->lex.current);
    edb_lexer_next(&p->lex);
    int join_kw = 0;
    if (accept(p, EDB_TOK_KW_LEFT)) {
        if (!expect(p, EDB_TOK_KW_JOIN, "JOIN")) { free(a); return NULL; }
        a->is_left_join = true;
        join_kw = 1;
    } else if (accept(p, EDB_TOK_KW_INNER)) {
        if (!expect(p, EDB_TOK_KW_JOIN, "JOIN")) { free(a); return NULL; }
        join_kw = 1;
    } else if (accept(p, EDB_TOK_KW_JOIN)) {
        join_kw = 1;
    }
    if (join_kw) {
        a->has_join = true;
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected join table"); free(a); return NULL; }
        copy_tok(a->join_table, sizeof a->join_table, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_KW_ON, "ON")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        copy_tok(a->join_left_col, sizeof a->join_left_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (!expect(p, EDB_TOK_EQ, "=")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        copy_tok(a->join_right_col, sizeof a->join_right_col, &p->lex.current);
        edb_lexer_next(&p->lex);
    }
    if (accept(p, EDB_TOK_KW_WHERE)) {
        /* minimal: col = integer-literal */
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->where = calloc(1, sizeof(edb_ast_expr));
        if (!a->where) { error_at(p, "oom"); free(a); return NULL; }
        a->where->kind = EXPR_BINOP;
        a->where->left = calloc(1, sizeof(edb_ast_expr));
        a->where->right = calloc(1, sizeof(edb_ast_expr));
        a->where->left->kind = EXPR_COL;
        copy_tok(a->where->left->name, sizeof a->where->left->name, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (accept(p, EDB_TOK_KW_IS)) {
            int is_not = accept(p, EDB_TOK_KW_NOT) ? 1 : 0;
            if (!expect(p, EDB_TOK_NULL, "NULL")) { free(a); return NULL; }
            a->where->op = is_not ? EDB_TOK_KW_NOT : EDB_TOK_NULL; /* NULL = IS NULL, NOT = IS NOT NULL */
            a->where->right->kind = EXPR_LIT;
            a->where->right->lit.type = EDB_VAL_NULL;
            goto where_done;
        }
        if (accept(p, EDB_TOK_EQ)) a->where->op = EDB_TOK_EQ;
        else if (accept(p, EDB_TOK_NE)) a->where->op = EDB_TOK_NE;
        else if (accept(p, EDB_TOK_LE)) a->where->op = EDB_TOK_LE;
        else if (accept(p, EDB_TOK_GE)) a->where->op = EDB_TOK_GE;
        else if (accept(p, EDB_TOK_LT)) a->where->op = EDB_TOK_LT;
        else if (accept(p, EDB_TOK_GT)) a->where->op = EDB_TOK_GT;
        else if (accept(p, EDB_TOK_KW_BETWEEN)) {
            a->where->op = EDB_TOK_KW_BETWEEN;
            a->where->right->kind = EXPR_LIT;
            a->where->right->lit.type = EDB_VAL_BLOB;
            uint8_t pack[17]; pack[0] = 2;
            edb_value lo = {0}, hi = {0};
            if (!parse_literal(p, &lo) || lo.type != EDB_VAL_INTEGER) { error_at(p, "BETWEEN low"); free(a); return NULL; }
            if (!expect(p, EDB_TOK_KW_AND, "AND")) { free(a); return NULL; }
            if (!parse_literal(p, &hi) || hi.type != EDB_VAL_INTEGER) { error_at(p, "BETWEEN high"); free(a); return NULL; }
            edb_store_u64_le(pack+1, (uint64_t)lo.u.i64);
            edb_store_u64_le(pack+9, (uint64_t)hi.u.i64);
            a->where->right->lit.u.bin.len = 17;
            a->where->right->lit.u.bin.p = malloc(17);
            memcpy((void*)a->where->right->lit.u.bin.p, pack, 17);
            goto where_done;
        } else if (accept(p, EDB_TOK_KW_LIKE)) {
            a->where->op = EDB_TOK_KW_LIKE;
            a->where->right->kind = EXPR_LIT;
            if (!parse_literal(p, &a->where->right->lit)) { free(a); return NULL; }
            goto where_done;
        } else if (accept(p, EDB_TOK_KW_IN)) {
            a->where->op = EDB_TOK_KW_IN;
            if (!expect(p, EDB_TOK_LPAREN, "(")) { free(a); return NULL; }
            /* pack list into right as TEXT of comma-separated for bootstrap — better: multi-lit */
            a->where->right->kind = EXPR_LIT;
            a->where->right->lit.type = EDB_VAL_BLOB; /* reuse: store count then values in bin */
            uint8_t pack[512]; size_t pp = 1; int nlit = 0;
            do {
                edb_value v = {0};
                if (!parse_literal(p, &v)) { free(a); return NULL; }
                if (v.type != EDB_VAL_INTEGER) { error_at(p, "IN list integers only"); free(a); return NULL; }
                if (pp + 8 > sizeof pack) { error_at(p, "IN list too long"); free(a); return NULL; }
                edb_store_u64_le(pack + pp, (uint64_t)v.u.i64); pp += 8; nlit++;
            } while (accept(p, EDB_TOK_COMMA));
            if (!expect(p, EDB_TOK_RPAREN, ")")) { free(a); return NULL; }
            pack[0] = (uint8_t)nlit;
            a->where->right->lit.u.bin.len = (uint32_t)pp;
            a->where->right->lit.u.bin.p = malloc(pp);
            memcpy((void*)a->where->right->lit.u.bin.p, pack, pp);
            goto where_done;
        } else { error_at(p, "expected comparison"); free(a); return NULL; }
        if (accept(p, EDB_TOK_LPAREN) && p->lex.current.kind == EDB_TOK_KW_SELECT) {
            a->where->right->kind = EXPR_SUBQUERY;
            /* capture raw text from current SELECT through matching ')' */
            /* We don't have original buffer span easily; rebuild from tokens */
            size_t sp = 0;
            a->where->right->subquery_sql[sp++] = 'S';
            a->where->right->subquery_sql[sp++] = 'E';
            a->where->right->subquery_sql[sp++] = 'L';
            a->where->right->subquery_sql[sp++] = 'E';
            a->where->right->subquery_sql[sp++] = 'C';
            a->where->right->subquery_sql[sp++] = 'T';
            a->where->right->subquery_sql[sp++] = ' ';
            edb_lexer_next(&p->lex); /* consume SELECT */
            int depth = 1;
            while (p->lex.current.kind != EDB_TOK_EOF && depth > 0 && sp + 32 < sizeof a->where->right->subquery_sql) {
                if (p->lex.current.kind == EDB_TOK_LPAREN) depth++;
                if (p->lex.current.kind == EDB_TOK_RPAREN) {
                    depth--;
                    if (depth == 0) { edb_lexer_next(&p->lex); break; }
                }
                /* append token text */
                size_t tl = p->lex.current.len;
                if (p->lex.current.start && tl > 0 && sp + tl + 2 < sizeof a->where->right->subquery_sql) {
                    memcpy(a->where->right->subquery_sql + sp, p->lex.current.start, tl);
                    sp += tl;
                    a->where->right->subquery_sql[sp++] = ' ';
                }
                edb_lexer_next(&p->lex);
            }
            a->where->right->subquery_sql[sp] = 0;
        } else {
            a->where->right->kind = EXPR_LIT;
            if (!parse_literal(p, &a->where->right->lit)) { free(a); return NULL; }
        }
        where_done:;
    }
    if (accept(p, EDB_TOK_KW_GROUP)) {
        if (!expect(p, EDB_TOK_KW_BY, "BY")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->has_group = true;
        copy_tok(a->group_col, sizeof a->group_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (accept(p, EDB_TOK_KW_HAVING)) {
            a->has_having = true;
            /* COUNT(*) > n  simplified: col = lit or just integer compare on aggregate result later */
            if (accept(p, EDB_TOK_KW_COUNT)) {
                expect(p, EDB_TOK_LPAREN, "(");
                expect(p, EDB_TOK_STAR, "*");
                expect(p, EDB_TOK_RPAREN, ")");
                strncpy(a->having_col, "COUNT", sizeof a->having_col - 1);
            } else if (p->lex.current.kind == EDB_TOK_ID) {
                copy_tok(a->having_col, sizeof a->having_col, &p->lex.current);
                edb_lexer_next(&p->lex);
            }
            if (accept(p, EDB_TOK_GT)) a->having_op = EDB_TOK_GT;
            else if (accept(p, EDB_TOK_LT)) a->having_op = EDB_TOK_LT;
            else if (accept(p, EDB_TOK_EQ)) a->having_op = EDB_TOK_EQ;
            else { error_at(p, "expected comparison"); free(a); return NULL; }
            if (!parse_literal(p, &a->having_lit)) { free(a); return NULL; }
        }
    }
    if (accept(p, EDB_TOK_KW_ORDER)) {
        if (!expect(p, EDB_TOK_KW_BY, "BY")) { free(a); return NULL; }
        if (p->lex.current.kind != EDB_TOK_ID) { error_at(p, "expected column"); free(a); return NULL; }
        a->has_order = true;
        copy_tok(a->order_col, sizeof a->order_col, &p->lex.current);
        edb_lexer_next(&p->lex);
        if (accept(p, EDB_TOK_KW_DESC)) a->order_desc = true;
        else accept(p, EDB_TOK_KW_ASC);
    }
    if (accept(p, EDB_TOK_KW_LIMIT)) {
        if (p->lex.current.kind != EDB_TOK_INTEGER) { error_at(p, "expected limit"); free(a); return NULL; }
        a->has_limit = true;
        a->limit = (int)p->lex.current.int_val;
        edb_lexer_next(&p->lex);
        if (accept(p, EDB_TOK_KW_OFFSET)) {
            if (p->lex.current.kind != EDB_TOK_INTEGER) { error_at(p, "expected offset"); free(a); return NULL; }
            a->has_offset = true;
            a->offset = (int)p->lex.current.int_val;
            edb_lexer_next(&p->lex);
        }
    } else if (accept(p, EDB_TOK_KW_OFFSET)) {
        if (p->lex.current.kind != EDB_TOK_INTEGER) { error_at(p, "expected offset"); free(a); return NULL; }
        a->has_offset = true;
        a->offset = (int)p->lex.current.int_val;
        edb_lexer_next(&p->lex);
    }
    accept(p, EDB_TOK_SEMI);
    return a;
}

edb_ast *edb_parse_statement(const char *sql, size_t len, edb_error *err) {
    edb_error_clear(err);
    parser_t p;
    memset(&p, 0, sizeof p);
    p.err = err;
    edb_lexer_init(&p.lex, sql, len);

    edb_ast *a = NULL;
    if (accept(&p, EDB_TOK_KW_CREATE)) {
        if (p.lex.current.kind == EDB_TOK_KW_TABLE)
            a = parse_create_table(&p);
        else if (p.lex.current.kind == EDB_TOK_KW_UNIQUE || p.lex.current.kind == EDB_TOK_KW_INDEX)
            a = parse_create_index(&p);
        else
            error_at(&p, "expected TABLE or INDEX");
    } else if (accept(&p, EDB_TOK_KW_INSERT)) {
        a = parse_insert(&p);
    } else if (accept(&p, EDB_TOK_KW_SELECT)) {
        a = parse_select(&p);
    } else if (accept(&p, EDB_TOK_KW_BEGIN)) {
        a = calloc(1, sizeof(*a)); if (a) a->kind = EDB_AST_BEGIN;
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_COMMIT)) {
        a = calloc(1, sizeof(*a)); if (a) a->kind = EDB_AST_COMMIT;
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_ROLLBACK)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        if (accept(&p, EDB_TOK_KW_TO)) {
            a->kind = EDB_AST_ROLLBACK_TO;
            accept(&p, EDB_TOK_KW_SAVEPOINT);
            if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected savepoint name"); free(a); return NULL; }
            copy_tok(a->name, sizeof a->name, &p.lex.current);
            edb_lexer_next(&p.lex);
        } else {
            a->kind = EDB_AST_ROLLBACK;
        }
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_DROP)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        if (accept(&p, EDB_TOK_KW_TABLE)) a->kind = EDB_AST_DROP_TABLE;
        else if (accept(&p, EDB_TOK_KW_INDEX)) a->kind = EDB_AST_DROP_INDEX;
        else { error_at(&p, "expected TABLE or INDEX"); free(a); return NULL; }
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected name"); free(a); return NULL; }
        copy_tok(a->name, sizeof a->name, &p.lex.current);
        edb_lexer_next(&p.lex);
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_UPDATE)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_UPDATE;
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected table"); free(a); return NULL; }
        copy_tok(a->table, sizeof a->table, &p.lex.current);
        edb_lexer_next(&p.lex);
        if (!expect(&p, EDB_TOK_KW_SET, "SET")) { free(a); return NULL; }
        /* SET col = literal  (single assignment minimal) */
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected column"); free(a); return NULL; }
        a->select_col_count = 1;
        copy_tok(a->select_cols[0], sizeof a->select_cols[0], &p.lex.current);
        edb_lexer_next(&p.lex);
        if (!expect(&p, EDB_TOK_EQ, "=")) { free(a); return NULL; }
        a->value_count = 1;
        if (!parse_literal(&p, &a->values[0])) { free(a); return NULL; }
        if (accept(&p, EDB_TOK_KW_WHERE)) {
            a->where = calloc(1, sizeof(edb_ast_expr));
            a->where->kind = EXPR_BINOP; a->where->op = EDB_TOK_EQ;
            a->where->left = calloc(1, sizeof(edb_ast_expr));
            a->where->right = calloc(1, sizeof(edb_ast_expr));
            a->where->left->kind = EXPR_COL;
            if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected column"); free(a); return NULL; }
            copy_tok(a->where->left->name, sizeof a->where->left->name, &p.lex.current);
            edb_lexer_next(&p.lex);
            if (!expect(&p, EDB_TOK_EQ, "=")) { free(a); return NULL; }
            a->where->right->kind = EXPR_LIT;
            if (!parse_literal(&p, &a->where->right->lit)) { free(a); return NULL; }
        }
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_DELETE)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_DELETE;
        if (!expect(&p, EDB_TOK_KW_FROM, "FROM")) { free(a); return NULL; }
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected table"); free(a); return NULL; }
        copy_tok(a->table, sizeof a->table, &p.lex.current);
        edb_lexer_next(&p.lex);
        if (accept(&p, EDB_TOK_KW_WHERE)) {
            a->where = calloc(1, sizeof(edb_ast_expr));
            a->where->kind = EXPR_BINOP; a->where->op = EDB_TOK_EQ;
            a->where->left = calloc(1, sizeof(edb_ast_expr));
            a->where->right = calloc(1, sizeof(edb_ast_expr));
            a->where->left->kind = EXPR_COL;
            if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected column"); free(a); return NULL; }
            copy_tok(a->where->left->name, sizeof a->where->left->name, &p.lex.current);
            edb_lexer_next(&p.lex);
            if (!expect(&p, EDB_TOK_EQ, "=")) { free(a); return NULL; }
            a->where->right->kind = EXPR_LIT;
            if (!parse_literal(&p, &a->where->right->lit)) { free(a); return NULL; }
        }
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_SAVEPOINT)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_SAVEPOINT;
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected savepoint name"); free(a); return NULL; }
        copy_tok(a->name, sizeof a->name, &p.lex.current);
        edb_lexer_next(&p.lex);
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_RELEASE)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_RELEASE;
        accept(&p, EDB_TOK_KW_SAVEPOINT);
        if (p.lex.current.kind != EDB_TOK_ID) { error_at(&p, "expected savepoint name"); free(a); return NULL; }
        copy_tok(a->name, sizeof a->name, &p.lex.current);
        edb_lexer_next(&p.lex);
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_ANALYZE)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_ANALYZE;
        accept(&p, EDB_TOK_SEMI);
    } else if (accept(&p, EDB_TOK_KW_EXPLAIN)) {
        if (!accept(&p, EDB_TOK_KW_SELECT)) { error_at(&p, "expected SELECT after EXPLAIN"); return NULL; }
        a = parse_select(&p);
        if (a) a->kind = EDB_AST_EXPLAIN;
    } else if (accept(&p, EDB_TOK_KW_VACUUM)) {
        a = calloc(1, sizeof(*a));
        if (!a) { error_at(&p, "oom"); return NULL; }
        a->kind = EDB_AST_VACUUM;
        a->is_vacuum = true;
        accept(&p, EDB_TOK_SEMI);
    } else {
        error_at(&p, "unsupported or invalid statement");
    }
    return a;
}

void edb_ast_free(edb_ast *a) {
    if (!a) return;
    if (a->where) {
        free(a->where->left);
        free(a->where->right);
        free(a->where);
    }
    free(a);
}
