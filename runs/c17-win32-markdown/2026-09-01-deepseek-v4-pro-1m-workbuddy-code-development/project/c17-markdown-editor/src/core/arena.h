/* arena.h - simple bump allocator with block freeing. */
#ifndef CE_ARENA_H
#define CE_ARENA_H

#include <stddef.h>

typedef struct ce_arena_block {
    struct ce_arena_block *next;
    size_t used, cap;
    /* data follows */
} ce_arena_block;

typedef struct {
    ce_arena_block *head;
} ce_arena;

void ce_arena_init(ce_arena *a);
void *ce_arena_alloc(ce_arena *a, size_t n);
char *ce_arena_strdup(ce_arena *a, const char *s);
char *ce_arena_strndup(ce_arena *a, const char *s, size_t n);
void ce_arena_free(ce_arena *a);
/* Reset: free all blocks, keep arena usable. */
void ce_arena_reset(ce_arena *a);

#endif /* CE_ARENA_H */
