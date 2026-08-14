#include "sensor.h"
#include "query.h"

typedef struct { int *count; } sctx;
static void scb(int idx, void *ctx) { (void)idx; (*(int*)ctx)++; }

void ps_sensor_init(ps_sensor *s, ps_vec2 min, ps_vec2 max) {
    s->min = min; s->max = max; s->overlap_count = 0; s->enabled = 1;
}

void ps_sensor_update(ps_sensor *s, ps_world *w) {
    if (!s->enabled) { s->overlap_count = 0; return; }
    s->overlap_count = 0;
    ps_world_query_aabb(w, s->min, s->max, scb, &s->overlap_count);
}
