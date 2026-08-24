#include "edb/btree.h"
#include "edb/byteorder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* On-disk leaf/internal layout (simplified v1, little-endian):
 * offset 0..EDB_HEADER_SIZE-1 : reserved (pager header area)
 * EDB_HEADER_SIZE + 0 : page type (already set by pager)
 * +1..+2 : flags
 * +3..+4 : number of cells (uint16)
 * +5..+8 : right-sibling page (leaf) or rightmost child (internal)
 * then cell directory growing down from end of usable area,
 * cells growing up from header.
 *
 * Cell format (leaf): key_len(u16) | val_len(u16) | key bytes | val bytes
 * Cell format (internal): key_len(u16) | child_page(u32) | key bytes
 */

#define HDR EDB_HEADER_SIZE
#define USABLE (EDB_PAGE_SIZE - HDR - 16) /* leave room for future tag */
#define CELL_DIR_START (EDB_PAGE_SIZE - 16)

static int key_cmp(const uint8_t *a, uint16_t alen, const uint8_t *b, uint16_t blen) {
    uint16_t n = alen < blen ? alen : blen;
    int c = memcmp(a, b, n);
    if (c != 0) return c;
    if (alen < blen) return -1;
    if (alen > blen) return 1;
    return 0;
}

static uint16_t cell_count(const uint8_t *page) {
    return edb_load_u16_le(page + HDR + 3);
}

static void set_cell_count(uint8_t *page, uint16_t n) {
    edb_store_u16_le(page + HDR + 3, n);
}

static uint32_t right_ptr(const uint8_t *page) {
    return edb_load_u32_le(page + HDR + 5);
}

static void set_right_ptr(uint8_t *page, uint32_t p) {
    edb_store_u32_le(page + HDR + 5, p);
}

/* Cell offset stored from the end: slot i lives at CELL_DIR_START - 2*(i+1) */
static uint16_t cell_off(const uint8_t *page, uint16_t i) {
    return edb_load_u16_le(page + CELL_DIR_START - 2*(i+1));
}

static void set_cell_off(uint8_t *page, uint16_t i, uint16_t off) {
    edb_store_u16_le(page + CELL_DIR_START - 2*(i+1), off);
}

static int leaf_cell_size(uint16_t klen, uint16_t vlen) {
    return 4 + klen + vlen;
}

static int internal_cell_size(uint16_t klen) {
    return 2 + 4 + klen;
}

/* Find first cell with key >= search key; returns index in [0..count] */
static int leaf_find_ge(const uint8_t *page, const edb_key *key) {
    uint16_t n = cell_count(page);
    int lo = 0, hi = (int)n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        uint16_t off = cell_off(page, (uint16_t)mid);
        uint16_t klen = edb_load_u16_le(page + off);
        const uint8_t *k = page + off + 4;
        int c = key_cmp(k, klen, key->data, key->len);
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static int internal_find_child(const uint8_t *page, const edb_key *key) {
    /* internal: cells are separator keys; child to the left of separator i
       is stored inside cell i; rightmost child is right_ptr. */
    uint16_t n = cell_count(page);
    int lo = 0, hi = (int)n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        uint16_t off = cell_off(page, (uint16_t)mid);
        uint16_t klen = edb_load_u16_le(page + off);
        const uint8_t *k = page + off + 6; /* after klen + child */
        int c = key_cmp(key->data, key->len, k, klen);
        if (c >= 0) lo = mid + 1;
        else hi = mid;
    }
    /* lo is first separator > key, so child is:
       if lo==0 -> leftmost child is inside cell 0? Actually for simplicity
       we store left child of separator i inside the cell. For key < all,
       we use the child of cell 0 if we treat separators as "min of right".
       Simpler design: children[0..n] where children[i] is left of separator i,
       children[n] = right_ptr. */
    if (lo == 0) {
        if (n == 0) return (int)right_ptr(page);
        uint16_t off = cell_off(page, 0);
        return (int)edb_load_u32_le(page + off + 2);
    }
    /* child is the one after separator (lo-1) */
    if (lo >= (int)n) return (int)right_ptr(page);
    uint16_t off = cell_off(page, (uint16_t)lo);
    return (int)edb_load_u32_le(page + off + 2);
}

/* Actually a cleaner internal layout:
 * For n separators there are n+1 children.
 * Cell i: key_len | child_page (left child of this separator) | key
 * right_ptr = rightmost child.
 * Search: walk separators; if key < sep[i] use cell[i].child, else continue;
 * if all seps <= key use right_ptr.
 */
static uint32_t internal_child_for_key(const uint8_t *page, const edb_key *key) {
    uint16_t n = cell_count(page);
    for (uint16_t i = 0; i < n; i++) {
        uint16_t off = cell_off(page, i);
        uint16_t klen = edb_load_u16_le(page + off);
        const uint8_t *k = page + off + 6;
        if (key_cmp(key->data, key->len, k, klen) < 0) {
            return edb_load_u32_le(page + off + 2);
        }
    }
    return right_ptr(page);
}

int edb_btree_create(edb_pager *pager, bool unique, uint32_t *out_root, edb_error *err) {
    edb_page *pg = edb_pager_new(pager, EDB_PAGE_BTREE_LEAF, err);
    if (!pg) return -1;
    set_cell_count(pg->data, 0);
    set_right_ptr(pg->data, 0);
    edb_pager_mark_dirty(pager, pg);
    *out_root = pg->page_no;
    edb_pager_unpin(pager, pg);
    (void)unique;
    return 0;
}

edb_btree *edb_btree_open(edb_pager *pager, uint32_t root, bool unique, edb_error *err) {
    edb_btree *t = calloc(1, sizeof(*t));
    if (!t) {
        edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
        return NULL;
    }
    t->pager = pager;
    t->root_page = root;
    t->unique = unique;
    t->height = 1;
    return t;
}

void edb_btree_close(edb_btree *t) {
    free(t);
}

int edb_btree_height(const edb_btree *t) {
    return t->height;
}

/* Descend to leaf, collecting path for possible splits. */
#define MAX_HEIGHT 16
typedef struct {
    uint32_t pages[MAX_HEIGHT];
    int depth;
} path_t;

static int descend(edb_btree *t, const edb_key *key, path_t *path, edb_page **out_leaf, edb_error *err) {
    path->depth = 0;
    uint32_t cur = t->root_page;
    for (;;) {
        if (path->depth >= MAX_HEIGHT) {
            edb_error_set(err, EDB_INTERNAL_INVARIANT, 0, "tree too tall");
            return -1;
        }
        path->pages[path->depth++] = cur;
        edb_page *pg = edb_pager_get(t->pager, cur, err);
        if (!pg) return -1;
        if (pg->type == EDB_PAGE_BTREE_LEAF || pg->data[HDR] == (uint8_t)EDB_PAGE_BTREE_LEAF) {
            *out_leaf = pg;
            return 0;
        }
        uint32_t child = internal_child_for_key(pg->data, key);
        edb_pager_unpin(t->pager, pg);
        if (child == 0) {
            edb_error_set(err, EDB_CORRUPTION, 0, "null child");
            return -1;
        }
        cur = child;
    }
}

int edb_btree_get(edb_btree *t, const edb_key *key,
                  uint8_t *val_out, uint16_t *val_len_io, edb_error *err) {
    path_t path;
    edb_page *leaf = NULL;
    if (descend(t, key, &path, &leaf, err) != 0) return -1;
    int idx = leaf_find_ge(leaf->data, key);
    uint16_t n = cell_count(leaf->data);
    if (idx < (int)n) {
        uint16_t off = cell_off(leaf->data, (uint16_t)idx);
        uint16_t klen = edb_load_u16_le(leaf->data + off);
        uint16_t vlen = edb_load_u16_le(leaf->data + off + 2);
        const uint8_t *k = leaf->data + off + 4;
        if (key_cmp(k, klen, key->data, key->len) == 0) {
            if (val_out && val_len_io) {
                uint16_t copy = *val_len_io < vlen ? *val_len_io : vlen;
                memcpy(val_out, leaf->data + off + 4 + klen, copy);
                *val_len_io = vlen;
            }
            edb_pager_unpin(t->pager, leaf);
            return 0;
        }
    }
    edb_pager_unpin(t->pager, leaf);
    edb_error_set(err, EDB_NOT_FOUND, 0, "key not found");
    return -1;
}

/* Insert into leaf; if no room, split. Returns separator key for parent if split. */
static int leaf_insert(edb_btree *t, edb_page *leaf, const edb_key *key,
                       const uint8_t *val, uint16_t val_len,
                       uint8_t **sep_key, uint16_t *sep_len, uint32_t *new_right,
                       edb_error *err) {
    int idx = leaf_find_ge(leaf->data, key);
    uint16_t n = cell_count(leaf->data);
    if (idx < (int)n) {
        uint16_t off = cell_off(leaf->data, (uint16_t)idx);
        uint16_t klen = edb_load_u16_le(leaf->data + off);
        const uint8_t *k = leaf->data + off + 4;
        if (key_cmp(k, klen, key->data, key->len) == 0) {
            if (t->unique) {
                edb_error_set(err, EDB_CONSTRAINT, 0, "unique violation");
                return -1;
            }
            /* non-unique: for now overwrite (real impl uses rowid discriminator) */
        }
    }

    int need = leaf_cell_size(key->len, val_len);
    /* compute free space roughly */
    uint16_t used = 9; /* fixed header inside usable */
    for (uint16_t i = 0; i < n; i++) {
        uint16_t off = cell_off(leaf->data, i);
        uint16_t kl = edb_load_u16_le(leaf->data + off);
        uint16_t vl = edb_load_u16_le(leaf->data + off + 2);
        used += leaf_cell_size(kl, vl) + 2; /* + dir slot */
    }
    if (used + need + 2 > USABLE) {
        /* split */
        edb_page *right = edb_pager_new(t->pager, EDB_PAGE_BTREE_LEAF, err);
        if (!right) return -1;
        set_cell_count(right->data, 0);
        set_right_ptr(right->data, right_ptr(leaf->data));
        set_right_ptr(leaf->data, right->page_no);

        /* collect all cells + new one, sort, distribute half */
        typedef struct { uint16_t klen, vlen; const uint8_t *k, *v; } cell_t;
        cell_t *cells = calloc(n + 1, sizeof(cell_t));
        if (!cells) {
            edb_pager_unpin(t->pager, right);
            edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
            return -1;
        }
        /* Own copies of key/val bytes so in-place rebuild cannot corrupt sources */
        uint8_t **owned = calloc(n + 1, sizeof(uint8_t*));
        if (!owned) {
            free(cells); edb_pager_unpin(t->pager, right);
            edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1;
        }
        int m = 0;
        bool inserted = false;
        for (uint16_t i = 0; i < n; i++) {
            if (!inserted && i == (uint16_t)idx) {
                cells[m].klen = key->len; cells[m].vlen = val_len;
                size_t tot = (size_t)key->len + val_len;
                owned[m] = malloc(tot ? tot : 1);
                if (!owned[m]) { /* leak on error path ok for now */ }
                if (key->len) memcpy(owned[m], key->data, key->len);
                if (val_len) memcpy(owned[m] + key->len, val, val_len);
                cells[m].k = owned[m]; cells[m].v = owned[m] + key->len;
                m++; inserted = true;
            }
            uint16_t off = cell_off(leaf->data, i);
            cells[m].klen = edb_load_u16_le(leaf->data + off);
            cells[m].vlen = edb_load_u16_le(leaf->data + off + 2);
            size_t tot = (size_t)cells[m].klen + cells[m].vlen;
            owned[m] = malloc(tot ? tot : 1);
            memcpy(owned[m], leaf->data + off + 4, cells[m].klen);
            memcpy(owned[m] + cells[m].klen, leaf->data + off + 4 + cells[m].klen, cells[m].vlen);
            cells[m].k = owned[m];
            cells[m].v = owned[m] + cells[m].klen;
            m++;
        }
        if (!inserted) {
            cells[m].klen = key->len; cells[m].vlen = val_len;
            size_t tot = (size_t)key->len + val_len;
            owned[m] = malloc(tot ? tot : 1);
            if (key->len) memcpy(owned[m], key->data, key->len);
            if (val_len) memcpy(owned[m] + key->len, val, val_len);
            cells[m].k = owned[m]; cells[m].v = owned[m] + key->len;
            m++;
        }

        int mid = m / 2;
        /* rebuild left */
        set_cell_count(leaf->data, 0);
        uint16_t write_at = HDR + 9;
        for (int i = 0; i < mid; i++) {
            uint16_t cnt = cell_count(leaf->data);
            set_cell_off(leaf->data, cnt, write_at);
            edb_store_u16_le(leaf->data + write_at, cells[i].klen);
            edb_store_u16_le(leaf->data + write_at + 2, cells[i].vlen);
            memcpy(leaf->data + write_at + 4, cells[i].k, cells[i].klen);
            memcpy(leaf->data + write_at + 4 + cells[i].klen, cells[i].v, cells[i].vlen);
            write_at += leaf_cell_size(cells[i].klen, cells[i].vlen);
            set_cell_count(leaf->data, cnt + 1);
        }
        /* rebuild right */
        write_at = HDR + 9;
        for (int i = mid; i < m; i++) {
            uint16_t cnt = cell_count(right->data);
            set_cell_off(right->data, cnt, write_at);
            edb_store_u16_le(right->data + write_at, cells[i].klen);
            edb_store_u16_le(right->data + write_at + 2, cells[i].vlen);
            memcpy(right->data + write_at + 4, cells[i].k, cells[i].klen);
            memcpy(right->data + write_at + 4 + cells[i].klen, cells[i].v, cells[i].vlen);
            write_at += leaf_cell_size(cells[i].klen, cells[i].vlen);
            set_cell_count(right->data, cnt + 1);
        }

        /* separator = first key of right */
        *sep_len = cells[mid].klen;
        *sep_key = malloc(*sep_len);
        if (!*sep_key) {
            free(cells);
            edb_pager_unpin(t->pager, right);
            edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom");
            return -1;
        }
        memcpy(*sep_key, cells[mid].k, *sep_len);
        *new_right = right->page_no;

        edb_pager_mark_dirty(t->pager, leaf);
        edb_pager_mark_dirty(t->pager, right);
        edb_pager_write_page(t->pager, right, err);
        edb_pager_unpin(t->pager, right);
        for (int oi = 0; oi < m; oi++) free(owned[oi]);
        free(owned);
        free(cells);
        return 1; /* split occurred */
    }

    /* enough room: insert in place by rebuilding cells after idx */
    /* simple approach: shift cell directory and copy new cell at end of data area */
    uint16_t write_at = HDR + 9;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t off = cell_off(leaf->data, i);
        uint16_t kl = edb_load_u16_le(leaf->data + off);
        uint16_t vl = edb_load_u16_le(leaf->data + off + 2);
        write_at += leaf_cell_size(kl, vl);
    }
    /* place new cell */
    edb_store_u16_le(leaf->data + write_at, key->len);
    edb_store_u16_le(leaf->data + write_at + 2, val_len);
    memcpy(leaf->data + write_at + 4, key->data, key->len);
    memcpy(leaf->data + write_at + 4 + key->len, val, val_len);

    /* shift directory entries */
    for (int i = (int)n; i > idx; i--) {
        set_cell_off(leaf->data, (uint16_t)i, cell_off(leaf->data, (uint16_t)(i-1)));
    }
    set_cell_off(leaf->data, (uint16_t)idx, write_at);
    set_cell_count(leaf->data, n + 1);
    edb_pager_mark_dirty(t->pager, leaf);
    *sep_key = NULL;
    *new_right = 0;
    return 0;
}

/* Insert separator into internal node; may split.
 * Convention: cell stores (key_len, LEFT_child, key); right_ptr = rightmost child.
 * When child P splits into P(left)+R(right) with separator K:
 *  - Find slot pointing to P
 *  - Insert K with left_child=P; make the following pointer be R
 */
static int internal_insert(edb_btree *t, edb_page *node,
                           const uint8_t *sep, uint16_t sep_len, uint32_t right_child,
                           uint8_t **up_sep, uint16_t *up_len, uint32_t *up_right,
                           edb_error *err) {
    uint16_t n = cell_count(node->data);
    int need = internal_cell_size(sep_len) + 2;
    uint16_t used = 9;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t off = cell_off(node->data, i);
        uint16_t kl = edb_load_u16_le(node->data + off);
        used += (uint16_t)(internal_cell_size(kl) + 2);
    }

    /* Collect existing cells + new one into array for uniform rebuild */
    typedef struct { uint16_t klen; uint32_t left_child; const uint8_t *k; } icell_t;
    icell_t *cells = calloc((size_t)n + 2, sizeof(icell_t));
    if (!cells) { edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }

    /* Determine insert index by key order */
    int idx = 0;
    for (; idx < (int)n; idx++) {
        uint16_t off = cell_off(node->data, (uint16_t)idx);
        uint16_t kl = edb_load_u16_le(node->data + off);
        const uint8_t *k = node->data + off + 6;
        if (key_cmp(sep, sep_len, k, kl) < 0) break;
    }

    uint32_t old_right = right_ptr(node->data);
    uint8_t **owned_keys = calloc((size_t)n + 2, sizeof(uint8_t*));
    if (!owned_keys) { free(cells); edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1; }
    int m = 0;
    for (int i = 0; i < idx; i++) {
        uint16_t off = cell_off(node->data, (uint16_t)i);
        cells[m].klen = edb_load_u16_le(node->data + off);
        cells[m].left_child = edb_load_u32_le(node->data + off + 2);
        owned_keys[m] = malloc(cells[m].klen ? cells[m].klen : 1);
        memcpy(owned_keys[m], node->data + off + 6, cells[m].klen);
        cells[m].k = owned_keys[m];
        m++;
    }
    uint32_t prev_child;
    if (idx < (int)n) {
        uint16_t off = cell_off(node->data, (uint16_t)idx);
        prev_child = edb_load_u32_le(node->data + off + 2);
    } else {
        prev_child = old_right;
    }
    cells[m].klen = sep_len;
    cells[m].left_child = prev_child;
    owned_keys[m] = malloc(sep_len ? sep_len : 1);
    memcpy(owned_keys[m], sep, sep_len);
    cells[m].k = owned_keys[m];
    m++;
    for (int i = idx; i < (int)n; i++) {
        uint16_t off = cell_off(node->data, (uint16_t)i);
        cells[m].klen = edb_load_u16_le(node->data + off);
        cells[m].left_child = (i == idx) ? right_child : edb_load_u32_le(node->data + off + 2);
        owned_keys[m] = malloc(cells[m].klen ? cells[m].klen : 1);
        memcpy(owned_keys[m], node->data + off + 6, cells[m].klen);
        cells[m].k = owned_keys[m];
        m++;
    }
    uint32_t final_right = (idx >= (int)n) ? right_child : old_right;

    if (used + need > USABLE) {
        /* Split: promote middle separator */
        edb_page *right = edb_pager_new(t->pager, EDB_PAGE_BTREE_INTERNAL, err);
        if (!right) { free(cells); return -1; }
        set_cell_count(right->data, 0);

        int mid = m / 2;
        *up_len = cells[mid].klen;
        *up_sep = malloc(*up_len);
        if (!*up_sep) {
            free(cells); edb_pager_unpin(t->pager, right);
            edb_error_set(err, EDB_OUT_OF_MEMORY, 0, "oom"); return -1;
        }
        memcpy(*up_sep, cells[mid].k, *up_len);
        *up_right = right->page_no;

        /* left half: cells[0..mid) ; right_ptr = left_child of promoted */
        set_cell_count(node->data, 0);
        uint16_t write_at = HDR + 9;
        for (int i = 0; i < mid; i++) {
            uint16_t cnt = cell_count(node->data);
            set_cell_off(node->data, cnt, write_at);
            edb_store_u16_le(node->data + write_at, cells[i].klen);
            edb_store_u32_le(node->data + write_at + 2, cells[i].left_child);
            memcpy(node->data + write_at + 6, cells[i].k, cells[i].klen);
            write_at = (uint16_t)(write_at + internal_cell_size(cells[i].klen));
            set_cell_count(node->data, cnt + 1);
        }
        set_right_ptr(node->data, cells[mid].left_child);

        /* right half: cells[mid+1..m) ; right_ptr = final_right */
        write_at = HDR + 9;
        for (int i = mid + 1; i < m; i++) {
            uint16_t cnt = cell_count(right->data);
            set_cell_off(right->data, cnt, write_at);
            edb_store_u16_le(right->data + write_at, cells[i].klen);
            edb_store_u32_le(right->data + write_at + 2, cells[i].left_child);
            memcpy(right->data + write_at + 6, cells[i].k, cells[i].klen);
            write_at = (uint16_t)(write_at + internal_cell_size(cells[i].klen));
            set_cell_count(right->data, cnt + 1);
        }
        set_right_ptr(right->data, final_right);

        edb_pager_mark_dirty(t->pager, node);
        edb_pager_mark_dirty(t->pager, right);
        edb_pager_write_page(t->pager, right, err);
        edb_pager_unpin(t->pager, right);
        for (int oi = 0; oi < m; oi++) free(owned_keys[oi]);
        free(owned_keys);
        free(cells);
        return 1;
    }

    /* No split: rebuild node from cells array */
    set_cell_count(node->data, 0);
    uint16_t write_at = HDR + 9;
    for (int i = 0; i < m; i++) {
        uint16_t cnt = cell_count(node->data);
        set_cell_off(node->data, cnt, write_at);
        edb_store_u16_le(node->data + write_at, cells[i].klen);
        edb_store_u32_le(node->data + write_at + 2, cells[i].left_child);
        memcpy(node->data + write_at + 6, cells[i].k, cells[i].klen);
        write_at = (uint16_t)(write_at + internal_cell_size(cells[i].klen));
        set_cell_count(node->data, cnt + 1);
    }
    set_right_ptr(node->data, final_right);
    edb_pager_mark_dirty(t->pager, node);
    for (int oi = 0; oi < m; oi++) free(owned_keys[oi]);
    free(owned_keys);
    free(cells);
    *up_sep = NULL;
    *up_right = 0;
    *up_len = 0;
    return 0;
}

int edb_btree_insert(edb_btree *t, const edb_key *key,
                     const uint8_t *val, uint16_t val_len, edb_error *err) {
    path_t path;
    edb_page *leaf = NULL;
    if (descend(t, key, &path, &leaf, err) != 0) return -1;

    uint8_t *sep = NULL;
    uint16_t sep_len = 0;
    uint32_t new_right = 0;
    int rc = leaf_insert(t, leaf, key, val, val_len, &sep, &sep_len, &new_right, err);
    edb_pager_write_page(t->pager, leaf, err);
    edb_pager_unpin(t->pager, leaf);
    if (rc < 0) return -1;
    if (rc == 0) return 0; /* no split */

    /* split happened – propagate separators up the path */
    int level = path.depth - 1; /* leaf level index in path */
    while (sep != NULL) {
        if (level == 0) {
            /* create new root */
            edb_page *root = edb_pager_new(t->pager, EDB_PAGE_BTREE_INTERNAL, err);
            if (!root) { free(sep); return -1; }
            set_cell_count(root->data, 0);
            uint16_t write_at = HDR + 9;
            edb_store_u16_le(root->data + write_at, sep_len);
            edb_store_u32_le(root->data + write_at + 2, t->root_page);
            memcpy(root->data + write_at + 6, sep, sep_len);
            set_cell_off(root->data, 0, write_at);
            set_cell_count(root->data, 1);
            set_right_ptr(root->data, new_right);
            edb_pager_mark_dirty(t->pager, root);
            edb_pager_write_page(t->pager, root, err);
            t->root_page = root->page_no;
            t->height++;
            edb_pager_unpin(t->pager, root);
            free(sep);
            sep = NULL;
            break;
        }
        level--;
        edb_page *parent = edb_pager_get(t->pager, path.pages[level], err);
        if (!parent) { free(sep); return -1; }
        uint8_t *up_sep = NULL;
        uint16_t up_len = 0;
        uint32_t up_right = 0;
        rc = internal_insert(t, parent, sep, sep_len, new_right, &up_sep, &up_len, &up_right, err);
        free(sep);
        sep = NULL;
        edb_pager_write_page(t->pager, parent, err);
        edb_pager_unpin(t->pager, parent);
        if (rc < 0) { free(up_sep); return -1; }
        if (rc == 1) {
            /* parent split – continue upward */
            sep = up_sep;
            sep_len = up_len;
            new_right = up_right;
        } else {
            free(up_sep);
        }
    }
    return 0;
}


int edb_btree_delete(edb_btree *t, const edb_key *key, edb_error *err) {
    path_t path;
    edb_page *leaf = NULL;
    if (descend(t, key, &path, &leaf, err) != 0) return -1;
    int idx = leaf_find_ge(leaf->data, key);
    uint16_t n = cell_count(leaf->data);
    if (idx >= (int)n) {
        edb_pager_unpin(t->pager, leaf);
        edb_error_set(err, EDB_NOT_FOUND, 0, "key not found");
        return -1;
    }
    uint16_t off = cell_off(leaf->data, (uint16_t)idx);
    uint16_t klen = edb_load_u16_le(leaf->data + off);
    const uint8_t *k = leaf->data + off + 4;
    if (key_cmp(k, klen, key->data, key->len) != 0) {
        edb_pager_unpin(t->pager, leaf);
        edb_error_set(err, EDB_NOT_FOUND, 0, "key not found");
        return -1;
    }
    /* remove directory slot */
    for (uint16_t i = (uint16_t)idx; i + 1 < n; i++)
        set_cell_off(leaf->data, i, cell_off(leaf->data, i + 1));
    set_cell_count(leaf->data, n - 1);
    n = n - 1;
    edb_pager_mark_dirty(t->pager, leaf);

    /* Underflow handling: if leaf empty and not root, merge away; if sparse, try merge with right sibling */
    if (path.depth > 1 && n == 0) {
        uint32_t empty_page = leaf->page_no;
        edb_pager_write_page(t->pager, leaf, err);
        edb_pager_unpin(t->pager, leaf);
        /* Remove reference from parent: find which child we are and remove separator */
        edb_page *parent = edb_pager_get(t->pager, path.pages[path.depth - 2], err);
        if (!parent) return -1;
        uint16_t pn = cell_count(parent->data);
        int removed = 0;
        /* If empty_page is right_ptr, just clear right_ptr or set to previous left */
        if (right_ptr(parent->data) == empty_page) {
            if (pn > 0) {
                uint16_t loff = cell_off(parent->data, pn - 1);
                uint32_t leftc = edb_load_u32_le(parent->data + loff + 2);
                set_right_ptr(parent->data, leftc);
                /* remove last separator */
                set_cell_count(parent->data, pn - 1);
            } else {
                set_right_ptr(parent->data, 0);
            }
            removed = 1;
        } else {
            for (uint16_t i = 0; i < pn; i++) {
                uint16_t coff = cell_off(parent->data, i);
                uint32_t ch = edb_load_u32_le(parent->data + coff + 2);
                if (ch == empty_page) {
                    for (uint16_t j = i; j + 1 < pn; j++)
                        set_cell_off(parent->data, j, cell_off(parent->data, j + 1));
                    set_cell_count(parent->data, pn - 1);
                    removed = 1;
                    break;
                }
            }
        }
        edb_pager_mark_dirty(t->pager, parent);
        edb_pager_write_page(t->pager, parent, err);
        edb_pager_unpin(t->pager, parent);
        edb_pager_free_page(t->pager, empty_page);
        (void)removed;
        /* Root collapse: if height>1 and root has 0 separators and one child */
        if (path.depth == 2) {
            edb_page *root = edb_pager_get(t->pager, t->root_page, err);
            if (root && cell_count(root->data) == 0) {
                uint32_t only = right_ptr(root->data);
                if (only != 0) {
                    uint32_t old = t->root_page;
                    t->root_page = only;
                    t->height = 1;
                    edb_pager_unpin(t->pager, root);
                    edb_pager_free_page(t->pager, old);
                    return 0;
                }
            }
            if (root) edb_pager_unpin(t->pager, root);
        }
        return 0;
    }

    /* Try merge with right sibling if both under half-full (approx: n < 4) */
    if (path.depth > 1 && n > 0 && n < 4) {
        uint32_t sib = right_ptr(leaf->data);
        if (sib != 0) {
            edb_page *right = edb_pager_get(t->pager, sib, err);
            if (right) {
                uint16_t rn = cell_count(right->data);
                if (rn > 0 && rn < 4) {
                    /* merge right into leaf if fits */
                    int need = 0;
                    for (uint16_t i = 0; i < rn; i++) {
                        uint16_t ro = cell_off(right->data, i);
                        uint16_t kl = edb_load_u16_le(right->data + ro);
                        uint16_t vl = edb_load_u16_le(right->data + ro + 2);
                        need += leaf_cell_size(kl, vl) + 2;
                    }
                    uint16_t used = 9;
                    for (uint16_t i = 0; i < n; i++) {
                        uint16_t lo = cell_off(leaf->data, i);
                        uint16_t kl = edb_load_u16_le(leaf->data + lo);
                        uint16_t vl = edb_load_u16_le(leaf->data + lo + 2);
                        used += (uint16_t)(leaf_cell_size(kl, vl) + 2);
                    }
                    if (used + need < USABLE) {
                        /* append right cells onto leaf */
                        uint16_t write_at = HDR + 9;
                        for (uint16_t i = 0; i < n; i++) {
                            uint16_t lo = cell_off(leaf->data, i);
                            if (lo + leaf_cell_size(edb_load_u16_le(leaf->data+lo), edb_load_u16_le(leaf->data+lo+2)) > write_at)
                                write_at = (uint16_t)(lo + leaf_cell_size(edb_load_u16_le(leaf->data+lo), edb_load_u16_le(leaf->data+lo+2)));
                        }
                        for (uint16_t i = 0; i < rn; i++) {
                            uint16_t ro = cell_off(right->data, i);
                            uint16_t kl = edb_load_u16_le(right->data + ro);
                            uint16_t vl = edb_load_u16_le(right->data + ro + 2);
                            uint16_t cnt = cell_count(leaf->data);
                            set_cell_off(leaf->data, cnt, write_at);
                            memcpy(leaf->data + write_at, right->data + ro, leaf_cell_size(kl, vl));
                            write_at = (uint16_t)(write_at + leaf_cell_size(kl, vl));
                            set_cell_count(leaf->data, cnt + 1);
                        }
                        set_right_ptr(leaf->data, right_ptr(right->data));
                        edb_pager_mark_dirty(t->pager, leaf);
                        edb_pager_write_page(t->pager, leaf, err);
                        edb_pager_unpin(t->pager, leaf);
                        edb_pager_unpin(t->pager, right);
                        edb_pager_free_page(t->pager, sib);
                        return 0;
                    }
                }
                edb_pager_unpin(t->pager, right);
            }
        }
    }

    edb_pager_write_page(t->pager, leaf, err);
    edb_pager_unpin(t->pager, leaf);
    return 0;
}


int edb_btree_validate(edb_btree *t, edb_error *err) {
    /* Basic: walk leaves via right sibling and check ordering. */
    path_t path;
    edb_key dummy = { .data = (const uint8_t*)"", .len = 0 };
    edb_page *leaf = NULL;
    if (descend(t, &dummy, &path, &leaf, err) != 0) return -1;
    /* go to leftmost: for now assume root path left is leftmost when key empty */
    edb_pager_unpin(t->pager, leaf);

    uint32_t cur = path.pages[path.depth-1];
    /* if height>1 we need leftmost leaf – simplified: start from root and always take left child */
    cur = t->root_page;
    for (;;) {
        edb_page *pg = edb_pager_get(t->pager, cur, err);
        if (!pg) return -1;
        if (pg->data[HDR] == (uint8_t)EDB_PAGE_BTREE_LEAF || pg->type == EDB_PAGE_BTREE_LEAF) {
            edb_pager_unpin(t->pager, pg);
            break;
        }
        uint16_t n = cell_count(pg->data);
        uint32_t left;
        if (n > 0) {
            uint16_t off = cell_off(pg->data, 0);
            left = edb_load_u32_le(pg->data + off + 2);
        } else {
            left = right_ptr(pg->data);
        }
        edb_pager_unpin(t->pager, pg);
        cur = left;
        if (cur == 0) {
            edb_error_set(err, EDB_CORRUPTION, 0, "null leftmost");
            return -1;
        }
    }

    const uint8_t *prev_k = NULL;
    uint16_t prev_len = 0;
    uint8_t prev_buf[512];
    while (cur) {
        edb_page *pg = edb_pager_get(t->pager, cur, err);
        if (!pg) return -1;
        uint16_t n = cell_count(pg->data);
        for (uint16_t i = 0; i < n; i++) {
            uint16_t off = cell_off(pg->data, i);
            uint16_t klen = edb_load_u16_le(pg->data + off);
            const uint8_t *k = pg->data + off + 4;
            if (prev_k) {
                if (key_cmp(prev_k, prev_len, k, klen) > 0) {
                    edb_pager_unpin(t->pager, pg);
                    edb_error_set(err, EDB_CORRUPTION, 0, "key order violation");
                    return -1;
                }
            }
            if (klen <= sizeof prev_buf) {
                memcpy(prev_buf, k, klen);
                prev_k = prev_buf;
                prev_len = klen;
            }
        }
        cur = right_ptr(pg->data);
        edb_pager_unpin(t->pager, pg);
    }
    return 0;
}
