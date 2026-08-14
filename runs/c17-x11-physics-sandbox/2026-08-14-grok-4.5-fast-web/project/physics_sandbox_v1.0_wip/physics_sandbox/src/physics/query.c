#include "query.h"

typedef struct {
    ps_world *w;
    ps_query_cb user_cb;
    void *user_ctx;
} qctx;

static void bridge(int proxy_id, void *ctx) {
    qctx *c = (qctx*)ctx;
    if (proxy_id >= 0 && proxy_id < c->w->body_count)
        c->user_cb(proxy_id, c->user_ctx);
}

void ps_world_query_aabb(ps_world *w, ps_vec2 min, ps_vec2 max, ps_query_cb cb, void *ctx) {
    if (!w || !cb) return;
    ps_aabb aabb = {min, max};
    qctx qc = {w, cb, ctx};
    ps_bvh_query(&w->broadphase, &aabb, bridge, &qc);
}

int ps_world_query_point(ps_world *w, ps_vec2 point) {
    int found = -1;
    /* linear fallback for reliability */
    for (int i = 0; i < w->body_count; i++) {
        ps_body *b = &w->bodies[i];
        ps_vec2 amin, amax;
        ps_shape_compute_aabb(&b->shape, b->xf, &amin, &amax);
        if (point.x >= amin.x && point.x <= amax.x && point.y >= amin.y && point.y <= amax.y)
            found = i;
    }
    return found;
}
