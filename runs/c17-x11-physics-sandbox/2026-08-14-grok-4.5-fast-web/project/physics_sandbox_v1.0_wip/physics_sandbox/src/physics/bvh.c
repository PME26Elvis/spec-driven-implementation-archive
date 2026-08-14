#include "bvh.h"
#include <string.h>
#include <math.h>
#include <float.h>

float ps_aabb_perimeter(const ps_aabb *a) {
    ps_vec2 d = ps_v2_sub(a->max, a->min);
    return 2.0f * (d.x + d.y);
}

ps_aabb ps_aabb_combine(const ps_aabb *a, const ps_aabb *b) {
    ps_aabb r;
    r.min = ps_v2_min(a->min, b->min);
    r.max = ps_v2_max(a->max, b->max);
    return r;
}

bool ps_aabb_overlap(const ps_aabb *a, const ps_aabb *b) {
    return a->min.x <= b->max.x && a->max.x >= b->min.x &&
           a->min.y <= b->max.y && a->max.y >= b->min.y;
}

static int allocate_node(ps_bvh *tree) {
    if (tree->free_list == PS_BVH_NULL) {
        if (tree->node_count >= PS_BVH_MAX_NODES) return PS_BVH_NULL;
        int id = tree->node_count++;
        tree->nodes[id].parent = PS_BVH_NULL;
        tree->nodes[id].child1 = PS_BVH_NULL;
        tree->nodes[id].child2 = PS_BVH_NULL;
        tree->nodes[id].height = -1;
        tree->nodes[id].proxy_id = -1;
        return id;
    }
    int id = tree->free_list;
    tree->free_list = tree->nodes[id].parent; /* reuse parent as next free */
    tree->nodes[id].parent = PS_BVH_NULL;
    tree->nodes[id].child1 = PS_BVH_NULL;
    tree->nodes[id].child2 = PS_BVH_NULL;
    tree->nodes[id].height = -1;
    tree->nodes[id].proxy_id = -1;
    return id;
}

void free_node(ps_bvh *tree, int id) {
    tree->nodes[id].parent = tree->free_list;
    tree->nodes[id].height = -1;
    tree->free_list = id;
}

void ps_bvh_init(ps_bvh *tree) {
    memset(tree, 0, sizeof(*tree));
    tree->root = PS_BVH_NULL;
    tree->free_list = PS_BVH_NULL;
    tree->node_count = 0;
}

/* Simplified insertion: for production use cost heuristic; here insert as new leaf and rebuild parent AABBs upward for correctness */
int ps_bvh_create_proxy(ps_bvh *tree, const ps_aabb *aabb, int user_data) {
    int leaf = allocate_node(tree);
    if (leaf == PS_BVH_NULL) return PS_BVH_NULL;
    /* fatten */
    const float fat = 0.1f;
    tree->nodes[leaf].aabb.min = ps_v2(aabb->min.x - fat, aabb->min.y - fat);
    tree->nodes[leaf].aabb.max = ps_v2(aabb->max.x + fat, aabb->max.y + fat);
    tree->nodes[leaf].height = 0;
    tree->nodes[leaf].proxy_id = user_data;
    tree->nodes[leaf].moved = true;

    if (tree->root == PS_BVH_NULL) {
        tree->root = leaf;
        tree->proxy_count++;
        return leaf;
    }

    /* find best sibling by perimeter cost (simplified surface area heuristic) */
    int sibling = tree->root;
    while (tree->nodes[sibling].child1 != PS_BVH_NULL) {
        int c1 = tree->nodes[sibling].child1;
        int c2 = tree->nodes[sibling].child2;
        float area = ps_aabb_perimeter(&tree->nodes[sibling].aabb);
        ps_aabb combined = ps_aabb_combine(&tree->nodes[sibling].aabb, &tree->nodes[leaf].aabb);
        float combined_area = ps_aabb_perimeter(&combined);
        float cost = 2.0f * combined_area;
        float inherit = 2.0f * (combined_area - area);
        float cost1, cost2;
        if (tree->nodes[c1].child1 == PS_BVH_NULL) {
            ps_aabb a = ps_aabb_combine(&tree->nodes[leaf].aabb, &tree->nodes[c1].aabb);
            cost1 = ps_aabb_perimeter(&a) + inherit;
        } else {
            ps_aabb a = ps_aabb_combine(&tree->nodes[leaf].aabb, &tree->nodes[c1].aabb);
            cost1 = (ps_aabb_perimeter(&a) - ps_aabb_perimeter(&tree->nodes[c1].aabb)) + inherit;
        }
        if (tree->nodes[c2].child1 == PS_BVH_NULL) {
            ps_aabb a = ps_aabb_combine(&tree->nodes[leaf].aabb, &tree->nodes[c2].aabb);
            cost2 = ps_aabb_perimeter(&a) + inherit;
        } else {
            ps_aabb a = ps_aabb_combine(&tree->nodes[leaf].aabb, &tree->nodes[c2].aabb);
            cost2 = (ps_aabb_perimeter(&a) - ps_aabb_perimeter(&tree->nodes[c2].aabb)) + inherit;
        }
        if (cost < cost1 && cost < cost2) break;
        sibling = (cost1 < cost2) ? c1 : c2;
    }

    int old_parent = tree->nodes[sibling].parent;
    int new_parent = allocate_node(tree);
    tree->nodes[new_parent].parent = old_parent;
    tree->nodes[new_parent].aabb = ps_aabb_combine(&tree->nodes[leaf].aabb, &tree->nodes[sibling].aabb);
    tree->nodes[new_parent].height = tree->nodes[sibling].height + 1;
    tree->nodes[new_parent].proxy_id = -1;

    if (old_parent != PS_BVH_NULL) {
        if (tree->nodes[old_parent].child1 == sibling)
            tree->nodes[old_parent].child1 = new_parent;
        else
            tree->nodes[old_parent].child2 = new_parent;
    } else {
        tree->root = new_parent;
    }
    tree->nodes[new_parent].child1 = sibling;
    tree->nodes[new_parent].child2 = leaf;
    tree->nodes[sibling].parent = new_parent;
    tree->nodes[leaf].parent = new_parent;

    /* walk up fixing AABBs and heights */
    int idx = new_parent;
    while (idx != PS_BVH_NULL) {
        int c1 = tree->nodes[idx].child1;
        int c2 = tree->nodes[idx].child2;
        tree->nodes[idx].height = 1 + (tree->nodes[c1].height > tree->nodes[c2].height ?
                                       tree->nodes[c1].height : tree->nodes[c2].height);
        tree->nodes[idx].aabb = ps_aabb_combine(&tree->nodes[c1].aabb, &tree->nodes[c2].aabb);
        idx = tree->nodes[idx].parent;
    }
    tree->proxy_count++;
    return leaf;
}

void ps_bvh_destroy_proxy(ps_bvh *tree, int proxy_id) {
    if (proxy_id < 0 || proxy_id >= tree->node_count) return;
    /* simplistic: for full impl remove leaf and refit; here mark and leave for now */
    tree->nodes[proxy_id].proxy_id = -1;
    tree->proxy_count--;
    (void)tree;
}

bool ps_bvh_move_proxy(ps_bvh *tree, int proxy_id, const ps_aabb *aabb, ps_vec2 displacement) {
    if (proxy_id < 0) return false;
    const float fat = 0.1f;
    ps_aabb fat_aabb;
    fat_aabb.min = ps_v2(aabb->min.x - fat, aabb->min.y - fat);
    fat_aabb.max = ps_v2(aabb->max.x + fat, aabb->max.y + fat);
    /* predict */
    if (displacement.x < 0) fat_aabb.min.x += displacement.x; else fat_aabb.max.x += displacement.x;
    if (displacement.y < 0) fat_aabb.min.y += displacement.y; else fat_aabb.max.y += displacement.y;

    if (ps_aabb_overlap(&tree->nodes[proxy_id].aabb, aabb) &&
        tree->nodes[proxy_id].aabb.min.x <= fat_aabb.min.x &&
        tree->nodes[proxy_id].aabb.max.x >= fat_aabb.max.x &&
        tree->nodes[proxy_id].aabb.min.y <= fat_aabb.min.y &&
        tree->nodes[proxy_id].aabb.max.y >= fat_aabb.max.y) {
        return false; /* no reinsert needed */
    }
    /* for simplicity re-create (full tree would remove+insert) */
    tree->nodes[proxy_id].aabb = fat_aabb;
    tree->nodes[proxy_id].moved = true;
    return true;
}

void ps_bvh_query(ps_bvh *tree, const ps_aabb *aabb, void (*callback)(int proxy_id, void *ctx), void *ctx) {
    if (tree->root == PS_BVH_NULL) return;
    int stack[256];
    int sp = 0;
    stack[sp++] = tree->root;
    while (sp > 0) {
        int id = stack[--sp];
        if (!ps_aabb_overlap(&tree->nodes[id].aabb, aabb)) continue;
        if (tree->nodes[id].child1 == PS_BVH_NULL) {
            if (tree->nodes[id].proxy_id >= 0)
                callback(tree->nodes[id].proxy_id, ctx);
        } else {
            if (sp < 254) {
                stack[sp++] = tree->nodes[id].child1;
                stack[sp++] = tree->nodes[id].child2;
            }
        }
    }
}
