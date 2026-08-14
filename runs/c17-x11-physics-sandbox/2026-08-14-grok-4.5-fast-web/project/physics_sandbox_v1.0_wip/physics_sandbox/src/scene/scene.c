#include "scene.h"
#include <string.h>

int ps_scene_load_starter(ps_world *w) {
    if (!w) return -1;
    w->body_count = 0;
    w->next_id = 1;
    /* floor */
    ps_body *floor = ps_world_create_body(w, PS_BODY_STATIC);
    ps_shape fs = {0};
    fs.type = PS_SHAPE_RECTANGLE;
    fs.data.rectangle.hx = 40.f; fs.data.rectangle.hy = 1.f;
    fs.friction = 0.5f;
    ps_body_set_shape(floor, &fs);
    ps_body_set_transform(floor, ps_v2(0, 18), 0);
    /* several dynamics */
    for (int i = 0; i < 6; i++) {
        ps_body *b = ps_world_create_body(w, PS_BODY_DYNAMIC);
        ps_shape s = {0};
        if (i % 2 == 0) {
            s.type = PS_SHAPE_CIRCLE;
            s.data.circle.radius = 0.6f + 0.2f*(i%3);
        } else {
            s.type = PS_SHAPE_RECTANGLE;
            s.data.rectangle.hx = 0.7f; s.data.rectangle.hy = 0.7f;
        }
        s.density = 1.0f;
        s.friction = 0.3f;
        s.restitution = 0.25f;
        ps_body_set_shape(b, &s);
        ps_body_set_transform(b, ps_v2(-6.f + i*2.5f, 2.f + (i%2)*3.f), 0.1f*i);
    }
    return 0;
}
