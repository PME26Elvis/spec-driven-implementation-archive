#include "shape.h"
#include <string.h>
#include <math.h>

void ps_shape_compute_mass(const ps_shape *shape, ps_scalar *mass, ps_scalar *inertia, ps_vec2 *com) {
    *mass = 0.0f;
    *inertia = 0.0f;
    *com = ps_v2_zero();
    if (!shape || shape->density <= 0.0f) return;

    switch (shape->type) {
    case PS_SHAPE_CIRCLE: {
        ps_scalar r = shape->data.circle.radius;
        *mass = shape->density * PS_PI * r * r;
        *inertia = 0.5f * (*mass) * r * r;
        *com = ps_v2_zero();
        break;
    }
    case PS_SHAPE_RECTANGLE: {
        ps_scalar w = 2.0f * shape->data.rectangle.hx;
        ps_scalar h = 2.0f * shape->data.rectangle.hy;
        *mass = shape->density * w * h;
        *inertia = (*mass) * (w * w + h * h) / 12.0f;
        *com = ps_v2_zero();
        break;
    }
    case PS_SHAPE_POLYGON: {
        /* Compute area, centroid, inertia for convex polygon about centroid */
        int n = shape->data.polygon.count;
        if (n < 3) return;
        const ps_vec2 *v = shape->data.polygon.vertices;
        ps_scalar area = 0.0f;
        ps_vec2 centroid = ps_v2_zero();
        ps_scalar I = 0.0f;

        /* Use triangulation from vertex 0 */
        for (int i = 1; i < n - 1; i++) {
            ps_vec2 e1 = ps_v2_sub(v[i], v[0]);
            ps_vec2 e2 = ps_v2_sub(v[i + 1], v[0]);
            ps_scalar a = 0.5f * ps_v2_cross(e1, e2);
            area += a;
            /* triangle centroid relative to v0 */
            ps_vec2 tri_c = ps_v2_mul(ps_v2_add(e1, e2), 1.0f / 3.0f);
            centroid = ps_v2_add(centroid, ps_v2_mul(tri_c, a));
        }
        if (fabsf(area) < PS_SCALAR_EPSILON) return;
        centroid = ps_v2_div(centroid, area);
        centroid = ps_v2_add(centroid, v[0]);

        /* Inertia about centroid - simplified using parallel axis later if needed */
        /* For uniform density, integrate */
        for (int i = 0; i < n; i++) {
            ps_vec2 p1 = ps_v2_sub(v[i], centroid);
            ps_vec2 p2 = ps_v2_sub(v[(i + 1) % n], centroid);
            ps_scalar cross = ps_v2_cross(p1, p2);
            I += cross * (ps_v2_dot(p1, p1) + ps_v2_dot(p1, p2) + ps_v2_dot(p2, p2));
        }
        I = (shape->density / 6.0f) * I;
        *mass = shape->density * area;
        *inertia = I;
        *com = centroid;
        break;
    }
    }
}

void ps_shape_compute_aabb(const ps_shape *shape, ps_xform xf, ps_vec2 *out_min, ps_vec2 *out_max) {
    if (!shape) {
        *out_min = *out_max = ps_v2_zero();
        return;
    }
    switch (shape->type) {
    case PS_SHAPE_CIRCLE: {
        ps_scalar r = shape->data.circle.radius;
        *out_min = ps_v2(xf.p.x - r, xf.p.y - r);
        *out_max = ps_v2(xf.p.x + r, xf.p.y + r);
        break;
    }
    case PS_SHAPE_RECTANGLE: {
        ps_vec2 local_corners[4] = {
            ps_v2(-shape->data.rectangle.hx, -shape->data.rectangle.hy),
            ps_v2( shape->data.rectangle.hx, -shape->data.rectangle.hy),
            ps_v2( shape->data.rectangle.hx,  shape->data.rectangle.hy),
            ps_v2(-shape->data.rectangle.hx,  shape->data.rectangle.hy)
        };
        *out_min = ps_v2(1e30f, 1e30f);
        *out_max = ps_v2(-1e30f, -1e30f);
        for (int i = 0; i < 4; i++) {
            ps_vec2 w = ps_xform_point(xf, local_corners[i]);
            *out_min = ps_v2_min(*out_min, w);
            *out_max = ps_v2_max(*out_max, w);
        }
        break;
    }
    case PS_SHAPE_POLYGON: {
        *out_min = ps_v2(1e30f, 1e30f);
        *out_max = ps_v2(-1e30f, -1e30f);
        for (int i = 0; i < shape->data.polygon.count; i++) {
            ps_vec2 w = ps_xform_point(xf, shape->data.polygon.vertices[i]);
            *out_min = ps_v2_min(*out_min, w);
            *out_max = ps_v2_max(*out_max, w);
        }
        break;
    }
    }
}

bool ps_shape_validate(const ps_shape *shape) {
    if (!shape) return false;
    if (shape->density < 0.0f) return false;
    switch (shape->type) {
    case PS_SHAPE_CIRCLE:
        return shape->data.circle.radius > PS_SCALAR_EPSILON;
    case PS_SHAPE_RECTANGLE:
        return shape->data.rectangle.hx > PS_SCALAR_EPSILON &&
               shape->data.rectangle.hy > PS_SCALAR_EPSILON;
    case PS_SHAPE_POLYGON:
        if (shape->data.polygon.count < 3 || shape->data.polygon.count > PS_MAX_POLYGON_VERTICES)
            return false;
        /* basic convexity / area check omitted for brevity; full validation later */
        return true;
    }
    return false;
}
