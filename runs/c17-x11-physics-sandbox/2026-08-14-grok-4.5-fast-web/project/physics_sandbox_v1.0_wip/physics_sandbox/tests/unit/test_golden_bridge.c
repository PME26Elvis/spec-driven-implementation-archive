#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    ps_body *prev = NULL;
    for (int i = 0; i < 5; i++) {
        ps_body *seg = ps_world_create_body(&w, PS_BODY_DYNAMIC);
        ps_shape s={0}; s.type=PS_SHAPE_RECTANGLE; s.density=0.8f;
        s.data.rectangle.hx=1.0f; s.data.rectangle.hy=0.2f; s.friction=0.4f;
        ps_body_set_shape(seg,&s);
        ps_body_set_transform(seg, ps_v2(-4.0f + i*2.2f, 5.0f), 0);
        if (prev) {
            ps_joint *j = ps_world_create_joint(&w);
            ps_joint_init_distance(j, prev, seg, prev->xf.p, seg->xf.p);
        }
        prev = seg;
    }
    for (int i=0;i<180;i++) ps_world_step(&w, 1.f/60.f);
    int finite=1;
    for (int i=0;i<w.body_count;i++)
        if (!isfinite(w.bodies[i].xf.p.x) || !isfinite(w.bodies[i].xf.p.y)) finite=0;
    EXPECT(finite);
    EXPECT(w.joint_count >= 4);
    printf("Golden bridge tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
