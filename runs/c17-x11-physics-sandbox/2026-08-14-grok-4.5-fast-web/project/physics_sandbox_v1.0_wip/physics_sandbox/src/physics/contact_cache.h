#ifndef PS_CONTACT_CACHE_H
#define PS_CONTACT_CACHE_H
#include "collision.h"

#define PS_CACHE_SIZE 256

typedef struct {
    uint32_t key_a, key_b; /* body ids sorted */
    int point_count;
    float normal_impulse[PS_MAX_MANIFOLD_POINTS];
    float tangent_impulse[PS_MAX_MANIFOLD_POINTS];
} ps_cached_manifold;

typedef struct {
    ps_cached_manifold entries[PS_CACHE_SIZE];
    int count;
} ps_contact_cache;

void ps_cache_init(ps_contact_cache *c);
void ps_cache_store(ps_contact_cache *c, const ps_manifold *m);
int  ps_cache_lookup(ps_contact_cache *c, ps_manifold *m); /* fills impulses if found */

#endif
