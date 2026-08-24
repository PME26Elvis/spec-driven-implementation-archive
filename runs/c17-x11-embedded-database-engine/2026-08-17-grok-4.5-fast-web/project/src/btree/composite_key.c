#include "edb/composite_key.h"
#include "edb/byteorder.h"
#include <string.h>
#include <math.h>

/* Encoding per component:
 *   type (1 byte)
 *   if NULL: nothing more
 *   if INTEGER: 8-byte little-endian int64
 *   if REAL: 8-byte IEEE-754 little-endian
 *   if TEXT/BLOB: 4-byte length + bytes
 * After all components, optional rowid: type tag 0xFF + 8-byte rowid
 * This is unambiguous under concatenation.
 */

int edb_composite_encode(const edb_composite_key *ck, uint8_t *buf, size_t bufcap) {
    size_t pos = 0;
    for (int i = 0; i < ck->n; i++) {
        const edb_value *v = &ck->comps[i];
        if (pos + 1 > bufcap) return -1;
        buf[pos++] = (uint8_t)v->type;
        switch (v->type) {
        case EDB_VAL_NULL:
            break;
        case EDB_VAL_INTEGER:
            if (pos + 8 > bufcap) return -1;
            edb_store_u64_le(buf + pos, (uint64_t)v->u.i64);
            pos += 8;
            break;
        case EDB_VAL_REAL: {
            if (pos + 8 > bufcap) return -1;
            uint64_t bits;
            memcpy(&bits, &v->u.real, 8);
            edb_store_u64_le(buf + pos, bits);
            pos += 8;
            break;
        }
        case EDB_VAL_TEXT:
        case EDB_VAL_BLOB:
            if (pos + 4 + v->u.bin.len > bufcap) return -1;
            edb_store_u32_le(buf + pos, v->u.bin.len);
            pos += 4;
            if (v->u.bin.len)
                memcpy(buf + pos, v->u.bin.p, v->u.bin.len);
            pos += v->u.bin.len;
            break;
        default:
            return -1;
        }
    }
    if (ck->has_rowid) {
        if (pos + 1 + 8 > bufcap) return -1;
        buf[pos++] = 0xFF;
        edb_store_u64_le(buf + pos, ck->rowid);
        pos += 8;
    }
    if (pos > EDB_MAX_KEY_BYTES) return -1;
    return (int)pos;
}

int edb_composite_decode(const uint8_t *buf, size_t len, edb_composite_key *out) {
    memset(out, 0, sizeof(*out));
    size_t pos = 0;
    int i = 0;
    while (pos < len && i < EDB_MAX_COMPOSITE_COLS) {
        if (buf[pos] == 0xFF) {
            /* rowid discriminator */
            pos++;
            if (pos + 8 > len) return -1;
            out->rowid = edb_load_u64_le(buf + pos);
            out->has_rowid = true;
            pos += 8;
            break;
        }
        edb_val_type t = (edb_val_type)buf[pos++];
        out->comps[i].type = t;
        switch (t) {
        case EDB_VAL_NULL:
            break;
        case EDB_VAL_INTEGER:
            if (pos + 8 > len) return -1;
            out->comps[i].u.i64 = (int64_t)edb_load_u64_le(buf + pos);
            pos += 8;
            break;
        case EDB_VAL_REAL: {
            if (pos + 8 > len) return -1;
            uint64_t bits = edb_load_u64_le(buf + pos);
            memcpy(&out->comps[i].u.real, &bits, 8);
            pos += 8;
            break;
        }
        case EDB_VAL_TEXT:
        case EDB_VAL_BLOB: {
            if (pos + 4 > len) return -1;
            uint32_t l = edb_load_u32_le(buf + pos);
            pos += 4;
            if (pos + l > len) return -1;
            out->comps[i].u.bin.p = buf + pos;
            out->comps[i].u.bin.len = l;
            pos += l;
            break;
        }
        default:
            return -1;
        }
        i++;
    }
    out->n = i;
    if (pos != len && !(out->has_rowid)) {
        /* trailing data without rowid tag */
        if (pos < len && buf[pos] == 0xFF) {
            pos++;
            if (pos + 8 > len) return -1;
            out->rowid = edb_load_u64_le(buf + pos);
            out->has_rowid = true;
            pos += 8;
        }
        if (pos != len) return -1;
    }
    return 0;
}

static int cmp_bytes(const uint8_t *a, uint32_t alen, const uint8_t *b, uint32_t blen) {
    uint32_t n = alen < blen ? alen : blen;
    int c = memcmp(a, b, n);
    if (c != 0) return c;
    if (alen < blen) return -1;
    if (alen > blen) return 1;
    return 0;
}

/* NULL sorts first (deterministic). */
static int cmp_value(const edb_value *a, const edb_value *b) {
    if (a->type == EDB_VAL_NULL && b->type == EDB_VAL_NULL) return 0;
    if (a->type == EDB_VAL_NULL) return -1;
    if (b->type == EDB_VAL_NULL) return 1;
    if (a->type != b->type) {
        /* different types: order by type tag for determinism */
        return (int)a->type - (int)b->type;
    }
    switch (a->type) {
    case EDB_VAL_INTEGER:
        if (a->u.i64 < b->u.i64) return -1;
        if (a->u.i64 > b->u.i64) return 1;
        return 0;
    case EDB_VAL_REAL:
        if (a->u.real < b->u.real) return -1;
        if (a->u.real > b->u.real) return 1;
        /* NaN handling: treat equal NaNs as equal for ordering stability */
        return 0;
    case EDB_VAL_TEXT:
    case EDB_VAL_BLOB:
        return cmp_bytes(a->u.bin.p, a->u.bin.len, b->u.bin.p, b->u.bin.len);
    default:
        return 0;
    }
}

int edb_composite_compare(const edb_composite_key *a, const edb_composite_key *b) {
    int n = a->n < b->n ? a->n : b->n;
    for (int i = 0; i < n; i++) {
        int c = cmp_value(&a->comps[i], &b->comps[i]);
        if (c != 0) return c;
    }
    if (a->n < b->n) return -1;
    if (a->n > b->n) return 1;
    if (a->has_rowid && b->has_rowid) {
        if (a->rowid < b->rowid) return -1;
        if (a->rowid > b->rowid) return 1;
    } else if (a->has_rowid) return 1;
    else if (b->has_rowid) return -1;
    return 0;
}

int edb_composite_compare_encoded(const uint8_t *a, size_t alen,
                                  const uint8_t *b, size_t blen) {
    edb_composite_key ka, kb;
    if (edb_composite_decode(a, alen, &ka) != 0) return -1;
    if (edb_composite_decode(b, blen, &kb) != 0) return 1;
    return edb_composite_compare(&ka, &kb);
}

bool edb_composite_unique_conflict(const edb_composite_key *a, const edb_composite_key *b) {
    if (a->n != b->n) return false;
    bool any_null = false;
    for (int i = 0; i < a->n; i++) {
        if (a->comps[i].type == EDB_VAL_NULL || b->comps[i].type == EDB_VAL_NULL)
            any_null = true;
        if (cmp_value(&a->comps[i], &b->comps[i]) != 0)
            return false;
    }
    /* CIDX-009: NULL-containing tuples do not conflict with each other */
    if (any_null) return false;
    return true; /* fully non-NULL and equal */
}
