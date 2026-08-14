#ifndef PS_JOINT_H
#define PS_JOINT_H

#include "body.h"

typedef enum ps_joint_type {
    PS_JOINT_DISTANCE = 0,
    PS_JOINT_REVOLUTE = 1,
    PS_JOINT_MOUSE = 2
} ps_joint_type;

typedef struct ps_joint {
    ps_joint_type type;
    ps_body *body_a;
    ps_body *body_b;
    ps_vec2 local_anchor_a;
    ps_vec2 local_anchor_b;
    /* distance */
    ps_scalar length;
    ps_scalar frequency_hz;
    ps_scalar damping_ratio;
    /* revolute */
    bool enable_limit;
    ps_scalar lower_angle;
    ps_scalar upper_angle;
    bool enable_motor;
    ps_scalar motor_speed;
    ps_scalar max_motor_torque;
    /* mouse */
    ps_vec2 target;
    ps_scalar max_force;
    /* solver state */
    ps_scalar impulse;
    ps_scalar motor_impulse;
} ps_joint;

#define PS_MAX_JOINTS 128

void ps_joint_init_distance(ps_joint *j, ps_body *a, ps_body *b, ps_vec2 world_anchor_a, ps_vec2 world_anchor_b);
void ps_joint_init_revolute(ps_joint *j, ps_body *a, ps_body *b, ps_vec2 world_anchor);
void ps_joint_init_mouse(ps_joint *j, ps_body *a, ps_vec2 target);
void ps_joint_solve_velocity(ps_joint *j, ps_scalar dt);
void ps_joint_solve_position(ps_joint *j);

#endif /* PS_JOINT_H */
