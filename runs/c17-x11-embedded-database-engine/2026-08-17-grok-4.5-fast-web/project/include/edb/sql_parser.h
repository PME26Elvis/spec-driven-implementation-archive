#ifndef EDB_SQL_PARSER_H
#define EDB_SQL_PARSER_H

#include "edb/sql_ast.h"
#include "edb/sql_lexer.h"

/* Parse one statement. Returns heap-allocated AST or NULL on error. */
edb_ast *edb_parse_statement(const char *sql, size_t len, edb_error *err);

#endif
