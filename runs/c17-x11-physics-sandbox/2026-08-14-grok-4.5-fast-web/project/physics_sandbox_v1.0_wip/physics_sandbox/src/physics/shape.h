#ifndef PS_SHAPE_H
#define PS_SHAPE_H

#include "../math/math.h"

typedef enum ps_shape_type {
    PS_SHAPE_CIRCLE = 0,
    PS_SHAPE_RECTANGLE = 1,
    PS_SHAPE_POLYGON = 2
} ps_shape_type;

#define PS_MAX_POLYGON_VERTICES 16

typedef struct ps_shape {
    ps_shape_type type;
    ps_scalar density;
    ps_scalar friction;
    ps_scalar restitution;
    ps_scalar rolling_resistance;
    /* local AABB relative to body origin (shape local) */
    ps_vec2 local_aabb_min;
    ps_vec2 local_aabb_max;
    union {
        struct {
            ps_scalar radius;
        } circle;
        struct {
            ps_scalar hx; /* half width */
            ps_scalar hy; /* half height */
        } rectangle;
        struct {
            int count;
            ps_vec2 vertices[PS_MAX_POLYGON_VERTICES];
            ps_vec2 normals[PS_MAX_POLYGON_VERTICES];
            ps_vec2 centroid;
        } polygon;
    } data;
} ps_shape;

void ps_shape_compute_mass(const ps_shape *shape, ps_scalar *mass, ps_scalar *inertia, ps_vec2 *com);
void ps_shape_compute_aabb(const ps_shape *shape, ps_xform xf, ps_vec2 *out_min, ps_vec2 *out_max);
bool ps_shape_validate(const ps_shape *shape);

#endif /* PS_SHAPE_H */
