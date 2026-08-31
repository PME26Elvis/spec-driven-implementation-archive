#include "cvc_repo.h"
#include "cvc_object.h"
#include "cvc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int store_empty_tree(const char *cvc, uint8_t out[32]) {
    unsigned char payload[4] = {0,0,0,0};
    return cvc_object_store(cvc, CVC_OBJ_TREE, payload, sizeof payload, out);
}

static int store_commit(const char *cvc, const uint8_t tree[32],
                        uint8_t pc, const uint8_t *parents,
                        int64_t ts, const char *msg, uint8_t out[32]) {
    return cvc_commit_store(cvc, tree, pc, parents, ts,
                            (const unsigned char *)msg, strlen(msg), out);
}

int main(void) {
    char td[] = "/tmp/cvc-mergebase-XXXXXX";
    char *root = mkdtemp(td);
    if (!root) { perror("mkdtemp"); return 2; }
    char *cvc = cvc_path_join(root, ".cvc");
    char *objects = cvc_path_join(cvc, "objects");
    if (cvc_mkdir_p(objects, 0777) < 0) { perror("mkdir"); free(objects); free(cvc); return 2; }

    uint8_t tree[32], r[32], a[32], b[32], m1[32], m2[32], pars[64], got[32];
    int rc = 2;
    if (store_empty_tree(cvc, tree) < 0) goto done;
    if (store_commit(cvc, tree, 0, NULL, 1, "root", r) < 0) goto done;
    if (store_commit(cvc, tree, 1, r, 2, "A", a) < 0) goto done;
    if (store_commit(cvc, tree, 1, r, 3, "B", b) < 0) goto done;
    memcpy(pars, a, 32); memcpy(pars + 32, b, 32);
    if (store_commit(cvc, tree, 2, pars, 4, "M1", m1) < 0) goto done;
    memcpy(pars, b, 32); memcpy(pars + 32, a, 32);
    if (store_commit(cvc, tree, 2, pars, 5, "M2", m2) < 0) goto done;

    CvcRepo repo;
    memset(&repo, 0, sizeof repo);
    repo.lock_fd = -1;
    repo.root = cvc_xstrdup(root);
    repo.cvc = cvc_xstrdup(cvc);
    if (cvc_merge_base(&repo, m1, m2, got) < 0) {
        cvc_repo_close(&repo);
        goto done;
    }
    const uint8_t *expect = memcmp(a, b, 32) < 0 ? a : b;
    if (memcmp(got, expect, 32) != 0) {
        char gh[65], eh[65]; cvc_hex_encode(got, 32, gh); cvc_hex_encode(expect, 32, eh);
        fprintf(stderr, "merge-base tie-break mismatch: got %s expected %s\n", gh, eh);
        cvc_repo_close(&repo);
        goto done;
    }
    cvc_repo_close(&repo);
    puts("mergebase_unit: PASS");
    rc = 0;

done:
    free(objects); free(cvc);
    if (cvc_remove_tree_nofollow(root) < 0 && rc == 0) { perror("cleanup"); rc = 2; }
    return rc;
}
