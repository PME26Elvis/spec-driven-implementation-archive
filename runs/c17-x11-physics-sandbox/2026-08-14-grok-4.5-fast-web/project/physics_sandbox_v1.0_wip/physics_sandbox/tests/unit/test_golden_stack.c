#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    ps_body *floor = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape fs={0}; fs.type=PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=15; fs.data.rectangle.hy=1; fs.friction=0.6f;
    ps_body_set_shape(floor,&fs); ps_body_set_transform(floor, ps_v2(0,12),0);
    for (int i=0;i<3;i++) {
        ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
        ps_shape s={0}; s.type=PS_SHAPE_RECTANGLE; s.density=1; s.data.rectangle.hx=0.8f; s.data.rectangle.hy=0.8f; s.friction=0.5f; s.restitution=0.05f;
        ps_body_set_shape(b,&s);
        ps_body_set_transform(b, ps_v2(0, 8.0f - i*2.0f), 0);
    }
    for (int i=0;i<240;i++) ps_world_step(&w, 1.f/60.f);
    int finite=1;
    for (int i=0;i<w.body_count;i++)
        if (!isfinite(w.bodies[i].xf.p.x) || !isfinite(w.bodies[i].xf.p.y)) finite=0;
    EXPECT(finite);
    EXPECT(w.body_count == 4);
    printf("Golden stack tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
