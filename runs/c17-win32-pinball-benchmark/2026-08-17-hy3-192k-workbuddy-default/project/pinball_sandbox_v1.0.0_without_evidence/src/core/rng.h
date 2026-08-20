#ifndef PB_RNG_H
#define PB_RNG_H

#include <stdint.h>

/* Deterministic PRNG (splitmix64). Seedable; identical seed -> identical
   stream. Used only where spec requires explicit deterministic randomness
   (doc 17.15). The default sim path is fully deterministic without RNG. */

typedef struct { uint64_t s; } Rng;

void rng_seed(Rng *r, uint64_t seed);
uint64_t rng_next_u64(Rng *r);          /* uniform 64-bit */
uint32_t rng_next_u32(Rng *r);
double   rng_next_double(Rng *r);       /* [0,1) */
int      rng_next_int(Rng *r, int n);   /* [0,n) */

#endif
