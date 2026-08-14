#include "collision.h"
#include <math.h>
#include <string.h>
#include <float.h>

int ps_collide_circle_circle(const ps_body *a, const ps_body *b, ps_manifold *m) {
    if (!a || !b || !m) return 0;
    if (a->shape.type != PS_SHAPE_CIRCLE || b->shape.type != PS_SHAPE_CIRCLE) return 0;

    ps_vec2 pa = a->xf.p;
    ps_vec2 pb = b->xf.p;
    ps_scalar ra = a->shape.data.circle.radius;
    ps_scalar rb = b->shape.data.circle.radius;

    ps_vec2 d = ps_v2_sub(pb, pa);
    ps_scalar dist_sq = ps_v2_len_sq(d);
    ps_scalar rsum = ra + rb;
    if (dist_sq > rsum * rsum) return 0;

    memset(m, 0, sizeof(*m));
    m->body_a = (ps_body *)a;
    m->body_b = (ps_body *)b;
    m->point_count = 1;
    m->friction = sqrtf(fmaxf(0.0f, a->shape.friction * b->shape.friction));
    m->restitution = fmaxf(a->shape.restitution, b->shape.restitution);

    ps_scalar dist = sqrtf(dist_sq);
    if (dist > PS_SCALAR_EPSILON) {
        m->normal = ps_v2_div(d, dist);
    } else {
        m->normal = ps_v2(1.0f, 0.0f);
        dist = 0.0f;
    }
    m->points[0].world_point = ps_v2_add(pa, ps_v2_mul(m->normal, ra));
    m->points[0].separation = dist - rsum;
    m->points[0].local_point_a = ps_xform_point_inv(a->xf, m->points[0].world_point);
    m->points[0].local_point_b = ps_xform_point_inv(b->xf, m->points[0].world_point);
    return 1;
}

/* Circle vs rectangle / convex polygon (treat rect as 4-vert poly) */
static int collide_circle_poly(const ps_body *circle_body, const ps_body *poly_body, ps_manifold *m) {
    const ps_shape *cs = &circle_body->shape;
    const ps_shape *ps = &poly_body->shape;
    if (cs->type != PS_SHAPE_CIRCLE) return 0;

    ps_vec2 center = circle_body->xf.p;
    ps_scalar radius = cs->data.circle.radius;

    /* get polygon vertices in world */
    ps_vec2 verts[PS_MAX_POLYGON_VERTICES];
    int n = 0;
    if (ps->type == PS_SHAPE_RECTANGLE) {
        ps_vec2 local[4] = {
            ps_v2(-ps->data.rectangle.hx, -ps->data.rectangle.hy),
            ps_v2( ps->data.rectangle.hx, -ps->data.rectangle.hy),
            ps_v2( ps->data.rectangle.hx,  ps->data.rectangle.hy),
            ps_v2(-ps->data.rectangle.hx,  ps->data.rectangle.hy)
        };
        n = 4;
        for (int i = 0; i < 4; i++) verts[i] = ps_xform_point(poly_body->xf, local[i]);
    } else if (ps->type == PS_SHAPE_POLYGON) {
        n = ps->data.polygon.count;
        for (int i = 0; i < n; i++) verts[i] = ps_xform_point(poly_body->xf, ps->data.polygon.vertices[i]);
    } else return 0;
    if (n < 3) return 0;

    /* Find closest edge / penetration using SAT-like separation */
    float best_sep = -FLT_MAX;
    ps_vec2 best_normal = ps_v2(0,0);
    int best_edge = -1;

    for (int i = 0; i < n; i++) {
        ps_vec2 v0 = verts[i];
        ps_vec2 v1 = verts[(i + 1) % n];
        ps_vec2 edge = ps_v2_sub(v1, v0);
        ps_vec2 normal = ps_v2_normalize(ps_v2_perp(edge)); /* outward approx */
        /* ensure outward by checking against centroid */
        ps_vec2 centroid = ps_v2_zero();
        for (int k = 0; k < n; k++) centroid = ps_v2_add(centroid, verts[k]);
        centroid = ps_v2_div(centroid, (ps_scalar)n);
        if (ps_v2_dot(normal, ps_v2_sub(v0, centroid)) < 0) normal = ps_v2_mul(normal, -1.0f);

        float sep = ps_v2_dot(ps_v2_sub(center, v0), normal) - radius;
        if (sep > best_sep) {
            best_sep = sep;
            best_normal = normal;
            best_edge = i;
        }
    }

    if (best_sep > 0.0f) return 0; /* separated */

    /* also check against vertices for corner cases */
    float min_dist = FLT_MAX;
    ps_vec2 closest = center;
    for (int i = 0; i < n; i++) {
        float d = ps_v2_len(ps_v2_sub(center, verts[i]));
        if (d < min_dist) { min_dist = d; closest = verts[i]; }
    }
    if (min_dist < radius && min_dist < -best_sep + radius) {
        /* vertex is closer */
        ps_vec2 nrm = ps_v2_normalize(ps_v2_sub(center, closest));
        best_normal = nrm;
        best_sep = min_dist - radius;
    }

    memset(m, 0, sizeof(*m));
    m->body_a = (ps_body *)circle_body;
    m->body_b = (ps_body *)poly_body;
    m->point_count = 1;
    m->normal = best_normal;
    m->friction = sqrtf(fmaxf(0.0f, cs->friction * ps->friction));
    m->restitution = fmaxf(cs->restitution, ps->restitution);
    m->points[0].world_point = ps_v2_sub(center, ps_v2_mul(best_normal, radius));
    m->points[0].separation = best_sep;
    m->points[0].local_point_a = ps_xform_point_inv(circle_body->xf, m->points[0].world_point);
    m->points[0].local_point_b = ps_xform_point_inv(poly_body->xf, m->points[0].world_point);
    (void)best_edge;
    return 1;
}

int ps_collide_circle_polygon(const ps_body *a, const ps_body *b, ps_manifold *m) {
    if (a->shape.type == PS_SHAPE_CIRCLE)
        return collide_circle_poly(a, b, m);
    else if (b->shape.type == PS_SHAPE_CIRCLE) {
        int hit = collide_circle_poly(b, a, m);
        if (hit) {
            /* flip normal and bodies */
            m->normal = ps_v2_mul(m->normal, -1.0f);
            ps_body *tmp = m->body_a;
            m->body_a = m->body_b;
            m->body_b = tmp;
        }
        return hit;
    }
    return 0;
}


/* Basic SAT for convex polygons (including rectangles treated as polys) */



static void get_poly_verts(const ps_body *b, ps_vec2 *out, int *n) {
    const ps_shape *s = &b->shape;
    if (s->type == PS_SHAPE_RECTANGLE) {
        ps_vec2 local[4] = {
            ps_v2(-s->data.rectangle.hx, -s->data.rectangle.hy),
            ps_v2( s->data.rectangle.hx, -s->data.rectangle.hy),
            ps_v2( s->data.rectangle.hx,  s->data.rectangle.hy),
            ps_v2(-s->data.rectangle.hx,  s->data.rectangle.hy)
        };
        *n = 4;
        for (int i = 0; i < 4; i++) out[i] = ps_xform_point(b->xf, local[i]);
    } else if (s->type == PS_SHAPE_POLYGON) {
        *n = s->data.polygon.count;
        for (int i = 0; i < *n; i++) out[i] = ps_xform_point(b->xf, s->data.polygon.vertices[i]);
    } else {
        *n = 0;
    }
}

/* Sutherland-Hodgman style edge clipping for contact manifolds */

typedef struct {
    ps_vec2 v;
    float sep;
} clip_vertex;

static int clip_segment(clip_vertex *in, int in_count, clip_vertex *out,
                        ps_vec2 normal, float offset) {
    int out_count = 0;
    clip_vertex a = in[in_count - 1];
    for (int i = 0; i < in_count; i++) {
        clip_vertex b = in[i];
        float da = ps_v2_dot(normal, a.v) - offset;
        float db = ps_v2_dot(normal, b.v) - offset;
        if (da <= 0.0f && db <= 0.0f) {
            out[out_count++] = b;
        } else if (da <= 0.0f && db > 0.0f) {
            float t = da / (da - db);
            clip_vertex c;
            c.v = ps_v2_add(a.v, ps_v2_mul(ps_v2_sub(b.v, a.v), t));
            c.sep = 0.0f;
            out[out_count++] = c;
        } else if (da > 0.0f && db <= 0.0f) {
            float t = da / (da - db);
            clip_vertex c;
            c.v = ps_v2_add(a.v, ps_v2_mul(ps_v2_sub(b.v, a.v), t));
            c.sep = 0.0f;
            out[out_count++] = c;
            out[out_count++] = b;
        }
        a = b;
    }
    return out_count;
}

/* For two polygons: find best separating axis, identify reference edge,
   clip incident edge against reference side planes, keep points with negative separation */

static int generate_poly_manifold(const ps_body *body_a, const ps_body *body_b,
                                  ps_vec2 *verts_a, int na,
                                  ps_vec2 *verts_b, int nb,
                                  ps_manifold *m) {
    float min_overlap = 1e30f;
    ps_vec2 best_normal = ps_v2(1,0);
    int ref_edge = 0;
    int flip = 0; /* 0 = A is reference, 1 = B is reference */

    /* Test axes of A */
    for (int i = 0; i < na; i++) {
        ps_vec2 e = ps_v2_sub(verts_a[(i+1)%na], verts_a[i]);
        float elen = ps_v2_len(e);
        if (elen < 1e-8f) continue;
        ps_vec2 axis = ps_v2( -e.y/elen, e.x/elen ); /* outward-ish */
        /* project */
        float minA=1e30f, maxA=-1e30f, minB=1e30f, maxB=-1e30f;
        for (int k=0;k<na;k++){ float p=ps_v2_dot(verts_a[k],axis); if(p<minA)minA=p; if(p>maxA)maxA=p; }
        for (int k=0;k<nb;k++){ float p=ps_v2_dot(verts_b[k],axis); if(p<minB)minB=p; if(p>maxB)maxB=p; }
        float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
        if (overlap <= 0.0f) return 0;
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_normal = axis;
            ref_edge = i;
            flip = 0;
            /* ensure normal points from A to B */
            ps_vec2 midA={0}, midB={0};
            for(int k=0;k<na;k++) midA=ps_v2_add(midA,verts_a[k]);
            for(int k=0;k<nb;k++) midB=ps_v2_add(midB,verts_b[k]);
            midA=ps_v2_div(midA,(float)na); midB=ps_v2_div(midB,(float)nb);
            if (ps_v2_dot(ps_v2_sub(midB,midA), best_normal) < 0) best_normal = ps_v2_mul(best_normal,-1.f);
        }
    }
    /* Test axes of B */
    for (int i = 0; i < nb; i++) {
        ps_vec2 e = ps_v2_sub(verts_b[(i+1)%nb], verts_b[i]);
        float elen = ps_v2_len(e);
        if (elen < 1e-8f) continue;
        ps_vec2 axis = ps_v2( -e.y/elen, e.x/elen );
        float minA=1e30f, maxA=-1e30f, minB=1e30f, maxB=-1e30f;
        for (int k=0;k<na;k++){ float p=ps_v2_dot(verts_a[k],axis); if(p<minA)minA=p; if(p>maxA)maxA=p; }
        for (int k=0;k<nb;k++){ float p=ps_v2_dot(verts_b[k],axis); if(p<minB)minB=p; if(p>maxB)maxB=p; }
        float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
        if (overlap <= 0.0f) return 0;
        if (overlap < min_overlap) {
            min_overlap = overlap;
            best_normal = axis;
            ref_edge = i;
            flip = 1;
            ps_vec2 midA={0}, midB={0};
            for(int k=0;k<na;k++) midA=ps_v2_add(midA,verts_a[k]);
            for(int k=0;k<nb;k++) midB=ps_v2_add(midB,verts_b[k]);
            midA=ps_v2_div(midA,(float)na); midB=ps_v2_div(midB,(float)nb);
            if (ps_v2_dot(ps_v2_sub(midB,midA), best_normal) < 0) best_normal = ps_v2_mul(best_normal,-1.f);
        }
    }

    /* Reference face is the one whose normal is closest to best_normal */
    ps_vec2 *ref_v, *inc_v;
    int nref, ninc, re;
    if (!flip) {
        ref_v = verts_a; nref = na; re = ref_edge;
        inc_v = verts_b; ninc = nb;
    } else {
        ref_v = verts_b; nref = nb; re = ref_edge;
        inc_v = verts_a; ninc = na;
    }

    ps_vec2 rv0 = ref_v[re];
    ps_vec2 rv1 = ref_v[(re+1)%nref];
    ps_vec2 redge = ps_v2_normalize(ps_v2_sub(rv1, rv0));

    /* incident edge = most opposite to normal */
    int ii = 0;
    float mind = 1e30f;
    for (int i=0;i<ninc;i++) {
        ps_vec2 e = ps_v2_sub(inc_v[(i+1)%ninc], inc_v[i]);
        float el = ps_v2_len(e);
        if (el < 1e-8f) continue;
        ps_vec2 en = ps_v2(-e.y/el, e.x/el);
        float d = ps_v2_dot(en, best_normal);
        if (d < mind) { mind = d; ii = i; }
    }

    clip_vertex inc[2];
    inc[0].v = inc_v[ii]; inc[1].v = inc_v[(ii+1)%ninc];
    inc[0].sep = inc[1].sep = 0;

    clip_vertex c1[4], c2[4];
    /* clip against side planes of reference edge */
    int n1 = clip_segment(inc, 2, c1, ps_v2_mul(redge,-1.f), -ps_v2_dot(redge, rv0));
    if (n1 < 2) return 0;
    int n2 = clip_segment(c1, n1, c2, redge, ps_v2_dot(redge, rv1));
    if (n2 < 1) return 0;

    memset(m, 0, sizeof(*m));
    m->body_a = (ps_body*)body_a;
    m->body_b = (ps_body*)body_b;
    m->normal = best_normal;
    m->friction = 0.3f;
    m->restitution = 0.0f;
    m->point_count = 0;

    float ref_off = ps_v2_dot(best_normal, rv0);
    for (int i=0; i<n2 && m->point_count < PS_MAX_MANIFOLD_POINTS; i++) {
        float sep = ps_v2_dot(best_normal, c2[i].v) - ref_off;
        /* when flip, the normal was already oriented A->B, but ref face is on B,
           so separation sign needs care; keep points with negative relative penetration */
        if ((!flip && sep <= 0.0f) || (flip && sep >= 0.0f)) {
            /* for flip case invert sep */
            if (flip) sep = -sep;
            if (sep > 0) continue;
            ps_contact_point *cp = &m->points[m->point_count++];
            cp->world_point = c2[i].v;
            cp->separation = sep;
            cp->local_point_a = ps_xform_point_inv(body_a->xf, cp->world_point);
            cp->local_point_b = ps_xform_point_inv(body_b->xf, cp->world_point);
        }
    }
    /* fallback: if clipping produced nothing, still emit one approximate point */
    if (m->point_count == 0) {
        m->point_count = 1;
        m->points[0].world_point = ps_v2_mul(ps_v2_add(body_a->xf.p, body_b->xf.p), 0.5f);
        m->points[0].separation = -min_overlap;
        m->points[0].local_point_a = ps_xform_point_inv(body_a->xf, m->points[0].world_point);
        m->points[0].local_point_b = ps_xform_point_inv(body_b->xf, m->points[0].world_point);
    }
    return 1;
}

int ps_collide_polygon_polygon(const ps_body *a, const ps_body *b, ps_manifold *m) {
    if (!a || !b || !m) return 0;
    ps_vec2 va[PS_MAX_POLYGON_VERTICES], vb[PS_MAX_POLYGON_VERTICES];
    int na = 0, nb = 0;
    get_poly_verts(a, va, &na);
    get_poly_verts(b, vb, &nb);
    if (na < 3 || nb < 3) return 0;
    return generate_poly_manifold(a, b, va, na, vb, nb, m);
}
