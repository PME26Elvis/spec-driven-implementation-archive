#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    ps_body *st = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape s={0}; s.type=PS_SHAPE_RECTANGLE; s.data.rectangle.hx=2; s.data.rectangle.hy=2;
    ps_body_set_shape(st,&s); ps_body_set_transform(st, ps_v2(0,0),0);
    float px = st->xf.p.x, py = st->xf.p.y;
    for (int i=0;i<60;i++) ps_world_step(&w, 1.f/60.f);
    EXPECT(fabsf(st->xf.p.x - px) < 1e-5f);
    EXPECT(fabsf(st->xf.p.y - py) < 1e-5f);
    EXPECT(st->linear_vel.x == 0 && st->linear_vel.y == 0);
    printf("Golden static tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
