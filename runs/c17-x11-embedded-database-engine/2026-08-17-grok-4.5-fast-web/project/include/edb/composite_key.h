#ifndef EDB_COMPOSITE_KEY_H
#define EDB_COMPOSITE_KEY_H

#include "edb/common.h"
#include <stdbool.h>

/* Value types matching REC-001 */
typedef enum edb_val_type {
    EDB_VAL_NULL = 0,
    EDB_VAL_INTEGER = 1,
    EDB_VAL_REAL = 2,
    EDB_VAL_TEXT = 3,
    EDB_VAL_BLOB = 4
} edb_val_type;

typedef struct edb_value {
    edb_val_type type;
    union {
        int64_t i64;
        double  real;
        struct { const uint8_t *p; uint32_t len; } bin; /* text or blob */
    } u;
} edb_value;

/* Composite key: ordered components. Encoding is unambiguous
 * (type tag + length + payload) so ("ab","c") != ("a","bc").
 * CIDX-003, CIDX-004, UT-039, UT-040.
 */
#define EDB_MAX_COMPOSITE_COLS 8
#define EDB_MAX_KEY_BYTES 1024

typedef struct edb_composite_key {
    int n;
    edb_value comps[EDB_MAX_COMPOSITE_COLS];
    /* optional row-identity discriminator for non-unique indexes */
    uint64_t rowid;
    bool has_rowid;
} edb_composite_key;

/* Encode into buffer; returns encoded length or -1 on error / too large. */
int edb_composite_encode(const edb_composite_key *ck, uint8_t *buf, size_t bufcap);

/* Decode; returns 0 on success. */
int edb_composite_decode(const uint8_t *buf, size_t len, edb_composite_key *out);

/* Lexicographic compare of two encoded keys (or of two structures). */
int edb_composite_compare_encoded(const uint8_t *a, size_t alen,
                                  const uint8_t *b, size_t blen);
int edb_composite_compare(const edb_composite_key *a, const edb_composite_key *b);

/* NULL-containing UNIQUE semantics (CIDX-009): only fully non-NULL equal tuples conflict. */
bool edb_composite_unique_conflict(const edb_composite_key *a, const edb_composite_key *b);

#endif
