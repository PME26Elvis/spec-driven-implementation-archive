#ifndef PS_QUERY_H
#define PS_QUERY_H
#include "world.h"

typedef void (*ps_query_cb)(int body_index, void *ctx);

/* Query all bodies whose AABB overlaps the given region */
void ps_world_query_aabb(ps_world *w, ps_vec2 min, ps_vec2 max, ps_query_cb cb, void *ctx);

/* Point query: find first dynamic body containing the point (approx AABB) */
int ps_world_query_point(ps_world *w, ps_vec2 point);

#endif
