#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    ps_body *anchor = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape as={0}; as.type=PS_SHAPE_CIRCLE; as.data.circle.radius=0.2f;
    ps_body_set_shape(anchor,&as); ps_body_set_transform(anchor, ps_v2(0,0),0);
    ps_body *bob = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape bs={0}; bs.type=PS_SHAPE_CIRCLE; bs.density=1; bs.data.circle.radius=0.4f;
    ps_body_set_shape(bob,&bs); ps_body_set_transform(bob, ps_v2(2,0),0);
    ps_joint *j = ps_world_create_joint(&w);
    ps_joint_init_distance(j, anchor, bob, anchor->xf.p, bob->xf.p);
    float len0 = ps_v2_len(ps_v2_sub(bob->xf.p, anchor->xf.p));
    for (int i=0;i<120;i++) ps_world_step(&w, 1.f/60.f);
    float len1 = ps_v2_len(ps_v2_sub(bob->xf.p, anchor->xf.p));
    EXPECT(fabsf(len1 - len0) < 0.5f);
    EXPECT(isfinite(bob->xf.p.x) && isfinite(bob->xf.p.y));
    printf("Golden pendulum tests: %d run, %d failed (len0=%.3f len1=%.3f)\n", t, f, len0, len1);
    return f?1:0;
}
