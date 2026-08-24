#ifndef EDB_SQL_AST_H
#define EDB_SQL_AST_H

#include "edb/common.h"
#include "edb/composite_key.h"
#include <stdbool.h>

typedef enum edb_ast_kind {
    EDB_AST_CREATE_TABLE,
    EDB_AST_DROP_TABLE,
    EDB_AST_CREATE_INDEX,
    EDB_AST_DROP_INDEX,
    EDB_AST_INSERT,
    EDB_AST_SELECT,
    EDB_AST_UPDATE,
    EDB_AST_DELETE,
    EDB_AST_BEGIN,
    EDB_AST_COMMIT,
    EDB_AST_ROLLBACK,
    EDB_AST_EXPLAIN,
    EDB_AST_VACUUM,
    EDB_AST_ANALYZE,
    EDB_AST_SAVEPOINT,
    EDB_AST_RELEASE,
    EDB_AST_ROLLBACK_TO
} edb_ast_kind;

typedef struct edb_ast_coldef {
    char name[128];
    edb_val_type type;
    bool not_null;
    bool primary_key;
    bool has_default;
    edb_value default_val;
} edb_ast_coldef;

typedef struct edb_ast_expr {
    enum { EXPR_COL, EXPR_LIT, EXPR_BINOP, EXPR_UNOP, EXPR_STAR, EXPR_SUBQUERY } kind;
    char name[128];          /* column or table.column */
    edb_value lit;
    int op;                  /* token kind for operators */
    struct edb_ast_expr *left, *right;
    char subquery_sql[512];
} edb_ast_expr;

#define EDB_MAX_COLS 64
#define EDB_MAX_IDX_COLS 8

typedef struct edb_ast {
    edb_ast_kind kind;
    char name[128];          /* table or index name */
    char table[128];         /* for index / insert target */
    int col_count;
    edb_ast_coldef cols[EDB_MAX_COLS];
    /* index */
    bool unique_index;
    int idx_col_count;
    char idx_cols[EDB_MAX_IDX_COLS][128];
    /* insert values (simple literal rows only for now) */
    int value_count;
    int insert_nrows;
    edb_value values[EDB_MAX_COLS * 8];
    /* select */
    bool select_star;
    bool is_distinct;
    int select_col_count;
    char select_cols[EDB_MAX_COLS][128];
    edb_ast_expr *where;
    int limit;
    bool has_limit;
    bool has_offset;
    int offset;
    bool has_order;
    char order_col[128];
    bool order_desc;
    bool is_count_star;
    bool is_sum;
    bool is_avg;
    bool is_min;
    bool is_max;
    char sum_col[128];
    bool is_vacuum;
    bool has_join;
    bool is_left_join;
    char join_table[128];
    char join_left_col[128];
    char join_right_col[128];
    bool has_group;
    char group_col[128];
    bool has_having;
    char having_col[128];
    int having_op; /* token */
    edb_value having_lit;
} edb_ast;

void edb_ast_free(edb_ast *a); /* frees expressions */

#endif
