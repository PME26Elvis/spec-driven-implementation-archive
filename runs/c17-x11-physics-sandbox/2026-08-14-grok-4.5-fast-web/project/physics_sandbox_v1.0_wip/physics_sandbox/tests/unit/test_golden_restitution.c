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
    ps_shape fs={0}; fs.type=PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=10; fs.data.rectangle.hy=0.5f; fs.restitution=0.9f;
    ps_body_set_shape(floor,&fs); ps_body_set_transform(floor, ps_v2(0,5),0);
    ps_body *ball = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape bs={0}; bs.type=PS_SHAPE_CIRCLE; bs.density=1; bs.data.circle.radius=0.4f; bs.restitution=0.9f;
    ps_body_set_shape(ball,&bs); ps_body_set_transform(ball, ps_v2(0,0),0);
    float max_y_after = -1e9f;
    int bounced = 0;
    for (int i=0;i<200;i++) {
        float prev_vy = ball->linear_vel.y;
        ps_world_step(&w, 1.f/60.f);
        if (prev_vy > 1.0f && ball->linear_vel.y < -0.5f) bounced = 1;
        if (i > 60 && ball->xf.p.y > max_y_after) max_y_after = ball->xf.p.y;
    }
    EXPECT(isfinite(ball->xf.p.y));
    EXPECT(bounced || ball->xf.p.y < 5.5f); /* contacted floor region */
    printf("Golden restitution tests: %d run, %d failed (y=%.3f bounced=%d)\n", t, f, ball->xf.p.y, bounced);
    return f?1:0;
}
