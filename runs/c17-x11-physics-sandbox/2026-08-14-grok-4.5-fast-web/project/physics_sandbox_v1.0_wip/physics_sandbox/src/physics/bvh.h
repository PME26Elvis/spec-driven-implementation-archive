#ifndef PS_BVH_H
#define PS_BVH_H

#include "../math/math.h"
#include <stdint.h>
#include <stdbool.h>

#define PS_BVH_NULL -1
#define PS_BVH_MAX_NODES 2048

typedef struct ps_aabb {
    ps_vec2 min;
    ps_vec2 max;
} ps_aabb;

typedef struct ps_bvh_node {
    ps_aabb aabb;
    int parent;
    int child1;
    int child2;
    int height; /* leaf = 0 */
    int proxy_id; /* user body index or -1 for internal */
    bool moved;
} ps_bvh_node;

typedef struct ps_bvh {
    ps_bvh_node nodes[PS_BVH_MAX_NODES];
    int root;
    int free_list;
    int node_count;
    int proxy_count;
} ps_bvh;

void ps_bvh_init(ps_bvh *tree);
int  ps_bvh_create_proxy(ps_bvh *tree, const ps_aabb *aabb, int user_data);
void ps_bvh_destroy_proxy(ps_bvh *tree, int proxy_id);
bool ps_bvh_move_proxy(ps_bvh *tree, int proxy_id, const ps_aabb *aabb, ps_vec2 displacement);
void ps_bvh_query(ps_bvh *tree, const ps_aabb *aabb, void (*callback)(int proxy_id, void *ctx), void *ctx);
float ps_aabb_perimeter(const ps_aabb *a);
ps_aabb ps_aabb_combine(const ps_aabb *a, const ps_aabb *b);
bool ps_aabb_overlap(const ps_aabb *a, const ps_aabb *b);

#endif /* PS_BVH_H */
