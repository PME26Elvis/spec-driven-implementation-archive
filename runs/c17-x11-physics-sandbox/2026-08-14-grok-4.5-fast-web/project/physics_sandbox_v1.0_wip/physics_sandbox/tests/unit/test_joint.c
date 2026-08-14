#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"

static int fails = 0, tests = 0;
#define EXPECT_NEAR(a,b,e) do { tests++; if (fabsf((a)-(b))>(e)) { fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); fails++; } } while(0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    w.gravity = ps_v2(0, 0); /* no gravity for pure distance test */

    ps_body *a = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s = {0};
    s.type = PS_SHAPE_CIRCLE; s.density = 1.f; s.data.circle.radius = 0.5f;
    ps_body_set_shape(a, &s); ps_body_set_shape(b, &s);
    ps_body_set_transform(a, ps_v2(0,0), 0);
    ps_body_set_transform(b, ps_v2(3,0), 0);

    ps_joint *j = ps_world_create_joint(&w);
    ps_joint_init_distance(j, a, b, a->xf.p, b->xf.p);

    /* step many times; distance should stay near 3 */
    for (int i = 0; i < 120; i++) ps_world_step(&w, 1.f/60.f);
    float d = ps_v2_len(ps_v2_sub(b->xf.p, a->xf.p));
    EXPECT_NEAR(d, 3.0f, 0.3f);

    printf("Joint distance tests: %d run, %d failed\n", tests, fails);
    return fails ? 1 : 0;
}
