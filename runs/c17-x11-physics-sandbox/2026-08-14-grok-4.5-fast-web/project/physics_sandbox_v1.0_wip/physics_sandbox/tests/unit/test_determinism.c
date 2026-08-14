#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../src/physics/world.h"

static void run_scene(ps_world *w, float *out_y, int n) {
    ps_world_init(w);
    ps_body *floor = ps_world_create_body(w, PS_BODY_STATIC);
    ps_shape fs = {0}; fs.type = PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=20; fs.data.rectangle.hy=1; fs.friction=0.6f;
    ps_body_set_shape(floor, &fs);
    ps_body_set_transform(floor, ps_v2(0,15), 0);
    for (int i=0;i<3;i++) {
        ps_body *b = ps_world_create_body(w, PS_BODY_DYNAMIC);
        ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.5f; s.friction=0.4f;
        ps_body_set_shape(b, &s);
        ps_body_set_transform(b, ps_v2(-2+i*2.f, 5.f), 0);
    }
    for (int i=0;i<n;i++) ps_world_step(w, 1.f/60.f);
    for (int i=0;i<3;i++) out_y[i] = w->bodies[i+1].xf.p.y;
}

int main(void) {
    ps_world w1, w2;
    float y1[3], y2[3];
    run_scene(&w1, y1, 90);
    run_scene(&w2, y2, 90);
    int ok = 1;
    for (int i=0;i<3;i++) {
        if (fabsf(y1[i]-y2[i]) > 1e-4f) ok = 0;
    }
    printf("Determinism tests: 1 run, %d failed\n", ok?0:1);
    return ok?0:1;
}
