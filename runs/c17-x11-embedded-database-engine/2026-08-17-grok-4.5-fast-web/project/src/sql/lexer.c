#include "edb/sql_lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>

static int is_id_start(unsigned char c) {
    if (c == '@') return 1;
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}
static int is_id_cont(unsigned char c) {
    return is_id_start(c) || (c >= '0' && c <= '9');
}

static void skip_ws_and_comments(edb_lexer *lex) {
    while (lex->pos < lex->length) {
        char c = lex->input[lex->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            lex->pos++;
            continue;
        }
        if (c == '-' && lex->pos + 1 < lex->length && lex->input[lex->pos+1] == '-') {
            lex->pos += 2;
            while (lex->pos < lex->length && lex->input[lex->pos] != '\n')
                lex->pos++;
            continue;
        }
        break;
    }
}

static edb_token_kind keyword_or_id(const char *s, size_t len) {
#define KW(str, tok) do { \
        if (len == sizeof(str)-1 && strncasecmp(s, str, len) == 0) return tok; \
    } while (0)
    KW("SELECT", EDB_TOK_KW_SELECT);
    KW("FROM", EDB_TOK_KW_FROM);
    KW("WHERE", EDB_TOK_KW_WHERE);
    KW("INSERT", EDB_TOK_KW_INSERT);
    KW("INTO", EDB_TOK_KW_INTO);
    KW("VALUES", EDB_TOK_KW_VALUES);
    KW("UPDATE", EDB_TOK_KW_UPDATE);
    KW("SET", EDB_TOK_KW_SET);
    KW("DELETE", EDB_TOK_KW_DELETE);
    KW("CREATE", EDB_TOK_KW_CREATE);
    KW("TABLE", EDB_TOK_KW_TABLE);
    KW("DROP", EDB_TOK_KW_DROP);
    KW("INDEX", EDB_TOK_KW_INDEX);
    KW("UNIQUE", EDB_TOK_KW_UNIQUE);
    KW("PRIMARY", EDB_TOK_KW_PRIMARY);
    KW("KEY", EDB_TOK_KW_KEY);
    KW("NOT", EDB_TOK_KW_NOT);
    KW("AND", EDB_TOK_KW_AND);
    KW("OR", EDB_TOK_KW_OR);
    KW("AS", EDB_TOK_KW_AS);
    KW("ORDER", EDB_TOK_KW_ORDER);
    KW("BY", EDB_TOK_KW_BY);
    KW("ASC", EDB_TOK_KW_ASC);
    KW("DESC", EDB_TOK_KW_DESC);
    KW("LIMIT", EDB_TOK_KW_LIMIT);
    KW("OFFSET", EDB_TOK_KW_OFFSET);
    KW("JOIN", EDB_TOK_KW_JOIN);
    KW("INNER", EDB_TOK_KW_INNER);
    KW("LEFT", EDB_TOK_KW_LEFT);
    KW("ON", EDB_TOK_KW_ON);
    KW("GROUP", EDB_TOK_KW_GROUP);
    KW("HAVING", EDB_TOK_KW_HAVING);
    KW("BEGIN", EDB_TOK_KW_BEGIN);
    KW("COMMIT", EDB_TOK_KW_COMMIT);
    KW("ROLLBACK", EDB_TOK_KW_ROLLBACK);
    KW("SAVEPOINT", EDB_TOK_KW_SAVEPOINT);
    KW("RELEASE", EDB_TOK_KW_RELEASE);
    KW("EXPLAIN", EDB_TOK_KW_EXPLAIN);
    KW("VACUUM", EDB_TOK_KW_VACUUM);
    KW("ANALYZE", EDB_TOK_KW_ANALYZE);
    KW("ALTER", EDB_TOK_KW_ALTER);
    KW("ADD", EDB_TOK_KW_ADD);
    KW("COLUMN", EDB_TOK_KW_COLUMN);
    KW("RENAME", EDB_TOK_KW_RENAME);
    KW("TO", EDB_TOK_KW_TO);
    KW("INTEGER", EDB_TOK_KW_INTEGER_TYPE);
    KW("REAL", EDB_TOK_KW_REAL_TYPE);
    KW("TEXT", EDB_TOK_KW_TEXT_TYPE);
    KW("BLOB", EDB_TOK_KW_BLOB_TYPE);
    KW("NULL", EDB_TOK_NULL);
    KW("IS", EDB_TOK_KW_IS);
    KW("IN", EDB_TOK_KW_IN);
    KW("BETWEEN", EDB_TOK_KW_BETWEEN);
    KW("LIKE", EDB_TOK_KW_LIKE);
    KW("DISTINCT", EDB_TOK_KW_DISTINCT);
    KW("DEFAULT", EDB_TOK_KW_DEFAULT);
    KW("COUNT", EDB_TOK_KW_COUNT);
    KW("SUM", EDB_TOK_KW_SUM);
    KW("AVG", EDB_TOK_KW_AVG);
    KW("MIN", EDB_TOK_KW_MIN);
    KW("MAX", EDB_TOK_KW_MAX);
#undef KW
    return EDB_TOK_ID;
}

void edb_lexer_init(edb_lexer *lex, const char *sql, size_t len) {
    memset(lex, 0, sizeof(*lex));
    lex->input = sql;
    lex->length = len;
    lex->pos = 0;
    edb_lexer_next(lex);
}

const edb_token *edb_lexer_peek(edb_lexer *lex) {
    return &lex->current;
}

void edb_lexer_next(edb_lexer *lex) {
    skip_ws_and_comments(lex);
    edb_token *t = &lex->current;
    memset(t, 0, sizeof(*t));
    t->byte_offset = lex->pos;
    if (lex->pos >= lex->length) {
        t->kind = EDB_TOK_EOF;
        return;
    }
    const char *s = lex->input + lex->pos;
    unsigned char c = (unsigned char)*s;

    switch (c) {
    case ';': t->kind = EDB_TOK_SEMI; t->start = s; t->len = 1; lex->pos++; return;
    case ',': t->kind = EDB_TOK_COMMA; t->start = s; t->len = 1; lex->pos++; return;
    case '.': t->kind = EDB_TOK_DOT; t->start = s; t->len = 1; lex->pos++; return;
    case '*': t->kind = EDB_TOK_STAR; t->start = s; t->len = 1; lex->pos++; return;
    case '(': t->kind = EDB_TOK_LPAREN; t->start = s; t->len = 1; lex->pos++; return;
    case ')': t->kind = EDB_TOK_RPAREN; t->start = s; t->len = 1; lex->pos++; return;
    case '+': t->kind = EDB_TOK_PLUS; t->start = s; t->len = 1; lex->pos++; return;
    case '-': t->kind = EDB_TOK_MINUS; t->start = s; t->len = 1; lex->pos++; return;
    case '/': t->kind = EDB_TOK_SLASH; t->start = s; t->len = 1; lex->pos++; return;
    case '%': t->kind = EDB_TOK_PERCENT; t->start = s; t->len = 1; lex->pos++; return;
    case '?': t->kind = EDB_TOK_PARAM; t->start = s; t->len = 1; lex->pos++; return;
    case '=': t->kind = EDB_TOK_EQ; t->start = s; t->len = 1; lex->pos++; return;
    case '<':
        if (lex->pos+1 < lex->length && lex->input[lex->pos+1] == '=') {
            t->kind = EDB_TOK_LE; t->start = s; t->len = 2; lex->pos += 2; return;
        }
        if (lex->pos+1 < lex->length && lex->input[lex->pos+1] == '>') {
            t->kind = EDB_TOK_NE; t->start = s; t->len = 2; lex->pos += 2; return;
        }
        t->kind = EDB_TOK_LT; t->start = s; t->len = 1; lex->pos++; return;
    case '>':
        if (lex->pos+1 < lex->length && lex->input[lex->pos+1] == '=') {
            t->kind = EDB_TOK_GE; t->start = s; t->len = 2; lex->pos += 2; return;
        }
        t->kind = EDB_TOK_GT; t->start = s; t->len = 1; lex->pos++; return;
    case '!':
        if (lex->pos+1 < lex->length && lex->input[lex->pos+1] == '=') {
            t->kind = EDB_TOK_NE; t->start = s; t->len = 2; lex->pos += 2; return;
        }
        break;
    }

    if (c == '\'') {
        lex->pos++;
        t->start = lex->input + lex->pos;
        size_t start = lex->pos;
        while (lex->pos < lex->length) {
            if (lex->input[lex->pos] == '\'') {
                if (lex->pos+1 < lex->length && lex->input[lex->pos+1] == '\'') {
                    lex->pos += 2;
                    continue;
                }
                break;
            }
            if (lex->input[lex->pos] == '\0') {
                t->kind = EDB_TOK_ERROR;
                edb_error_set(&lex->error, EDB_SQL_LEX, 0, "NUL in string");
                return;
            }
            lex->pos++;
        }
        t->len = lex->pos - start;
        t->kind = EDB_TOK_STRING;
        if (lex->pos < lex->length) lex->pos++;
        else {
            t->kind = EDB_TOK_ERROR;
            edb_error_set(&lex->error, EDB_SQL_LEX, 0, "unterminated string");
        }
        return;
    }

    if ((c >= '0' && c <= '9') || (c == '.' && lex->pos+1 < lex->length &&
            lex->input[lex->pos+1] >= '0' && lex->input[lex->pos+1] <= '9')) {
        t->start = s;
        size_t start = lex->pos;
        int is_real = 0;
        while (lex->pos < lex->length && lex->input[lex->pos] >= '0' && lex->input[lex->pos] <= '9')
            lex->pos++;
        if (lex->pos < lex->length && lex->input[lex->pos] == '.') {
            is_real = 1;
            lex->pos++;
            while (lex->pos < lex->length && lex->input[lex->pos] >= '0' && lex->input[lex->pos] <= '9')
                lex->pos++;
        }
        t->len = lex->pos - start;
        if (is_real) {
            t->kind = EDB_TOK_REAL;
            char tmp[64];
            size_t copy = t->len < 63 ? t->len : 63;
            memcpy(tmp, s, copy); tmp[copy] = 0;
            t->real_val = atof(tmp);
        } else {
            t->kind = EDB_TOK_INTEGER;
            int64_t v = 0;
            for (size_t i = 0; i < t->len; i++) {
                int dig = s[i] - '0';
                if (v > (INT64_MAX - dig) / 10) {
                    t->kind = EDB_TOK_ERROR;
                    edb_error_set(&lex->error, EDB_SQL_LEX, 0, "integer overflow");
                    return;
                }
                v = v * 10 + dig;
            }
            t->int_val = v;
        }
        return;
    }

    if (is_id_start(c)) {
        t->start = s;
        size_t start = lex->pos;
        lex->pos++;
        while (lex->pos < lex->length && is_id_cont((unsigned char)lex->input[lex->pos]))
            lex->pos++;
        t->len = lex->pos - start;
        t->kind = keyword_or_id(s, t->len);
        return;
    }

    t->kind = EDB_TOK_ERROR;
    t->start = s;
    t->len = 1;
    lex->pos++;
    edb_error_set(&lex->error, EDB_SQL_LEX, 0, "unexpected character");
}
