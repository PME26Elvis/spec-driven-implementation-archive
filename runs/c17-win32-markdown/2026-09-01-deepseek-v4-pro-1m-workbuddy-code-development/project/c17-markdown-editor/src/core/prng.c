/* prng.c - xorshift64* PRNG. */
#include "prng.h"

void ce_prng_seed(ce_prng *p, uint64_t seed){
    p->s = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

uint64_t ce_prng_next(ce_prng *p){
    uint64_t x = p->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    p->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

uint64_t ce_prng_range(ce_prng *p, uint64_t n){
    if(n == 0) return 0;
    /* rejection-free bias reduction using bitmask on 64-bit */
    uint64_t mask = n - 1;
    if((n & (n - 1)) == 0){ return ce_prng_next(p) & mask; }
    return ce_prng_next(p) % n;
}
