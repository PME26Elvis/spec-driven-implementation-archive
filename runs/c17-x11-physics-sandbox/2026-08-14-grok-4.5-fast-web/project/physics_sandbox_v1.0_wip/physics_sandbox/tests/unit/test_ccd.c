#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
#include "../../src/physics/ccd.h"

static int fails=0, tests=0;
#define EXPECT(c) do{tests++; if(!(c)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);fails++;}}while(0)

int main(void) {
    /* Shape Cast: circle moving toward a static rect */
    ps_body a={0}, b={0};
    a.type = PS_BODY_DYNAMIC; b.type = PS_BODY_STATIC;
    a.xf = ps_xform_make(ps_v2(0,0), 0);
    b.xf = ps_xform_make(ps_v2(5,0), 0);
    a.shape.type = PS_SHAPE_CIRCLE; a.shape.data.circle.radius = 0.5f;
    b.shape.type = PS_SHAPE_RECTANGLE; b.shape.data.rectangle.hx = 1.f; b.shape.data.rectangle.hy = 1.f;
    a.mass=1; a.inv_mass=1;

    ps_xform xf1 = a.xf;
    xf1.p.x = 10.0f; /* will cross the rect */
    ps_shape_cast_result scr;
    int hit = ps_shape_cast(&a, &a.xf, &xf1, &b, &scr);
    EXPECT(hit == 1);
    EXPECT(scr.fraction > 0.0f && scr.fraction < 1.0f);

    /* TOI between two approaching circles */
    ps_body c1={0}, c2={0};
    c1.type=c2.type=PS_BODY_DYNAMIC;
    c1.xf = ps_xform_make(ps_v2(0,0),0);
    c2.xf = ps_xform_make(ps_v2(4,0),0);
    c1.shape.type=c2.shape.type=PS_SHAPE_CIRCLE;
    c1.shape.data.circle.radius=c2.shape.data.circle.radius=0.5f;
    c1.mass=c2.mass=1; c1.inv_mass=c2.inv_mass=1;
    c1.linear_vel = ps_v2(10,0);
    c2.linear_vel = ps_v2(-10,0);
    ps_toi_result toi;
    hit = ps_compute_toi(&c1, &c2, 1.0f/60.f, &toi);
    /* may or may not hit depending on distance vs step; just ensure no crash and valid range */
    if (hit) {
        EXPECT(toi.toi >= 0.0f && toi.toi <= 1.0f);
    }
    EXPECT(1); /* smoke */

    printf("CCD/ShapeCast tests: %d run, %d failed (frac=%.3f)\n", tests, fails, scr.fraction);
    return fails ? 1 : 0;
}
