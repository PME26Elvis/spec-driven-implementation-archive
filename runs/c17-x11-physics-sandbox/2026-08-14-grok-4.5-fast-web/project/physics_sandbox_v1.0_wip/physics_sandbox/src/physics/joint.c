#include "joint.h"
#include <math.h>
#include <string.h>

void ps_joint_init_distance(ps_joint *j, ps_body *a, ps_body *b, ps_vec2 wa, ps_vec2 wb) {
    memset(j, 0, sizeof(*j));
    j->type = PS_JOINT_DISTANCE;
    j->body_a = a;
    j->body_b = b;
    j->local_anchor_a = ps_xform_point_inv(a->xf, wa);
    j->local_anchor_b = ps_xform_point_inv(b->xf, wb);
    j->length = ps_v2_len(ps_v2_sub(wb, wa));
    j->frequency_hz = 0.0f; /* rigid */
    j->damping_ratio = 0.0f;
}

void ps_joint_init_revolute(ps_joint *j, ps_body *a, ps_body *b, ps_vec2 world_anchor) {
    memset(j, 0, sizeof(*j));
    j->type = PS_JOINT_REVOLUTE;
    j->body_a = a;
    j->body_b = b;
    j->local_anchor_a = ps_xform_point_inv(a->xf, world_anchor);
    j->local_anchor_b = ps_xform_point_inv(b->xf, world_anchor);
}

void ps_joint_init_mouse(ps_joint *j, ps_body *a, ps_vec2 target) {
    memset(j, 0, sizeof(*j));
    j->type = PS_JOINT_MOUSE;
    j->body_a = a;
    j->body_b = NULL;
    j->local_anchor_a = ps_xform_point_inv(a->xf, target);
    j->target = target;
    j->max_force = 1000.0f;
    j->frequency_hz = 5.0f;
    j->damping_ratio = 0.7f;
}

void ps_joint_solve_velocity(ps_joint *j, ps_scalar dt) {
    if (!j || !j->body_a) return;
    ps_body *a = j->body_a;
    ps_body *b = j->body_b;

    if (j->type == PS_JOINT_DISTANCE && b) {
        ps_vec2 pa = ps_xform_point(a->xf, j->local_anchor_a);
        ps_vec2 pb = ps_xform_point(b->xf, j->local_anchor_b);
        ps_vec2 d = ps_v2_sub(pb, pa);
        ps_scalar len = ps_v2_len(d);
        if (len < PS_SCALAR_EPSILON) return;
        ps_vec2 n = ps_v2_div(d, len);
        ps_vec2 ra = ps_v2_sub(pa, a->xf.p);
        ps_vec2 rb = ps_v2_sub(pb, b->xf.p);
        ps_vec2 va = ps_v2_add(a->linear_vel, ps_v2_cross_s_v(a->angular_vel, ra));
        ps_vec2 vb = ps_v2_add(b->linear_vel, ps_v2_cross_s_v(b->angular_vel, rb));
        ps_scalar vn = ps_v2_dot(ps_v2_sub(vb, va), n);
        ps_scalar C = len - j->length;
        ps_scalar bias = 0.2f * C / dt;
        ps_scalar kn = a->inv_mass + b->inv_mass +
                       ps_v2_cross(ra, n)*ps_v2_cross(ra, n)*a->inv_inertia +
                       ps_v2_cross(rb, n)*ps_v2_cross(rb, n)*b->inv_inertia;
        if (kn < PS_SCALAR_EPSILON) return;
        ps_scalar lambda = -(vn + bias) / kn;
        j->impulse += lambda;
        ps_vec2 P = ps_v2_mul(n, lambda);
        a->linear_vel = ps_v2_sub(a->linear_vel, ps_v2_mul(P, a->inv_mass));
        a->angular_vel -= a->inv_inertia * ps_v2_cross(ra, P);
        b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(P, b->inv_mass));
        b->angular_vel += b->inv_inertia * ps_v2_cross(rb, P);
    } else if (j->type == PS_JOINT_MOUSE) {
        ps_vec2 pa = ps_xform_point(a->xf, j->local_anchor_a);
        ps_vec2 d = ps_v2_sub(j->target, pa);
        ps_vec2 ra = ps_v2_sub(pa, a->xf.p);
        ps_scalar omega = 2.0f * PS_PI * j->frequency_hz;
        ps_scalar damp = 2.0f * j->damping_ratio * omega;
        ps_scalar k = omega * omega;
        ps_vec2 va = ps_v2_add(a->linear_vel, ps_v2_cross_s_v(a->angular_vel, ra));
        ps_vec2 force = ps_v2_add(ps_v2_mul(d, k), ps_v2_mul(va, -damp));
        float flen = ps_v2_len(force);
        if (flen > j->max_force) force = ps_v2_mul(force, j->max_force / flen);
        a->linear_vel = ps_v2_add(a->linear_vel, ps_v2_mul(force, a->inv_mass * dt));
        a->angular_vel += a->inv_inertia * ps_v2_cross(ra, force) * dt;
    } else if (j->type == PS_JOINT_REVOLUTE && b) {
        ps_vec2 pa = ps_xform_point(a->xf, j->local_anchor_a);
        ps_vec2 pb = ps_xform_point(b->xf, j->local_anchor_b);
        ps_vec2 d = ps_v2_sub(pb, pa);
        ps_vec2 ra = ps_v2_sub(pa, a->xf.p);
        ps_vec2 rb = ps_v2_sub(pb, b->xf.p);
        ps_vec2 va = ps_v2_add(a->linear_vel, ps_v2_cross_s_v(a->angular_vel, ra));
        ps_vec2 vb = ps_v2_add(b->linear_vel, ps_v2_cross_s_v(b->angular_vel, rb));
        ps_vec2 rv = ps_v2_sub(vb, va);
        for (int axis = 0; axis < 2; axis++) {
            ps_vec2 n = axis == 0 ? ps_v2(1,0) : ps_v2(0,1);
            ps_scalar vn = ps_v2_dot(rv, n);
            ps_scalar C = ps_v2_dot(d, n);
            ps_scalar bias = 0.2f * C / dt;
            ps_scalar rna = ps_v2_cross(ra, n);
            ps_scalar rnb = ps_v2_cross(rb, n);
            ps_scalar kn = a->inv_mass + b->inv_mass + rna*rna*a->inv_inertia + rnb*rnb*b->inv_inertia;
            if (kn < 1e-8f) continue;
            ps_scalar lambda = -(vn + bias) / kn;
            ps_vec2 P = ps_v2_mul(n, lambda);
            a->linear_vel = ps_v2_sub(a->linear_vel, ps_v2_mul(P, a->inv_mass));
            a->angular_vel -= a->inv_inertia * ps_v2_cross(ra, P);
            b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(P, b->inv_mass));
            b->angular_vel += b->inv_inertia * ps_v2_cross(rb, P);
        }
        if (j->enable_limit) {
            float ang_a = ps_rot2_angle(a->xf.q);
            float ang_b = ps_rot2_angle(b->xf.q);
            float rel = ang_b - ang_a;
            if (rel < j->lower_angle || rel > j->upper_angle) {
                float target = rel < j->lower_angle ? j->lower_angle : j->upper_angle;
                float Cang = rel - target;
                float bias_a = 0.2f * Cang / dt;
                float kn_a = a->inv_inertia + b->inv_inertia;
                if (kn_a > 1e-8f) {
                    float lam = -(a->angular_vel - b->angular_vel + bias_a) / kn_a;
                    a->angular_vel -= a->inv_inertia * lam;
                    b->angular_vel += b->inv_inertia * lam;
                }
            }
        }
        /* motor */
        if (j->enable_motor && j->max_motor_torque > 0.0f) {
            float rel_w = b->angular_vel - a->angular_vel - j->motor_speed;
            float kn = a->inv_inertia + b->inv_inertia;
            if (kn > 1e-8f) {
                float lam = -rel_w / kn;
                float max_lam = j->max_motor_torque * dt;
                float old_imp = j->motor_impulse;
                j->motor_impulse = fmaxf(-max_lam, fminf(old_imp + lam, max_lam));
                float dlam = j->motor_impulse - old_imp;
                a->angular_vel -= a->inv_inertia * dlam;
                b->angular_vel += b->inv_inertia * dlam;
            }
        }
    }
}

void ps_joint_solve_position(ps_joint *j) {
    (void)j;
}
