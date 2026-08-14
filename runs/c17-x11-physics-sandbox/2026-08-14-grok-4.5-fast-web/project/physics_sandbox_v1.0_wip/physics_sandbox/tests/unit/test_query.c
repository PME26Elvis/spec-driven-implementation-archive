#include <stdio.h>
#include "../../src/physics/world.h"
#include "../../src/physics/query.h"
static int f=0,t=0,hits=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL\n");f++;}}while(0)
static void cb(int idx, void *ctx){ (void)ctx; hits++; (void)idx; }
int main(void) {
    ps_world w; ps_world_init(&w);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=1;
    ps_body_set_shape(b,&s); ps_body_set_transform(b, ps_v2(0,0),0);
    ps_world_sync_proxies(&w);
    hits=0;
    ps_world_query_aabb(&w, ps_v2(-2,-2), ps_v2(2,2), cb, NULL);
    EXPECT(hits >= 1);
    int idx = ps_world_query_point(&w, ps_v2(0.1f, 0.1f));
    EXPECT(idx >= 0);
    printf("Query tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
