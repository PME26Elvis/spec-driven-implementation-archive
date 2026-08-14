#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"

static int fails=0, tests=0;
#define EXPECT(c) do{tests++; if(!(c)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);fails++;}}while(0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    w.gravity = ps_v2(0,0);

    ps_body *a = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_RECTANGLE; s.density=1; s.data.rectangle.hx=0.5f; s.data.rectangle.hy=0.5f;
    ps_body_set_shape(a, &s);
    ps_body_set_shape(b, &s);
    ps_body_set_transform(a, ps_v2(0,0), 0);
    ps_body_set_transform(b, ps_v2(1.5f,0), 0);
    /* give b some inertia */
    b->inertia = 0.2f; b->inv_inertia = 1.0f/0.2f;

    ps_joint *j = ps_world_create_joint(&w);
    ps_joint_init_revolute(j, a, b, ps_v2(0.75f,0));
    j->enable_motor = true;
    j->motor_speed = 2.0f; /* rad/s */
    j->max_motor_torque = 50.0f;

    float ang0 = ps_rot2_angle(b->xf.q);
    for (int i=0;i<60;i++) ps_world_step(&w, 1.f/60.f);
    float ang1 = ps_rot2_angle(b->xf.q);
    float delta = ang1 - ang0;
    /* should have rotated in the positive direction */
    EXPECT(delta > 0.1f);

    printf("Revolute motor tests: %d run, %d failed (delta=%.3f)\n", tests, fails, delta);
    return fails ? 1 : 0;
}
