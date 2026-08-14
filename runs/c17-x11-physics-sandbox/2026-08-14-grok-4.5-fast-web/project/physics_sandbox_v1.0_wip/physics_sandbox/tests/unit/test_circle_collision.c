#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
#include "../../src/physics/collision.h"

static int g_failures = 0;
static int g_tests = 0;

#define EXPECT_TRUE(c) do { g_tests++; if (!(c)) { fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); g_failures++; } } while(0)
#define EXPECT_NEAR(a,b,e) do { g_tests++; if (fabsf((a)-(b))>(e)) { fprintf(stderr, "FAIL %s:%d %f != %f\n", __FILE__, __LINE__, (float)(a), (float)(b)); g_failures++; } } while(0)

int main(void) {
    ps_body a = {0}, b = {0};
    a.type = PS_BODY_DYNAMIC;
    b.type = PS_BODY_DYNAMIC;
    a.xf = ps_xform_make(ps_v2(0,0), 0);
    b.xf = ps_xform_make(ps_v2(1.5f, 0), 0);
    a.shape.type = PS_SHAPE_CIRCLE;
    a.shape.data.circle.radius = 1.0f;
    a.shape.friction = 0.3f;
    a.shape.restitution = 0.0f;
    b.shape = a.shape;
    a.mass = 1.0f; a.inv_mass = 1.0f;
    b.mass = 1.0f; b.inv_mass = 1.0f;

    ps_manifold m;
    int hit = ps_collide_circle_circle(&a, &b, &m);
    EXPECT_TRUE(hit == 1);
    EXPECT_NEAR(m.normal.x, 1.0f, 1e-4f);
    EXPECT_NEAR(m.points[0].separation, -0.5f, 1e-4f); /* penetration 0.5 */

    /* no collision */
    b.xf.p.x = 3.0f;
    hit = ps_collide_circle_circle(&a, &b, &m);
    EXPECT_TRUE(hit == 0);

    printf("Circle collision tests: %d run, %d failed\n", g_tests, g_failures);
    return g_failures ? 1 : 0;
}
