#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    /* floor */
    ps_body *floor = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape fs={0}; fs.type=PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=20; fs.data.rectangle.hy=1; fs.friction=0.1f; fs.restitution=0.8f;
    ps_body_set_shape(floor,&fs); ps_body_set_transform(floor, ps_v2(0,10),0);
    /* ball */
    ps_body *ball = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape bs={0}; bs.type=PS_SHAPE_CIRCLE; bs.density=1; bs.data.circle.radius=0.5f; bs.restitution=0.8f; bs.friction=0.1f;
    ps_body_set_shape(ball,&bs); ps_body_set_transform(ball, ps_v2(0,0),0);
    float min_y = 1e9f;
    for (int i=0;i<300;i++) {
        ps_world_step(&w, 1.f/60.f);
        if (ball->xf.p.y < min_y) min_y = ball->xf.p.y;
    }
    /* should have bounced (not stuck deep below floor) and stayed finite */
    EXPECT(isfinite(ball->xf.p.y));
    EXPECT(ball->xf.p.y < 15.0f);
    EXPECT(min_y < 10.0f); /* reached near floor */
    printf("Golden bounce tests: %d run, %d failed (y=%.3f min=%.3f)\n", t, f, ball->xf.p.y, min_y);
    return f?1:0;
}
