#ifndef PS_COLLISION_H
#define PS_COLLISION_H

#include "body.h"

#define PS_MAX_MANIFOLD_POINTS 2

typedef struct ps_contact_point {
    ps_vec2 local_point_a;
    ps_vec2 local_point_b;
    ps_vec2 world_point;
    ps_scalar normal_impulse;
    ps_scalar tangent_impulse;
    ps_scalar separation;
} ps_contact_point;

typedef struct ps_manifold {
    ps_body *body_a;
    ps_body *body_b;
    ps_vec2 normal; /* from A to B */
    int point_count;
    ps_contact_point points[PS_MAX_MANIFOLD_POINTS];
    ps_scalar friction;
    ps_scalar restitution;
} ps_manifold;

int ps_collide_circle_circle(const ps_body *a, const ps_body *b, ps_manifold *m);
int ps_collide_circle_polygon(const ps_body *a, const ps_body *b, ps_manifold *m);
int ps_collide_polygon_polygon(const ps_body *a, const ps_body *b, ps_manifold *m);

#endif /* PS_COLLISION_H */
