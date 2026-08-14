#ifndef PS_XFORM_H
#define PS_XFORM_H

#include "rot2.h"

typedef struct ps_xform {
    ps_vec2 p; /* position */
    ps_rot2 q; /* rotation */
} ps_xform;

static inline ps_xform ps_xform_identity(void) {
    ps_xform x;
    x.p = ps_v2_zero();
    x.q = ps_rot2_identity();
    return x;
}

static inline ps_xform ps_xform_make(ps_vec2 p, ps_scalar angle) {
    ps_xform x;
    x.p = p;
    x.q = ps_rot2_from_angle(angle);
    return x;
}

static inline ps_vec2 ps_xform_point(ps_xform xf, ps_vec2 local) {
    return ps_v2_add(ps_rot2_mul_v(xf.q, local), xf.p);
}

static inline ps_vec2 ps_xform_point_inv(ps_xform xf, ps_vec2 world) {
    return ps_rot2_mul_t_v(xf.q, ps_v2_sub(world, xf.p));
}

static inline ps_vec2 ps_xform_vec(ps_xform xf, ps_vec2 local) {
    return ps_rot2_mul_v(xf.q, local);
}

static inline ps_vec2 ps_xform_vec_inv(ps_xform xf, ps_vec2 world) {
    return ps_rot2_mul_t_v(xf.q, world);
}

#endif /* PS_XFORM_H */
