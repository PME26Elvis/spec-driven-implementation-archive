#include "body.h"
#include <string.h>

void ps_body_set_type(ps_body *b, ps_body_type type) {
    if (!b) return;
    b->type = type;
    if (type == PS_BODY_STATIC || type == PS_BODY_KINEMATIC) {
        b->inv_mass = 0.0f;
        b->inv_inertia = 0.0f;
        b->linear_vel = ps_v2_zero();
        b->angular_vel = 0.0f;
        b->force = ps_v2_zero();
        b->torque = 0.0f;
    } else {
        /* recompute from shape if needed */
        if (b->mass > 0.0f) {
            b->inv_mass = 1.0f / b->mass;
            b->inv_inertia = (b->inertia > 0.0f) ? 1.0f / b->inertia : 0.0f;
        }
    }
}

void ps_body_set_shape(ps_body *b, const ps_shape *shape) {
    if (!b || !shape) return;
    b->shape = *shape;
    ps_scalar mass, inertia;
    ps_vec2 com;
    ps_shape_compute_mass(shape, &mass, &inertia, &com);
    b->mass = mass;
    b->inertia = inertia;
    b->local_com = com;
    if (b->type == PS_BODY_DYNAMIC && mass > 0.0f) {
        b->inv_mass = 1.0f / mass;
        b->inv_inertia = (inertia > 0.0f) ? 1.0f / inertia : 0.0f;
    } else {
        b->inv_mass = 0.0f;
        b->inv_inertia = 0.0f;
    }
}

void ps_body_apply_force(ps_body *b, ps_vec2 force, ps_vec2 point) {
    if (!b || b->type != PS_BODY_DYNAMIC) return;
    b->force = ps_v2_add(b->force, force);
    ps_vec2 r = ps_v2_sub(point, ps_xform_point(b->xf, b->local_com));
    b->torque += ps_v2_cross(r, force);
    b->awake = true;
}

void ps_body_apply_impulse(ps_body *b, ps_vec2 impulse, ps_vec2 point) {
    if (!b || b->type != PS_BODY_DYNAMIC) return;
    b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(impulse, b->inv_mass));
    ps_vec2 r = ps_v2_sub(point, ps_xform_point(b->xf, b->local_com));
    b->angular_vel += b->inv_inertia * ps_v2_cross(r, impulse);
    b->awake = true;
}

void ps_body_apply_torque(ps_body *b, ps_scalar torque) {
    if (!b || b->type != PS_BODY_DYNAMIC) return;
    b->torque += torque;
    b->awake = true;
}

void ps_body_set_transform(ps_body *b, ps_vec2 pos, ps_scalar angle) {
    if (!b) return;
    b->xf.p = pos;
    b->xf.q = ps_rot2_from_angle(angle);
}
