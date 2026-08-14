#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../src/math/math.h"

static int g_failures = 0;
static int g_tests = 0;

#define EXPECT_TRUE(cond) do { \
    g_tests++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
    } \
} while (0)

#define EXPECT_NEAR(a, b, eps) do { \
    g_tests++; \
    if (fabsf((a) - (b)) > (eps)) { \
        fprintf(stderr, "FAIL %s:%d: %f near %f (eps %f)\n", __FILE__, __LINE__, (float)(a), (float)(b), (float)(eps)); \
        g_failures++; \
    } \
} while (0)

static void test_vec2(void) {
    ps_vec2 a = ps_v2(3.0f, 4.0f);
    EXPECT_NEAR(ps_v2_len(a), 5.0f, 1e-5f);
    EXPECT_NEAR(ps_v2_dot(a, a), 25.0f, 1e-5f);
    ps_vec2 n = ps_v2_normalize(a);
    EXPECT_NEAR(ps_v2_len(n), 1.0f, 1e-5f);
    EXPECT_NEAR(ps_v2_cross(ps_v2(1,0), ps_v2(0,1)), 1.0f, 1e-5f);
    ps_vec2 z = ps_v2_normalize(ps_v2_zero());
    EXPECT_TRUE(ps_v2_near_zero(z));
}

static void test_rot2(void) {
    ps_rot2 r = ps_rot2_from_angle(PS_PI / 2.0f);
    ps_vec2 v = ps_rot2_mul_v(r, ps_v2(1.0f, 0.0f));
    EXPECT_NEAR(v.x, 0.0f, 1e-5f);
    EXPECT_NEAR(v.y, 1.0f, 1e-5f);
    /* positive angle clockwise? Spec says positive angles rotate clockwise on screen.
       With +Y down, standard math rotation (ccw) would need sign flip for screen clockwise.
       For now we use standard math; adjust if needed later. */
}

static void test_xform(void) {
    ps_xform xf = ps_xform_make(ps_v2(10.0f, 20.0f), 0.0f);
    ps_vec2 w = ps_xform_point(xf, ps_v2(1.0f, 2.0f));
    EXPECT_NEAR(w.x, 11.0f, 1e-5f);
    EXPECT_NEAR(w.y, 22.0f, 1e-5f);
    ps_vec2 l = ps_xform_point_inv(xf, w);
    EXPECT_NEAR(l.x, 1.0f, 1e-5f);
    EXPECT_NEAR(l.y, 2.0f, 1e-5f);
}

int main(void) {
    test_vec2();
    test_rot2();
    test_xform();
    printf("Math tests: %d run, %d failed\n", g_tests, g_failures);
    return g_failures ? 1 : 0;
}
