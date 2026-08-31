/* test_tinyvcs.c - unit tests for the tinyvcs snapshot version-control tool.
 *
 * Exercises the public C API (docs/19 canonical formats + docs/03 dev tool):
 *   init / re-init rejection, blob write+read round-trip, index save/load,
 *   content-addressed commit (blob+tree+commit+ref), verify, branch creation
 *   with a second commit, and status classification.
 *
 * Entry point: wmain -> sdk_test_main("unit-tinyvcs", register_all, ...).
 */
#include "tinyvcs_core.h"
#include "test/sdk_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/sdk_common.h"
#include "common/sdk_win.h"

/* ------------------------------------------------------------------ */
/* Fixture: isolated temp repository on the D: drive.                  */
/* ------------------------------------------------------------------ */

static wchar_t g_root[320];
static tv_repo g_r;

static wchar_t *wcpath(const wchar_t *base, const char *rel_utf8) {
    wchar_t *wrel = sdk_utf8_to_utf16(rel_utf8, strlen(rel_utf8), NULL);
    if (!wrel) return NULL;
    for (size_t i = 0; wrel[i]; ++i) if (wrel[i] == L'/') wrel[i] = L'\\';
    wchar_t *out = sdk_wpath_join(base, wrel);
    free(wrel);
    return out;
}

static int write_file(const wchar_t *base, const char *rel, const char *content) {
    wchar_t *full = wcpath(base, rel);
    if (!full) return 0;
    int rc = (sdk_file_write_all_w(full, (const void *)content, strlen(content),
                                  NULL) == SDK_OK);
    free(full);
    return rc;
}

static void setup_repo(void) {
    DWORD pid = GetCurrentProcessId();
    _snwprintf_s(g_root, sizeof g_root / sizeof g_root[0], _TRUNCATE,
                 L"D:\\tvunit_%u", pid);
    sdk_rmdir_w(g_root, NULL);
    SDK_T_TRUE(NULL, SDK_OK == tv_init_repo(g_root));
    SDK_T_TRUE(NULL, SDK_OK == tv_open_repo(g_root, &g_r));
}

static void teardown_repo(void) {
    tv_close_repo(&g_r);
    sdk_rmdir_w(g_root, NULL);
}

/* Replicates the core of cmd_commit via the public API, targeting `branch`. */
static sdk_status api_commit(const char *branch, const char *author,
                             const char *msg, const char *const *paths,
                             const char *const *contents, size_t n) {
    tv_index idx;
    tv_index_init(&idx);
    tv_oid *blobs = (tv_oid *)malloc((n ? n : 1) * sizeof(tv_oid));
    uint8_t *flags = (uint8_t *)malloc((n ? n : 1) * sizeof(uint8_t));
    if (!blobs || !flags) { free(blobs); free(flags); tv_index_free(&idx); return SDK_ERR_NOMEM; }
    for (size_t i = 0; i < n; ++i) {
        if (!write_file(g_r.root, paths[i], contents[i])) { free(blobs); free(flags); tv_index_free(&idx); return SDK_ERR_IO; }
        wchar_t *wp = wcpath(g_r.root, paths[i]);
        tv_oid id;
        if (tv_file_blob_id(wp, id, NULL, NULL) != SDK_OK) { free(wp); free(blobs); free(flags); tv_index_free(&idx); return SDK_ERR_IO; }
        free(wp);
        uint8_t *data = NULL; size_t len = 0; uint32_t we = 0;
        wchar_t *wp2 = wcpath(g_r.root, paths[i]);
        sdk_status rs = sdk_file_read_all_w(wp2, SDK_LIMIT_FILE_BYTES, &data, &len, &we);
        free(wp2);
        if (rs != SDK_OK) { free(blobs); free(flags); tv_index_free(&idx); return rs; }
        tv_oid id2; tv_object_id_for("blob", data, len, id2);
        int ex = 0;
        if (tv_object_exists(&g_r, id2, &ex) != SDK_OK || !ex)
            tv_write_object(&g_r, TV_OBJ_BLOB, data, len, id2);
        free(data);
        tv_index_entry e;
        memset(&e, 0, sizeof e);
        snprintf(e.path, sizeof e.path, "%s", paths[i]);
        e.path[sizeof e.path - 1] = '\0';
        e.stage_state = 0;
        memcpy(e.blob, id2, 32);
        e.size = (uint64_t)strlen(contents[i]);
        tv_index_upsert(&idx, &e);
        memcpy(blobs[i], id2, 32);
        flags[i] = 0;
    }
    tv_oid root_tree;
    sdk_status st = tv_build_tree_from_paths(&g_r, paths, blobs, flags, n, &root_tree);
    if (st != SDK_OK) { free(blobs); free(flags); tv_index_free(&idx); return st; }
    tv_oid parent; int unborn;
    if (tv_read_ref(&g_r, branch, &parent, &unborn) != SDK_OK) { free(blobs); free(flags); tv_index_free(&idx); return SDK_ERR_DATA; }
    tv_oid commit_id;
    tv_create_commit(&g_r, unborn ? NULL : &parent, &root_tree, author, msg, commit_id);
    tv_write_ref(&g_r, branch, &commit_id);
    tv_index_save(&g_r, &idx);
    free(blobs); free(flags); tv_index_free(&idx);
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_init_and_reinit(sdk_test_ctx *t) {
    (void)t;
    /* setup_repo already initialised; a second init must be rejected. */
    SDK_T_EQ_ST(t, SDK_ERR_DATA, tv_init_repo(g_root));
    tv_repo r2;
    SDK_T_EQ_ST(t, SDK_OK, tv_open_repo(g_root, &r2));
    tv_close_repo(&r2);
}

static void test_blob_roundtrip(sdk_test_ctx *t) {
    const char *content = "int main(){ return 0; }\n";
    if (!write_file(g_r.root, "blobfile.txt", content)) { SDK_T_TRUE(t, 0); return; }
    wchar_t *wp = wcpath(g_r.root, "blobfile.txt");
    tv_oid id;
    SDK_T_EQ_ST(t, SDK_OK, tv_file_blob_id(wp, id, NULL, NULL));
    free(wp);
    tv_oid expect;
    tv_object_id_for("blob", (const uint8_t *)content, strlen(content), expect);
    SDK_T_TRUE(t, tv_oid_equal(id, expect));
    SDK_T_EQ_ST(t, SDK_OK, tv_write_object(&g_r, TV_OBJ_BLOB,
                                           (const uint8_t *)content,
                                           strlen(content), id));
    int ex = 0;
    SDK_T_EQ_ST(t, SDK_OK, tv_object_exists(&g_r, id, &ex));
    SDK_T_TRUE(t, ex);
    tv_obj_type ty; uint8_t *pay = NULL; size_t plen = 0;
    SDK_T_EQ_ST(t, SDK_OK, tv_read_object(&g_r, id, &ty, &pay, &plen));
    SDK_T_EQ_U(t, (unsigned)TV_OBJ_BLOB, (unsigned)ty);
    SDK_T_EQ_U(t, (unsigned)strlen(content), (unsigned)plen);
    SDK_T_TRUE(t, pay && memcmp(pay, content, plen) == 0);
    free(pay);
}

static void test_index_roundtrip(sdk_test_ctx *t) {
    tv_index idx;
    tv_index_init(&idx);
    tv_index_entry e;
    memset(&e, 0, sizeof e);
    snprintf(e.path, sizeof e.path, "%s", "a.txt");
    e.stage_state = 0;
    memset(e.blob, 0xAB, 32);
    e.size = 5;
    tv_index_upsert(&idx, &e);
    memset(&e, 0, sizeof e);
    snprintf(e.path, sizeof e.path, "%s", "b.txt");
    memset(e.blob, 0xCD, 32);
    tv_index_upsert(&idx, &e);
    memset(&e, 0, sizeof e);
    snprintf(e.path, sizeof e.path, "%s", "sub/dir/c.txt");
    memset(e.blob, 0xEF, 32);
    tv_index_upsert(&idx, &e);
    SDK_T_EQ_U(t, 3u, (unsigned)idx.count);
    SDK_T_EQ_ST(t, SDK_OK, tv_index_save(&g_r, &idx));

    tv_index idx2;
    SDK_T_EQ_ST(t, SDK_OK, tv_index_load(&g_r, &idx2));
    SDK_T_EQ_U(t, 3u, (unsigned)idx2.count);
    tv_index_entry *f = tv_index_find(&idx2, "a.txt");
    SDK_T_TRUE(t, f != NULL);
    if (f) {
        tv_oid want; memset(want, 0xAB, 32);
        SDK_T_TRUE(t, tv_oid_equal(f->blob, want));
    }
    tv_index_entry *g = tv_index_find(&idx2, "sub/dir/c.txt");
    SDK_T_TRUE(t, g != NULL);
    if (g) {
        tv_oid want; memset(want, 0xEF, 32);
        SDK_T_TRUE(t, tv_oid_equal(g->blob, want));
    }
    tv_index_free(&idx);
    tv_index_free(&idx2);
}

static void test_commit_and_verify(sdk_test_ctx *t) {
    const char *paths[] = { "a.txt", "b.txt" };
    const char *cts[]   = { "hello\n", "world\n" };
    SDK_T_EQ_ST(t, SDK_OK, api_commit("main", "tester <t@e.com>", "first commit", paths, cts, 2));

    tv_verify_result res;
    SDK_T_EQ_ST(t, SDK_OK, tv_verify(&g_r, &res));
    SDK_T_TRUE(t, res.ok);
    SDK_T_EQ_U(t, 0u, (unsigned)res.missing);
    SDK_T_EQ_U(t, 0u, (unsigned)res.malformed);

    char br[SDK_LIMIT_BRANCH_NAME_BYTES + 1];
    SDK_T_EQ_ST(t, SDK_OK, tv_head_branch(&g_r, br, sizeof br));
    SDK_T_EQ_STR(t, "main", br);
    tv_oid c; int unborn;
    SDK_T_EQ_ST(t, SDK_OK, tv_read_ref(&g_r, "main", &c, &unborn));
    SDK_T_TRUE(t, !unborn);

    tv_commit_info ci;
    SDK_T_EQ_ST(t, SDK_OK, tv_read_commit(&g_r, c, &ci));
    SDK_T_EQ_STR(t, "tester <t@e.com>", ci.author);
    SDK_T_EQ_STR(t, "first commit", ci.message);
    SDK_T_EQ_U(t, 0u, (unsigned)ci.parent_count);
}

static void test_branch_second_commit(sdk_test_ctx *t) {
    const char *paths[] = { "a.txt" };
    const char *cts[]   = { "hello\n" };
    SDK_T_EQ_ST(t, SDK_OK, api_commit("main", "tester <t@e.com>", "base", paths, cts, 1));

    tv_oid base; int ub;
    SDK_T_EQ_ST(t, SDK_OK, tv_read_ref(&g_r, "main", &base, &ub));
    SDK_T_TRUE(t, !ub);

    SDK_T_EQ_ST(t, SDK_OK, tv_create_branch(&g_r, "dev", &base));

    const char *p2[] = { "a.txt" };
    const char *c2[] = { "hello-dev\n" };
    SDK_T_EQ_ST(t, SDK_OK, api_commit("dev", "tester <t@e.com>", "dev work", p2, c2, 1));

    tv_oid devc; int ub2;
    SDK_T_EQ_ST(t, SDK_OK, tv_read_ref(&g_r, "dev", &devc, &ub2));
    SDK_T_TRUE(t, !ub2);
    SDK_T_TRUE(t, !tv_oid_equal(devc, base));

    tv_verify_result res;
    SDK_T_EQ_ST(t, SDK_OK, tv_verify(&g_r, &res));
    SDK_T_TRUE(t, res.ok);
}

static void test_status_classification(sdk_test_ctx *t) {
    const char *paths[] = { "x.txt" };
    const char *cts[]   = { "x\n" };
    SDK_T_EQ_ST(t, SDK_OK, api_commit("main", "tester <t@e.com>", "c", paths, cts, 1));

    tv_status_info s;
    SDK_T_EQ_ST(t, SDK_OK, tv_status_collect(&g_r, &s));
    /* Working tree is clean vs the index (no staged / unstaged changes). */
    SDK_T_EQ_U(t, 0u, (unsigned)s.staged_added_n);
    SDK_T_EQ_U(t, 0u, (unsigned)s.staged_modified_n);
    SDK_T_EQ_U(t, 0u, (unsigned)s.unstaged_modified_n);
    unsigned before = (unsigned)s.untracked_n;
    tv_status_free(&s);

    SDK_T_TRUE(t, write_file(g_r.root, "new.txt", "fresh\n"));
    tv_status_info s2;
    SDK_T_EQ_ST(t, SDK_OK, tv_status_collect(&g_r, &s2));
    SDK_T_EQ_U(t, before + 1u, (unsigned)s2.untracked_n);
    tv_status_free(&s2);
}

/* ------------------------------------------------------------------ */
/* Registration                                                       */
/* ------------------------------------------------------------------ */

static void register_all(void) {
    sdk_test_add("tinyvcs.init_and_reinit", "docs03-init",
                 test_init_and_reinit);
    sdk_test_add("tinyvcs.blob_roundtrip", "docs19-blob",
                 test_blob_roundtrip);
    sdk_test_add("tinyvcs.index_roundtrip", "docs19-index",
                 test_index_roundtrip);
    sdk_test_add("tinyvcs.commit_and_verify", "docs19-commit",
                 test_commit_and_verify);
    sdk_test_add("tinyvcs.branch_second_commit", "docs19-branch",
                 test_branch_second_commit);
    sdk_test_add("tinyvcs.status_classification", "docs19-status",
                 test_status_classification);
}

int wmain(int argc, wchar_t *argv[]) {
    setup_repo();
    int rc = sdk_test_main("unit-tinyvcs", register_all, argc, argv);
    teardown_repo();
    return rc;
}
