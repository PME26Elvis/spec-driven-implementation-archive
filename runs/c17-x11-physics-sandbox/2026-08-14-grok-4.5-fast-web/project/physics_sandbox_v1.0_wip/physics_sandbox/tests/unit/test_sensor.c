#include <stdio.h>
#include "../../src/physics/world.h"
#include "../../src/physics/sensor.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL\n");f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.5f;
    ps_body_set_shape(b,&s); ps_body_set_transform(b, ps_v2(0,0),0);
    ps_world_sync_proxies(&w);
    ps_sensor sen; ps_sensor_init(&sen, ps_v2(-2,-2), ps_v2(2,2));
    ps_sensor_update(&sen, &w);
    EXPECT(sen.overlap_count >= 1);
    printf("Sensor tests: %d run, %d failed (count=%d)\n", t, f, sen.overlap_count);
    return f?1:0;
}
