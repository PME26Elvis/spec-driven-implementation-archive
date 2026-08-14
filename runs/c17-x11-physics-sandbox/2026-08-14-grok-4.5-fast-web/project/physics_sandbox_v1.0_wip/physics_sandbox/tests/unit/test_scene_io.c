#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
#include "../../src/scene/scene_io.h"

static int fails=0, tests=0;
#define EXPECT(c) do{tests++; if(!(c)){fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__);fails++;}}while(0)

int main(void) {
    ps_world w;
    ps_world_init(&w);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.7f; s.friction=0.4f;
    ps_body_set_shape(b, &s);
    ps_body_set_transform(b, ps_v2(3.5f, -2.0f), 0.3f);

    EXPECT(ps_scene_save_json(&w, "/tmp/test_scene.json") == 0);

    ps_world w2;
    ps_world_init(&w2);
    EXPECT(ps_scene_load_json(&w2, "/tmp/test_scene.json") == 0);
    EXPECT(w2.body_count >= 1);
    /* find a dynamic circle near 3.5 */
    int found = 0;
    for (int i=0;i<w2.body_count;i++) {
        if (w2.bodies[i].type == PS_BODY_DYNAMIC &&
            fabsf(w2.bodies[i].xf.p.x - 3.5f) < 0.1f) found = 1;
    }
    EXPECT(found);

    printf("Scene IO tests: %d run, %d failed\n", tests, fails);
    return fails ? 1 : 0;
}
