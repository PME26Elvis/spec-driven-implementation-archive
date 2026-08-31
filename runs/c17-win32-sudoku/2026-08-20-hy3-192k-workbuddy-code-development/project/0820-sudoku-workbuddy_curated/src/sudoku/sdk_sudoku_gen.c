/* sdk_sudoku_gen.c - puzzle generator (docs/07 sections 13-15, 21, 22, 28,
 * 31).
 *
 * Generation procedure (docs/07 section 14):
 *   1. Produce a fully valid solved grid (randomized, seed-driven).
 *   2. Remove clues in a random order, keeping the puzzle UNIQUE at every
 *      removal (count_solutions / logic-solve completeness gate).
 *   3. Once the clue count enters the requested difficulty's sanity window,
 *      run the human-logic solver and classify.
 *   4. Accept the first removal point whose classification matches the
 *      requested difficulty; otherwise reseed and retry, bounded by all
 *      guards (docs/07 section 28).
 *
 * Determinism: with fixed_seed set, identical (seed, requested) yields an
 * identical puzzle and difficulty trace (docs/07 sections 21, 29). Entropy
 * for the production path comes from BCryptGenRandom via sdk_random_bytes
 * (docs/26 section 16) - never a weak fallback unless that call fails.
 */
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"
#include "common/sdk_win.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Deterministic PRNG (xoshiro256**, seeded by splitmix64)             */
/* ------------------------------------------------------------------ */

static uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t xr_next(uint64_t s[4]) {
    uint64_t res = rotl64(s[1] * 5u, 7) * 9u;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);
    return res;
}

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void xr_seed(uint64_t s[4], uint64_t seed) {
    uint64_t x = seed ? seed : 0x123456789ABCDEF0ULL;
    int i;
    for (i = 0; i < 4; ++i) s[i] = splitmix64(&x);
}

static void shuffle_int(int *arr, int n, uint64_t s[4]) {
    int i;
    for (i = n - 1; i > 0; --i) {
        uint64_t r = xr_next(s);
        int j = (int)(r % (uint64_t)(i + 1));
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

/* ------------------------------------------------------------------ */
/* Full-grid construction                                              */
/* ------------------------------------------------------------------ */

static int grid_ok(const sdk_board *b, int cell, int d) {
    int r = cell / 9, c = cell % 9, br = (r / 3) * 3, bc = (c / 3) * 3, i;
    for (i = 0; i < 9; ++i) {
        if (b->cells[r * 9 + i].value == (uint8_t)d) return 0;
        if (b->cells[i * 9 + c].value == (uint8_t)d) return 0;
    }
    for (i = 0; i < 3; ++i) {
        int j;
        for (j = 0; j < 3; ++j)
            if (b->cells[(br + i) * 9 + (bc + j)].value == (uint8_t)d) return 0;
    }
    return 1;
}

static int grid_cand_count(const sdk_board *b, int cell) {
    int d, n = 0;
    for (d = 1; d <= 9; ++d) if (grid_ok(b, cell, d)) ++n;
    return n;
}

/* Recursive full-grid fill using minimum-remaining-value selection and a
 * shuffled digit order. The diagonal boxes are pre-filled so a solution is
 * guaranteed to exist. Returns 1 on completion. */
static int grid_rec(sdk_board *b, uint64_t s[4], long *nodes, long limit) {
    int cell = -1, best = 10, i;
    if (*nodes > limit) return 0;
    (*nodes)++;
    for (i = 0; i < 81; ++i) {
        if (b->cells[i].value == 0) {
            int n = grid_cand_count(b, i);
            if (n == 0) return 0;
            if (n < best) { best = n; cell = i; if (best == 1) break; }
        }
    }
    if (cell == -1) return 1;
    int order[9], k;
    for (k = 0; k < 9; ++k) order[k] = k + 1;
    shuffle_int(order, 9, s);
    for (k = 0; k < 9; ++k) {
        int d = order[k];
        if (grid_ok(b, cell, d)) {
            b->cells[cell].value = (uint8_t)d;
            if (grid_rec(b, s, nodes, limit)) return 1;
            b->cells[cell].value = 0;
        }
    }
    return 0;
}

static void fill_diagonal(sdk_board *b, uint64_t s[4]) {
    static const int diag[3] = { 0, 4, 8 };   /* row-block == col-block */
    int k;
    for (k = 0; k < 3; ++k) {
        int box = diag[k];
        int nums[9], i;
        for (i = 0; i < 9; ++i) nums[i] = i + 1;
        shuffle_int(nums, 9, s);
        for (i = 0; i < 9; ++i) {
            int r = (box / 3) * 3 + i / 3;
            int c = (box % 3) * 3 + i % 3;
            b->cells[r * 9 + c].value = (uint8_t)nums[i];
        }
    }
}

static void generate_full_grid(uint64_t s[4], sdk_board *out) {
    int i;
    memset(out, 0, sizeof *out);
    fill_diagonal(out, s);
    {
        long nodes = 0;
        grid_rec(out, s, &nodes, 8000000L);
    }
    for (i = 0; i < 81; ++i) {
        if (out->cells[i].value != 0) out->cells[i].origin = (uint8_t)SDK_O_GIVEN;
    }
}

/* ------------------------------------------------------------------ */
/* Uniqueness / classification gate                                    */
/* ------------------------------------------------------------------ */

static int count_given(const sdk_board *b) {
    int i, n = 0;
    for (i = 0; i < 81; ++i) if (b->cells[i].value != 0) n++;
    return n;
}

static void gen_put(sdk_board *b, int cell, int v) {
    b->cells[cell].value = (uint8_t)v;
    b->cells[cell].origin = (uint8_t)(v ? SDK_O_GIVEN : SDK_O_EMPTY);
    b->cells[cell].notes = 0;
}

/* ------------------------------------------------------------------ */
/* Difficulty-shaped candidate evaluation                              */
/* ------------------------------------------------------------------ */

typedef struct gen_eval {
    int solvable;      /* human-logic solver completed the board */
    int score;         /* logic_score (sum of step weights) */
    int steps;
    int maxt;
    int clue;
    int tc[9];         /* per-technique effective step counts */
    int near_hi;       /* triple applicable while a T5/T6 step was chosen */
    int near_lo;       /* triple applicable while a T3/T4 step was chosen */
} gen_eval;

/* Runs the human-logic solver over `p` and records exactly the trace data the
 * classifier uses. When `probe_triples` is set it additionally records how
 * often a Naked/Hidden Triple *would* already have been applicable at a state
 * where a lower-priority technique was legitimately chosen first. Those
 * "pre-empted triple" counts are used only to steer the HARD adjustment search
 * of docs/07 section 14 step 7; they never change the technique priority of an
 * actual solve, and they are not part of any accepted result. */
static void gen_evaluate(const sdk_board *p, int probe_triples, gen_eval *e) {
    sdk_board work;
    sdk_cand m[81], tmp[81];
    int i;

    memset(e, 0, sizeof *e);
    e->clue = count_given(p);
    sdk_board_copy(p, &work);
    if (!sdk_derive_candidates(&work, m)) return;

    for (;;) {
        int empty = 0;
        sdk_logic_step step, probe;

        if (!sdk_derive_candidates(&work, tmp)) return;   /* contradiction */
        for (i = 0; i < 81; ++i) {
            if (work.cells[i].value == 0) {
                empty++;
                m[i] &= tmp[i];
                if (m[i] == SDK_CAND_EMPTY) return;
            }
        }
        if (empty == 0) { e->solvable = 1; return; }
        if (!sdk_logic_find_step(&work, m, &step)) return;  /* stalled */

        if (probe_triples && step.tech >= SDK_TECH_LOCKED_POINTING) {
            if (sdk_logic_find_step_from(&work, m, SDK_TECH_NAKED_TRIPLE, &probe)) {
                if (step.tech >= SDK_TECH_NAKED_PAIR) e->near_hi++;
                else                                  e->near_lo++;
            }
        }

        if (step.kind == SDK_STEP_PLACE) {
            int peers[20], pn;
            sdk_board_set(&work, step.target_cell, step.placed_digit, SDK_O_PLAYER);
            pn = sdk_peers(step.target_cell, peers);
            m[step.target_cell] = SDK_CAND_EMPTY;
            for (i = 0; i < pn; ++i)
                m[peers[i]] &= (sdk_cand)~SDK_CAND_BIT(step.placed_digit);
        } else {
            for (i = 0; i < step.target_count; ++i)
                m[step.target_cells[i]] &= (sdk_cand)~step.affected_candidates;
        }
        e->score += step.score_weight;
        e->steps++;
        if (step.tech > e->maxt) e->maxt = step.tech;
        if (step.tech >= 1 && step.tech <= 8) e->tc[step.tech]++;
    }
}

#define GEN_BAD (-1000000L)

static long band_penalty(int v, int lo, int hi) {
    if (v < lo) return -8L * (long)(lo - v);
    if (v > hi) return -8L * (long)(v - hi);
    return 0;
}

/* Shaping objective for the "continue adjusting" phase. Higher is closer to
 * the acceptance conditions of docs/07 section 12 for `req`. */
static long gen_objective(sdk_difficulty req, const gen_eval *e) {
    if (!e->solvable) return GEN_BAD;

    if (req == SDK_DIFF_EASY) {
        if (e->maxt > SDK_TECH_HIDDEN_SINGLE) return GEN_BAD + e->maxt;
        return 600L * (e->tc[2] > 0 ? 1 : 0) + 4L * e->tc[2]
             + band_penalty(e->score, 20, 120);
    }

    if (req == SDK_DIFF_MEDIUM) {
        int adv = e->tc[3] + e->tc[4] + e->tc[5] + e->tc[6];
        if (e->tc[7] || e->tc[8]) return GEN_BAD + 1;
        return 600L * (adv > 0 ? 1 : 0) + 20L * (adv < 4 ? adv : 4)
             + band_penalty(e->score, 70, 260) + (long)e->score;
    }

    /* HARD: two acceptance branches (docs/07 section 12 / 27). The logic_score
     * band is by far the binding one - a 24-30 clue puzzle scores about 60-100
     * naturally, so the search has to roughly double it - which is why the
     * objective is staged instead of rewarding everything at once. */
    {
        int t7t8 = e->tc[7] + e->tc[8];
        int t5t6 = e->tc[5] + e->tc[6];
        int nh = e->near_hi > 12 ? 12 : e->near_hi;
        int nl = e->near_lo > 24 ? 24 : e->near_lo;
        long obj = 6L * e->score;

        /* Reward states where a triple is already applicable but was legally
         * pre-empted by a cheaper technique - those are one swap away from
         * requiring it. This drift has to outrank the raw score pull, or the
         * search converges on high-score puzzles that never need a triple.
         *
         * It deliberately stays switched on after a triple has fired. Measuring
         * the alternative (swap this term for a steep score pull once inside the
         * triple basin) showed logic_score capping around 175 - just short of
         * the band - because the states that carry the score higher are exactly
         * the elimination-rich ones this term rewards. */
        obj += 1200L * nh + 150L * nl;
        if (t7t8 > 0) obj += 100000L;                   /* branch A satisfied */
        obj += 40L * (t5t6 < 3 ? t5t6 : 3);             /* branch B progress */
        if (e->score > 520) obj -= 200L * (e->score - 520);
        return obj;
    }
}

/* Cheap pre-filter mirroring docs/07 section 12 so the authoritative
 * classifier is only invoked on genuinely promising candidates. */
static int gen_promising(sdk_difficulty req, const gen_eval *e, int lo, int hi) {
    if (!e->solvable) return 0;
    if (e->clue < lo || e->clue > hi) return 0;

    if (req == SDK_DIFF_EASY)
        return e->maxt <= SDK_TECH_HIDDEN_SINGLE && e->tc[2] > 0 &&
               e->score >= 20 && e->score <= 120;

    if (req == SDK_DIFF_MEDIUM) {
        int adv = e->tc[3] + e->tc[4] + e->tc[5] + e->tc[6];
        return adv > 0 && e->tc[7] == 0 && e->tc[8] == 0 &&
               e->score >= 70 && e->score <= 260;
    }

    {
        int t7t8 = e->tc[7] + e->tc[8];
        int t5t6 = e->tc[5] + e->tc[6];
        if (e->score < 180 || e->score > 520) return 0;
        return t7t8 > 0 || (t5t6 >= 3 && e->score >= 220);
    }
}

/* docs/07 section 14 step 8: independent re-verification of the accepted
 * puzzle - full rule validity, exactly one solution, and a difficulty trace
 * recomputed from scratch that still matches the requested difficulty. */
static int gen_final_verify(const sdk_board *puzzle, sdk_difficulty req,
                            sdk_difficulty_result *dr) {
    sdk_validation v;
    sdk_difficulty_result local;
    int cnt = 0;

    if (!sdk_validate_partial(puzzle)) return 0;
    sdk_validate(puzzle, &v);
    if (v.is_complete) return 0;                 /* a solved grid is no puzzle */
    if (!sdk_count_solutions(puzzle, 2, &cnt) || cnt != 1) return 0;

    sdk_classify_difficulty(puzzle, &local);
    if (local.stalled) return 0;
    if (local.difficulty != req) return 0;
    if (dr) *dr = local;
    return 1;
}

/* docs/07 section 14 step 7: bounded guided adjustment of a candidate puzzle
 * that is unique and logic-solvable but classified as the wrong difficulty.
 * Every accepted neighbour stays logic-solvable, and a logic-complete solve is
 * itself a uniqueness proof (the solver only ever derives forced values), so
 * the search never leaves the space of unique puzzles; step 8 re-verifies
 * uniqueness explicitly regardless. */
static int gen_adjust(const sdk_board *full, sdk_board *cur, sdk_difficulty req,
                      int lo, int hi, uint64_t s[4], int max_iters,
                      int64_t t0, int64_t wall_ms,
                      sdk_board *accept, sdk_difficulty_result *dr) {
    gen_eval e;
    sdk_board best;
    long cobj, bobj;
    int iter, probe = (req == SDK_DIFF_HARD);

    gen_evaluate(cur, probe, &e);
    cobj = gen_objective(req, &e);
    bobj = cobj;
    sdk_board_copy(cur, &best);

    for (iter = 0; iter < max_iters; ++iter) {
        int empties[81], en = 0, givens[81], gn = 0, i, tries, moved = 0;
        for (i = 0; i < 81; ++i) {
            if (cur->cells[i].value == 0) empties[en++] = i;
            else                          givens[gn++] = i;
        }

        for (tries = 0; tries < 96 && !moved; ++tries) {
            int kind = (int)(xr_next(s) % 10u);
            int a = -1, b = -1, bv = 0, nc = count_given(cur);
            long ob;

            if (kind < 7) {                    /* swap: clue count unchanged */
                if (en == 0 || gn == 0) continue;
                a = empties[(int)(xr_next(s) % (uint64_t)en)];
                b = givens[(int)(xr_next(s) % (uint64_t)gn)];
                if (cur->cells[a].value != 0 || cur->cells[b].value == 0) continue;
                bv = cur->cells[b].value;
                gen_put(cur, a, full->cells[a].value);
                gen_put(cur, b, 0);
            } else if (kind < 9) {              /* remove one clue */
                if (gn == 0 || nc <= lo) continue;
                b = givens[(int)(xr_next(s) % (uint64_t)gn)];
                if (cur->cells[b].value == 0) continue;
                bv = cur->cells[b].value;
                gen_put(cur, b, 0);
            } else {                            /* restore one clue */
                if (en == 0 || nc >= hi) continue;
                a = empties[(int)(xr_next(s) % (uint64_t)en)];
                if (cur->cells[a].value != 0) continue;
                gen_put(cur, a, full->cells[a].value);
            }

            gen_evaluate(cur, probe, &e);
            ob = gen_objective(req, &e);

            if (gen_promising(req, &e, lo, hi) &&
                gen_final_verify(cur, req, dr)) {
                sdk_board_copy(cur, accept);
                return 1;
            }

            if (ob > cobj) {
                cobj = ob;
                moved = 1;
                if (ob > bobj) { bobj = ob; sdk_board_copy(cur, &best); }
            } else {
                if (a >= 0) gen_put(cur, a, 0);
                if (b >= 0) gen_put(cur, b, bv);
            }
        }

        if (!moved) {
            int k;
            sdk_board_copy(&best, cur);
            cobj = bobj;
            for (k = 0; k < 32; ++k) {          /* solvability-preserving kick */
                int a, b, bv;
                if (en == 0 || gn == 0) break;
                a = empties[(int)(xr_next(s) % (uint64_t)en)];
                b = givens[(int)(xr_next(s) % (uint64_t)gn)];
                if (cur->cells[a].value != 0 || cur->cells[b].value == 0) continue;
                bv = cur->cells[b].value;
                gen_put(cur, a, full->cells[a].value);
                gen_put(cur, b, 0);
                gen_evaluate(cur, probe, &e);
                if (e.solvable) { cobj = gen_objective(req, &e); break; }
                gen_put(cur, a, 0);
                gen_put(cur, b, bv);
            }
        }

        if (wall_ms > 0 && (int64_t)sdk_monotonic_ms() - t0 > wall_ms)
            break;
    }
    return 0;
}

static void finalize_puzzle(const sdk_board *puzzle, sdk_board *out,
                            sdk_board *solution) {
    int i;
    sdk_board_copy(puzzle, out);
    for (i = 0; i < 81; ++i) {
        if (out->cells[i].value != 0) out->cells[i].origin = (uint8_t)SDK_O_GIVEN;
        else out->cells[i].origin = (uint8_t)SDK_O_EMPTY;
        out->cells[i].notes = 0;
    }
    if (solution) {
        sdk_solve_one(puzzle, solution);
        for (i = 0; i < 81; ++i) {
            if (solution->cells[i].value != 0)
                solution->cells[i].origin = (uint8_t)SDK_O_GIVEN;
            else solution->cells[i].origin = (uint8_t)SDK_O_EMPTY;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void sdk_gen_params_default(sdk_gen_params *p, sdk_difficulty requested) {
    if (!p) return;
    memset(p, 0, sizeof *p);
    p->requested = requested;
    p->seed = 0;
    p->fixed_seed = 0;
    p->full_grid_nodes = 8000000L;
    p->uniqueness_nodes = 6000000L;
    p->candidate_attempts = 600;
    p->wall_guard_ms = 30000;
    p->pbkdf2_iterations = 0;
}

int sdk_generate_once(uint64_t seed, sdk_difficulty req,
                      const sdk_gen_params *p, sdk_board *out,
                      sdk_board *solution, sdk_difficulty_result *dr) {
    uint64_t s[4];
    sdk_board full;
    int order[81], i, o, orders, adjust_iters, ceiling;
    int lo, hi;
    int64_t wall_ms;
    int64_t t0;

    if (req == SDK_DIFF_EASY)        { lo = 36; hi = 49; ceiling = SDK_TECH_HIDDEN_SINGLE; }
    else if (req == SDK_DIFF_MEDIUM) { lo = 30; hi = 40; ceiling = SDK_TECH_HIDDEN_PAIR; }
    else if (req == SDK_DIFF_HARD)   { lo = 24; hi = 34; ceiling = SDK_TECH_HIDDEN_TRIPLE; }
    else { return 0; }

    /* One attempt = one solved grid dug + a bounded guided adjust.  The adjust
     * is bounded by iteration count only (gen_adjust receives wall_ms == 0), so
     * sdk_generate_once is fully deterministic; sdk_generate's outer wall guard
     * then gets to try many different solved grids inside wall_guard_ms - which
     * is what makes HARD reliably reachable (a single grid's hill-climb sits at
     * a score ceiling well below 180, so the search must sample many grids). */
    if (req == SDK_DIFF_HARD)        { orders = 1;  adjust_iters = 260; }
    else if (req == SDK_DIFF_MEDIUM) { orders = 8;  adjust_iters = 120; }
    else                             { orders = 8;  adjust_iters = 80; }

    wall_ms = p ? p->wall_guard_ms : 0;
    t0 = (int64_t)sdk_monotonic_ms();

    /* Step 1: a fully valid solved grid. */
    xr_seed(s, seed);
    generate_full_grid(s, &full);

    for (i = 0; i < 81; ++i) order[i] = i;

    for (o = 0; o < orders; ++o) {
        sdk_board puzzle;
        gen_eval e;
        int k;

        /* Step 2: candidate cells in random order. */
        shuffle_int(order, 81, s);
        sdk_board_copy(&full, &puzzle);

        /* Steps 3-6: remove clues while the puzzle stays uniquely solvable by
         * human logic and inside the requested difficulty's technique ceiling;
         * classify as soon as the clue count enters the sanity window. */
        for (k = 0; k < 81; ++k) {
            int cell = order[k];
            uint8_t saved;

            if (puzzle.cells[cell].value == 0) continue;
            if (count_given(&puzzle) <= lo) break;

            saved = puzzle.cells[cell].value;
            gen_put(&puzzle, cell, 0);

            /* docs/07 section 14 step 4: keep the removal only while the puzzle
             * stays uniquely solvable by human logic.  This solver makes only
             * forced deductions (T1-T8), so logic-solvability implies a unique
             * solution; docs/07 section 14 step 8 re-verifies uniqueness with
             * count_solutions(limit=2) on the accepted puzzle.  A per-removal
             * count_solutions probe is correct but far too slow to let
             * sdk_generate sample enough solved grids within wall_guard_ms. */
            gen_evaluate(&puzzle, 0, &e);
            if (!e.solvable || e.maxt > ceiling) {
                gen_put(&puzzle, cell, saved);
                continue;
            }
            if (gen_promising(req, &e, lo, hi) &&
                gen_final_verify(&puzzle, req, dr)) {
                finalize_puzzle(&puzzle, out, solution);
                return 1;
            }
        }

        /* Step 7: bounded guided adjustment of the candidate.  wall_ms == 0 =>
         * the adjust is bounded by iteration count only, keeping each attempt
         * deterministic; the outer sdk_generate wall guard supplies the time
         * budget across many attempted grids. */
        {
            sdk_board accepted;
            int64_t tadj = (int64_t)sdk_monotonic_ms();
            (void)t0;
            if (gen_adjust(&full, &puzzle, req, lo, hi, s, adjust_iters,
                           tadj, 0, &accepted, dr)) {
                finalize_puzzle(&accepted, out, solution);
                return 1;
            }
        }

        if (wall_ms > 0 && (int64_t)sdk_monotonic_ms() - t0 > wall_ms)
            break;
    }
    return 0;
}

void sdk_generate(const sdk_gen_params *params, sdk_board *out,
                  sdk_board *solution, uint64_t *seed_out,
                  sdk_gen_report *report) {
    sdk_gen_params p;
    uint64_t base_seed = 0;
    int fixed = 0;
    int64_t t0;
    int regen, found = 0;
    int total_attempts = 0;
    sdk_difficulty req = SDK_DIFF_EASY;

    if (report) memset(report, 0, sizeof *report);
    if (!params || !out || !report) {
        if (report) report->reject_reason = "missing arguments";
        return;
    }
    p = *params;
    req = p.requested;
    if (req != SDK_DIFF_EASY && req != SDK_DIFF_MEDIUM && req != SDK_DIFF_HARD) {
        report->reject_reason = "unsupported difficulty";
        return;
    }

    fixed = p.fixed_seed;
    if (fixed) {
        base_seed = p.seed;
    } else {
        uint8_t buf[32];
        if (sdk_random_bytes(buf, sizeof buf) == SDK_OK) {
            uint64_t x = 0; int i;
            for (i = 0; i < 32; ++i) x = (x << 8) | (uint64_t)buf[i];
            base_seed = x ? x : 0xDEADBEEFULL;
        } else {
            base_seed = (uint64_t)time(NULL) ^ 0x9E3779B97F4A7C15ULL;
        }
    }

    t0 = (int64_t)sdk_monotonic_ms();

    for (regen = 0; regen < 600 && !found; ++regen) {
        uint64_t seed = base_seed + (uint64_t)regen * 0x2545F4914F6CDD1DULL + 1;
        sdk_difficulty_result dr;
        sdk_board sol;
        memset(&sol, 0, sizeof sol);
        if (sdk_generate_once(seed, req, &p, out, &sol, &dr)) {
            if (solution) sdk_board_copy(&sol, solution);
            found = 1;
            report->actual = req;
            report->clue_count = dr.clue_count;
            report->logic_score = dr.logic_score;
            report->max_technique = dr.max_technique;
            { int t; for (t = 0; t < 9; ++t) report->tech_counts[t] = dr.tech_counts[t]; }
            report->seed_used = seed;
            report->attempts = total_attempts;
            report->regenerations = regen;
            report->duration_ms = (int64_t)sdk_monotonic_ms() - t0;
            report->accepted = 1;
            report->reject_reason = "";
            if (seed_out) *seed_out = seed;
            return;
        }
        total_attempts++;
        if (p.wall_guard_ms > 0 &&
            (int64_t)sdk_monotonic_ms() - t0 > p.wall_guard_ms) {
            break;
        }
    }

    report->accepted = 0;
    report->seed_used = base_seed;
    report->attempts = total_attempts;
    report->regenerations = regen;
    report->duration_ms = (int64_t)sdk_monotonic_ms() - t0;
    report->reject_reason = found ? "" : "could not satisfy requested difficulty within guards";
}
