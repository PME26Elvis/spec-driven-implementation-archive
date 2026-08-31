/* sdk_sudoku_solver.c - backtracking search solver.
 *
 * Deterministic selection (docs/07 section 29): minimum-candidate cell with
 * lowest index on ties, digits 1..9. Production generation may randomize the
 * candidate order, but count_solutions results are independent of order.
 */
#include "sudoku/sdk_sudoku.h"

#include <string.h>

#define SDK_SOLVE_NODE_LIMIT 20000000L

static int cell_ok(const uint8_t *g, int cell, int d) {
    int r = cell / 9, c = cell % 9, br = (r / 3) * 3, bc = (c / 3) * 3, i;
    for (i = 0; i < 9; ++i) {
        if (g[r * 9 + i] == (uint8_t)d) return 0;
        if (g[i * 9 + c] == (uint8_t)d) return 0;
    }
    for (i = 0; i < 3; ++i) {
        int j;
        for (j = 0; j < 3; ++j)
            if (g[(br + i) * 9 + (bc + j)] == (uint8_t)d) return 0;
    }
    return 1;
}

static int cand_count(const uint8_t *g, int cell) {
    int d, n = 0;
    for (d = 1; d <= 9; ++d)
        if (cell_ok(g, cell, d)) ++n;
    return n;
}

typedef struct {
    uint8_t g[81];
    long nodes;
    long node_limit;
    int limit;     /* counting ceiling */
    int count;     /* capped at limit + 1 */
    int solved;    /* solve_one flag */
    int mode;      /* 0 solve_one, 1 count */
} sctx;

static void rec(sctx *c) {
    int cell = -1, best = 10, i;
    if (c->nodes++ > c->node_limit) return;
    for (i = 0; i < 81; ++i) {
        if (c->g[i] == 0) {
            int n = cand_count(c->g, i);
            if (n == 0) return;            /* dead end */
            if (n < best) { best = n; cell = i; if (best == 1) break; }
        }
    }
    if (cell == -1) {
        if (c->mode == 0) c->solved = 1;
        else {
            c->count++;
            if (c->count > c->limit) c->count = c->limit + 1;
        }
        return;
    }
    for (i = 1; i <= 9; ++i) {
        if (cell_ok(c->g, cell, (uint8_t)i)) {
            c->g[cell] = (uint8_t)i;
            rec(c);
            /* On success the assignment must survive: c->g is the solution
               that sdk_solve_one hands back to the caller. */
            if (c->mode == 0 && c->solved) return;
            c->g[cell] = 0;                    /* only empty cells recurse */
            if (c->mode == 1 && c->count > c->limit) return;
        }
    }
}

static void load_grid(const sdk_board *in, uint8_t *g) {
    int i;
    for (i = 0; i < 81; ++i) g[i] = in->cells[i].value;
}

int sdk_solve_one(const sdk_board *in, sdk_board *out) {
    sctx c;
    int i;
    memset(&c, 0, sizeof c);
    load_grid(in, c.g);
    c.node_limit = SDK_SOLVE_NODE_LIMIT;
    c.mode = 0;
    rec(&c);
    if (!c.solved) return 0;
    if (out) {
        sdk_board_copy(in, out);
        for (i = 0; i < 81; ++i)
            if (in->cells[i].value == 0) {
                out->cells[i].value = c.g[i];
                out->cells[i].origin = (uint8_t)SDK_O_PLAYER;
                out->cells[i].notes = 0;
            }
    }
    return 1;
}

int sdk_count_solutions(const sdk_board *in, int limit, int *out_count) {
    sctx c;
    memset(&c, 0, sizeof c);
    load_grid(in, c.g);
    c.node_limit = SDK_SOLVE_NODE_LIMIT;
    c.mode = 1;
    c.limit = limit;
    c.count = 0;
    rec(&c);
    if (out_count) *out_count = c.count;
    return 1;
}

int sdk_is_unique(const sdk_board *in) {
    int count = 0;
    sdk_count_solutions(in, 1, &count);
    return count == 1;
}
