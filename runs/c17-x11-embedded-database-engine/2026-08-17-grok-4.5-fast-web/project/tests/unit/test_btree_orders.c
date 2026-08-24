/* Insert ascending, descending, alternating; verify all keys */
#include "edb/pager.h"
#include "edb/btree.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void mk(int i, uint8_t *b, edb_key *k) {
    b[0]=(i>>24)&255;b[1]=(i>>16)&255;b[2]=(i>>8)&255;b[3]=i&255;
    k->data=b;k->len=4;
}

static int fill_and_check(const char *path, int *order, int n) {
    edb_error err;
    unlink(path);
    edb_pager *p = edb_pager_open(path, true, false, NULL, &err);
    if (!p) return -1;
    uint32_t root;
    edb_btree_create(p, true, &root, &err);
    edb_btree *t = edb_btree_open(p, root, true, &err);
    for (int i = 0; i < n; i++) {
        uint8_t kb[4]; edb_key key; mk(order[i], kb, &key);
        uint8_t val[8]; memcpy(val, &order[i], 4);
        if (edb_btree_insert(t, &key, val, 4, &err) != 0) {
            printf("insert fail %d: %s\n", order[i], err.message);
            edb_btree_close(t); edb_pager_close(p); return -1;
        }
    }
    for (int i = 0; i < n; i++) {
        uint8_t kb[4]; edb_key key; mk(order[i], kb, &key);
        uint8_t val[8]; uint16_t vl = 8;
        if (edb_btree_get(t, &key, val, &vl, &err) != 0) {
            printf("get fail %d: %s\n", order[i], err.message);
            edb_btree_close(t); edb_pager_close(p); return -1;
        }
    }
    if (edb_btree_validate(t, &err) != 0) {
        printf("validate fail: %s\n", err.message);
        edb_btree_close(t); edb_pager_close(p); return -1;
    }
    int h = edb_btree_height(t);
    edb_btree_close(t); edb_pager_close(p);
    return h;
}

int main(void) {
    int n = 2000;
    int *ord = malloc((size_t)n * sizeof(int));
    /* ascending */
    for (int i = 0; i < n; i++) ord[i] = i;
    int h = fill_and_check("/tmp/bt_asc.edb", ord, n);
    if (h < 0) return 1;
    printf("PASS ascending n=%d height=%d\n", n, h);
    /* descending */
    for (int i = 0; i < n; i++) ord[i] = n - 1 - i;
    h = fill_and_check("/tmp/bt_desc.edb", ord, n);
    if (h < 0) return 1;
    printf("PASS descending n=%d height=%d\n", n, h);
    /* alternating */
    int lo = 0, hi = n - 1, k = 0;
    while (lo <= hi) {
        ord[k++] = lo++;
        if (lo <= hi) ord[k++] = hi--;
    }
    h = fill_and_check("/tmp/bt_alt.edb", ord, n);
    if (h < 0) return 1;
    printf("PASS alternating n=%d height=%d\n", n, h);
    free(ord);
    return 0;
}
