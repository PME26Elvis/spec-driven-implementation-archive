#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"

static int fails=0, tests=0;
#define EXPECT(c) do{tests++; if(!(c)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);fails++;}}while(0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    w.bounds_min = ps_v2(-30,-30);
    w.bounds_max = ps_v2(30,40);

    ps_body *floor = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape fs={0}; fs.type=PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=20; fs.data.rectangle.hy=1; fs.friction=0.7f;
    ps_body_set_shape(floor, &fs);
    ps_body_set_transform(floor, ps_v2(0,20), 0);

    for (int i=0; i<3; i++) {
        ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
        ps_shape s={0}; s.type=PS_SHAPE_RECTANGLE; s.density=1; s.data.rectangle.hx=0.9f; s.data.rectangle.hy=0.9f; s.friction=0.5f; s.restitution=0.05f;
        ps_body_set_shape(b, &s);
        ps_body_set_transform(b, ps_v2(0, 15.0f - i*2.1f), 0);
    }

    int finite = 1;
    for (int i=0; i<180; i++) {
        ps_world_step(&w, 1.f/60.f);
        for (int j=0; j<w.body_count; j++) {
            if (!isfinite(w.bodies[j].xf.p.x) || !isfinite(w.bodies[j].xf.p.y)) finite = 0;
        }
    }
    EXPECT(finite);
    EXPECT(w.body_count == 4);

    printf("Stack smoke tests: %d run, %d failed\n", tests, fails);
    return fails ? 1 : 0;
}
