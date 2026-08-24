#ifndef EDB_OVERFLOW_H
#define EDB_OVERFLOW_H

#include "edb/pager.h"
#include "edb/common.h"

/* Overflow chain for values that do not fit in a leaf cell (OVF-001).
 * Layout of OVERFLOW page payload after page header byte:
 *   next_page u32 | payload_len u16 | payload bytes
 * Usable payload per page ~ EDB_PAGE_SIZE - 16 - 6.
 */

#define EDB_OVERFLOW_HDR 6
#define EDB_OVERFLOW_PAYLOAD (EDB_PAGE_SIZE - EDB_HEADER_SIZE - EDB_OVERFLOW_HDR - 16)

/* Write a large blob; returns first overflow page number */
int edb_overflow_write(edb_pager *p, const uint8_t *data, uint32_t len,
                       uint32_t *out_first_page, edb_error *err);

/* Read entire chain into caller buffer (cap bytes); sets *out_len */
int edb_overflow_read(edb_pager *p, uint32_t first_page,
                      uint8_t *buf, uint32_t cap, uint32_t *out_len,
                      edb_error *err);

/* Free all pages in chain onto freelist */
int edb_overflow_free(edb_pager *p, uint32_t first_page, edb_error *err);

#endif
