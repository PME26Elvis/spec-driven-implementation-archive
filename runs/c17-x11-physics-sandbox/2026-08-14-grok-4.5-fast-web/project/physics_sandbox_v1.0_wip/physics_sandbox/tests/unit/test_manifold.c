#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
#include "../../src/physics/collision.h"

static int fails=0, tests=0;
#define EXPECT(c) do{tests++; if(!(c)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);fails++;}}while(0)

int main(void) {
    /* two overlapping rectangles should produce manifold with 1 or 2 points */
    ps_body a={0}, b={0};
    a.type = PS_BODY_DYNAMIC; b.type = PS_BODY_DYNAMIC;
    a.xf = ps_xform_make(ps_v2(0,0), 0);
    b.xf = ps_xform_make(ps_v2(1.5f, 0.3f), 0);
    a.shape.type = PS_SHAPE_RECTANGLE;
    a.shape.data.rectangle.hx = 1.0f; a.shape.data.rectangle.hy = 1.0f;
    a.shape.friction = 0.3f;
    b.shape = a.shape;
    a.mass = 1; a.inv_mass = 1; b.mass = 1; b.inv_mass = 1;

    ps_manifold m;
    int hit = ps_collide_polygon_polygon(&a, &b, &m);
    EXPECT(hit == 1);
    EXPECT(m.point_count >= 1 && m.point_count <= 2);
    EXPECT(m.points[0].separation <= 0.0f);
    EXPECT(ps_v2_len(m.normal) > 0.9f);

    /* separated */
    b.xf.p.x = 5.0f;
    hit = ps_collide_polygon_polygon(&a, &b, &m);
    EXPECT(hit == 0);

    printf("Manifold clipping tests: %d run, %d failed (points=%d)\n", tests, fails, m.point_count);
    return fails ? 1 : 0;
}
