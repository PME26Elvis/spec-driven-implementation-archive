#include "contact_cache.h"
#include <string.h>

void ps_cache_init(ps_contact_cache *c) {
    memset(c, 0, sizeof(*c));
}


void ps_cache_store(ps_contact_cache *c, const ps_manifold *m) {
    if (!m || !m->body_a || !m->body_b) return;
    uint32_t ka = m->body_a->id, kb = m->body_b->id;
    if (ka > kb) { uint32_t t = ka; ka = kb; kb = t; }
    /* find or append */
    for (int i = 0; i < c->count; i++) {
        if (c->entries[i].key_a == ka && c->entries[i].key_b == kb) {
            c->entries[i].point_count = m->point_count;
            for (int p = 0; p < m->point_count; p++) {
                c->entries[i].normal_impulse[p] = m->points[p].normal_impulse;
                c->entries[i].tangent_impulse[p] = m->points[p].tangent_impulse;
            }
            return;
        }
    }
    if (c->count >= PS_CACHE_SIZE) return;
    ps_cached_manifold *e = &c->entries[c->count++];
    e->key_a = ka; e->key_b = kb;
    e->point_count = m->point_count;
    for (int p = 0; p < m->point_count; p++) {
        e->normal_impulse[p] = m->points[p].normal_impulse;
        e->tangent_impulse[p] = m->points[p].tangent_impulse;
    }
}

int ps_cache_lookup(ps_contact_cache *c, ps_manifold *m) {
    if (!m || !m->body_a || !m->body_b) return 0;
    uint32_t ka = m->body_a->id, kb = m->body_b->id;
    if (ka > kb) { uint32_t t = ka; ka = kb; kb = t; }
    for (int i = 0; i < c->count; i++) {
        if (c->entries[i].key_a == ka && c->entries[i].key_b == kb) {
            int n = m->point_count < c->entries[i].point_count ? m->point_count : c->entries[i].point_count;
            for (int p = 0; p < n; p++) {
                m->points[p].normal_impulse = c->entries[i].normal_impulse[p];
                m->points[p].tangent_impulse = c->entries[i].tangent_impulse[p];
            }
            return 1;
        }
    }
    return 0;
}
