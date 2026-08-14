#ifndef PS_BODY_H
#define PS_BODY_H

#include "shape.h"

typedef enum ps_body_type {
    PS_BODY_STATIC = 0,
    PS_BODY_DYNAMIC = 1,
    PS_BODY_KINEMATIC = 2
} ps_body_type;

typedef struct ps_body {
    uint32_t id;
    ps_body_type type;
    ps_xform xf;
    ps_xform xf_prev; /* for interpolation */
    ps_vec2 linear_vel;
    ps_scalar angular_vel;
    ps_vec2 force;
    ps_scalar torque;
    ps_scalar mass;
    ps_scalar inv_mass;
    ps_scalar inertia;
    ps_scalar inv_inertia;
    ps_vec2 local_com;
    ps_scalar linear_damping;
    ps_scalar angular_damping;
    bool awake;
    float sleep_timer;
    bool fixed_rotation;
    ps_shape shape;
    /* collision filter */
    uint16_t category_bits;
    uint16_t mask_bits;
    int16_t group_index;
    /* broadphase proxy id */
    int proxy_id;
    void *user_data;
} ps_body;

void ps_body_set_type(ps_body *b, ps_body_type type);
void ps_body_set_shape(ps_body *b, const ps_shape *shape);
void ps_body_apply_force(ps_body *b, ps_vec2 force, ps_vec2 point);
void ps_body_apply_impulse(ps_body *b, ps_vec2 impulse, ps_vec2 point);
void ps_body_apply_torque(ps_body *b, ps_scalar torque);
void ps_body_set_transform(ps_body *b, ps_vec2 pos, ps_scalar angle);

#endif /* PS_BODY_H */
