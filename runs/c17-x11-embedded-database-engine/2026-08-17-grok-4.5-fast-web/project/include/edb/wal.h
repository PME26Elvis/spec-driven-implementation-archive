#ifndef EDB_WAL_H
#define EDB_WAL_H

#include "edb/common.h"
#include "edb/pager.h"
#include <stdbool.h>

/* Minimal WAL design (WAL-001..):
 * Separate -wal file next to the database.
 * Records: header magic, sequence, type, payload length, payload, checksum.
 * Types: BEGIN, PAGE_IMAGE, COMMIT, ABORT, CHECKPOINT.
 */

typedef enum edb_wal_rec_type {
    EDB_WAL_BEGIN = 1,
    EDB_WAL_PAGE_IMAGE = 2,
    EDB_WAL_COMMIT = 3,
    EDB_WAL_ABORT = 4,
    EDB_WAL_CHECKPOINT = 5
} edb_wal_rec_type;

typedef struct edb_wal edb_wal;

edb_wal *edb_wal_open(const char *db_path, bool create, edb_error *err);
void     edb_wal_close(edb_wal *w);

int edb_wal_begin(edb_wal *w, uint64_t txn_id, edb_error *err);
int edb_wal_log_page(edb_wal *w, uint32_t page_no, const uint8_t *page_data, edb_error *err);
int edb_wal_commit(edb_wal *w, uint64_t txn_id, edb_error *err);
int edb_wal_abort(edb_wal *w, uint64_t txn_id, edb_error *err);
int edb_wal_checkpoint(edb_wal *w, edb_pager *pager, edb_error *err);

/* Recovery on open */
int edb_wal_recover(edb_wal *w, edb_pager *pager, edb_error *err);

uint64_t edb_wal_next_seq(edb_wal *w);

#endif
