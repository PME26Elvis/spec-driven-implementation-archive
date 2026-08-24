#ifndef EDB_SCHEMA_H
#define EDB_SCHEMA_H

#include "edb/common.h"
#include "edb/sql_ast.h"
#include "edb/pager.h"
#include "edb/btree.h"
#include <stdbool.h>

#define EDB_MAX_TABLES 64
#define EDB_MAX_INDEXES 128

typedef struct edb_table {
    char name[128];
    int col_count;
    edb_ast_coldef cols[EDB_MAX_COLS];
    int pk_col;              /* -1 if none */
    uint32_t data_root;      /* B+ tree root for row storage */
    uint64_t next_rowid;
} edb_table;

typedef struct edb_index {
    char name[128];
    char table[128];
    bool unique;
    int col_count;
    int col_idxs[EDB_MAX_IDX_COLS]; /* indices into table cols */
    char col_names[EDB_MAX_IDX_COLS][128];
    uint32_t root;
} edb_index;

typedef struct edb_catalog {
    edb_pager *pager;
    int table_count;
    edb_table tables[EDB_MAX_TABLES];
    int index_count;
    edb_index indexes[EDB_MAX_INDEXES];
    bool dirty;
} edb_catalog;

edb_catalog *edb_catalog_open(edb_pager *pager, edb_error *err);
void         edb_catalog_close(edb_catalog *cat);
int          edb_catalog_save(edb_catalog *cat, edb_error *err);

edb_table   *edb_catalog_find_table(edb_catalog *cat, const char *name);
edb_index   *edb_catalog_find_index(edb_catalog *cat, const char *name);

int edb_catalog_create_table(edb_catalog *cat, const edb_ast *ast, edb_error *err);
int edb_catalog_create_index(edb_catalog *cat, const edb_ast *ast, edb_error *err);
int edb_catalog_drop_table(edb_catalog *cat, const char *name, edb_error *err);

#endif
