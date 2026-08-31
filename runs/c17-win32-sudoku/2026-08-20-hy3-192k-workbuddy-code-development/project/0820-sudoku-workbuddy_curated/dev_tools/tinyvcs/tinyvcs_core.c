/* tinyvcs_core.c - content-addressed snapshot version-control core.
 *
 * See tinyvcs_core.h and docs/04 + docs/19 sections 6-15, 29.
 */
#include "tinyvcs_core.h"
#include "common/sdk_win.h"
#include "common/sdk_sha256.h"
#include "common/sdk_crc32.h"
#include "common/sdk_lzss.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Error reporting state                                               */
/* ------------------------------------------------------------------ */

static const char *g_stage = "none";
static uint32_t    g_win32 = 0;

const char *tv_error_stage(void) { return g_stage; }
uint32_t    tv_error_win32(void) { return g_win32; }
void        tv_error_clear(void) { g_stage = "none"; g_win32 = 0; }
void        tv_error_set(const char *stage, uint32_t win32) {
    if (stage) g_stage = stage;
    g_win32 = win32;
}

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static wchar_t *tv_join(const wchar_t *base, const wchar_t *leaf) {
    return sdk_wpath_join(base, leaf);
}

static sdk_status tv_ensure_parent(const wchar_t *target) {
    size_t len = wcslen(target);
    while (len > 0 && target[len - 1] != L'\\' && target[len - 1] != L'/')
        --len;
    if (len <= 1) return SDK_OK;
    wchar_t *parent = sdk_wcsdup_n(target, len - 1);
    if (!parent) return SDK_ERR_NOMEM;
    uint32_t we = 0;
    sdk_status st = sdk_mkdir_parents_w(parent, &we);
    if (st != SDK_OK) { g_win32 = we; g_stage = "mkdir_parents"; }
    free(parent);
    return st;
}

void tv_oid_hex(const tv_oid id, char out_hex[65]) {
    sdk_hex_encode(id, 32, out_hex);
    out_hex[64] = '\0';
}

int tv_oid_from_hex(const char *hex, tv_oid out) {
    if (strlen(hex) != 64) return 0;
    return sdk_hex_decode(hex, 64, out);
}

int tv_oid_equal(const tv_oid a, const tv_oid b) {
    return sdk_ct_equal(a, b, 32);
}

/* ------------------------------------------------------------------ */
/* Object identity and store                                           */
/* ------------------------------------------------------------------ */

void tv_object_id_for(const char *type, const uint8_t *payload, size_t plen,
                      tv_oid out_id) {
    sdk_sha256_ctx c;
    char hdr[64];
    int n = snprintf(hdr, sizeof hdr, "%s %zu", type, plen);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof hdr) n = (int)(sizeof hdr - 1);
    sdk_sha256_init(&c);
    sdk_sha256_update(&c, hdr, (size_t)n);
    uint8_t nul = 0;
    sdk_sha256_update(&c, &nul, 1);
    sdk_sha256_update(&c, payload, plen);
    sdk_sha256_final(&c, out_id);
}

sdk_status tv_object_path(tv_repo *r, const tv_oid id, wchar_t **out) {
    char hex[65];
    tv_oid_hex(id, hex);
    wchar_t dir[3];
    dir[0] = (wchar_t)hex[0];
    dir[1] = (wchar_t)hex[1];
    dir[2] = L'\0';
    wchar_t *objdir = tv_join(r->meta, L"objects");
    if (!objdir) return SDK_ERR_NOMEM;
    wchar_t *dd = tv_join(objdir, dir);
    free(objdir);
    if (!dd) return SDK_ERR_NOMEM;
    wchar_t *p = sdk_wpath_join(dd, sdk_utf8_to_utf16(hex + 2, 62, NULL));
    free(dd);
    if (!p) return SDK_ERR_NOMEM;
    /* re-root: sdk_wpath_join already joined dd + leaf; but we need meta/objects/dd/leaf */
    *out = p;
    return SDK_OK;
}

sdk_status tv_object_exists(tv_repo *r, const tv_oid id, int *exists) {
    wchar_t *p;
    if (tv_object_path(r, id, &p) != SDK_OK) return SDK_ERR_NOMEM;
    sdk_fileinfo fi;
    sdk_status st = sdk_stat_w(p, &fi);
    free(p);
    if (st == SDK_OK && fi.exists && !fi.is_directory) *exists = 1;
    else *exists = 0;
    return SDK_OK;
}

sdk_status tv_write_object(tv_repo *r, tv_obj_type type,
                           const uint8_t *payload, size_t plen, tv_oid out_id) {
    const char *tn = type == TV_OBJ_BLOB ? "blob"
                   : type == TV_OBJ_TREE ? "tree" : "commit";
    tv_object_id_for(tn, payload, plen, out_id);

    uint8_t mode = 0;
    const uint8_t *stored = payload;
    size_t stored_len = plen;
    uint8_t *comp = NULL;

    if (plen > 0) {
        size_t maxc = sdk_lzss_max_compressed_size(plen);
        comp = (uint8_t *)malloc(maxc ? maxc : 1);
        if (!comp) return SDK_ERR_NOMEM;
        size_t complen = 0;
        sdk_status cs = sdk_lzss_compress(payload, plen, comp, maxc, &complen);
        if (cs == SDK_OK && complen < plen) {
            mode = 1;
            stored = comp;
            stored_len = complen;
        }
    }

    sdk_buf env;
    sdk_buf_init(&env);
    sdk_buf_append(&env, "TVCSOBJ1", 8);
    sdk_buf_append_u8(&env, mode);
    sdk_buf_append_u8(&env, (uint8_t)type);
    sdk_buf_append_u16le(&env, 0);
    sdk_buf_append_u64le(&env, (uint64_t)plen);
    sdk_buf_append_u64le(&env, (uint64_t)stored_len);
    sdk_buf_append_u32le(&env, sdk_crc32(payload, plen));
    sdk_buf_append(&env, out_id, 32);
    sdk_buf_append(&env, stored, stored_len);

    sdk_status st = SDK_ERR_NOMEM;
    if (!env.failed) {
        wchar_t *opath;
        if (tv_object_path(r, out_id, &opath) == SDK_OK) {
            tv_ensure_parent(opath);
            const char *stage = "object_write";
            uint32_t we = 0;
            st = sdk_file_write_atomic_w(opath, NULL, env.data, env.len,
                                         &stage, &we);
            if (st != SDK_OK) { g_stage = stage; g_win32 = we; }
            free(opath);
        }
    }
    free(comp);
    sdk_buf_free(&env);
    return st;
}

sdk_status tv_read_object(tv_repo *r, const tv_oid id, tv_obj_type *out_type,
                          uint8_t **out_payload, size_t *out_len) {
    wchar_t *opath;
    if (tv_object_path(r, id, &opath) != SDK_OK) return SDK_ERR_NOMEM;
    uint8_t *data = NULL;
    size_t len = 0;
    uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(opath,
                                        (size_t)SDK_LIMIT_FILE_BYTES + 256,
                                        &data, &len, &we);
    free(opath);
    if (st == SDK_ERR_NOT_FOUND) return SDK_ERR_NOT_FOUND;
    if (st != SDK_OK) { g_stage = "object_read"; g_win32 = we; return st; }
    if (len < 64) { free(data); return SDK_ERR_DATA; }
    if (memcmp(data, "TVCSOBJ1", 8) != 0) { free(data); return SDK_ERR_DATA; }
    uint8_t mode = data[8];
    uint8_t otype = data[9];
    uint16_t reserved = sdk_get_u16le(data + 10);
    uint64_t uncomp = sdk_get_u64le(data + 12);
    uint64_t stored = sdk_get_u64le(data + 20);
    uint32_t crc = sdk_get_u32le(data + 28);
    if (reserved != 0) { free(data); return SDK_ERR_DATA; }
    if (mode > 1) { free(data); return SDK_ERR_DATA; }
    if (otype < 1 || otype > 3) { free(data); return SDK_ERR_DATA; }
    if (mode == 1 && stored >= uncomp) { free(data); return SDK_ERR_DATA; }
    if (stored > len - 64) { free(data); return SDK_ERR_DATA; }
    if ((uint64_t)64 + stored != len) { free(data); return SDK_ERR_DATA; }

    uint8_t *payload = (uint8_t *)malloc(uncomp ? (size_t)uncomp : 1);
    if (!payload) { free(data); return SDK_ERR_NOMEM; }
    if (mode == 0) {
        if (uncomp != stored) { free(payload); free(data); return SDK_ERR_DATA; }
        memcpy(payload, data + 64, (size_t)uncomp);
    } else {
        sdk_status ds = sdk_lzss_decompress(data + 64, (size_t)stored,
                                            payload, (size_t)uncomp);
        if (ds != SDK_OK) { free(payload); free(data); return SDK_ERR_DATA; }
    }
    if (sdk_crc32(payload, (size_t)uncomp) != crc) {
        free(payload); free(data); return SDK_ERR_VERIFY;
    }
    const char *tn = otype == 1 ? "blob" : otype == 2 ? "tree" : "commit";
    tv_oid calc;
    tv_object_id_for(tn, payload, (size_t)uncomp, calc);
    if (!tv_oid_equal(calc, data + 32)) {
        free(payload); free(data); return SDK_ERR_VERIFY;
    }
    free(data);
    *out_type = (tv_obj_type)otype;
    *out_payload = payload;
    *out_len = (size_t)uncomp;
    return SDK_OK;
}

static sdk_status root_path(tv_repo *r, const char *rel_utf8, wchar_t **out);

sdk_status tv_file_blob_id(const wchar_t *work_path, tv_oid out_id,
                           uint64_t *out_size, int *out_readonly) {
    sdk_fileinfo fi;
    if (sdk_stat_w(work_path, &fi) != SDK_OK) { g_stage = "stat"; return SDK_ERR_IO; }
    if (!fi.exists || fi.is_directory) { g_stage = "stat"; return SDK_ERR_DATA; }
    if (out_readonly) *out_readonly = fi.is_readonly;
    if (out_size) *out_size = fi.size;
    uint8_t *data = NULL;
    size_t len = 0;
    uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(work_path, SDK_LIMIT_FILE_BYTES,
                                        &data, &len, &we);
    if (st != SDK_OK) { g_stage = "file_read"; g_win32 = we; return st; }
    tv_object_id_for("blob", data, len, out_id);
    free(data);
    return SDK_OK;
}

/* Write the blob object for an index entry (idempotent: skips if present).
 * The computed oid must match the one already stored in the index entry. */
static sdk_status tv_write_blob_for_entry(tv_repo *r, const tv_index_entry *e) {
    wchar_t *wpath = NULL;
    if (root_path(r, e->path, &wpath) != SDK_OK) return SDK_ERR_NOMEM;
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(wpath, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
    free(wpath);
    if (st != SDK_OK) { g_stage = "blob_read"; g_win32 = we; return st; }
    tv_oid id;
    tv_object_id_for("blob", data, len, id);
    if (!tv_oid_equal(id, e->blob)) { free(data); g_stage = "blob_mismatch"; return SDK_ERR_DATA; }
    int ex = 0;
    if (tv_object_exists(r, id, &ex) == SDK_OK && ex) { free(data); return SDK_OK; }
    st = tv_write_object(r, TV_OBJ_BLOB, data, len, id);
    free(data);
    return st;
}

/* ------------------------------------------------------------------ */
/* Tree parsing and building                                           */
/* ------------------------------------------------------------------ */

sdk_status tv_tree_parse(const uint8_t *payload, size_t len,
                         tv_tree_entry **out, size_t *out_count) {
    sdk_rd rd;
    sdk_rd_init(&rd, payload, len);
    tv_tree_entry *arr = NULL;
    size_t n = 0, cap = 0;
    char last_name[256];
    int have_last = 0;
    while (!sdk_rd_at_end(&rd)) {
        uint8_t et, ff;
        uint16_t nl;
        if (!sdk_rd_u8(&rd, &et)) { free(arr); return SDK_ERR_DATA; }
        if (!sdk_rd_u8(&rd, &ff)) { free(arr); return SDK_ERR_DATA; }
        if (!sdk_rd_u16le(&rd, &nl)) { free(arr); return SDK_ERR_DATA; }
        if (et != 1 && et != 2) { free(arr); return SDK_ERR_DATA; }
        if (et == 2 && ff != 0) { free(arr); return SDK_ERR_DATA; }
        if (nl < 1 || nl > 255) { free(arr); return SDK_ERR_DATA; }
        const uint8_t *nm = sdk_rd_take(&rd, nl);
        if (!nm) { free(arr); return SDK_ERR_DATA; }
        for (size_t i = 0; i < nl; ++i) {
            if (nm[i] == '/' || nm[i] == 0) { free(arr); return SDK_ERR_DATA; }
        }
        const uint8_t *op = sdk_rd_take(&rd, 32);
        if (!op) { free(arr); return SDK_ERR_DATA; }
        char namebuf[256];
        memcpy(namebuf, nm, nl);
        namebuf[nl] = '\0';
        if (have_last && sdk_canon_path_cmp(namebuf, last_name) <= 0) {
            free(arr); return SDK_ERR_DATA;
        }
        if (n == cap) {
            size_t ncap = cap ? cap * 2 : 16;
            tv_tree_entry *ni = (tv_tree_entry *)realloc(arr, ncap * sizeof *ni);
            if (!ni) { free(arr); return SDK_ERR_NOMEM; }
            arr = ni; cap = ncap;
        }
        tv_tree_entry *e = &arr[n++];
        memcpy(e->name, namebuf, nl + 1);
        e->entry_type = et;
        e->file_flags = ff;
        memcpy(e->oid, op, 32);
        memcpy(last_name, namebuf, nl + 1);
        have_last = 1;
    }
    *out = arr;
    *out_count = n;
    return SDK_OK;
}

/* Build-time directory node used to assemble nested trees. */
typedef struct bchild {
    char     name[256];
    int      is_dir;
    int      node_idx;   /* valid when is_dir */
    tv_oid   oid;        /* valid when !is_dir */
    uint8_t  flags;
} bchild;

typedef struct bnode {
    char    *dir;
    bchild  *children;
    size_t   n, cap;
    int      has_id;
    tv_oid   id;
} bnode;

static bnode *g_nodes;
static size_t g_nnodes, g_capnodes;

static int node_find(const char *dir) {
    for (size_t i = 0; i < g_nnodes; ++i)
        if (strcmp(g_nodes[i].dir, dir) == 0) return (int)i;
    return -1;
}

static int node_create(const char *dir) {
    if (g_nnodes == g_capnodes) {
        size_t nc = g_capnodes ? g_capnodes * 2 : 16;
        bnode *nn = (bnode *)realloc(g_nodes, nc * sizeof *nn);
        if (!nn) return -1;
        g_nodes = nn; g_capnodes = nc;
    }
    bnode *b = &g_nodes[g_nnodes];
    memset(b, 0, sizeof *b);
    b->dir = _strdup(dir);
    if (!b->dir) return -1;
    return (int)(g_nnodes++);
}

static int add_dir_child(const char *parent_dir, const char *name) {
    int pi = node_find(parent_dir);
    if (pi < 0) return -1;
    for (size_t i = 0; i < g_nodes[pi].n; ++i) {
        bchild *c = &g_nodes[pi].children[i];
        if (strcmp(c->name, name) == 0) {
            if (c->is_dir) return c->node_idx; /* already present */
            return -1; /* collision with a file */
        }
        if (sdk_canon_path_cmp(c->name, name) == 0) return -1; /* case collision */
    }
    /* ensure child node */
    char childdir[SDK_LIMIT_VCS_PATH_BYTES];
    if (parent_dir[0] == '\0')
        snprintf(childdir, sizeof childdir, "%s", name);
    else
        snprintf(childdir, sizeof childdir, "%s/%s", parent_dir, name);
    int ci = node_find(childdir);
    if (ci < 0) { ci = node_create(childdir); if (ci < 0) return -1; }
    if (g_nodes[pi].n == g_nodes[pi].cap) {
        size_t nc = g_nodes[pi].cap ? g_nodes[pi].cap * 2 : 8;
        bchild *nb = (bchild *)realloc(g_nodes[pi].children, nc * sizeof *nb);
        if (!nb) return -1;
        g_nodes[pi].children = nb; g_nodes[pi].cap = nc;
    }
    bchild *c = &g_nodes[pi].children[g_nodes[pi].n++];
    memset(c, 0, sizeof *c);
    memcpy(c->name, name, strlen(name) + 1);
    c->is_dir = 1;
    c->node_idx = ci;
    return ci;
}

static int add_file_child(const char *parent_dir, const char *name,
                          const tv_oid *oid, uint8_t flags) {
    int pi = node_find(parent_dir);
    if (pi < 0) return -1;
    for (size_t i = 0; i < g_nodes[pi].n; ++i) {
        bchild *c = &g_nodes[pi].children[i];
        if (sdk_canon_path_cmp(c->name, name) == 0) return -1; /* collision */
    }
    if (g_nodes[pi].n == g_nodes[pi].cap) {
        size_t nc = g_nodes[pi].cap ? g_nodes[pi].cap * 2 : 8;
        bchild *nb = (bchild *)realloc(g_nodes[pi].children, nc * sizeof *nb);
        if (!nb) return -1;
        g_nodes[pi].children = nb; g_nodes[pi].cap = nc;
    }
    bchild *c = &g_nodes[pi].children[g_nodes[pi].n++];
    memset(c, 0, sizeof *c);
    memcpy(c->name, name, strlen(name) + 1);
    c->is_dir = 0;
    memcpy(c->oid, oid, 32);
    c->flags = (uint8_t)(flags & 0x01u);
    return 0;
}

static int cmp_entry_name(const void *a, const void *b) {
    const tv_tree_entry *x = (const tv_tree_entry *)a;
    const tv_tree_entry *y = (const tv_tree_entry *)b;
    return sdk_canon_path_cmp(x->name, y->name);
}

/* Forward declaration for post-order id computation. */
static sdk_status compute_node_ids(tv_repo *r, int idx);

static sdk_status compute_node_ids(tv_repo *r, int idx) {
    bnode *nd = &g_nodes[idx];
    if (nd->has_id) return SDK_OK;

    tv_tree_entry *entries = NULL;
    size_t ec = 0, ecap = 0;
    for (size_t i = 0; i < nd->n; ++i) {
        bchild *c = &nd->children[i];
        if (c->is_dir) {
            if (compute_node_ids(r, c->node_idx) != SDK_OK) { free(entries); return SDK_ERR_NOMEM; }
            memcpy(c->oid, g_nodes[c->node_idx].id, 32);
        }
        if (ec == ecap) {
            size_t nc = ecap ? ecap * 2 : 16;
            tv_tree_entry *ne = (tv_tree_entry *)realloc(entries, nc * sizeof *ne);
            if (!ne) { free(entries); return SDK_ERR_NOMEM; }
            entries = ne; ecap = nc;
        }
        tv_tree_entry *e = &entries[ec++];
        memcpy(e->name, c->name, strlen(c->name) + 1);
        e->entry_type = c->is_dir ? 2 : 1;
        e->file_flags = c->is_dir ? 0 : c->flags;
        memcpy(e->oid, c->oid, 32);
    }
    qsort(entries, ec, sizeof *entries, cmp_entry_name);
    for (size_t i = 1; i < ec; ++i) {
        if (sdk_canon_path_cmp(entries[i - 1].name, entries[i].name) == 0) {
            free(entries); return SDK_ERR_DATA;
        }
    }
    sdk_buf pay;
    sdk_buf_init(&pay);
    for (size_t i = 0; i < ec; ++i) {
        uint16_t nl = (uint16_t)strlen(entries[i].name);
        sdk_buf_append_u8(&pay, entries[i].entry_type);
        sdk_buf_append_u8(&pay, entries[i].file_flags);
        sdk_buf_append_u16le(&pay, nl);
        sdk_buf_append(&pay, entries[i].name, nl);
        sdk_buf_append(&pay, entries[i].oid, 32);
    }
    sdk_status st = SDK_ERR_NOMEM;
    if (!pay.failed)
        st = tv_write_object(r, TV_OBJ_TREE, pay.data, pay.len, nd->id);
    sdk_buf_free(&pay);
    free(entries);
    if (st != SDK_OK) return st;
    nd->has_id = 1;
    return SDK_OK;
}

sdk_status tv_build_tree_from_paths(tv_repo *r,
                                    const char *const *paths,
                                    const tv_oid *blobs,
                                    const uint8_t *flags,
                                    size_t count, tv_oid *out_root) {
    g_nodes = NULL; g_nnodes = 0; g_capnodes = 0;
    int rc = 0;
    sdk_status st = SDK_OK;
    int root = node_create("");
    if (root < 0) { st = SDK_ERR_NOMEM; goto done; }
    for (size_t i = 0; i < count; ++i) {
        /* split into components */
        const char *p = paths[i];
        size_t len = strlen(p);
        /* build directory chain */
        char curdir[SDK_LIMIT_VCS_PATH_BYTES];
        curdir[0] = '\0';
        size_t start = 0;
        size_t ci = 0;
        for (size_t j = 0; j <= len; ++j) {
            if (j == len || p[j] == '/') {
                if (j > start) {
                    char comp[256];
                    size_t cl = j - start;
                    if (cl > 255) { st = SDK_ERR_DATA; goto done; }
                    memcpy(comp, p + start, cl);
                    comp[cl] = '\0';
                    if (ci == 0) {
                        /* last component is the file */
                        if (j == len) {
                            if (add_file_child(curdir, comp, &blobs[i],
                                               flags ? flags[i] : 0) != 0) {
                                st = SDK_ERR_DATA; goto done;
                            }
                        } else {
                            if (add_dir_child(curdir, comp) < 0) {
                                st = SDK_ERR_DATA; goto done;
                            }
                            snprintf(curdir, sizeof curdir, "%s", comp);
                        }
                    } else {
                        if (j == len) {
                            if (add_file_child(curdir, comp, &blobs[i],
                                               flags ? flags[i] : 0) != 0) {
                                st = SDK_ERR_DATA; goto done;
                            }
                        } else {
                            if (add_dir_child(curdir, comp) < 0) {
                                st = SDK_ERR_DATA; goto done;
                            }
                            char nd[SDK_LIMIT_VCS_PATH_BYTES];
                            if (curdir[0] == '\0')
                                snprintf(nd, sizeof nd, "%s", comp);
                            else
                                snprintf(nd, sizeof nd, "%s/%s", curdir, comp);
                            snprintf(curdir, sizeof curdir, "%s", nd);
                        }
                    }
                    ci++;
                }
                start = j + 1;
            }
        }
    }
    if (compute_node_ids(r, root) == SDK_OK)
        memcpy(out_root, g_nodes[root].id, 32);
    else
        st = SDK_ERR_DATA;

done:
    for (size_t i = 0; i < g_nnodes; ++i) free(g_nodes[i].dir);
    for (size_t i = 0; i < g_nnodes; ++i) free(g_nodes[i].children);
    free(g_nodes);
    g_nodes = NULL; g_nnodes = 0; g_capnodes = 0;
    (void)rc;
    return st;
}

typedef struct file_acc {
    tv_file_entry *arr;
    size_t n, cap;
} file_acc;

static int acc_add(file_acc *a, const char *path, uint8_t flags,
                   const tv_oid blob) {
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 64;
        tv_file_entry *ne = (tv_file_entry *)realloc(a->arr, nc * sizeof *ne);
        if (!ne) return 0;
        a->arr = ne; a->cap = nc;
    }
    tv_file_entry *e = &a->arr[a->n++];
    snprintf(e->path, sizeof e->path, "%s", path);
    e->file_flags = (uint8_t)(flags & 0x01u);
    memcpy(e->blob, blob, 32);
    return 1;
}

/* Grows the parallel oid/prefix stacks so at least one more frame fits. */
static int collect_stack_reserve(tv_oid **stack, char ***prefixes,
                                 size_t need, size_t *cap) {
    size_t nc;
    tv_oid *ns;
    char **np;
    if (need <= *cap) {
        return 1;
    }
    nc = *cap ? *cap * 2u : 64u;
    while (nc < need) {
        nc *= 2u;
    }
    ns = (tv_oid *)realloc(*stack, nc * sizeof *ns);
    if (ns == NULL) {
        return 0;
    }
    *stack = ns;
    np = (char **)realloc(*prefixes, nc * sizeof *np);
    if (np == NULL) {
        return 0;
    }
    *prefixes = np;
    *cap = nc;
    return 1;
}

sdk_status tv_tree_collect_files(tv_repo *r, const tv_oid tree_id,
                                 tv_file_entry **out, size_t *out_count) {
    file_acc acc;
    tv_oid *stack = NULL;
    char **prefixes = NULL;
    size_t sn = 0, scap = 0, i;
    char *prefix = NULL;
    sdk_status st = SDK_OK;

    acc.arr = NULL;
    acc.n = 0;
    acc.cap = 0;

    if (!collect_stack_reserve(&stack, &prefixes, 1u, &scap)) {
        st = SDK_ERR_NOMEM;
        goto done;
    }
    memcpy(stack[0], tree_id, 32);
    prefixes[0] = _strdup("");
    if (prefixes[0] == NULL) {
        st = SDK_ERR_NOMEM;
        goto done;
    }
    sn = 1;

    while (sn > 0) {
        tv_oid tid;
        tv_obj_type t;
        uint8_t *pay = NULL;
        size_t plen = 0;
        tv_tree_entry *ents = NULL;
        size_t ec = 0;
        sdk_status rs;

        --sn;
        memcpy(tid, stack[sn], 32);
        /* This frame owns `prefix` until the end of the iteration; the child
         * path formatting below reads it, so it must not be released early. */
        prefix = prefixes[sn];
        prefixes[sn] = NULL;

        rs = tv_read_object(r, tid, &t, &pay, &plen);
        if (rs != SDK_OK) {
            free(pay);
            st = rs;
            goto done;
        }
        if (t != TV_OBJ_TREE) {
            free(pay);
            st = SDK_ERR_DATA;
            goto done;
        }
        if (tv_tree_parse(pay, plen, &ents, &ec) != SDK_OK) {
            free(pay);
            st = SDK_ERR_DATA;
            goto done;
        }

        for (i = 0; i < ec; ++i) {
            char child[SDK_LIMIT_VCS_PATH_BYTES];
            int written;
            if (prefix[0] == '\0') {
                written = snprintf(child, sizeof child, "%s", ents[i].name);
            } else {
                written = snprintf(child, sizeof child, "%s/%s", prefix,
                                   ents[i].name);
            }
            if (written < 0 || (size_t)written >= sizeof child) {
                free(ents);
                free(pay);
                st = SDK_ERR_LIMIT;
                goto done;
            }
            if (ents[i].entry_type == 1) {
                if (!acc_add(&acc, child, ents[i].file_flags, ents[i].oid)) {
                    free(ents);
                    free(pay);
                    st = SDK_ERR_NOMEM;
                    goto done;
                }
                continue;
            }
            if (!collect_stack_reserve(&stack, &prefixes, sn + 1u, &scap)) {
                free(ents);
                free(pay);
                st = SDK_ERR_NOMEM;
                goto done;
            }
            memcpy(stack[sn], ents[i].oid, 32);
            prefixes[sn] = _strdup(child);
            if (prefixes[sn] == NULL) {
                free(ents);
                free(pay);
                st = SDK_ERR_NOMEM;
                goto done;
            }
            ++sn;
        }

        free(ents);
        free(pay);
        free(prefix);
        prefix = NULL;
    }

done:
    free(prefix);
    for (i = 0; i < sn; ++i) {
        free(prefixes[i]);
    }
    free(prefixes);
    free(stack);
    if (st == SDK_OK) {
        *out = acc.arr;
        *out_count = acc.n;
    } else {
        free(acc.arr);
        *out = NULL;
        *out_count = 0;
    }
    return st;
}

/* ------------------------------------------------------------------ */
/* Commits                                                             */
/* ------------------------------------------------------------------ */

sdk_status tv_create_commit(tv_repo *r, const tv_oid *parent,
                            const tv_oid *root_tree, const char *author,
                            const char *message, tv_oid out_id) {
    size_t alen = strlen(author);
    size_t mlen = strlen(message);
    sdk_buf pay;
    sdk_buf_init(&pay);
    sdk_buf_append_u8(&pay, 1);                       /* format_version */
    sdk_buf_append_u8(&pay, parent ? 1 : 0);          /* parent_count */
    if (parent) sdk_buf_append(&pay, parent, 32);
    sdk_buf_append(&pay, root_tree, 32);
    sdk_buf_append_i64le(&pay, sdk_now_epoch_ms());
    sdk_buf_append_u16le(&pay, (uint16_t)alen);
    sdk_buf_append(&pay, author, alen);
    sdk_buf_append_u32le(&pay, (uint32_t)mlen);
    sdk_buf_append(&pay, message, mlen);
    sdk_status st = SDK_ERR_NOMEM;
    if (!pay.failed)
        st = tv_write_object(r, TV_OBJ_COMMIT, pay.data, pay.len, out_id);
    sdk_buf_free(&pay);
    return st;
}

sdk_status tv_read_commit(tv_repo *r, const tv_oid id, tv_commit_info *out) {
    tv_obj_type t;
    uint8_t *pay = NULL;
    size_t plen = 0;
    sdk_status st = tv_read_object(r, id, &t, &pay, &plen);
    if (st != SDK_OK) return st;
    if (t != TV_OBJ_COMMIT) { free(pay); return SDK_ERR_DATA; }
    memset(out, 0, sizeof *out);
    sdk_rd rd;
    sdk_rd_init(&rd, pay, plen);
    uint8_t fv, pc;
    if (!sdk_rd_u8(&rd, &fv) || fv != 1) { free(pay); return SDK_ERR_DATA; }
    if (!sdk_rd_u8(&rd, &pc) || pc > 1) { free(pay); return SDK_ERR_DATA; }
    out->format_version = fv;
    out->parent_count = pc;
    if (pc == 1) {
        const uint8_t *pp = sdk_rd_take(&rd, 32);
        if (!pp) { free(pay); return SDK_ERR_DATA; }
        memcpy(out->parent, pp, 32);
    }
    const uint8_t *rt = sdk_rd_take(&rd, 32);
    if (!rt) { free(pay); return SDK_ERR_DATA; }
    memcpy(out->root_tree, rt, 32);
    int64_t ts;
    if (!sdk_rd_i64le(&rd, &ts)) { free(pay); return SDK_ERR_DATA; }
    out->timestamp_ms = ts;
    uint16_t al;
    if (!sdk_rd_u16le(&rd, &al)) { free(pay); return SDK_ERR_DATA; }
    if (al < 1 || al > SDK_LIMIT_VCS_AUTHOR_BYTES) { free(pay); return SDK_ERR_DATA; }
    const uint8_t *ap = sdk_rd_take(&rd, al);
    if (!ap) { free(pay); return SDK_ERR_DATA; }
    for (uint16_t i = 0; i < al; ++i) {
        if (ap[i] < 32 || ap[i] > 126) { free(pay); return SDK_ERR_DATA; }
    }
    if (ap[0] == ' ' || ap[al - 1] == ' ') { free(pay); return SDK_ERR_DATA; }
    memcpy(out->author, ap, al);
    out->author[al] = '\0';
    uint32_t ml;
    if (!sdk_rd_u32le(&rd, &ml)) { free(pay); return SDK_ERR_DATA; }
    if (ml < 1 || ml > SDK_LIMIT_VCS_MESSAGE_BYTES) { free(pay); return SDK_ERR_DATA; }
    const uint8_t *mp = sdk_rd_take(&rd, ml);
    if (!mp) { free(pay); return SDK_ERR_DATA; }
    int any_nonws = 0;
    for (uint32_t i = 0; i < ml; ++i) {
        if (mp[i] == 0) { free(pay); return SDK_ERR_DATA; }
        if (mp[i] != ' ' && mp[i] != '\t' && mp[i] != '\n' &&
            mp[i] != '\r' && mp[i] != '\v' && mp[i] != '\f')
            any_nonws = 1;
    }
    if (!any_nonws) { free(pay); return SDK_ERR_DATA; }
    if (!sdk_rd_at_end(&rd)) { free(pay); return SDK_ERR_DATA; }
    memcpy(out->message, mp, ml);
    out->message[ml] = '\0';
    free(pay);
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Index                                                               */
/* ------------------------------------------------------------------ */

void tv_index_init(tv_index *idx) { idx->entries = NULL; idx->count = 0; idx->cap = 0; }
void tv_index_free(tv_index *idx) {
    free(idx->entries);
    idx->entries = NULL; idx->count = 0; idx->cap = 0;
}

tv_index_entry *tv_index_find(tv_index *idx, const char *path) {
    for (size_t i = 0; i < idx->count; ++i)
        if (strcmp(idx->entries[i].path, path) == 0) return &idx->entries[i];
    return NULL;
}

static int idx_entry_cmp(const void *a, const void *b) {
    const tv_index_entry *x = (const tv_index_entry *)a;
    const tv_index_entry *y = (const tv_index_entry *)b;
    return sdk_canon_path_cmp(x->path, y->path);
}

tv_index_entry *tv_index_upsert(tv_index *idx, const tv_index_entry *e) {
    for (size_t i = 0; i < idx->count; ++i) {
        if (strcmp(idx->entries[i].path, e->path) == 0) {
            idx->entries[i] = *e;
            return &idx->entries[i];
        }
    }
    if (idx->count == idx->cap) {
        size_t nc = idx->cap ? idx->cap * 2 : 32;
        tv_index_entry *ne = (tv_index_entry *)realloc(idx->entries, nc * sizeof *ne);
        if (!ne) return NULL;
        idx->entries = ne; idx->cap = nc;
    }
    idx->entries[idx->count] = *e;
    return &idx->entries[idx->count++];
}

void tv_index_remove(tv_index *idx, const char *path) {
    for (size_t i = 0; i < idx->count; ++i) {
        if (strcmp(idx->entries[i].path, path) == 0) {
            for (size_t j = i; j + 1 < idx->count; ++j)
                idx->entries[j] = idx->entries[j + 1];
            idx->count--;
            return;
        }
    }
}

sdk_status tv_index_load(tv_repo *r, tv_index *idx) {
    tv_index_init(idx);
    wchar_t *p = tv_join(r->meta, L"index");
    if (!p) return SDK_ERR_NOMEM;
    uint8_t *data = NULL;
    size_t len = 0;
    uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(p, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
    free(p);
    if (st == SDK_ERR_NOT_FOUND) return SDK_OK; /* empty repo, no index yet */
    if (st != SDK_OK) { g_stage = "index_read"; g_win32 = we; return st; }
    /* 8 magic + 4 entry_count + entries + 4 trailing crc32 */
    if (len < 16 || memcmp(data, "TVCSIDX1", 8) != 0) { free(data); return SDK_ERR_DATA; }
    uint32_t count = sdk_get_u32le(data + 8);
    sdk_rd rd;
    sdk_rd_init(&rd, data + 12, len - 16);
    char last[SDK_LIMIT_VCS_PATH_BYTES];
    int have_last = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t pl;
        if (!sdk_rd_u16le(&rd, &pl)) { free(data); return SDK_ERR_DATA; }
        if (pl < 1 || pl >= SDK_LIMIT_VCS_PATH_BYTES) { free(data); return SDK_ERR_DATA; }
        const uint8_t *pp = sdk_rd_take(&rd, pl);
        if (!pp) { free(data); return SDK_ERR_DATA; }
        for (uint16_t k = 0; k < pl; ++k)
            if (pp[k] == 0 || pp[k] == '\\' || pp[k] < 0x20) { free(data); return SDK_ERR_DATA; }
        uint8_t ff, ss;
        if (!sdk_rd_u8(&rd, &ff)) { free(data); return SDK_ERR_DATA; }
        if (!sdk_rd_u8(&rd, &ss)) { free(data); return SDK_ERR_DATA; }
        if (ff & 0xFEu) { free(data); return SDK_ERR_DATA; }
        if (ss > 1) { free(data); return SDK_ERR_DATA; }
        const uint8_t *op = sdk_rd_take(&rd, 32);
        if (!op) { free(data); return SDK_ERR_DATA; }
        uint64_t sz; int64_t mt;
        if (!sdk_rd_u64le(&rd, &sz)) { free(data); return SDK_ERR_DATA; }
        if (!sdk_rd_i64le(&rd, &mt)) { free(data); return SDK_ERR_DATA; }
        if (idx->count == idx->cap) {
            size_t nc = idx->cap ? idx->cap * 2 : 32;
            tv_index_entry *ne = (tv_index_entry *)realloc(idx->entries, nc * sizeof *ne);
            if (!ne) { free(data); return SDK_ERR_NOMEM; }
            idx->entries = ne; idx->cap = nc;
        }
        tv_index_entry *e = &idx->entries[idx->count++];
        memcpy(e->path, pp, pl);
        e->path[pl] = '\0';
        if (sdk_path_relative_check(e->path, pl, SDK_LIMIT_LOCSTAT_PATH_DEPTH) != SDK_PATH_OK) {
            free(data); return SDK_ERR_DATA;
        }
        if (have_last && sdk_canon_path_cmp(e->path, last) <= 0) {
            free(data); return SDK_ERR_DATA;
        }
        memcpy(last, e->path, pl + 1); have_last = 1;
        e->file_flags = ff;
        e->stage_state = ss;
        memcpy(e->blob, op, 32);
        if (ss == 1) memset(e->blob, 0, 32);
        e->size = sz;
        e->mtime_100ns = mt;
    }
    if (!sdk_rd_at_end(&rd)) { free(data); return SDK_ERR_DATA; }
    /* crc over everything before the trailing crc */
    if (len < 4) { free(data); return SDK_ERR_DATA; }
    uint32_t stored_crc = sdk_get_u32le(data + len - 4);
    uint32_t calc_crc = sdk_crc32(data, len - 4);
    if (stored_crc != calc_crc) { free(data); return SDK_ERR_DATA; }
    free(data);
    return SDK_OK;
}

sdk_status tv_index_save(tv_repo *r, const tv_index *idx) {
    tv_index tmp = *idx;
    qsort(tmp.entries, tmp.count, sizeof *tmp.entries, idx_entry_cmp);
    sdk_buf b;
    sdk_buf_init(&b);
    sdk_buf_append(&b, "TVCSIDX1", 8);
    sdk_buf_append_u32le(&b, (uint32_t)tmp.count);
    for (size_t i = 0; i < tmp.count; ++i) {
        const tv_index_entry *e = &tmp.entries[i];
        uint16_t pl = (uint16_t)strlen(e->path);
        sdk_buf_append_u16le(&b, pl);
        sdk_buf_append(&b, e->path, pl);
        sdk_buf_append_u8(&b, e->file_flags & 0x01u);
        sdk_buf_append_u8(&b, e->stage_state);
        sdk_buf_append(&b, e->blob, 32);
        sdk_buf_append_u64le(&b, e->size);
        sdk_buf_append_i64le(&b, e->mtime_100ns);
    }
    uint32_t crc = sdk_crc32(b.data, b.len);
    sdk_buf_append_u32le(&b, crc);
    wchar_t *p = tv_join(r->meta, L"index");
    if (!p) { sdk_buf_free(&b); return SDK_ERR_NOMEM; }
    tv_ensure_parent(p);
    const char *stage = "index_write";
    uint32_t we = 0;
    sdk_status st = sdk_file_write_atomic_w(p, NULL, b.data, b.len, &stage, &we);
    if (st != SDK_OK) { g_stage = stage; g_win32 = we; }
    free(p);
    sdk_buf_free(&b);
    return st;
}

/* ------------------------------------------------------------------ */
/* Repository discovery and open                                       */
/* ------------------------------------------------------------------ */

sdk_status tv_find_repo_root(wchar_t **out_root) {
    wchar_t *cur = sdk_getcwd_w();
    if (!cur) return SDK_ERR_USAGE;
    for (;;) {
        wchar_t *probe = tv_join(cur, L".tinyvcs");
        if (probe) {
            sdk_fileinfo fi;
            if (sdk_stat_w(probe, &fi) == SDK_OK && fi.exists && fi.is_directory) {
                free(probe);
                *out_root = cur;
                return SDK_OK;
            }
            free(probe);
        }
        size_t len = wcslen(cur);
        while (len > 0 && (cur[len - 1] == L'\\' || cur[len - 1] == L'/'))
            cur[--len] = L'\0';
        if (len <= 3) { free(cur); return SDK_ERR_USAGE; }
        while (len > 0 && cur[len - 1] != L'\\' && cur[len - 1] != L'/')
            --len;
        if (len <= 3) { free(cur); return SDK_ERR_USAGE; }
        cur[len] = L'\0';
    }
}

sdk_status tv_open_repo(const wchar_t *root, tv_repo *r) {
    r->root = sdk_wcsdup_n(root, wcslen(root));
    r->meta = tv_join(root, L".tinyvcs");
    if (!r->root || !r->meta) { tv_close_repo(r); return SDK_ERR_NOMEM; }
    sdk_fileinfo fi;
    if (sdk_stat_w(r->meta, &fi) != SDK_OK || !fi.exists || !fi.is_directory) {
        g_stage = "open"; g_win32 = 0;
        tv_close_repo(r);
        return SDK_ERR_USAGE;
    }
    return SDK_OK;
}

void tv_close_repo(tv_repo *r) {
    free(r->root); r->root = NULL;
    free(r->meta); r->meta = NULL;
}

static sdk_status meta_path(tv_repo *r, const wchar_t *rel, wchar_t **out) {
    *out = tv_join(r->meta, rel);
    return *out ? SDK_OK : SDK_ERR_NOMEM;
}
static sdk_status root_path(tv_repo *r, const char *rel_utf8, wchar_t **out) {
    wchar_t *w = sdk_utf8_to_utf16(rel_utf8, strlen(rel_utf8), NULL);
    if (!w) return SDK_ERR_NOMEM;
    for (size_t i = 0; w[i]; ++i) if (w[i] == L'/') w[i] = L'\\';
    *out = tv_join(r->root, w);
    free(w);
    return *out ? SDK_OK : SDK_ERR_NOMEM;
}

sdk_status tv_init_repo(const wchar_t *root) {
    wchar_t *existing = NULL;
    if (tv_find_repo_root(&existing) == SDK_OK) {
        free(existing);
        g_stage = "init"; g_win32 = 0;
        return SDK_ERR_DATA; /* repository already exists (nested or duplicate) */
    }
    tv_repo r;
    r.root = sdk_wcsdup_n(root, wcslen(root));
    r.meta = tv_join(root, L".tinyvcs");
    if (!r.root || !r.meta) { tv_close_repo(&r); return SDK_ERR_NOMEM; }

    sdk_fileinfo fi;
    if (sdk_stat_w(r.meta, &fi) == SDK_OK && fi.exists) {
        tv_close_repo(&r); g_stage = "init"; g_win32 = 0;
        return SDK_ERR_DATA;
    }
    uint32_t we = 0;
    sdk_status st;
    const wchar_t *dirs[] = { L".tinyvcs", L".tinyvcs/objects",
                              L".tinyvcs/refs/heads", L".tinyvcs/locks",
                              L".tinyvcs/tmp" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; ++i) {
        wchar_t *d = tv_join(root, dirs[i]);
        if (!d) { st = SDK_ERR_NOMEM; goto done; }
        we = 0;
        st = sdk_mkdir_parents_w(d, &we);
        free(d);
        if (st != SDK_OK) { g_stage = "init_mkdir"; g_win32 = we; goto done; }
    }
    /* FORMAT */
    {
        const char *fmt = "tinyvcs 1\n";
        wchar_t *p = meta_path(&r, L"FORMAT", &p) == SDK_OK ? p : NULL;
        if (!p) { st = SDK_ERR_NOMEM; goto done; }
        st = sdk_file_write_all_w(p, fmt, strlen(fmt), &we);
        free(p);
        if (st != SDK_OK) { g_stage = "init_format"; g_win32 = we; goto done; }
    }
    /* HEAD */
    {
        const char *head = "ref: refs/heads/main\n";
        wchar_t *p;
        if (meta_path(&r, L"HEAD", &p) != SDK_OK) { st = SDK_ERR_NOMEM; goto done; }
        st = sdk_file_write_all_w(p, head, strlen(head), &we);
        free(p);
        if (st != SDK_OK) { g_stage = "init_head"; g_win32 = we; goto done; }
    }
    /* index (empty) */
    {
        sdk_buf b; sdk_buf_init(&b);
        sdk_buf_append(&b, "TVCSIDX1", 8);
        sdk_buf_append_u32le(&b, 0);
        uint32_t crc = sdk_crc32(b.data, b.len);
        sdk_buf_append_u32le(&b, crc);
        wchar_t *p;
        if (meta_path(&r, L"index", &p) != SDK_OK) { sdk_buf_free(&b); st = SDK_ERR_NOMEM; goto done; }
        tv_ensure_parent(p);
        st = sdk_file_write_atomic_w(p, NULL, b.data, b.len, NULL, &we);
        free(p); sdk_buf_free(&b);
        if (st != SDK_OK) { g_stage = "init_index"; g_win32 = we; goto done; }
    }
    /* refs/heads/main (unborn, empty) */
    {
        wchar_t *p;
        if (meta_path(&r, L"refs/heads/main", &p) != SDK_OK) { st = SDK_ERR_NOMEM; goto done; }
        tv_ensure_parent(p);
        st = sdk_file_write_all_w(p, "", 0, &we);
        free(p);
        if (st != SDK_OK) { g_stage = "init_refs"; g_win32 = we; goto done; }
    }
    st = SDK_OK;
done:
    tv_close_repo(&r);
    return st;
}

/* ------------------------------------------------------------------ */
/* References and HEAD                                                 */
/* ------------------------------------------------------------------ */

sdk_status tv_head_branch(tv_repo *r, char *out_branch, size_t cap) {
    wchar_t *p;
    if (meta_path(r, L"HEAD", &p) != SDK_OK) return SDK_ERR_NOMEM;
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(p, 4096, &data, &len, &we);
    free(p);
    if (st != SDK_OK) { g_stage = "head_read"; g_win32 = we; return st; }
    /* content: "ref: refs/heads/<branch>\n" */
    char *txt = (char *)malloc(len + 1);
    if (!txt) { free(data); return SDK_ERR_NOMEM; }
    memcpy(txt, data, len); txt[len] = '\0';
    free(data);
    const char *prefix = "ref: refs/heads/";
    if (strncmp(txt, prefix, strlen(prefix)) != 0) {
        free(txt); g_stage = "head_parse"; return SDK_ERR_DATA;
    }
    char *br = txt + strlen(prefix);
    /* strip trailing newline/CR */
    size_t bl = strlen(br);
    while (bl > 0 && (br[bl - 1] == '\n' || br[bl - 1] == '\r' ||
                      br[bl - 1] == ' ' || br[bl - 1] == '\t'))
        br[--bl] = '\0';
    if (tv_branch_name_check(br) != SDK_OK) { free(txt); g_stage = "head_branch"; return SDK_ERR_DATA; }
    if (bl + 1 > cap) { free(txt); return SDK_ERR_LIMIT; }
    memcpy(out_branch, br, bl + 1);
    free(txt);
    return SDK_OK;
}

sdk_status tv_read_ref(tv_repo *r, const char *branch, tv_oid *out_commit,
                       int *unborn) {
    wchar_t *p;
    if (meta_path(r, L"refs/heads", &p) != SDK_OK) return SDK_ERR_NOMEM;
    wchar_t *full = tv_join(p, sdk_utf8_to_utf16(branch, strlen(branch), NULL));
    free(p);
    if (!full) return SDK_ERR_NOMEM;
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(full, 4096, &data, &len, &we);
    free(full);
    if (st == SDK_ERR_NOT_FOUND) { memset(out_commit, 0, 32); *unborn = 1; return SDK_OK; }
    if (st != SDK_OK) { g_stage = "ref_read"; g_win32 = we; return st; }
    /* trim */
    char *txt = (char *)malloc(len + 1);
    if (!txt) { free(data); return SDK_ERR_NOMEM; }
    memcpy(txt, data, len); txt[len] = '\0';
    free(data);
    size_t bl = strlen(txt);
    while (bl > 0 && (txt[bl - 1] == '\n' || txt[bl - 1] == '\r' ||
                      txt[bl - 1] == ' ' || txt[bl - 1] == '\t'))
        txt[--bl] = '\0';
    if (bl == 0) { free(txt); memset(out_commit, 0, 32); *unborn = 1; return SDK_OK; }
    if (bl != 64 || !tv_oid_from_hex(txt, *out_commit)) {
        free(txt); g_stage = "ref_parse"; return SDK_ERR_DATA;
    }
    free(txt);
    *unborn = 0;
    return SDK_OK;
}

static sdk_status ref_lock_path(tv_repo *r, const char *branch, wchar_t **out) {
    /* sanitise branch name into a safe lock filename */
    char san[SDK_LIMIT_BRANCH_NAME_BYTES * 2 + 16];
    size_t j = 0;
    san[j++] = 'r'; san[j++] = 'e'; san[j++] = 'f'; san[j++] = '_';
    for (size_t i = 0; branch[i] && j + 1 < sizeof san; ++i) {
        char c = branch[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
            san[j++] = c;
        else
            san[j++] = '_';
    }
    san[j] = '\0';
    char full[SDK_LIMIT_BRANCH_NAME_BYTES * 2 + 32];
    snprintf(full, sizeof full, "locks/%s.lock", san);
    return meta_path(r, sdk_utf8_to_utf16(full, strlen(full), NULL), out);
}

sdk_status tv_write_ref(tv_repo *r, const char *branch, const tv_oid *commit) {
    char hex[65];
    tv_oid_hex(*commit, hex);
    char buf[80];
    int n = snprintf(buf, sizeof buf, "%s\n", hex);

    wchar_t *lock;
    if (ref_lock_path(r, branch, &lock) != SDK_OK) return SDK_ERR_NOMEM;
    sdk_lock lk;
    uint32_t we = 0;
    sdk_status st = sdk_lock_acquire_w(lock, "ref_update", &lk, &we);
    if (st != SDK_OK) {
        free(lock);
        g_stage = "ref_lock"; g_win32 = we;
        return st; /* BUSY or IO */
    }
    wchar_t *p;
    if (meta_path(r, L"refs/heads", &p) != SDK_OK) {
        sdk_lock_release(&lk, NULL); free(lock);
        return SDK_ERR_NOMEM;
    }
    wchar_t *full = tv_join(p, sdk_utf8_to_utf16(branch, strlen(branch), NULL));
    free(p);
    if (!full) { sdk_lock_release(&lk, NULL); free(lock); return SDK_ERR_NOMEM; }
    tv_ensure_parent(full);
    const char *stage = "ref_write";
    st = sdk_file_write_atomic_w(full, NULL, buf, (size_t)n, &stage, &we);
    if (st != SDK_OK) { g_stage = stage; g_win32 = we; }
    uint32_t re = 0;
    sdk_lock_release(&lk, &re);
    free(full); free(lock);
    return st;
}

sdk_status tv_branch_exists(tv_repo *r, const char *branch, int *exists) {
    wchar_t *p;
    if (meta_path(r, L"refs/heads", &p) != SDK_OK) return SDK_ERR_NOMEM;
    wchar_t *full = tv_join(p, sdk_utf8_to_utf16(branch, strlen(branch), NULL));
    free(p);
    if (!full) return SDK_ERR_NOMEM;
    sdk_fileinfo fi;
    sdk_status st = sdk_stat_w(full, &fi);
    free(full);
    *exists = (st == SDK_OK && fi.exists && !fi.is_directory);
    return SDK_OK;
}

sdk_status tv_branch_name_check(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n > SDK_LIMIT_BRANCH_NAME_BYTES) return SDK_ERR_DATA;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return SDK_ERR_DATA;
    if (name[0] == '/' || name[n - 1] == '/') return SDK_ERR_DATA;
    for (size_t i = 0; i + 1 < n; ++i)
        if (name[i] == '/' && name[i + 1] == '/') return SDK_ERR_DATA;
    for (size_t i = 0; i < n; ++i) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/'))
            return SDK_ERR_DATA;
    }
    return SDK_OK;
}

sdk_status tv_create_branch(tv_repo *r, const char *branch, const tv_oid *commit) {
    sdk_status st = tv_branch_name_check(branch);
    if (st != SDK_OK) { g_stage = "branch_name"; return st; }
    int exists = 0;
    if (tv_branch_exists(r, branch, &exists) != SDK_OK) return SDK_ERR_IO;
    if (exists) { g_stage = "branch_exists"; return SDK_ERR_EXISTS; }
    return tv_write_ref(r, branch, commit);
}

sdk_status tv_list_branches(tv_repo *r, char ***out_names, size_t *out_count,
                            char *out_current, size_t current_cap) {
    *out_names = NULL; *out_count = 0;
    if (out_current) out_current[0] = '\0';
    wchar_t *p;
    if (meta_path(r, L"refs/heads", &p) != SDK_OK) return SDK_ERR_NOMEM;
    sdk_dirlist dl;
    uint32_t we = 0;
    sdk_status st = sdk_dir_list_w(p, &dl, NULL, &we);
    free(p);
    if (st != SDK_OK) { g_stage = "branch_list"; g_win32 = we; return st; }
    size_t cap = 0;
    for (size_t i = 0; i < dl.count; ++i) {
        if (dl.items[i].info.is_directory) continue;
        char *nm = dl.items[i].name_u8;
        if (!nm) continue;
        char **nn = (char **)realloc(*out_names, (*out_count + 1) * sizeof(char *));
        if (!nn) { sdk_dirlist_free(&dl); goto nomem; }
        *out_names = nn;
        (*out_names)[*out_count] = _strdup(nm);
        if (!(*out_names)[*out_count]) { sdk_dirlist_free(&dl); goto nomem; }
        ++*out_count; (void)cap;
    }
    /* sort bytewise ascending */
    for (size_t i = 0; i + 1 < *out_count; ++i)
        for (size_t j = i + 1; j < *out_count; ++j)
            if (strcmp((*out_names)[i], (*out_names)[j]) > 0) {
                char *t = (*out_names)[i]; (*out_names)[i] = (*out_names)[j];
                (*out_names)[j] = t;
            }
    if (out_current) {
        char br[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        if (tv_head_branch(r, br, sizeof br) == SDK_OK) {
            if (strlen(br) + 1 <= current_cap)
                memcpy(out_current, br, strlen(br) + 1);
        }
    }
    sdk_dirlist_free(&dl);
    return SDK_OK;
nomem:
    for (size_t i = 0; i < *out_count; ++i) free((*out_names)[i]);
    free(*out_names); *out_names = NULL; *out_count = 0;
    sdk_dirlist_free(&dl);
    return SDK_ERR_NOMEM;
}

void tv_branch_list_free(char **names, size_t count) {
    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
}

sdk_status tv_resolve_commit(tv_repo *r, const char *spec, tv_oid *out) {
    size_t n = strlen(spec);
    if (n == 64) {
        int allhex = 1;
        for (size_t i = 0; i < 64; ++i) {
            char c = spec[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) { allhex = 0; break; }
        }
        if (allhex && tv_oid_from_hex(spec, *out)) return SDK_OK;
    }
    if (strcmp(spec, "HEAD") == 0) {
        char br[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        if (tv_head_branch(r, br, sizeof br) != SDK_OK) return SDK_ERR_DATA;
        int unborn = 0;
        sdk_status st = tv_read_ref(r, br, out, &unborn);
        if (st == SDK_OK && unborn) { memset(out, 0, 32); return SDK_ERR_DATA; }
        return st;
    }
    /* treat as branch name */
    int unborn = 0;
    return tv_read_ref(r, spec, out, &unborn);
}

/* ------------------------------------------------------------------ */
/* Ignore rules                                                        */
/* ------------------------------------------------------------------ */

void tv_ignore_init(tv_ignore *ig) { ig->patterns = NULL; ig->count = 0; ig->cap = 0; }
void tv_ignore_free(tv_ignore *ig) {
    for (size_t i = 0; i < ig->count; ++i) free(ig->patterns[i]);
    free(ig->patterns);
    ig->patterns = NULL; ig->count = 0; ig->cap = 0;
}

sdk_status tv_ignore_load(tv_repo *r, tv_ignore *ig) {
    tv_ignore_init(ig);
    wchar_t *p;
    if (root_path(r, ".tinyignore", &p) != SDK_OK) return SDK_ERR_NOMEM;
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(p, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
    free(p);
    if (st == SDK_ERR_NOT_FOUND) return SDK_OK;
    if (st != SDK_OK) { g_stage = "ignore_read"; g_win32 = we; return st; }
    /* split into lines */
    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && data[i] != '\n' && data[i] != '\r') ++i;
        size_t end = i;
        /* strip a single trailing \r if present */
        if (end > start && data[end - 1] == '\r') --end;
        /* skip if first non-space char is '#' (comment) or all whitespace */
        int is_comment = 0, all_ws = 1;
        for (size_t m = start; m < end; ++m) {
            if (data[m] == '#') { is_comment = (m == start); break; }
            if (data[m] != ' ' && data[m] != '\t') all_ws = 0;
        }
        if (!is_comment && !all_ws) {
            size_t plen = end - start;
            if (ig->count == ig->cap) {
                size_t nc = ig->cap ? ig->cap * 2 : 16;
                char **np = (char **)realloc(ig->patterns, nc * sizeof(char *));
                if (!np) { free(data); return SDK_ERR_NOMEM; }
                ig->patterns = np; ig->cap = nc;
            }
            char *pat = (char *)malloc(plen + 1);
            if (!pat) { free(data); return SDK_ERR_NOMEM; }
            memcpy(pat, data + start, plen);
            pat[plen] = '\0';
            ig->patterns[ig->count++] = pat;
        }
        while (i < len && (data[i] == '\n' || data[i] == '\r')) ++i;
    }
    free(data);
    return SDK_OK;
}

int tv_is_ignored(const tv_ignore *ig, const char *rel_path, int is_dir) {
    for (size_t i = 0; i < ig->count; ++i) {
        if (sdk_ignore_match(ig->patterns[i], rel_path, is_dir)) return 1;
    }
    return 0;
}

int tv_is_meta_name(const char *component) {
    return strcmp(component, ".tinyvcs") == 0;
}

/* ------------------------------------------------------------------ */
/* Working-tree enumeration                                            */
/* ------------------------------------------------------------------ */

void tv_workfile_list_free(tv_workfile *list, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        free(list[i].rel);
        free(list[i].wpath);
    }
    free(list);
}

sdk_status tv_collect_working(tv_repo *r, const tv_ignore *ig,
                              tv_workfile **out, size_t *out_count) {
    tv_workfile *list = NULL;
    size_t n = 0, cap = 0;

    typedef struct { wchar_t *w; char *rel; } frame;
    frame *stack = NULL;
    size_t sn = 0, scap = 0;

    sdk_status st = SDK_OK;
    if (sn == scap) {
        scap = scap ? scap * 2 : 64;
        frame *ns = (frame *)realloc(stack, scap * sizeof *ns);
        if (!ns) { st = SDK_ERR_NOMEM; goto done; }
        stack = ns;
    }
    stack[sn].w = sdk_wcsdup_n(r->root, wcslen(r->root));
    stack[sn].rel = _strdup("");
    if (!stack[sn].w || !stack[sn].rel) { st = SDK_ERR_NOMEM; goto done; }
    sn++;

    while (sn > 0) {
        --sn;
        wchar_t *dir_w = stack[sn].w;
        char *dir_rel = stack[sn].rel;
        sdk_dirlist dl;
        uint32_t we = 0;
        sdk_status ds = sdk_dir_list_w(dir_w, &dl, NULL, &we);
        if (ds != SDK_OK) {
            free(dir_w); free(dir_rel);
            if (ds == SDK_ERR_NOT_FOUND) continue;
            g_stage = "walk"; g_win32 = we; st = ds; goto done;
        }
        for (size_t i = 0; i < dl.count; ++i) {
            sdk_dirent *e = &dl.items[i];
            if (!e->name_u8) continue;
            if (tv_is_meta_name(e->name_u8) && dir_rel[0] == '\0') continue;
            char child_rel[SDK_LIMIT_VCS_PATH_BYTES];
            if (dir_rel[0] == '\0')
                snprintf(child_rel, sizeof child_rel, "%s", e->name_u8);
            else
                snprintf(child_rel, sizeof child_rel, "%s/%s", dir_rel, e->name_u8);
            int is_dir = e->info.is_directory;
            if (tv_is_ignored(ig, child_rel, is_dir)) continue;
            wchar_t *cw = tv_join(dir_w, e->name_w);
            if (!cw) { sdk_dirlist_free(&dl); st = SDK_ERR_NOMEM; goto done; }
            if (is_dir && !e->info.is_reparse_point) {
                if (sn == scap) {
                    scap = scap ? scap * 2 : 64;
                    frame *nf = (frame *)realloc(stack, scap * sizeof *nf);
                    if (!nf) { free(cw); sdk_dirlist_free(&dl); st = SDK_ERR_NOMEM; goto done; }
                    stack = nf;
                }
                stack[sn].w = cw;
                stack[sn].rel = _strdup(child_rel);
                if (!stack[sn].rel) { free(cw); sdk_dirlist_free(&dl); st = SDK_ERR_NOMEM; goto done; }
                sn++;
            } else if (!is_dir && !e->info.is_reparse_point) {
                if (n == cap) {
                    size_t nc = cap ? cap * 2 : 64;
                    tv_workfile *nl = (tv_workfile *)realloc(list, nc * sizeof *nl);
                    if (!nl) { free(cw); sdk_dirlist_free(&dl); st = SDK_ERR_NOMEM; goto done; }
                    list = nl; cap = nc;
                }
                list[n].wpath = cw;
                list[n].rel = _strdup(child_rel);
                if (!list[n].rel) { sdk_dirlist_free(&dl); st = SDK_ERR_NOMEM; goto done; }
                n++;
            } else {
                free(cw); /* skip reparse points */
            }
        }
        sdk_dirlist_free(&dl);
        free(dir_w); free(dir_rel);
    }

done:
    for (size_t i = 0; i < sn; ++i) { free(stack[i].w); free(stack[i].rel); }
    free(stack);
    if (st == SDK_OK) { *out = list; *out_count = n; }
    else { tv_workfile_list_free(list, n); *out = NULL; *out_count = 0; }
    return st;
}

/* ------------------------------------------------------------------ */
/* Status                                                              */
/* ------------------------------------------------------------------ */

static const uint8_t *find_file_blob(const tv_file_entry *arr, size_t n,
                                     const char *path) {
    for (size_t i = 0; i < n; ++i)
        if (strcmp(arr[i].path, path) == 0) return arr[i].blob;
    return NULL;
}

static int strlist_push(char ***list, size_t *count, const char *s) {
    char *c = _strdup(s);
    if (!c) return 0;
    char **nl = (char **)realloc(*list, (*count + 1) * sizeof(char *));
    if (!nl) { free(c); return 0; }
    *list = nl;
    (*list)[*count] = c;
    ++*count;
    return 1;
}

static int in_index(const tv_index *idx, const char *path, int *stage_state) {
    tv_index_entry *e = tv_index_find((tv_index *)idx, path);
    if (!e) return 0;
    if (stage_state) *stage_state = e->stage_state;
    return 1;
}

void tv_status_free(tv_status_info *s) {
    char **lists[6] = { s->staged_added, s->staged_modified, s->staged_deleted,
                        s->unstaged_modified, s->unstaged_deleted, s->untracked };
    size_t *counts[6] = { &s->staged_added_n, &s->staged_modified_n,
                          &s->staged_deleted_n, &s->unstaged_modified_n,
                          &s->unstaged_deleted_n, &s->untracked_n };
    for (int i = 0; i < 6; ++i) {
        for (size_t j = 0; j < *counts[i]; ++j) free(lists[i][j]);
        free(lists[i]);
        lists[i] = NULL; *counts[i] = 0;
    }
}

sdk_status tv_status_collect(tv_repo *r, tv_status_info *out) {
    memset(out, 0, sizeof *out);
    tv_index idx;
    if (tv_index_load(r, &idx) != SDK_OK) { g_stage = "status_index"; return SDK_ERR_DATA; }
    if (tv_head_branch(r, out->branch, sizeof out->branch) != SDK_OK) {
        tv_index_free(&idx); g_stage = "status_head"; return SDK_ERR_DATA;
    }
    /* HEAD tree files */
    tv_file_entry *head_files = NULL;
    size_t head_n = 0;
    tv_oid head_commit; int unborn = 1;
    if (tv_read_ref(r, out->branch, &head_commit, &unborn) != SDK_OK) {
        tv_index_free(&idx); g_stage = "status_ref"; return SDK_ERR_DATA;
    }
    if (!unborn) {
        tv_commit_info ci;
        if (tv_read_commit(r, head_commit, &ci) != SDK_OK) {
            tv_index_free(&idx); g_stage = "status_commit"; return SDK_ERR_DATA;
        }
        if (tv_tree_collect_files(r, ci.root_tree, &head_files, &head_n) != SDK_OK) {
            tv_index_free(&idx); g_stage = "status_tree"; return SDK_ERR_DATA;
        }
    }
    /* working files */
    tv_ignore ig;
    if (tv_ignore_load(r, &ig) != SDK_OK) { tv_index_free(&idx); free(head_files); g_stage = "status_ignore"; return SDK_ERR_DATA; }
    tv_workfile *wf = NULL; size_t wn = 0;
    if (tv_collect_working(r, &ig, &wf, &wn) != SDK_OK) {
        tv_index_free(&idx); free(head_files); tv_ignore_free(&ig);
        g_stage = "status_walk"; return SDK_ERR_IO;
    }

    /* staged */
    for (size_t i = 0; i < idx.count; ++i) {
        tv_index_entry *e = &idx.entries[i];
        if (e->stage_state == 0) {
            const uint8_t *hb = find_file_blob(head_files, head_n, e->path);
            if (!hb) {
                if (!strlist_push(&out->staged_added, &out->staged_added_n, e->path)) { goto oom; }
            } else if (!tv_oid_equal(hb, e->blob)) {
                if (!strlist_push(&out->staged_modified, &out->staged_modified_n, e->path)) { goto oom; }
            }
        } else {
            if (!unborn && find_file_blob(head_files, head_n, e->path)) {
                if (!strlist_push(&out->staged_deleted, &out->staged_deleted_n, e->path)) { goto oom; }
            }
        }
    }
    /* unstaged + untracked */
    for (size_t i = 0; i < idx.count; ++i) {
        tv_index_entry *e = &idx.entries[i];
        if (e->stage_state != 0) continue;
        int found = 0;
        for (size_t k = 0; k < wn; ++k) {
            if (strcmp(wf[k].rel, e->path) == 0) { found = 1; break; }
        }
        if (!found) {
            if (!strlist_push(&out->unstaged_deleted, &out->unstaged_deleted_n, e->path)) { goto oom; }
            continue;
        }
        wchar_t *wpath = NULL;
        if (root_path(r, e->path, &wpath) != SDK_OK) { goto oom; }
        tv_oid wid;
        sdk_status bs = tv_file_blob_id(wpath, wid, NULL, NULL);
        free(wpath);
        if (bs != SDK_OK || !tv_oid_equal(wid, e->blob)) {
            if (!strlist_push(&out->unstaged_modified, &out->unstaged_modified_n, e->path)) { goto oom; }
        }
    }
    for (size_t k = 0; k < wn; ++k) {
        int stg = 0;
        if (in_index(&idx, wf[k].rel, &stg)) continue; /* tracked */
        if (!strlist_push(&out->untracked, &out->untracked_n, wf[k].rel)) { goto oom; }
    }

    tv_workfile_list_free(wf, wn);
    tv_ignore_free(&ig);
    free(head_files);
    tv_index_free(&idx);
    return SDK_OK;
oom:
    tv_workfile_list_free(wf, wn);
    tv_ignore_free(&ig);
    free(head_files);
    tv_index_free(&idx);
    tv_status_free(out);
    return SDK_ERR_NOMEM;
}

/* ------------------------------------------------------------------ */
/* Checkout apply (switch / reset --hard)                              */
/* ------------------------------------------------------------------ */

typedef struct backup {
    wchar_t *orig;
    wchar_t *backup;
    int      created;
} backup;

static sdk_status backup_make(const wchar_t *orig, const wchar_t *backup) {
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(orig, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
    if (st != SDK_OK) { g_stage = "backup_read"; g_win32 = we; return st; }
    st = sdk_file_write_all_w(backup, data, len, &we);
    free(data);
    if (st != SDK_OK) { g_stage = "backup_write"; g_win32 = we; }
    return st;
}

static sdk_status backup_restore(const wchar_t *backup, const wchar_t *orig) {
    uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
    sdk_status st = sdk_file_read_all_w(backup, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
    if (st != SDK_OK) { g_stage = "rollback_read"; g_win32 = we; return st; }
    const char *stage = "rollback_write";
    st = sdk_file_write_atomic_w(orig, NULL, data, len, &stage, &we);
    free(data);
    if (st != SDK_OK) { g_stage = stage; g_win32 = we; }
    return st;
}

sdk_status tv_apply_checkout(tv_repo *r, const tv_file_entry *desired,
                             size_t desired_n, int update_index) {
    backup *bk = NULL;
    size_t bn = 0, bcap = 0;
    int failed = 0;

    /* Load current index to know tracked files for deletion. */
    tv_index idx;
    if (tv_index_load(r, &idx) != SDK_OK) { g_stage = "checkout_index"; return SDK_ERR_DATA; }

    /* Apply desired files. */
    for (size_t i = 0; i < desired_n && !failed; ++i) {
        wchar_t *wpath = NULL;
        if (root_path(r, desired[i].path, &wpath) != SDK_OK) { failed = 1; break; }
        sdk_fileinfo fi;
        int exists = (sdk_stat_w(wpath, &fi) == SDK_OK && fi.exists);
        int created = 0;
        if (exists) {
            /* back it up before overwriting */
            if (bn == bcap) {
                size_t nc = bcap ? bcap * 2 : 32;
                backup *nb = (backup *)realloc(bk, nc * sizeof *nb);
                if (!nb) { free(wpath); failed = 1; break; }
                bk = nb; bcap = nc;
            }
            wchar_t *bpath = tv_join(r->meta, L"tmp");
            if (!bpath) { free(wpath); failed = 1; break; }
            unsigned char rb[8];
            sdk_random_bytes(rb, 8);
            char hex[17]; sdk_hex_encode(rb, 8, hex);
            wchar_t suff[32];
            _snwprintf_s(suff, sizeof suff / sizeof suff[0], _TRUNCATE, L"bk_%hs", hex);
            wchar_t *fullbk = tv_join(bpath, suff);
            free(bpath);
            if (!fullbk) { free(wpath); failed = 1; break; }
            tv_ensure_parent(fullbk);
            if (backup_make(wpath, fullbk) != SDK_OK) { free(wpath); free(fullbk); failed = 1; break; }
            bk[bn].orig = _wcsdup(wpath);
            bk[bn].backup = fullbk;
            bk[bn].created = 0;
            ++bn;
            /* wpath is retained for the write below and freed at end of loop. */
        } else {
            created = 1;
            tv_ensure_parent(wpath);
        }
        /* read blob payload */
        tv_obj_type t;
        uint8_t *pay = NULL; size_t plen = 0;
        sdk_status rs = tv_read_object(r, desired[i].blob, &t, &pay, &plen);
        if (rs != SDK_OK || t != TV_OBJ_BLOB) {
            free(wpath);
            if (rs == SDK_OK) free(pay);
            g_stage = "checkout_blob";
            if (rs == SDK_OK) rs = SDK_ERR_DATA;
            failed = 1; break;
        }
        const char *stage = "checkout_write";
        uint32_t we = 0;
        sdk_status ws = sdk_file_write_atomic_w(wpath, NULL, pay, plen, &stage, &we);
        free(pay);
        if (ws != SDK_OK) {
            free(wpath); g_stage = stage; g_win32 = we; failed = 1; break;
        }
        uint32_t re = 0;
        sdk_status ss = sdk_set_readonly_w(wpath, desired[i].file_flags & 0x01u ? 1 : 0, &re);
        if (ss != SDK_OK) {
            free(wpath); g_stage = "checkout_attr"; g_win32 = re; failed = 1; break;
        }
        free(wpath);
        (void)created;
    }

    /* Delete tracked files not in desired set. */
    if (!failed) {
        for (size_t i = 0; i < idx.count && !failed; ++i) {
            tv_index_entry *e = &idx.entries[i];
            if (e->stage_state != 0) continue;
            int in_desired = 0;
            for (size_t k = 0; k < desired_n; ++k)
                if (strcmp(desired[k].path, e->path) == 0) { in_desired = 1; break; }
            if (in_desired) continue;
            wchar_t *wpath = NULL;
            if (root_path(r, e->path, &wpath) != SDK_OK) { failed = 1; break; }
            sdk_fileinfo fi;
            if (sdk_stat_w(wpath, &fi) == SDK_OK && fi.exists) {
                if (bn == bcap) {
                    size_t nc = bcap ? bcap * 2 : 32;
                    backup *nb = (backup *)realloc(bk, nc * sizeof *nb);
                    if (!nb) { free(wpath); failed = 1; break; }
                    bk = nb; bcap = nc;
                }
                wchar_t *bpath = tv_join(r->meta, L"tmp");
                if (!bpath) { free(wpath); failed = 1; break; }
                unsigned char rb[8];
                sdk_random_bytes(rb, 8);
                char hex[17]; sdk_hex_encode(rb, 8, hex);
                wchar_t suff[32];
                _snwprintf_s(suff, sizeof suff / sizeof suff[0], _TRUNCATE, L"bk_%hs", hex);
                wchar_t *fullbk = tv_join(bpath, suff);
                free(bpath);
                if (!fullbk) { free(wpath); failed = 1; break; }
                tv_ensure_parent(fullbk);
                if (backup_make(wpath, fullbk) != SDK_OK) { free(wpath); free(fullbk); failed = 1; break; }
                bk[bn].orig = wpath; bk[bn].backup = fullbk; bk[bn].created = 0;
                wpath = NULL;
                ++bn;
                uint32_t we = 0;
                sdk_status ds = sdk_delete_file_w(bk[bn - 1].orig, &we);
                if (ds != SDK_OK) { g_stage = "checkout_delete"; g_win32 = we; failed = 1; break; }
            }
            free(wpath);
        }
    }

    if (failed) {
        /* Roll back: restore overwritten files, delete created files. */
        for (size_t i = 0; i < bn; ++i) {
            if (bk[i].created) {
                sdk_delete_file_w(bk[i].orig, NULL);
            } else {
                backup_restore(bk[i].backup, bk[i].orig);
            }
            sdk_delete_file_w(bk[i].backup, NULL);
            free(bk[i].orig);
            free(bk[i].backup);
        }
        free(bk);
        tv_index_free(&idx);
        return SDK_ERR_IO;
    }

    /* Discard backups. */
    for (size_t i = 0; i < bn; ++i) {
        sdk_delete_file_w(bk[i].backup, NULL);
        free(bk[i].orig);
        free(bk[i].backup);
    }
    free(bk);

    if (update_index) {
        tv_index newidx;
        tv_index_init(&newidx);
        for (size_t i = 0; i < desired_n; ++i) {
            tv_index_entry e;
            memset(&e, 0, sizeof e);
            snprintf(e.path, sizeof e.path, "%s", desired[i].path);
            e.path[sizeof e.path - 1] = '\0';
            e.file_flags = (uint8_t)(desired[i].file_flags & 0x01u);
            e.stage_state = 0;
            memcpy(e.blob, desired[i].blob, 32);
            wchar_t *wpath = NULL;
            if (root_path(r, desired[i].path, &wpath) == SDK_OK) {
                sdk_fileinfo fi;
                if (sdk_stat_w(wpath, &fi) == SDK_OK) {
                    e.size = fi.size;
                    e.mtime_100ns = fi.mtime_100ns;
                }
                free(wpath);
            }
            tv_index_upsert(&newidx, &e);
        }
        if (tv_index_save(r, &newidx) != SDK_OK) {
            tv_index_free(&newidx); tv_index_free(&idx);
            g_stage = "checkout_index_save"; return SDK_ERR_IO;
        }
        tv_index_free(&newidx);
    }

    tv_index_free(&idx);
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Verify                                                               */
/* ------------------------------------------------------------------ */

static int oid_in_set(const tv_oid *set, size_t n, const tv_oid *id) {
    for (size_t i = 0; i < n; ++i) if (tv_oid_equal(set[i], *id)) return 1;
    return 0;
}

/* Walks .tinyvcs/objects depth-first.  Every regular file must sit at
 * objects/<first-2-hex>/<remaining-62-hex>; the object id is taken from the
 * path (never from the payload) and the stored object is then decoded and
 * re-hashed by tv_read_object, which also validates the CRC-32 framing and
 * the LZSS payload.  Any file that fails those checks counts as corrupt. */
static sdk_status verify_walk_objects(tv_repo *r, tv_verify_result *res,
                                      tv_oid **all_ids, size_t *all_n) {
    wchar_t *objdir = NULL;
    wchar_t **stack = NULL;
    size_t sn = 0, scap = 64;
    sdk_status st = SDK_OK;

    if (meta_path(r, L"objects", &objdir) != SDK_OK) {
        return SDK_ERR_NOMEM;
    }
    stack = (wchar_t **)malloc(scap * sizeof *stack);
    if (stack == NULL) {
        free(objdir);
        return SDK_ERR_NOMEM;
    }
    /* The root frame owns a copy so `objdir` stays valid for prefix math. */
    stack[sn] = sdk_wcsdup_n(objdir, wcslen(objdir));
    if (stack[sn] == NULL) {
        free(stack);
        free(objdir);
        return SDK_ERR_NOMEM;
    }
    ++sn;

    while (sn > 0 && st == SDK_OK) {
        wchar_t *dir = stack[--sn];
        sdk_dirlist dl;
        uint32_t we = 0;
        sdk_status ds = sdk_dir_list_w(dir, &dl, NULL, &we);
        size_t i;

        if (ds != SDK_OK) {
            free(dir);
            if (ds == SDK_ERR_NOT_FOUND) {
                continue;
            }
            tv_error_set("verify_scan", we);
            st = ds;
            break;
        }

        for (i = 0; i < dl.count; ++i) {
            wchar_t *cw = tv_join(dir, dl.items[i].name_w);
            if (cw == NULL) {
                st = SDK_ERR_NOMEM;
                break;
            }
            if (dl.items[i].info.is_directory) {
                if (sn == scap) {
                    size_t nc = scap * 2u;
                    wchar_t **ns = (wchar_t **)realloc(stack,
                                                       nc * sizeof *ns);
                    if (ns == NULL) {
                        free(cw);
                        st = SDK_ERR_NOMEM;
                        break;
                    }
                    stack = ns;
                    scap = nc;
                }
                stack[sn++] = cw;
                continue;
            }

            /* Regular file: recover the object id from its path. */
            res->scanned++;
            {
                size_t ol = wcslen(objdir);
                size_t cl = wcslen(cw);
                char *rel = NULL;
                char hex[65];
                tv_oid id;

                if (cl > ol && (cw[ol] == L'\\' || cw[ol] == L'/')) {
                    rel = sdk_utf16_to_utf8(cw + ol + 1, cl - ol - 1, NULL);
                }
                /* Strip the separator: "aa/bbbb..." -> "aabbbb...". */
                if (rel != NULL && strlen(rel) == 65u &&
                    (rel[2] == '/' || rel[2] == '\\')) {
                    memcpy(hex, rel, 2u);
                    memcpy(hex + 2, rel + 3, 62u);
                    hex[64] = '\0';
                } else {
                    hex[0] = '\0';
                }

                if (hex[0] != '\0' && tv_oid_from_hex(hex, id)) {
                    tv_obj_type t;
                    uint8_t *pay = NULL;
                    size_t plen = 0;
                    sdk_status rs = tv_read_object(r, id, &t, &pay, &plen);
                    if (rs == SDK_OK) {
                        tv_oid *ns = (tv_oid *)realloc(
                            *all_ids, (*all_n + 1u) * sizeof(tv_oid));
                        if (ns == NULL) {
                            free(pay);
                            free(rel);
                            free(cw);
                            st = SDK_ERR_NOMEM;
                            break;
                        }
                        *all_ids = ns;
                        memcpy((*all_ids)[*all_n], id, 32);
                        (*all_n)++;
                        free(pay);
                    } else {
                        res->corrupt++;
                        res->ok = 0;
                        fprintf(stderr,
                                "tinyvcs verify: object %s unreadable or "
                                "corrupt\n", hex);
                    }
                } else {
                    res->malformed++;
                    res->ok = 0;
                    fprintf(stderr,
                            "tinyvcs verify: object store contains a file that "
                            "is not a canonical object path\n");
                }
                free(rel);
            }
            free(cw);
        }

        sdk_dirlist_free(&dl);
        free(dir);
    }

    while (sn > 0) {
        free(stack[--sn]);
    }
    free(stack);
    free(objdir);
    return st;
}

static sdk_status verify_reachable(tv_repo *r, tv_verify_result *res,
                                   const tv_oid *roots, size_t root_n,
                                   tv_oid **visited, size_t *vis_n) {
    tv_oid *stack = NULL; size_t sn = 0, scap = 0;
    sdk_status st = SDK_OK;
    for (size_t i = 0; i < root_n; ++i) {
        if (sn == scap) { scap = scap?scap*2:64; tv_oid *ns=(tv_oid*)realloc(stack,scap*sizeof(tv_oid)); if(!ns){st=SDK_ERR_NOMEM;goto done;} stack=ns; }
        memcpy(stack[sn++], roots[i], 32);
    }
    while (sn > 0) {
        --sn;
        tv_oid id; memcpy(id, stack[sn], 32);
        if (oid_in_set(*visited, *vis_n, &id)) continue;
        tv_oid *nv = (tv_oid *)realloc(*visited, (*vis_n + 1) * sizeof(tv_oid));
        if (!nv) { st = SDK_ERR_NOMEM; goto done; }
        *visited = nv; memcpy((*visited)[*vis_n], id, 32); (*vis_n)++;
        res->reachable++;
        tv_obj_type t; uint8_t *pay; size_t plen;
        sdk_status rs = tv_read_object(r, id, &t, &pay, &plen);
        if (rs != SDK_OK) {
            if (rs == SDK_ERR_NOT_FOUND) res->missing++;
            else res->corrupt++;
            res->ok = 0;
            continue;
        }
        if (t == TV_OBJ_COMMIT) {
            tv_commit_info ci;
            if (tv_read_commit(r, id, &ci) != SDK_OK) { free(pay); res->corrupt++; res->ok=0; continue; }
            if (ci.parent_count == 1) {
                if (sn == scap) { scap=scap?scap*2:64; tv_oid *ns=(tv_oid*)realloc(stack,scap*sizeof(tv_oid)); if(!ns){free(pay);st=SDK_ERR_NOMEM;goto done;} stack=ns; }
                memcpy(stack[sn++], ci.parent, 32);
            }
            if (sn == scap) { scap=scap?scap*2:64; tv_oid *ns=(tv_oid*)realloc(stack,scap*sizeof(tv_oid)); if(!ns){free(pay);st=SDK_ERR_NOMEM;goto done;} stack=ns; }
            memcpy(stack[sn++], ci.root_tree, 32);
            free(pay);
        } else if (t == TV_OBJ_TREE) {
            tv_tree_entry *ents; size_t ec;
            if (tv_tree_parse(pay, plen, &ents, &ec) != SDK_OK) { free(pay); res->corrupt++; res->ok=0; continue; }
            for (size_t i = 0; i < ec; ++i) {
                if (sn == scap) { scap=scap?scap*2:64; tv_oid *ns=(tv_oid*)realloc(stack,scap*sizeof(tv_oid)); if(!ns){free(ents);free(pay);st=SDK_ERR_NOMEM;goto done;} stack=ns; }
                memcpy(stack[sn++], ents[i].oid, 32);
            }
            free(ents); free(pay);
        } else {
            free(pay);
        }
    }
done:
    free(stack);
    return st;
}

sdk_status tv_verify(tv_repo *r, tv_verify_result *out) {
    memset(out, 0, sizeof *out);
    out->ok = 1;

    /* FORMAT */
    {
        wchar_t *p; sdk_status st;
        if (meta_path(r, L"FORMAT", &p) != SDK_OK) return SDK_ERR_NOMEM;
        uint8_t *data; size_t len; uint32_t we=0;
        st = sdk_file_read_all_w(p, 64, &data, &len, &we);
        free(p);
        if (st != SDK_OK || len != 10 || memcmp(data, "tinyvcs 1\n", 10) != 0) {
            out->malformed++; out->ok = 0;
            fprintf(stderr, "tinyvcs verify: FORMAT missing or invalid\n");
        }
        free(data);
    }
    /* HEAD */
    {
        char br[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        if (tv_head_branch(r, br, sizeof br) != SDK_OK) {
            out->malformed++; out->ok = 0;
            fprintf(stderr, "tinyvcs verify: HEAD missing or invalid\n");
        }
    }
    /* refs */
    char **branches = NULL; size_t bc = 0;
    char cur[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    if (tv_list_branches(r, &branches, &bc, cur, sizeof cur) != SDK_OK) {
        out->malformed++; out->ok = 0;
        fprintf(stderr, "tinyvcs verify: cannot list branches\n");
    }
    tv_oid *roots = NULL; size_t root_n = 0;
    for (size_t i = 0; i < bc; ++i) {
        tv_oid c; int unborn;
        if (tv_read_ref(r, branches[i], &c, &unborn) != SDK_OK) {
            out->malformed++; out->ok = 0;
            fprintf(stderr, "tinyvcs verify: branch %s unreadable\n", branches[i]);
            continue;
        }
        if (unborn) continue;
        tv_oid *nr = (tv_oid *)realloc(roots, (root_n + 1) * sizeof(tv_oid));
        if (!nr) { out->malformed++; break; }
        roots = nr; memcpy(roots[root_n], c, 32); root_n++;
    }
    tv_branch_list_free(branches, bc);

    /* index framing/CRC + referenced blob existence */
    {
        tv_index idx;
        if (tv_index_load(r, &idx) != SDK_OK) {
            out->malformed++; out->ok = 0;
            fprintf(stderr, "tinyvcs verify: index malformed\n");
        } else {
            for (size_t i = 0; i < idx.count; ++i) {
                if (idx.entries[i].stage_state != 0) continue;
                int ex = 0;
                if (tv_object_exists(r, idx.entries[i].blob, &ex) != SDK_OK) { out->malformed++; out->ok = 0; break; }
                if (!ex) { out->missing++; out->ok = 0;
                    fprintf(stderr, "tinyvcs verify: missing blob for index %s\n", idx.entries[i].path); }
            }
            tv_index_free(&idx);
        }
    }

    /* scan all objects */
    tv_oid *all_ids = NULL; size_t all_n = 0;
    if (verify_walk_objects(r, out, &all_ids, &all_n) != SDK_OK) {
        free(all_ids); free(roots);
        return SDK_ERR_IO;
    }
    /* reachable */
    tv_oid *visited = NULL; size_t vis_n = 0;
    if (verify_reachable(r, out, roots, root_n, &visited, &vis_n) != SDK_OK) {
        free(all_ids); free(roots); free(visited);
        return SDK_ERR_IO;
    }
    /* unreachable */
    for (size_t i = 0; i < all_n; ++i) {
        if (!oid_in_set(visited, vis_n, &all_ids[i])) out->unreachable++;
    }

    /* .tmp and stale locks warnings */
    {
        wchar_t *tp;
        if (meta_path(r, L"tmp", &tp) == SDK_OK) {
            sdk_dirlist dl; uint32_t we=0;
            if (sdk_dir_list_w(tp, &dl, NULL, &we) == SDK_OK) {
                for (size_t i = 0; i < dl.count; ++i) {
                    if (!dl.items[i].info.is_directory) {
                        out->warnings++;
                        fprintf(stderr, "tinyvcs verify: warning: temporary file %ls\n", dl.items[i].name_w);
                    }
                }
                sdk_dirlist_free(&dl);
            }
            free(tp);
        }
        wchar_t *lp;
        if (meta_path(r, L"locks", &lp) == SDK_OK) {
            sdk_dirlist dl; uint32_t we=0;
            if (sdk_dir_list_w(lp, &dl, NULL, &we) == SDK_OK) {
                for (size_t i = 0; i < dl.count; ++i) {
                    if (!dl.items[i].info.is_directory) {
                        out->warnings++;
                        fprintf(stderr, "tinyvcs verify: warning: stale lock %ls\n", dl.items[i].name_w);
                    }
                }
                sdk_dirlist_free(&dl);
            }
            free(lp);
        }
    }

    free(all_ids);
    free(roots);
    free(visited);

    printf("tinyvcs verify summary:\n");
    printf("  scanned    %u\n", out->scanned);
    printf("  reachable  %u\n", out->reachable);
    printf("  unreachable %u\n", out->unreachable);
    printf("  corrupt    %u\n", out->corrupt);
    printf("  missing    %u\n", out->missing);
    printf("  malformed  %u\n", out->malformed);
    printf("  warnings   %u\n", out->warnings);
    if (!out->ok)
        printf("tinyvcs verify: repository incomplete\n");
    else
        printf("tinyvcs verify: repository OK\n");
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Command dispatch (shared by CLI and tests)                          */
/* ------------------------------------------------------------------ */

/* Forward declarations of command handlers. */
static sdk_status cmd_init(int argc, wchar_t **argv);
static sdk_status cmd_status(int argc, wchar_t **argv);
static sdk_status cmd_add(int argc, wchar_t **argv);
static sdk_status cmd_commit(int argc, wchar_t **argv);
static sdk_status cmd_log(int argc, wchar_t **argv);
static sdk_status cmd_branch(int argc, wchar_t **argv);
static sdk_status cmd_switch(int argc, wchar_t **argv);
static sdk_status cmd_restore(int argc, wchar_t **argv);
static sdk_status cmd_reset(int argc, wchar_t **argv);
static sdk_status cmd_show(int argc, wchar_t **argv);
static sdk_status cmd_verify(int argc, wchar_t **argv);

static sdk_status open_repo(tv_repo *r) {
    wchar_t *root = NULL;
    sdk_status st = tv_find_repo_root(&root);
    if (st != SDK_OK) { g_stage = "discovery"; g_win32 = 0; return SDK_ERR_USAGE; }
    st = tv_open_repo(root, r);
    free(root);
    return st;
}

/* Convert a command-line path argument to a repo-relative canonical path.
 * Returns SDK_OK and fills rel, or an error. *outside set when outside repo. */
static sdk_status arg_to_rel(tv_repo *r, const wchar_t *arg,
                             char *rel, size_t cap, int *outside) {
    wchar_t *full = sdk_full_path_w(arg);
    if (!full) return SDK_ERR_NOMEM;
    size_t rl = wcslen(r->root);
    if (wcsncmp(full, r->root, rl) != 0) { free(full); *outside = 1; return SDK_ERR_USAGE; }
    if (full[rl] != L'\\' && full[rl] != L'/' && full[rl] != L'\0') {
        free(full); *outside = 1; return SDK_ERR_USAGE;
    }
    wchar_t *relw = sdk_wcsdup_n(full + (full[rl] ? rl + 1 : rl),
                                wcslen(full) - (full[rl] ? rl + 1 : rl));
    free(full);
    if (!relw) return SDK_ERR_NOMEM;
    for (size_t i = 0; relw[i]; ++i) if (relw[i] == L'\\') relw[i] = L'/';
    char *u8 = sdk_utf16_to_utf8(relw, wcslen(relw), NULL);
    free(relw);
    if (!u8) return SDK_ERR_NOMEM;
    if (strlen(u8) >= cap) { free(u8); return SDK_ERR_LIMIT; }
    if (sdk_path_relative_check(u8, strlen(u8), SDK_LIMIT_LOCSTAT_PATH_DEPTH) != SDK_PATH_OK) {
        free(u8); return SDK_ERR_DATA;
    }
    if (strncmp(u8, ".tinyvcs", 8) == 0 && (u8[8] == '/' || u8[8] == '\0')) {
        free(u8); return SDK_ERR_DATA;
    }
    memcpy(rel, u8, strlen(u8) + 1);
    free(u8);
    return SDK_OK;
}

int tv_dispatch(int argc, wchar_t **argv) {
    tv_error_clear();
    if (argc < 2) {
        fprintf(stderr, "tinyvcs: usage: tinyvcs <command> [args]\n");
        return SDK_EXIT_USAGE;
    }
    const wchar_t *cmd = argv[1];
    sdk_status st;
    if (wcscmp(cmd, L"init") == 0) st = cmd_init(argc, argv);
    else if (wcscmp(cmd, L"status") == 0) st = cmd_status(argc, argv);
    else if (wcscmp(cmd, L"add") == 0) st = cmd_add(argc, argv);
    else if (wcscmp(cmd, L"commit") == 0) st = cmd_commit(argc, argv);
    else if (wcscmp(cmd, L"log") == 0) st = cmd_log(argc, argv);
    else if (wcscmp(cmd, L"branch") == 0) st = cmd_branch(argc, argv);
    else if (wcscmp(cmd, L"switch") == 0) st = cmd_switch(argc, argv);
    else if (wcscmp(cmd, L"restore") == 0) st = cmd_restore(argc, argv);
    else if (wcscmp(cmd, L"reset") == 0) st = cmd_reset(argc, argv);
    else if (wcscmp(cmd, L"show") == 0) st = cmd_show(argc, argv);
    else if (wcscmp(cmd, L"verify") == 0) st = cmd_verify(argc, argv);
    else if (wcscmp(cmd, L"help") == 0 || wcscmp(cmd, L"--help") == 0 || wcscmp(cmd, L"-h") == 0) {
        printf("tinyvcs - snapshot version control\n");
        printf("commands: init status add commit log branch switch restore reset show verify help\n");
        return SDK_EXIT_OK;
    } else {
        fprintf(stderr, "tinyvcs: unknown command: %ls\n", cmd);
        return SDK_EXIT_USAGE;
    }
    if (st != SDK_OK) {
        fprintf(stderr, "tinyvcs: %s: %s (win32=0x%08X)\n",
                tv_error_stage(), sdk_status_name(st), tv_error_win32());
    }
    if (st == SDK_OK) return SDK_EXIT_OK;
    return sdk_status_to_exit(st);
}

/* --------------------------- init --------------------------- */
static sdk_status cmd_init(int argc, wchar_t **argv) {
    (void)argc; (void)argv;
    wchar_t *root = sdk_getcwd_w();
    if (!root) return SDK_ERR_NOMEM;
    sdk_status st = tv_init_repo(root);
    if (st == SDK_OK) printf("initialized empty tinyvcs repository\n");
    free(root);
    return st;
}

/* --------------------------- status ------------------------- */
static sdk_status print_section(const char *title, char **list, size_t n) {
    printf("%s:\n", title);
    if (n == 0) { printf("  (none)\n"); return SDK_OK; }
    for (size_t i = 0; i < n; ++i) printf("  %s\n", list[i]);
    return SDK_OK;
}

static sdk_status cmd_status(int argc, wchar_t **argv) {
    (void)argc; (void)argv;
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    tv_status_info s;
    st = tv_status_collect(&r, &s);
    if (st != SDK_OK) { tv_close_repo(&r); return st; }
    printf("On branch %s\n", s.branch);
    print_section("Staged changes", s.staged_added, s.staged_added_n);
    print_section("Unstaged changes", s.unstaged_modified, s.unstaged_modified_n);
    print_section("Untracked files", s.untracked, s.untracked_n);
    printf("summary: staged=%zu unstaged=%zu untracked=%zu\n",
           s.staged_added_n + s.staged_modified_n + s.staged_deleted_n,
           s.unstaged_modified_n + s.unstaged_deleted_n,
           s.untracked_n);
    tv_status_free(&s);
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- add ----------------------------- */
static int add_one_file(tv_repo *r, const char *rel, tv_index *idx) {
    wchar_t *wpath;
    if (root_path(r, rel, &wpath) != SDK_OK) return 0;
    tv_oid id; uint64_t sz; int ro;
    sdk_status st = tv_file_blob_id(wpath, id, &sz, &ro);
    free(wpath);
    if (st != SDK_OK) return 0;
    tv_index_entry e;
    memset(&e, 0, sizeof e);
    snprintf(e.path, sizeof e.path, "%s", rel);
    e.path[sizeof e.path - 1] = '\0';
    e.file_flags = (uint8_t)(ro ? 1 : 0);
    e.stage_state = 0;
    memcpy(e.blob, id, 32);
    e.size = sz;
    e.mtime_100ns = 0;
    tv_index_upsert(idx, &e);
    return 1;
}

static sdk_status cmd_add(int argc, wchar_t **argv) {
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;

    int all = 0;
    int path_count = 0;
    for (int i = 2; i < argc; ++i) {
        if (wcscmp(argv[i], L"--all") == 0) all = 1;
        else if (wcscmp(argv[i], L"--") == 0) { /* treat rest as paths */ }
        else path_count++;
    }
    if (all && path_count > 0) { tv_close_repo(&r); g_stage="add"; return SDK_ERR_USAGE; }
    if (!all && path_count == 0) { tv_close_repo(&r); g_stage="add"; return SDK_ERR_USAGE; }

    tv_ignore ig;
    if (tv_ignore_load(&r, &ig) != SDK_OK) { tv_close_repo(&r); g_stage="add_ignore"; return SDK_ERR_IO; }
    tv_index idx;
    if (tv_index_load(&r, &idx) != SDK_OK) { tv_ignore_free(&ig); tv_close_repo(&r); g_stage="add_index"; return SDK_ERR_DATA; }

    int ok = 1;
    if (all) {
        tv_workfile *wf = NULL; size_t wn = 0;
        if (tv_collect_working(&r, &ig, &wf, &wn) != SDK_OK) { ok = 0; g_stage="add_walk"; }
        else {
            for (size_t i = 0; i < wn; ++i) {
                if (!add_one_file(&r, wf[i].rel, &idx)) { ok = 0; g_stage="add_file"; break; }
            }
            /* stage deletions for tracked files removed from disk */
            for (size_t i = 0; i < idx.count && ok; ++i) {
                tv_index_entry *e = &idx.entries[i];
                if (e->stage_state != 0) continue;
                int found = 0;
                for (size_t k = 0; k < wn; ++k) if (strcmp(wf[k].rel, e->path) == 0) { found = 1; break; }
                if (!found) {
                    int ign = tv_is_ignored(&ig, e->path, 0);
                    if (!ign) {
                        e->stage_state = 1;
                        memset(e->blob, 0, 32);
                    }
                }
            }
            tv_workfile_list_free(wf, wn);
        }
    } else {
        for (int i = 2; i < argc; ++i) {
            if (wcscmp(argv[i], L"--all") == 0 || wcscmp(argv[i], L"--") == 0) continue;
            char rel[SDK_LIMIT_VCS_PATH_BYTES];
            int outside = 0;
            sdk_status rs = arg_to_rel(&r, argv[i], rel, sizeof rel, &outside);
            if (rs != SDK_OK) {
                if (rs == SDK_ERR_DATA) { fprintf(stderr, "tinyvcs: cannot track metadata or invalid path: %ls\n", argv[i]); }
                ok = 0; g_stage="add_path"; break;
            }
            if (tv_is_meta_name(rel)) { fprintf(stderr, "tinyvcs: cannot track .tinyvcs\n"); ok=0; g_stage="add_meta"; break; }
            wchar_t *wpath;
            if (root_path(&r, rel, &wpath) != SDK_OK) { ok=0; g_stage="add_root"; break; }
            sdk_fileinfo fi;
            sdk_status ss = sdk_stat_w(wpath, &fi);
            int is_dir = (ss == SDK_OK && fi.exists && fi.is_directory);
            int is_file = (ss == SDK_OK && fi.exists && !fi.is_directory);
            if (tv_is_ignored(&ig, rel, is_dir)) {
                fprintf(stderr, "tinyvcs: warning: %s is ignored\n", rel);
                free(wpath);
                continue;
            }
            if (is_dir) {
                /* recurse: collect working files under rel */
                tv_workfile *wf = NULL; size_t wn = 0;
                if (tv_collect_working(&r, &ig, &wf, &wn) != SDK_OK) { ok=0; g_stage="add_walk"; free(wpath); break; }
                for (size_t k = 0; k < wn; ++k) {
                    if (strncmp(wf[k].rel, rel, strlen(rel)) == 0 &&
                        wf[k].rel[strlen(rel)] == '/') {
                        if (!add_one_file(&r, wf[k].rel, &idx)) { ok=0; g_stage="add_file"; break; }
                    }
                }
                tv_workfile_list_free(wf, wn);
            } else if (is_file) {
                if (!add_one_file(&r, rel, &idx)) { ok=0; g_stage="add_file"; }
            } else {
                /* missing path */
                if (tv_index_find(&idx, rel) != NULL) {
                    tv_index_entry e; memset(&e,0,sizeof e);
                    snprintf(e.path,sizeof e.path,"%s",rel);
                    e.stage_state = 1; memset(e.blob,0,32);
                    tv_index_upsert(&idx, &e);
                } else {
                    fprintf(stderr, "tinyvcs: path not found and not tracked: %s\n", rel);
                    ok = 0; g_stage="add_missing";
                }
            }
            free(wpath);
            if (!ok) break;
        }
    }

    if (ok) {
        if (tv_index_save(&r, &idx) != SDK_OK) ok = 0;
    }
    tv_index_free(&idx);
    tv_ignore_free(&ig);
    tv_close_repo(&r);
    return ok ? SDK_OK : (g_stage ? SDK_ERR_USAGE : SDK_ERR_USAGE);
}

/* --------------------------- commit -------------------------- */
static sdk_status cmd_commit(int argc, wchar_t **argv) {
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;

    const wchar_t *msg = NULL;
    for (int i = 2; i < argc; ++i) {
        if (wcscmp(argv[i], L"-m") == 0 && i + 1 < argc) { msg = argv[++i]; }
    }
    if (!msg) { tv_close_repo(&r); g_stage="commit"; return SDK_ERR_USAGE; }
    char *message = sdk_utf16_to_utf8(msg, wcslen(msg), NULL);
    if (!message) { tv_close_repo(&r); return SDK_ERR_NOMEM; }

    char *author = sdk_getenv_utf8(L"TINYVCS_AUTHOR");
    if (!author || author[0] == '\0') {
        free(message); free(author); tv_close_repo(&r);
        g_stage="commit.author"; g_win32=0;
        fprintf(stderr, "tinyvcs: TINYVCS_AUTHOR is required\n");
        return SDK_ERR_DATA;
    }
    size_t al = strlen(author);
    int author_ok = (al >= 1 && al <= SDK_LIMIT_VCS_AUTHOR_BYTES &&
                     author[0] != ' ' && author[al-1] != ' ');
    for (size_t i = 0; author_ok && i < al; ++i)
        if (author[i] < 32 || author[i] > 126) author_ok = 0;
    if (!author_ok) {
        free(message); free(author); tv_close_repo(&r);
        g_stage="commit.author"; g_win32=0;
        fprintf(stderr, "tinyvcs: invalid TINYVCS_AUTHOR\n");
        return SDK_ERR_DATA;
    }

    /* message validation */
    size_t ml = strlen(message);
    int mok = (ml >= 1 && ml <= SDK_LIMIT_VCS_MESSAGE_BYTES);
    int any_ws = 0;
    for (size_t i = 0; mok && i < ml; ++i) {
        if (message[i] == 0) mok = 0;
        if (message[i] != ' ' && message[i] != '\t' && message[i] != '\n' &&
            message[i] != '\r') any_ws = 1;
    }
    if (!mok || !any_ws) {
        free(message); free(author); tv_close_repo(&r);
        g_stage="commit.message"; g_win32=0;
        fprintf(stderr, "tinyvcs: invalid commit message\n");
        return SDK_ERR_DATA;
    }

    tv_index idx;
    if (tv_index_load(&r, &idx) != SDK_OK) { free(message); free(author); tv_close_repo(&r); g_stage="commit_index"; return SDK_ERR_DATA; }

    size_t present = 0;
    for (size_t i = 0; i < idx.count; ++i) if (idx.entries[i].stage_state == 0) present++;
    if (present == 0) {
        free(message); free(author); tv_index_free(&idx); tv_close_repo(&r);
        g_stage="commit.empty"; g_win32=0;
        fprintf(stderr, "tinyvcs: nothing to commit (empty staging)\n");
        return SDK_ERR_USAGE;
    }

    /* build tree from present index entries */
    char **paths = (char **)malloc((present ? present : 1) * sizeof(char *));
    tv_oid *blobs = (tv_oid *)malloc((present ? present : 1) * sizeof(tv_oid));
    uint8_t *flags = (uint8_t *)malloc((present ? present : 1) * sizeof(uint8_t));
    if (!paths || !blobs || !flags) { free(paths); free(blobs); free(flags); free(message); free(author); tv_index_free(&idx); tv_close_repo(&r); return SDK_ERR_NOMEM; }
    size_t pi = 0;
    for (size_t i = 0; i < idx.count; ++i) {
        if (idx.entries[i].stage_state != 0) continue;
        paths[pi] = idx.entries[i].path;
        memcpy(blobs[pi], idx.entries[i].blob, 32);
        flags[pi] = idx.entries[i].file_flags;
        ++pi;
    }
    /* Persist each staged (present) blob object before building the tree.
     * Only stage_state==0 entries still exist on disk; deletions
     * (stage_state==1) must be skipped or the read fails with a
     * missing-file error (win32=ERROR_PATH_NOT_FOUND). */
    for (size_t i = 0; i < idx.count; ++i) {
        if (idx.entries[i].stage_state != 0) continue;
        if (tv_write_blob_for_entry(&r, &idx.entries[i]) != SDK_OK) {
            free(paths); free(blobs); free(flags); free(message); free(author);
            tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_blob"; return SDK_ERR_DATA;
        }
    }
    tv_oid root_tree;
    if (tv_build_tree_from_paths(&r, (const char *const *)paths, blobs, flags, present, &root_tree) != SDK_OK) {
        free(paths); free(blobs); free(flags); free(message); free(author);
        tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_tree"; return SDK_ERR_DATA;
    }

    char branch[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    if (tv_head_branch(&r, branch, sizeof branch) != SDK_OK) {
        free(paths); free(blobs); free(flags); free(message); free(author);
        tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_head"; return SDK_ERR_DATA;
    }
    tv_oid parent; int unborn;
    if (tv_read_ref(&r, branch, &parent, &unborn) != SDK_OK) {
        free(paths); free(blobs); free(flags); free(message); free(author);
        tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_ref"; return SDK_ERR_DATA;
    }
    if (!unborn) {
        tv_commit_info ci;
        if (tv_read_commit(&r, parent, &ci) == SDK_OK) {
            if (tv_oid_equal(ci.root_tree, root_tree)) {
                free(paths); free(blobs); free(flags); free(message); free(author);
                tv_index_free(&idx); tv_close_repo(&r);
                g_stage="commit.nothing"; g_win32=0;
                fprintf(stderr, "tinyvcs: nothing to commit (no changes)\n");
                return SDK_ERR_USAGE;
            }
        }
    }

    tv_oid commit_id;
    if (tv_create_commit(&r, unborn ? NULL : &parent, &root_tree, author, message, commit_id) != SDK_OK) {
        free(paths); free(blobs); free(flags); free(message); free(author);
        tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_create"; return SDK_ERR_DATA;
    }
    if (tv_write_ref(&r, branch, &commit_id) != SDK_OK) {
        free(paths); free(blobs); free(flags); free(message); free(author);
        tv_index_free(&idx); tv_close_repo(&r); g_stage="commit_refwrite"; return SDK_ERR_IO;
    }

    char hex[65];
    tv_oid_hex(commit_id, hex);
    printf("committed %s\n", hex);

    free(paths); free(blobs); free(flags); free(message); free(author);
    tv_index_free(&idx);
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- log ----------------------------- */
static sdk_status cmd_log(int argc, wchar_t **argv) {
    (void)argc; (void)argv;
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    char branch[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    if (tv_head_branch(&r, branch, sizeof branch) != SDK_OK) { tv_close_repo(&r); g_stage="log_head"; return SDK_ERR_DATA; }
    tv_oid cur; int unborn;
    if (tv_read_ref(&r, branch, &cur, &unborn) != SDK_OK) { tv_close_repo(&r); g_stage="log_ref"; return SDK_ERR_DATA; }
    if (unborn) {
        printf("no commits yet\n");
        tv_close_repo(&r);
        return SDK_OK;
    }
    size_t count = 0;
    while (count < 100000) {
        tv_commit_info ci;
        if (tv_read_commit(&r, cur, &ci) != SDK_OK) { tv_close_repo(&r); g_stage="log_commit"; return SDK_ERR_DATA; }
        char hex[65];
        tv_oid_hex(cur, hex);
        char shorthex[13];
        memcpy(shorthex, hex, 12); shorthex[12] = '\0';
        printf("commit %s\n", hex);
        printf("  short %s\n", shorthex);
        printf("  author %s\n", ci.author);
        printf("  date %lld\n", (long long)ci.timestamp_ms);
        printf("  message %s\n", ci.message);
        if (ci.parent_count == 0) break;
        memcpy(cur, ci.parent, 32);
        ++count;
    }
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- branch --------------------------- */
static sdk_status cmd_branch(int argc, wchar_t **argv) {
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    if (argc >= 3) {
        char name[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        char *u8 = sdk_utf16_to_utf8(argv[2], wcslen(argv[2]), NULL);
        if (!u8) { tv_close_repo(&r); return SDK_ERR_NOMEM; }
        snprintf(name, sizeof name, "%s", u8);
        free(u8);
        if (tv_branch_name_check(name) != SDK_OK) {
            tv_close_repo(&r); g_stage="branch_name"; g_win32=0;
            fprintf(stderr, "tinyvcs: invalid branch name\n");
            return SDK_ERR_USAGE;
        }
        int exists = 0;
        if (tv_branch_exists(&r, name, &exists) != SDK_OK) { tv_close_repo(&r); g_stage="branch_exists"; return SDK_ERR_IO; }
        if (exists) {
            tv_close_repo(&r); g_stage="branch_exists"; g_win32=0;
            fprintf(stderr, "tinyvcs: branch already exists: %s\n", name);
            return SDK_ERR_EXISTS;
        }
        char cur[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        if (tv_head_branch(&r, cur, sizeof cur) != SDK_OK) { tv_close_repo(&r); g_stage="branch_head"; return SDK_ERR_DATA; }
        tv_oid c; int unborn;
        if (tv_read_ref(&r, cur, &c, &unborn) != SDK_OK) { tv_close_repo(&r); g_stage="branch_ref"; return SDK_ERR_DATA; }
        if (unborn) {
            tv_close_repo(&r); g_stage="branch_unborn"; g_win32=0;
            fprintf(stderr, "tinyvcs: cannot create branch from unborn HEAD\n");
            return SDK_ERR_DATA;
        }
        if (tv_create_branch(&r, name, &c) != SDK_OK) { tv_close_repo(&r); g_stage="branch_create"; return SDK_ERR_IO; }
        printf("created branch %s\n", name);
    } else {
        char **names = NULL; size_t n = 0;
        char cur[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
        if (tv_list_branches(&r, &names, &n, cur, sizeof cur) != SDK_OK) { tv_close_repo(&r); g_stage="branch_list"; return SDK_ERR_DATA; }
        for (size_t i = 0; i < n; ++i) {
            if (strcmp(names[i], cur) == 0) printf("* %s\n", names[i]);
            else printf("  %s\n", names[i]);
        }
        tv_branch_list_free(names, n);
    }
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- switch --------------------------- */
static sdk_status cmd_switch(int argc, wchar_t **argv) {
    if (argc < 3) { g_stage="switch"; g_win32=0; fprintf(stderr, "tinyvcs: switch requires a branch name\n"); return SDK_ERR_USAGE; }
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    char name[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    char *u8 = sdk_utf16_to_utf8(argv[2], wcslen(argv[2]), NULL);
    if (!u8) { tv_close_repo(&r); return SDK_ERR_NOMEM; }
    snprintf(name, sizeof name, "%s", u8);
    free(u8);
    int exists = 0;
    if (tv_branch_exists(&r, name, &exists) != SDK_OK) { tv_close_repo(&r); g_stage="switch_exists"; return SDK_ERR_IO; }
    if (!exists) { tv_close_repo(&r); g_stage="switch"; g_win32=0; fprintf(stderr, "tinyvcs: branch does not exist: %s\n", name); return SDK_ERR_USAGE; }
    tv_oid target; int unborn;
    if (tv_read_ref(&r, name, &target, &unborn) != SDK_OK) { tv_close_repo(&r); g_stage="switch_ref"; return SDK_ERR_DATA; }
    if (unborn) { tv_close_repo(&r); g_stage="switch"; g_win32=0; fprintf(stderr, "tinyvcs: cannot switch to unborn branch\n"); return SDK_ERR_USAGE; }

    tv_commit_info ci;
    if (tv_read_commit(&r, target, &ci) != SDK_OK) { tv_close_repo(&r); g_stage="switch_commit"; return SDK_ERR_DATA; }
    tv_file_entry *desired = NULL; size_t dn = 0;
    if (tv_tree_collect_files(&r, ci.root_tree, &desired, &dn) != SDK_OK) { tv_close_repo(&r); g_stage="switch_tree"; return SDK_ERR_DATA; }

    /* preflight */
    tv_index idx;
    if (tv_index_load(&r, &idx) != SDK_OK) { free(desired); tv_close_repo(&r); g_stage="switch_index"; return SDK_ERR_DATA; }
    tv_ignore ig; tv_ignore_init(&ig);
    int conflict = 0;
    for (size_t i = 0; i < dn && !conflict; ++i) {
        wchar_t *wpath;
        int tracked = 0, is_dirty = 0, untracked = 0;
        if (root_path(&r, desired[i].path, &wpath) == SDK_OK) {
            sdk_fileinfo fi;
            if (sdk_stat_w(wpath, &fi) == SDK_OK && fi.exists) {
                tv_index_entry *e = tv_index_find(&idx, desired[i].path);
                if (e && e->stage_state == 0) {
                    tracked = 1;
                    tv_oid wid;
                    if (tv_file_blob_id(wpath, wid, NULL, NULL) == SDK_OK) {
                        if (!tv_oid_equal(wid, e->blob)) is_dirty = 1;
                    }
                } else {
                    untracked = 1;
                }
            }
            free(wpath);
        }
        if (untracked) {
            fprintf(stderr, "tinyvcs: switch would overwrite untracked file: %s\n", desired[i].path);
            conflict = 1;
        } else if (is_dirty) {
            fprintf(stderr, "tinyvcs: switch would overwrite dirty tracked file: %s\n", desired[i].path);
            conflict = 1;
        }
    }
    /* deletions of tracked files that are dirty */
    for (size_t i = 0; i < idx.count && !conflict; ++i) {
        tv_index_entry *e = &idx.entries[i];
        if (e->stage_state != 0) continue;
        int in_desired = 0;
        for (size_t k = 0; k < dn; ++k) if (strcmp(desired[k].path, e->path) == 0) { in_desired = 1; break; }
        if (in_desired) continue;
        wchar_t *wpath;
        if (root_path(&r, e->path, &wpath) != SDK_OK) continue;
        sdk_fileinfo fi;
        if (sdk_stat_w(wpath, &fi) == SDK_OK && fi.exists) {
            tv_oid wid;
            if (tv_file_blob_id(wpath, wid, NULL, NULL) == SDK_OK && !tv_oid_equal(wid, e->blob)) {
                fprintf(stderr, "tinyvcs: switch would delete dirty tracked file: %s\n", e->path);
                conflict = 1;
            }
        }
        free(wpath);
    }
    tv_ignore_free(&ig);
    if (conflict) { free(desired); tv_index_free(&idx); tv_close_repo(&r); g_stage="switch_preflight"; g_win32=0; return SDK_ERR_DATA; }

    /* apply working tree + index */
    if (tv_apply_checkout(&r, desired, dn, 1) != SDK_OK) {
        free(desired); tv_index_free(&idx); tv_close_repo(&r); g_stage="switch_apply"; return SDK_ERR_IO;
    }
    /* update HEAD last */
    {
        char buf[512];
        int n = snprintf(buf, sizeof buf, "ref: refs/heads/%s\n", name);
        wchar_t *p;
        if (meta_path(&r, L"HEAD", &p) != SDK_OK) { free(desired); tv_index_free(&idx); tv_close_repo(&r); return SDK_ERR_NOMEM; }
        const char *stage = "switch_head";
        uint32_t we = 0;
        sdk_status ws = sdk_file_write_atomic_w(p, NULL, buf, (size_t)n, &stage, &we);
        free(p);
        if (ws != SDK_OK) { free(desired); tv_index_free(&idx); tv_close_repo(&r); g_stage=stage; g_win32=we; return ws; }
    }
    printf("switched to branch %s\n", name);
    free(desired); tv_index_free(&idx);
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- restore -------------------------- */
static sdk_status restore_path(tv_repo *r, int from_index, const tv_oid *tree_id,
                               const char *path) {
    tv_oid blob;
    int found = 0;
    if (from_index) {
        tv_index idx;
        if (tv_index_load(r, &idx) != SDK_OK) return SDK_ERR_DATA;
        tv_index_entry *e = tv_index_find(&idx, path);
        if (e && e->stage_state == 0) { memcpy(blob, e->blob, 32); found = 1; }
        tv_index_free(&idx);
    } else {
        tv_file_entry *files = NULL; size_t fn = 0;
        if (tv_tree_collect_files(r, *tree_id, &files, &fn) != SDK_OK) return SDK_ERR_DATA;
        for (size_t i = 0; i < fn; ++i) if (strcmp(files[i].path, path) == 0) { memcpy(blob, files[i].blob, 32); found = 1; break; }
        free(files);
    }
    if (found) {
        wchar_t *wpath;
        if (root_path(r, path, &wpath) != SDK_OK) return SDK_ERR_NOMEM;
        tv_obj_type t; uint8_t *pay; size_t plen;
        sdk_status rs = tv_read_object(r, blob, &t, &pay, &plen);
        if (rs != SDK_OK || t != TV_OBJ_BLOB) { free(wpath); if (rs==SDK_OK) free(pay); return rs==SDK_OK?SDK_ERR_DATA:rs; }
        tv_ensure_parent(wpath);
        const char *stage = "restore_write";
        uint32_t we = 0;
        rs = sdk_file_write_atomic_w(wpath, NULL, pay, plen, &stage, &we);
        free(pay);
        if (rs != SDK_OK) { free(wpath); g_stage=stage; g_win32=we; return rs; }
        free(wpath);
        return SDK_OK;
    }
    /* not in source */
    tv_index idx;
    if (tv_index_load(r, &idx) != SDK_OK) return SDK_ERR_DATA;
    tv_index_entry *e = tv_index_find(&idx, path);
    int tracked = (e && e->stage_state == 0);
    tv_index_free(&idx);
    if (tracked) {
        wchar_t *wpath;
        if (root_path(r, path, &wpath) != SDK_OK) return SDK_ERR_NOMEM;
        uint32_t we = 0;
        sdk_status rs = sdk_delete_file_w(wpath, &we);
        free(wpath);
        if (rs != SDK_OK) { g_stage="restore_delete"; g_win32=we; return rs; }
        return SDK_OK;
    }
    fprintf(stderr, "tinyvcs: restore: path never tracked: %s\n", path);
    g_stage="restore_untracked"; g_win32=0;
    return SDK_ERR_USAGE;
}

static sdk_status cmd_restore(int argc, wchar_t **argv) {
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    int from_index = 1;
    tv_oid src_commit;
    int have_src = 0;
    int path_start = argc;
    for (int i = 2; i < argc; ++i) {
        if (wcscmp(argv[i], L"--source") == 0 && i + 1 < argc) {
            from_index = 0;
            char *u8 = sdk_utf16_to_utf8(argv[++i], wcslen(argv[i]), NULL);
            if (!u8) { tv_close_repo(&r); return SDK_ERR_NOMEM; }
            if (tv_resolve_commit(&r, u8, &src_commit) != SDK_OK) {
                free(u8); tv_close_repo(&r); g_stage="restore_source"; g_win32=0;
                fprintf(stderr, "tinyvcs: cannot resolve source commit\n");
                return SDK_ERR_USAGE;
            }
            free(u8);
            have_src = 1;
            continue;
        }
        path_start = i;
        break;
    }
    if (!have_src) {
        /* all remaining args are paths; from_index stays 1 */
        path_start = 2;
    }
    if (path_start >= argc) { tv_close_repo(&r); g_stage="restore"; g_win32=0; fprintf(stderr, "tinyvcs: restore requires a path\n"); return SDK_ERR_USAGE; }
    int ok = 1;
    for (int i = path_start; i < argc; ++i) {
        char rel[SDK_LIMIT_VCS_PATH_BYTES];
        int outside = 0;
        sdk_status rs = arg_to_rel(&r, argv[i], rel, sizeof rel, &outside);
        if (rs != SDK_OK) { ok = 0; g_stage="restore_path"; break; }
        if (restore_path(&r, from_index, &src_commit, rel) != SDK_OK) { ok = 0; g_stage="restore_apply"; break; }
    }
    tv_close_repo(&r);
    return ok ? SDK_OK : SDK_ERR_USAGE;
}

/* --------------------------- reset ---------------------------- */
static sdk_status cmd_reset(int argc, wchar_t **argv) {
    int have_hard = 0, have_yes = 0;
    const wchar_t *commit_spec = NULL;
    for (int i = 2; i < argc; ++i) {
        if (wcscmp(argv[i], L"--hard") == 0) have_hard = 1;
        else if (wcscmp(argv[i], L"--yes") == 0) have_yes = 1;
        else if (commit_spec == NULL) commit_spec = argv[i];
    }
    if (!have_hard || !have_yes || commit_spec == NULL) {
        g_stage="reset"; g_win32=0;
        fprintf(stderr, "tinyvcs: usage: reset --hard <commit> --yes\n");
        return SDK_ERR_USAGE;
    }
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    char *u8 = sdk_utf16_to_utf8(commit_spec, wcslen(commit_spec), NULL);
    if (!u8) { tv_close_repo(&r); return SDK_ERR_NOMEM; }
    tv_oid target;
    if (tv_resolve_commit(&r, u8, &target) != SDK_OK) {
        free(u8); tv_close_repo(&r); g_stage="reset_resolve"; g_win32=0;
        fprintf(stderr, "tinyvcs: cannot resolve commit\n");
        return SDK_ERR_USAGE;
    }
    free(u8);
    tv_commit_info ci;
    if (tv_read_commit(&r, target, &ci) != SDK_OK) { tv_close_repo(&r); g_stage="reset_commit"; return SDK_ERR_DATA; }
    tv_file_entry *desired = NULL; size_t dn = 0;
    if (tv_tree_collect_files(&r, ci.root_tree, &desired, &dn) != SDK_OK) { tv_close_repo(&r); g_stage="reset_tree"; return SDK_ERR_DATA; }

    /* preflight: untracked collision on desired paths */
    tv_ignore ig; tv_ignore_init(&ig);
    int conflict = 0;
    for (size_t i = 0; i < dn && !conflict; ++i) {
        wchar_t *wpath;
        if (root_path(&r, desired[i].path, &wpath) == SDK_OK) {
            sdk_fileinfo fi;
            if (sdk_stat_w(wpath, &fi) == SDK_OK && fi.exists) {
                tv_index idx2;
                if (tv_index_load(&r, &idx2) == SDK_OK) {
                    tv_index_entry *e = tv_index_find(&idx2, desired[i].path);
                    if (!(e && e->stage_state == 0)) {
                        fprintf(stderr, "tinyvcs: reset would overwrite untracked file: %s\n", desired[i].path);
                        conflict = 1;
                    }
                    tv_index_free(&idx2);
                }
            }
            free(wpath);
        }
    }
    tv_ignore_free(&ig);
    if (conflict) { free(desired); tv_close_repo(&r); g_stage="reset_preflight"; g_win32=0; return SDK_ERR_DATA; }

    if (tv_apply_checkout(&r, desired, dn, 1) != SDK_OK) { free(desired); tv_close_repo(&r); g_stage="reset_apply"; return SDK_ERR_IO; }

    char branch[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    if (tv_head_branch(&r, branch, sizeof branch) != SDK_OK) { free(desired); tv_close_repo(&r); g_stage="reset_head"; return SDK_ERR_DATA; }
    if (tv_write_ref(&r, branch, &target) != SDK_OK) { free(desired); tv_close_repo(&r); g_stage="reset_refwrite"; return SDK_ERR_IO; }
    printf("reset to %s\n", "");
    free(desired);
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- show ----------------------------- */
static sdk_status cmd_show(int argc, wchar_t **argv) {
    if (argc < 3) { g_stage="show"; g_win32=0; fprintf(stderr, "tinyvcs: show requires a commit\n"); return SDK_ERR_USAGE; }
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    char *u8 = sdk_utf16_to_utf8(argv[2], wcslen(argv[2]), NULL);
    if (!u8) { tv_close_repo(&r); return SDK_ERR_NOMEM; }
    tv_oid id;
    if (tv_resolve_commit(&r, u8, &id) != SDK_OK) { free(u8); tv_close_repo(&r); g_stage="show_resolve"; g_win32=0; fprintf(stderr, "tinyvcs: cannot resolve commit\n"); return SDK_ERR_USAGE; }
    free(u8);
    tv_commit_info ci;
    if (tv_read_commit(&r, id, &ci) != SDK_OK) { tv_close_repo(&r); g_stage="show_commit"; return SDK_ERR_DATA; }
    char hex[65]; tv_oid_hex(id, hex);
    printf("commit %s\n", hex);
    if (ci.parent_count) { char ph[65]; tv_oid_hex(ci.parent, ph); printf("parent %s\n", ph); }
    else printf("parent (none)\n");
    printf("author %s\n", ci.author);
    printf("date %lld\n", (long long)ci.timestamp_ms);
    printf("message %s\n", ci.message);

    /* changed file list vs parent */
    tv_file_entry *a = NULL, *b = NULL; size_t an = 0, bn = 0;
    if (ci.parent_count) {
        tv_commit_info pci;
        if (tv_read_commit(&r, ci.parent, &pci) == SDK_OK) {
            tv_tree_collect_files(&r, pci.root_tree, &a, &an);
        }
    }
    tv_tree_collect_files(&r, ci.root_tree, &b, &bn);
    printf("changed files:\n");
    for (size_t i = 0; i < bn; ++i) {
        const uint8_t *pa = NULL;
        for (size_t k = 0; k < an; ++k) if (strcmp(a[k].path, b[i].path) == 0) { pa = a[k].blob; break; }
        const uint8_t *pb = b[i].blob;
        if (!pa) printf("  added   %s\n", b[i].path);
        else if (!tv_oid_equal(pa, pb)) printf("  modified %s\n", b[i].path);
    }
    for (size_t i = 0; i < an; ++i) {
        int in_b = 0;
        for (size_t k = 0; k < bn; ++k) if (strcmp(a[i].path, b[k].path) == 0) { in_b = 1; break; }
        if (!in_b) printf("  deleted %s\n", a[i].path);
    }
    free(a); free(b);
    tv_close_repo(&r);
    return SDK_OK;
}

/* --------------------------- verify --------------------------- */
static sdk_status cmd_verify(int argc, wchar_t **argv) {
    (void)argc; (void)argv;
    tv_repo r;
    sdk_status st = open_repo(&r);
    if (st != SDK_OK) return st;
    tv_verify_result res;
    st = tv_verify(&r, &res);
    tv_close_repo(&r);
    if (st != SDK_OK) return st;
    return res.ok ? SDK_OK : SDK_ERR_VERIFY;
}
