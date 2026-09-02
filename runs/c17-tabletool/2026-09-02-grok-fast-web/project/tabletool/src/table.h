#ifndef TABLETOOL_TABLE_H
#define TABLETOOL_TABLE_H

#include "common.h"

Table *table_create(void);
void table_free(Table *t);

int table_add_column(Table *t, const char *name, size_t name_len, ColumnType type);
int table_find_column(const Table *t, const char *name, size_t name_len);
int table_drop_column(Table *t, int col_idx);
int table_rename_column(Table *t, int col_idx, const char *new_name, size_t new_len);
int table_move_column(Table *t, int from, int to_pos); /* 1-based final */
int table_swap_columns(Table *t, int a, int b);

int table_insert_row(Table *t, size_t at /*0-based*/, Cell *cells); /* takes ownership of cells array */
int table_append_row(Table *t, Cell *cells);
int table_delete_row(Table *t, size_t row /*0-based*/);
int table_move_row(Table *t, size_t from, size_t to);
int table_swap_rows(Table *t, size_t a, size_t b);

void cell_set_null(Cell *c);
void cell_clear(Cell *c);
int cell_set_string(Cell *c, const char *s, size_t n);
int cell_set_string_owned(Cell *c, char *s, size_t n);
int cell_copy(Cell *dst, const Cell *src, ColumnType type);
char *cell_canonical(const Cell *c, ColumnType type, size_t *out_len);

int parse_integer(const char *s, size_t n, int64_t *out);
int parse_decimal(const char *s, size_t n, Cell *out);
int parse_boolean(const char *s, size_t n, bool *out);
int parse_date(const char *s, size_t n, int *y, int *m, int *d);

ColumnType type_from_name(const char *s);
const char *type_name(ColumnType t);

int table_sort(Table *t, int *col_idxs, int *asc, size_t nkeys);

/* Convert entire column; returns 0 ok, or failing 1-based row */
int table_type_column(Table *t, int col_idx, ColumnType new_type, int *fail_row);

#endif
