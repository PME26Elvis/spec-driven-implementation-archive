/* arena.c - bump allocator. */
#include "arena.h"
#include "ce_common.h"

#define BLOCK_CAP 16384

void ce_arena_init(ce_arena *a){ a->head = NULL; }

void *ce_arena_alloc(ce_arena *a, size_t n){
    n = (n + 15) & ~(size_t)15; /* 16-byte align */
    if(n == 0) n = 16;
    ce_arena_block *b = a->head;
    if(!b || b->used + n > b->cap){
        size_t cap = n > BLOCK_CAP ? n : BLOCK_CAP;
        ce_arena_block *nb = ce_malloc(sizeof(ce_arena_block) + cap);
        nb->next = a->head; nb->used = 0; nb->cap = cap;
        a->head = nb; b = nb;
    }
    void *p = (char*)(b + 1) + b->used;
    b->used += n;
    return p;
}

char *ce_arena_strdup(ce_arena *a, const char *s){ return ce_arena_strndup(a, s, strlen(s)); }

char *ce_arena_strndup(ce_arena *a, const char *s, size_t n){
    char *p = ce_arena_alloc(a, n + 1);
    memcpy(p, s, n); p[n] = 0;
    return p;
}

void ce_arena_free(ce_arena *a){
    ce_arena_block *b = a->head;
    while(b){ ce_arena_block *n = b->next; ce_free(b); b = n; }
    a->head = NULL;
}

void ce_arena_reset(ce_arena *a){ ce_arena_free(a); }
