#ifndef EDB_SQL_LEXER_H
#define EDB_SQL_LEXER_H

#include "edb/common.h"
#include <stdbool.h>

typedef enum edb_token_kind {
    EDB_TOK_EOF = 0,
    EDB_TOK_ERROR,
    /* punctuation */
    EDB_TOK_SEMI, EDB_TOK_COMMA, EDB_TOK_DOT, EDB_TOK_STAR,
    EDB_TOK_LPAREN, EDB_TOK_RPAREN,
    EDB_TOK_EQ, EDB_TOK_NE, EDB_TOK_LT, EDB_TOK_LE, EDB_TOK_GT, EDB_TOK_GE,
    EDB_TOK_PLUS, EDB_TOK_MINUS, EDB_TOK_SLASH, EDB_TOK_PERCENT,
    /* literals */
    EDB_TOK_INTEGER, EDB_TOK_REAL, EDB_TOK_STRING, EDB_TOK_BLOB, EDB_TOK_NULL,
    EDB_TOK_PARAM,          /* ? */
    /* identifiers / keywords */
    EDB_TOK_ID,
    EDB_TOK_KW_SELECT, EDB_TOK_KW_FROM, EDB_TOK_KW_WHERE, EDB_TOK_KW_INSERT,
    EDB_TOK_KW_INTO, EDB_TOK_KW_VALUES, EDB_TOK_KW_UPDATE, EDB_TOK_KW_SET,
    EDB_TOK_KW_DELETE, EDB_TOK_KW_CREATE, EDB_TOK_KW_TABLE, EDB_TOK_KW_DROP,
    EDB_TOK_KW_INDEX, EDB_TOK_KW_UNIQUE, EDB_TOK_KW_PRIMARY, EDB_TOK_KW_KEY,
    EDB_TOK_KW_NOT, EDB_TOK_KW_AND, EDB_TOK_KW_OR, EDB_TOK_KW_AS,
    EDB_TOK_KW_ORDER, EDB_TOK_KW_BY, EDB_TOK_KW_ASC, EDB_TOK_KW_DESC,
    EDB_TOK_KW_LIMIT, EDB_TOK_KW_OFFSET, EDB_TOK_KW_JOIN, EDB_TOK_KW_INNER,
    EDB_TOK_KW_LEFT, EDB_TOK_KW_ON, EDB_TOK_KW_GROUP, EDB_TOK_KW_HAVING,
    EDB_TOK_KW_BEGIN, EDB_TOK_KW_COMMIT, EDB_TOK_KW_ROLLBACK, EDB_TOK_KW_SAVEPOINT,
    EDB_TOK_KW_RELEASE, EDB_TOK_KW_EXPLAIN, EDB_TOK_KW_VACUUM, EDB_TOK_KW_ANALYZE,
    EDB_TOK_KW_ALTER, EDB_TOK_KW_ADD, EDB_TOK_KW_COLUMN, EDB_TOK_KW_RENAME, EDB_TOK_KW_TO,
    EDB_TOK_KW_INTEGER_TYPE, EDB_TOK_KW_REAL_TYPE, EDB_TOK_KW_TEXT_TYPE, EDB_TOK_KW_BLOB_TYPE,
    EDB_TOK_KW_IS, EDB_TOK_KW_IN, EDB_TOK_KW_BETWEEN, EDB_TOK_KW_LIKE,
    EDB_TOK_KW_DISTINCT, EDB_TOK_KW_DEFAULT, EDB_TOK_KW_COUNT, EDB_TOK_KW_SUM,
    EDB_TOK_KW_AVG, EDB_TOK_KW_MIN, EDB_TOK_KW_MAX
} edb_token_kind;

typedef struct edb_token {
    edb_token_kind kind;
    const char    *start;   /* pointer into original input */
    size_t         len;
    size_t         byte_offset;
    int64_t        int_val; /* for INTEGER */
    double         real_val;
} edb_token;

typedef struct edb_lexer {
    const char *input;
    size_t      length;
    size_t      pos;
    edb_token   current;
    edb_error   error;
} edb_lexer;

void edb_lexer_init(edb_lexer *lex, const char *sql, size_t len);
void edb_lexer_next(edb_lexer *lex);  /* advance current */
const edb_token *edb_lexer_peek(edb_lexer *lex);

#endif
