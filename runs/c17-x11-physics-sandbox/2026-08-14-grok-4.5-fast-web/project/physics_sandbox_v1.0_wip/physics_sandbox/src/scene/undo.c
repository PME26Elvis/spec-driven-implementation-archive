#include "undo.h"
#include <string.h>

void ps_undo_init(ps_undo_stack *u) {
    memset(u, 0, sizeof(*u));
    u->top = -1;
}

void ps_undo_push(ps_undo_stack *u, const ps_world *w) {
    if (u->top >= PS_UNDO_MAX - 1) {
        memmove(&u->stack[0], &u->stack[1], sizeof(ps_undo_snapshot) * (PS_UNDO_MAX - 1));
        u->top = PS_UNDO_MAX - 2;
    }
    u->top++;
    ps_undo_snapshot *s = &u->stack[u->top];
    s->body_count = w->body_count < 128 ? w->body_count : 128;
    for (int i = 0; i < s->body_count; i++) {
        const ps_body *b = &w->bodies[i];
        s->bodies[i].id = b->id;
        s->bodies[i].type = (int)b->type;
        s->bodies[i].px = b->xf.p.x;
        s->bodies[i].py = b->xf.p.y;
        s->bodies[i].angle = ps_rot2_angle(b->xf.q);
        s->bodies[i].shape_type = (int)b->shape.type;
        if (b->shape.type == PS_SHAPE_CIRCLE)
            s->bodies[i].r_or_hx = b->shape.data.circle.radius;
        else {
            s->bodies[i].r_or_hx = b->shape.data.rectangle.hx;
            s->bodies[i].hy = b->shape.data.rectangle.hy;
        }
        s->bodies[i].density = b->shape.density;
        s->bodies[i].friction = b->shape.friction;
        s->bodies[i].restitution = b->shape.restitution;
    }
}

int ps_undo_pop(ps_undo_stack *u, ps_world *w) {
    if (u->top < 0) return 0;
    ps_undo_snapshot *s = &u->stack[u->top--];
    w->body_count = 0;
    w->joint_count = 0;
    w->next_id = 1;
    ps_bvh_init(&w->broadphase);
    for (int i = 0; i < s->body_count; i++) {
        ps_body *b = ps_world_create_body(w, (ps_body_type)s->bodies[i].type);
        if (!b) break;
        ps_shape sh = {0};
        sh.type = (ps_shape_type)s->bodies[i].shape_type;
        sh.density = s->bodies[i].density;
        sh.friction = s->bodies[i].friction;
        sh.restitution = s->bodies[i].restitution;
        if (sh.type == PS_SHAPE_CIRCLE)
            sh.data.circle.radius = s->bodies[i].r_or_hx;
        else {
            sh.data.rectangle.hx = s->bodies[i].r_or_hx;
            sh.data.rectangle.hy = s->bodies[i].hy;
        }
        ps_body_set_shape(b, &sh);
        ps_body_set_transform(b, ps_v2(s->bodies[i].px, s->bodies[i].py), s->bodies[i].angle);
        b->id = s->bodies[i].id;
        if (b->id >= w->next_id) w->next_id = b->id + 1;
    }
    return 1;
}
