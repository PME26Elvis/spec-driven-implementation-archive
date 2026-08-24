#include "edb/pager.h"
#include "edb/btree.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static void mk(int i, uint8_t *b, edb_key *k) {
    b[0]=(i>>24)&255;b[1]=(i>>16)&255;b[2]=(i>>8)&255;b[3]=i&255;
    k->data=b;k->len=4;
}
int main(void) {
    edb_error err;
    unlink("/tmp/ht.edb");
    edb_pager *p = edb_pager_open("/tmp/ht.edb", true, false, NULL, &err);
    uint32_t root; edb_btree_create(p, true, &root, &err);
    edb_btree *t = edb_btree_open(p, root, true, &err);
    int N = 30000;
    for (int i = 0; i < N; i++) {
        uint8_t kb[4]; edb_key key; mk(i, kb, &key);
        uint8_t val[128]; memset(val, 0, sizeof val); memcpy(val, &i, 4);
        if (edb_btree_insert(t, &key, val, sizeof val, &err) != 0) {
            printf("FAIL insert %d: %s height=%d\n", i, err.message, edb_btree_height(t));
            return 1;
        }
        if ((i % 1000) == 999 || i == N-1) {
            for (int j = 0; j <= i; j += 53) {
                uint8_t kb2[4]; edb_key k2; mk(j, kb2, &k2);
                uint8_t v[128]; uint16_t vl = sizeof v;
                if (edb_btree_get(t, &k2, v, &vl, &err) != 0) {
                    printf("FAIL get %d after %d height=%d: %s\n", j, i, edb_btree_height(t), err.message);
                    return 1;
                }
            }
            printf("ok through %d height=%d\n", i+1, edb_btree_height(t));
        }
    }
    if (edb_btree_validate(t, &err) != 0) {
        printf("FAIL validate: %s\n", err.message); return 1;
    }
    printf("PASS N=%d height=%d\n", N, edb_btree_height(t));
    edb_btree_close(t); edb_pager_close(p);
    return 0;
}
