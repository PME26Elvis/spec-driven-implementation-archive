#include "rng.h"

void rng_seed(Rng *r, uint64_t seed) { r->s = seed ? seed : 0x9E3779B97F4A7C15ULL; }

uint64_t rng_next_u64(Rng *r) {
  uint64_t z = (r->s += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
uint32_t rng_next_u32(Rng *r) { return (uint32_t)(rng_next_u64(r) >> 32); }
double rng_next_double(Rng *r) {
  /* 53-bit mantissa */
  uint64_t x = rng_next_u64(r) >> 11;
  return (double)x / (double)(1ULL << 53);
}
int rng_next_int(Rng *r, int n) {
  if (n <= 0) return 0;
  return (int)(rng_next_double(r) * (double)n);
}
