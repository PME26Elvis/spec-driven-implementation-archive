#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"

static int g_failures = 0;
static int g_tests = 0;

#define EXPECT_NEAR(a, b, eps) do { \
    g_tests++; \
    if (fabsf((a) - (b)) > (eps)) { \
        fprintf(stderr, "FAIL %s:%d: %f near %f\n", __FILE__, __LINE__, (float)(a), (float)(b)); \
        g_failures++; \
    } \
} while (0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    w.gravity = ps_v2(0.0f, 9.81f);

    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s;
    memset(&s, 0, sizeof(s));
    s.type = PS_SHAPE_CIRCLE;
    s.density = 1.0f;
    s.data.circle.radius = 1.0f;
    ps_body_set_shape(b, &s);
    ps_body_set_transform(b, ps_v2(0.0f, 0.0f), 0.0f);

    /* simulate 1 second at 60Hz */
    const int steps = 60;
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < steps; i++) {
        ps_world_step(&w, dt);
    }

    /* expected: y = 0.5 * g * t^2 = 0.5 * 9.81 * 1 = 4.905 */
    EXPECT_NEAR(b->xf.p.y, 4.905f, 0.15f); /* discrete integration error */
    EXPECT_NEAR(b->linear_vel.y, 9.81f, 0.1f);
    EXPECT_NEAR(b->xf.p.x, 0.0f, 1e-5f);

    printf("Free-fall tests: %d run, %d failed\n", g_tests, g_failures);
    return g_failures ? 1 : 0;
}
