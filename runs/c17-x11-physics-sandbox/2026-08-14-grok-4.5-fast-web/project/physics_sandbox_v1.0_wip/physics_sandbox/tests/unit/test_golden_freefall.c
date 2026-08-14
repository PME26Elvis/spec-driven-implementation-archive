#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"

/* Golden-style: free fall under gravity, position roughly 0.5*g*t^2 */
static int fails=0, tests=0;
#define EXPECT_NEAR(a,b,e) do{tests++; if(fabsf((a)-(b))>(e)){fprintf(stderr,"FAIL %s:%d %f vs %f\n",__FILE__,__LINE__,(float)(a),(float)(b));fails++;}}while(0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    w.gravity = ps_v2(0, 9.81f);
    w.bounds_min = ps_v2(-100,-100);
    w.bounds_max = ps_v2(100,100);

    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.5f;
    ps_body_set_shape(b, &s);
    ps_body_set_transform(b, ps_v2(0,0), 0);

    const int N = 60;
    for (int i=0;i<N;i++) ps_world_step(&w, 1.f/60.f);
    float t = N / 60.f;
    float expected = 0.5f * 9.81f * t * t;
    EXPECT_NEAR(b->xf.p.y, expected, 0.5f);
    EXPECT_NEAR(b->linear_vel.y, 9.81f * t, 0.5f);

    printf("Golden freefall tests: %d run, %d failed (y=%.3f exp=%.3f)\n", tests, fails, b->xf.p.y, expected);
    return fails ? 1 : 0;
}
