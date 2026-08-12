#ifndef DARC_BUZHASH_H
#define DARC_BUZHASH_H

#include <stdint.h>
#include <stddef.h>

#define DARC_BUZHASH_WINDOW 64
#define DARC_BUZHASH_TABLE_SIZE 256

typedef struct {
    uint64_t table[DARC_BUZHASH_TABLE_SIZE];
    uint64_t hash;
    uint8_t window[DARC_BUZHASH_WINDOW];
    size_t window_pos;
    size_t window_filled;
    size_t bytes_in_chunk;
} darc_buzhash_ctx;

void darc_buzhash_table_init(uint64_t table[DARC_BUZHASH_TABLE_SIZE]);
void darc_buzhash_reset(darc_buzhash_ctx *ctx, const uint64_t *table);
/* Feed one byte; returns 1 if a boundary should be cut (after min), 0 otherwise.
   Caller must enforce min/max. */
int darc_buzhash_feed(darc_buzhash_ctx *ctx, uint8_t byte, size_t min, size_t avg, size_t max);

#endif
