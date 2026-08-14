#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/physics/bvh.h"

static int g_fail = 0, g_tests = 0;
#define EXPECT(c) do { g_tests++; if (!(c)) { fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); g_fail++; } } while(0)

static int hit_count = 0;
static void cb(int id, void *ctx) { (void)ctx; hit_count++; (void)id; }

int main(void) {
    ps_bvh tree;
    ps_bvh_init(&tree);

    ps_aabb a1 = {ps_v2(0,0), ps_v2(2,2)};
    ps_aabb a2 = {ps_v2(3,3), ps_v2(5,5)};
    ps_aabb a3 = {ps_v2(1,1), ps_v2(4,4)};

    int p1 = ps_bvh_create_proxy(&tree, &a1, 10);
    int p2 = ps_bvh_create_proxy(&tree, &a2, 20);
    int p3 = ps_bvh_create_proxy(&tree, &a3, 30);
    EXPECT(p1 >= 0 && p2 >= 0 && p3 >= 0);
    EXPECT(tree.proxy_count == 3);

    hit_count = 0;
    ps_aabb q = {ps_v2(0.5f,0.5f), ps_v2(1.5f,1.5f)};
    ps_bvh_query(&tree, &q, cb, NULL);
    EXPECT(hit_count >= 1);

    printf("BVH tests: %d run, %d failed\n", g_tests, g_fail);
    return g_fail ? 1 : 0;
}
