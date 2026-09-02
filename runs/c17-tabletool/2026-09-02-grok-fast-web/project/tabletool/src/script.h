#ifndef TABLETOOL_SCRIPT_H
#define TABLETOOL_SCRIPT_H
#include "common.h"
#include "table.h"

typedef enum {
    STMT_LOAD,
    STMT_TYPE,
    STMT_ADD_COLUMN,
    STMT_DROP_COLUMN,
    STMT_RENAME_COLUMN,
    STMT_MOVE_COLUMN,
    STMT_SWAP_COLUMNS,
    STMT_INSERT_ROW,
    STMT_APPEND_ROW,
    STMT_DELETE_ROW,
    STMT_MOVE_ROW,
    STMT_SWAP_ROWS,
    STMT_SET_CELL,
    STMT_SET_NULL,
    STMT_SORT,
    STMT_FIND,
    STMT_WRITE,
    STMT_BARCODE_SHEET
} StmtKind;

typedef struct {
    StmtKind kind;
    int line;
    /* common fields; specific ones in union or fixed slots for simplicity */
    char *path;
    char *col;
    char *col2;
    char *type_name;
    char *fmt;
    char *null_token;
    int header; /* 1 yes, 0 no, -1 n/a */
    int pos;
    int pos2;
    int module, height, gap, text;
    char *query;
    char **in_cols;
    size_t n_in;
    int mode_sensitive;
    char **values;
    size_t n_values;
    char **sort_cols;
    int *sort_asc;
    size_t n_sort;
    char *default_val;
    int has_default;
    int has_at;
} Stmt;

typedef struct {
    Stmt *stmts;
    size_t n;
    size_t cap;
} Script;

int script_parse(const unsigned char *data, size_t len, Script *out, ErrorInfo *err);
void script_free(Script *s);
int script_execute(Script *s, const char *report_path, ErrorInfo *err);

#endif
