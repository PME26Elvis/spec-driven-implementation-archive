#ifndef EDB_API_H
#define EDB_API_H

#include "edb/common.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct edb_db edb_db;
typedef struct edb_stmt edb_stmt;

/* API-001..003 */
edb_db *edb_open(const char *path, bool create, bool read_only,
                 const char *password_or_null, edb_error *err);
void    edb_close(edb_db *db);

/* API-004..008 minimal */
int     edb_exec(edb_db *db, const char *sql, edb_error *err);
int     edb_prepare(edb_db *db, const char *sql, edb_stmt **out, edb_error *err);
int     edb_step(edb_stmt *stmt, edb_error *err);
int     edb_finalize(edb_stmt *stmt);

/* Result access */
int     edb_column_count(edb_stmt *stmt);
const char *edb_column_name(edb_stmt *stmt, int i);
int     edb_column_type(edb_stmt *stmt, int i); /* 0=null 1=int 2=real 3=text 4=blob */
int64_t edb_column_int64(edb_stmt *stmt, int i);
double  edb_column_double(edb_stmt *stmt, int i);
const char *edb_column_text(edb_stmt *stmt, int i);
const void *edb_column_blob(edb_stmt *stmt, int i, size_t *len);

/* Transactions */
int edb_begin(edb_db *db, edb_error *err);
int edb_commit(edb_db *db, edb_error *err);
int edb_rollback(edb_db *db, edb_error *err);

#endif
