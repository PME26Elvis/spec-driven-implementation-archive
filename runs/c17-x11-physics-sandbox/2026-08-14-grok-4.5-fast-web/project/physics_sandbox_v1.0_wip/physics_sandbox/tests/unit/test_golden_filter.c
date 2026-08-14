#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0,0);
    ps_body *a = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=1.0f;
    ps_body_set_shape(a,&s); ps_body_set_shape(b,&s);
    ps_body_set_transform(a, ps_v2(0,0),0);
    ps_body_set_transform(b, ps_v2(1.5f,0),0);
    a->category_bits = 0x1; a->mask_bits = 0x1;
    b->category_bits = 0x2; b->mask_bits = 0x2;
    ps_matrix_set(&w.collision_matrix, 0, 1, false);
    float dist0 = ps_v2_len(ps_v2_sub(b->xf.p, a->xf.p));
    for (int i=0;i<30;i++) ps_world_step(&w, 1.f/60.f);
    float dist1 = ps_v2_len(ps_v2_sub(b->xf.p, a->xf.p));
    /* without collision response they should not be strongly pushed apart */
    EXPECT(dist1 < dist0 + 0.5f);
    printf("Golden filter tests: %d run, %d failed (d0=%.3f d1=%.3f)\n", t, f, dist0, dist1);
    return f?1:0;
}
