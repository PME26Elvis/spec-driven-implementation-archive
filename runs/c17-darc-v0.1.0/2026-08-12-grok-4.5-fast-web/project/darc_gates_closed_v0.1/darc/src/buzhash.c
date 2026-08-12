#include "darc_buzhash.h"
#include <string.h>

static uint64_t rol64(uint64_t x, unsigned n) {
    n &= 63;
    if (n == 0) return x;
    return (x << n) | (x >> (64 - n));
}

void darc_buzhash_table_init(uint64_t table[DARC_BUZHASH_TABLE_SIZE]) {
    uint64_t state = 0xD6E8FEB86659FD93ULL;
    int i;
    for (i = 0; i < DARC_BUZHASH_TABLE_SIZE; ++i) {
        state = state + 0x9E3779B97F4A7C15ULL;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z = z ^ (z >> 31);
        table[i] = z;
    }
}

void darc_buzhash_reset(darc_buzhash_ctx *ctx, const uint64_t *table) {
    memcpy(ctx->table, table, sizeof(ctx->table));
    ctx->hash = 0;
    ctx->window_pos = 0;
    ctx->window_filled = 0;
    ctx->bytes_in_chunk = 0;
    memset(ctx->window, 0, sizeof(ctx->window));
}

int darc_buzhash_feed(darc_buzhash_ctx *ctx, uint8_t byte, size_t min, size_t avg, size_t max) {
    if (ctx->window_filled < DARC_BUZHASH_WINDOW) {
        ctx->hash = rol64(ctx->hash, 1) ^ ctx->table[byte];
        ctx->window[ctx->window_pos] = byte;
        ctx->window_pos = (ctx->window_pos + 1) % DARC_BUZHASH_WINDOW;
        ctx->window_filled++;
    } else {
        uint8_t out_byte = ctx->window[ctx->window_pos];
        /* window == 64 so rol64(T[out], 0) == T[out] */
        ctx->hash = rol64(ctx->hash, 1) ^ ctx->table[byte] ^ ctx->table[out_byte];
        ctx->window[ctx->window_pos] = byte;
        ctx->window_pos = (ctx->window_pos + 1) % DARC_BUZHASH_WINDOW;
    }
    ctx->bytes_in_chunk++;

    if (ctx->bytes_in_chunk >= max)
        return 1;
    if (ctx->bytes_in_chunk >= min && (ctx->hash & (avg - 1)) == 0)
        return 1;
    return 0;
}
