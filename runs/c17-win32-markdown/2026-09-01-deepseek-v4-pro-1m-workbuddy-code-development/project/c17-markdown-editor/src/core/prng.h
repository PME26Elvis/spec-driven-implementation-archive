/* prng.h - deterministic xorshift64* PRNG (independent of libc rand()). */
#ifndef CE_PRNG_H
#define CE_PRNG_H

#include <stdint.h>

typedef struct { uint64_t s; } ce_prng;

void ce_prng_seed(ce_prng *p, uint64_t seed);
uint64_t ce_prng_next(ce_prng *p);
/* Uniform in [0, n). */
uint64_t ce_prng_range(ce_prng *p, uint64_t n);

#endif /* CE_PRNG_H */
