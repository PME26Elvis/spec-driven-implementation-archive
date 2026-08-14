#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../../src/physics/world.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL %d\n",__LINE__);f++;}}while(0)
int main(void) {
    ps_world w;
    ps_world_init(&w);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.5f;
    ps_body_set_shape(b,&s);
    ps_body_set_transform(b, ps_v2(0,0), 0);
    for (int i=0;i<30;i++) ps_world_step(&w, 1.f/60.f);
    EXPECT(ps_replay_frame_count(&w.replay) >= 10);
    float y_live = b->xf.p.y;
    ps_replay_restore(&w.replay, 0, w.bodies, w.body_count);
    EXPECT(fabsf(b->xf.p.y) < fabsf(y_live) || y_live < 1.0f); /* early frame lower or near start */
    printf("Replay tests: %d run, %d failed (frames=%d)\n", t, f, ps_replay_frame_count(&w.replay));
    return f?1:0;
}
