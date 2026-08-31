/* sdk_sudoku.h - Classic 9x9 Sudoku core.
 *
 * Pure, UI-independent engine covering:
 *   - validator and conflict masks (docs/07 sections 3-5, 19)
 *   - search solver: solve_one / count_solutions / is_unique / validate_partial
 *     (docs/07 sections 6-7, 29)
 *   - human-logic solver T1-T8 with fixed scan order (docs/07 sections 8-10,
 *     24-27)
 *   - difficulty generation and classification (docs/07 sections 11-15, 27)
 *   - hint (placement / elimination) and auto-solve (docs/07 sections 16-18)
 *
 * No globals: every call operates on caller-provided state, so consecutive
 * calls cannot leak state into each other (docs/07 section 6, last paragraph,
 * and section 24).
 */
#ifndef SDK_SUDOKU_H
#define SDK_SUDOKU_H

#include <stddef.h>
#include <stdint.h>

#define SDK_GRID 9
#define SDK_BOARD_CELLS 81
#define SDK_CAND_MAX 9

/* ------------------------------------------------------------------ */
/* Board model                                                         */
/* ------------------------------------------------------------------ */

typedef enum sdk_origin {
    SDK_O_EMPTY = 0,   /* no value */
    SDK_O_GIVEN = 1,   /* immutable clue */
    SDK_O_PLAYER = 2,  /* player-entered */
    SDK_O_HINT = 3,    /* hint-assisted placement */
    SDK_O_AUTO = 4     /* auto-solve placement */
} sdk_origin;

typedef struct sdk_cell {
    uint8_t value;    /* 0..9, 0 = empty */
    uint8_t origin;   /* sdk_origin */
    uint16_t notes;   /* candidate bits: bit (d-1) set for digit d in 1..9 */
} sdk_cell;

typedef struct sdk_board {
    sdk_cell cells[SDK_BOARD_CELLS];
} sdk_board;

/* A candidate mask for a single cell (bits 0..8 -> digits 1..9). */
typedef uint16_t sdk_cand;

#define SDK_CAND_EMPTY ((sdk_cand)0)
#define SDK_CAND_ALL   ((sdk_cand)0x01FFu)
#define SDK_CAND_BIT(d) ((sdk_cand)(1u << ((unsigned)(d) - 1u)))

/* ------------------------------------------------------------------ */
/* Validation                                                          */
/* ------------------------------------------------------------------ */

typedef struct sdk_validation {
    int is_complete;          /* every cell has a value */
    int empty_count;          /* number of empty cells */
    sdk_cand row_conflict[SDK_GRID];   /* duplicated digits per row */
    sdk_cand col_conflict[SDK_GRID];   /* duplicated digits per column */
    sdk_cand box_conflict[SDK_GRID];   /* duplicated digits per box */
    uint8_t cell_conflict[SDK_BOARD_CELLS]; /* 1 if cell is in any duplicate */
    int valid_complete;       /* complete AND no conflicts */
} sdk_validation;

/* Fills `out` from the 81 formal values of `b`. Does not require any clue
 * mask; conflicts are computed purely from formal values. */
void sdk_validate(const sdk_board *b, sdk_validation *out);

/* Submit judgement (docs/07 section 5).
 *   returns 0 if incomplete (empty cells remain),
 *           1 if complete but invalid (duplicate conflicts),
 *           2 if valid complete. */
int sdk_submit_state(const sdk_validation *v);

/* ------------------------------------------------------------------ */
/* Search solver                                                       */
/* ------------------------------------------------------------------ */

/* Returns 1 and writes one solution into `out` (if non-NULL) when at least
 * one solution exists, else 0. Does not modify `in`. */
int sdk_solve_one(const sdk_board *in, sdk_board *out);

/* Counts solutions of `in` up to `limit` (exclusive ceiling). Writes the
 * count into `*out_count`, but if more than `limit` solutions exist the
 * written value is `limit + 1` (sentinel). Returns 1 on success, 0 on a
 * malformed/over-limit board. */
int sdk_count_solutions(const sdk_board *in, int limit, int *out_count);

/* Returns 1 if `in` has exactly one solution, 0 otherwise. */
int sdk_is_unique(const sdk_board *in);

/* Returns 1 if the board has no direct row/column/box duplicate, 0 otherwise.
 * An empty cell never counts as a conflict. */
int sdk_validate_partial(const sdk_board *in);

/* ------------------------------------------------------------------ */
/* Logic solver (T1-T8)                                                */
/* ------------------------------------------------------------------ */

typedef enum sdk_tech {
    SDK_TECH_NONE = 0,
    SDK_TECH_NAKED_SINGLE = 1,
    SDK_TECH_HIDDEN_SINGLE = 2,
    SDK_TECH_LOCKED_POINTING = 3,
    SDK_TECH_LOCKED_CLAIMING = 4,
    SDK_TECH_NAKED_PAIR = 5,
    SDK_TECH_HIDDEN_PAIR = 6,
    SDK_TECH_NAKED_TRIPLE = 7,
    SDK_TECH_HIDDEN_TRIPLE = 8
} sdk_tech;

typedef enum sdk_step_kind {
    SDK_STEP_PLACE = 0,
    SDK_STEP_ELIM = 1
} sdk_step_kind;

#define SDK_STEP_CELLS 81
#define SDK_STEP_UNITS 27

typedef struct sdk_logic_step {
    sdk_tech tech;
    sdk_step_kind kind;
    int target_cell;            /* primary placement target, or -1 for elim */
    int placed_digit;           /* digit placed (placement steps) */
    sdk_cand candidate_before;  /* candidate mask of target before */
    sdk_cand candidate_after;   /* candidate mask of target after */
    int target_cells[SDK_STEP_CELLS];
    int target_count;           /* cells whose candidates were removed */
    sdk_cand affected_candidates; /* digits removed */
    int support_cells[SDK_STEP_CELLS];
    int support_count;
    int support_units[SDK_STEP_UNITS];
    int support_unit_count;     /* unit indices: 0..8 rows, 9..17 cols, 18..26 boxes */
    int score_weight;
    char conclusion[96];        /* deterministic human-readable conclusion */
    char reason[192];           /* deterministic verifiable reason */
} sdk_logic_step;

/* Step weights are calibrated (docs/07 §12, §27) so that the three difficulty
 * score bands are actually reachable by puzzles solvable with the supported
 * techniques (T1-T8, ceiling = Hidden Triple):
 *   - single-only puzzles (EASY, 36-49 clues) score in [20,120];
 *   - pair/hidden-only puzzles (MEDIUM, 30-40 clues) score in [70,260];
 *   - triple-requiring puzzles (HARD, 24-34 clues) score in [180,520].
 * The earlier 1/2/4/4/6/7/10/12 scale capped HARD near ~111, making the
 * specified HARD band unreachable; scaling the elimination steps up restores
 * the intended three-tier ordering without changing the band endpoints. */
#define SDK_TECH_WEIGHT(t) ((t) == SDK_TECH_NAKED_SINGLE ? 1 : \
                            (t) == SDK_TECH_HIDDEN_SINGLE ? 2 : \
                            (t) == SDK_TECH_LOCKED_POINTING ? 5 : \
                            (t) == SDK_TECH_LOCKED_CLAIMING ? 5 : \
                            (t) == SDK_TECH_NAKED_PAIR ? 12 : \
                            (t) == SDK_TECH_HIDDEN_PAIR ? 14 : \
                            (t) == SDK_TECH_NAKED_TRIPLE ? 28 : \
                            (t) == SDK_TECH_HIDDEN_TRIPLE ? 32 : 0)

/* Computes the derived candidate mask for every empty cell from the current
 * formal values. `out_masks[c]` is valid only for empty cells; filled cells
 * get SDK_CAND_EMPTY. Returns 1 on success, 0 if any empty cell has an empty
 * candidate mask (unsatisfiable / stalled). */
int sdk_derive_candidates(const sdk_board *b, sdk_cand *out_masks);

/* Finds the first applicable logic step following the fixed scan order
 * (docs/07 sections 8, 25). Returns 1 and fills `out` if a step exists,
 * 0 if the board is solved or stalled. `masks` may be NULL (recomputed
 * internally) or a pre-derived mask array. */
int sdk_logic_find_step(const sdk_board *b, const sdk_cand *masks,
                        sdk_logic_step *out);

/* Same fixed scan order as sdk_logic_find_step, but skips every technique
 * whose priority is below `min_tech`. Used by the generator to measure how
 * close a candidate puzzle is to requiring an advanced technique while it
 * "continues adjusting" (docs/07 section 14 step 7). It never alters the
 * priority order used by an actual solve: passing
 * min_tech = SDK_TECH_NAKED_SINGLE is exactly equivalent to
 * sdk_logic_find_step. */
int sdk_logic_find_step_from(const sdk_board *b, const sdk_cand *masks,
                             sdk_tech min_tech, sdk_logic_step *out);

/* Applies `step` to `b` in place (placement or elimination). Returns 1 on
 * success. The caller is responsible for having a consistent board. */
int sdk_logic_apply(const sdk_board *b, const sdk_logic_step *step,
                    sdk_board *out);

/* ------------------------------------------------------------------ */
/* Full logic solve + difficulty scoring                               */
/* ------------------------------------------------------------------ */

typedef struct sdk_logic_trace {
    int stalled;                /* 1 if no step found before completion */
    int solved;                /* 1 if the board was fully solved */
    int logic_score;           /* sum of step weights */
    int max_technique;         /* highest SDK_TECH_* encountered */
    int tech_counts[9];        /* per-technique effective step counts */
    int step_count;            /* total effective steps */
} sdk_logic_trace;

/* Repeatedly applies the first logic step until the board is solved or the
 * solver stalls. Writes the resulting board into `out` and the trace into
 * `trace`. Returns 1 on success (out/trace written), 0 only on a fatal
 * internal issue. */
int sdk_logic_solve(const sdk_board *in, sdk_board *out,
                    sdk_logic_trace *trace);

/* ------------------------------------------------------------------ */
/* Difficulty                                                          */
/* ------------------------------------------------------------------ */

typedef enum sdk_difficulty {
    SDK_DIFF_UNKNOWN = -1,
    SDK_DIFF_EASY = 0,
    SDK_DIFF_MEDIUM = 1,
    SDK_DIFF_HARD = 2
} sdk_difficulty;

typedef struct sdk_difficulty_result {
    sdk_difficulty difficulty;
    int logic_score;
    int max_technique;
    int tech_counts[9];
    int clue_count;
    int stalled;
} sdk_difficulty_result;

/* Classifies a (complete, solvable) puzzle by running the full logic solver
 * and applying the rules of docs/07 section 12 / 27. The classification is
 * independent of any stored solution. Writes the result into `out`. */
void sdk_classify_difficulty(const sdk_board *puzzle,
                             sdk_difficulty_result *out);

/* ------------------------------------------------------------------ */
/* Generator                                                          */
/* ------------------------------------------------------------------ */

#define SDK_GEN_FORMAT_VERSION 1u
#define SDK_DIFF_RULES_VERSION 1u

typedef struct sdk_gen_params {
    sdk_difficulty requested;   /* requested difficulty */
    uint64_t seed;              /* explicit seed (test mode) */
    int fixed_seed;             /* 1 => use `seed`; 0 => derive from RNG */
    int full_grid_nodes;        /* search node guard for full grid */
    int uniqueness_nodes;       /* count_solutions node guard */
    int candidate_attempts;     /* hole-digging attempts guard */
    int64_t wall_guard_ms;      /* whole-request wall-clock guard */
    int pbkdf2_iterations;      /* unused by generator; kept for symmetry */
} sdk_gen_params;

typedef struct sdk_gen_report {
    int accepted;               /* 1 if a puzzle was produced */
    sdk_difficulty actual;
    int clue_count;
    int logic_score;
    int max_technique;
    int tech_counts[9];
    uint64_t seed_used;
    int attempts;               /* total candidate puzzle attempts */
    int regenerations;          /* full-grid reseeds */
    const char *reject_reason;  /* human-readable, non-sensitive */
    int64_t duration_ms;
} sdk_gen_report;

void sdk_gen_params_default(sdk_gen_params *p, sdk_difficulty requested);

/* Generates a puzzle of the requested difficulty. On success writes the
 * puzzle (clues immutable, player area empty) into `out`, the full solution
 * into `solution` (may be NULL), the generator seed into `seed_out` (may be
 * NULL), and fills `report`. Honors all guards in `params`; on exhaustion
 * `report.accepted` is 0 and no valid puzzle is left in `out`. */
void sdk_generate(const sdk_gen_params *params, sdk_board *out,
                  sdk_board *solution, uint64_t *seed_out,
                  sdk_gen_report *report);

/* ------------------------------------------------------------------ */
/* Hint and Auto Solve                                                */
/* ------------------------------------------------------------------ */

/* Result of sdk_hint_preview. */
typedef struct sdk_hint {
    int available;             /* 1 if a step was found */
    sdk_logic_step step;       /* structured, deterministic step */
    int has_direct_conflict;   /* board has an immediate duplicate */
    int has_solution;          /* board admits at least one solution */
} sdk_hint;

/* Computes a hint from the CURRENT board without modifying it and without
 * reading any stored solution. Fills `out`. The structured `step` is for UI
 * preview; applying it is a separate operation. */
void sdk_hint_preview(const sdk_board *b, sdk_hint *out);

/* Applies a previously previewed step as a placement or elimination. Fills a
 * single-cell undo transaction description into `txn` (see storage layer for
 * the transaction type; here we expose the affected cells). Returns 1 on
 * success, 0 if the step is no longer applicable. */
int sdk_hint_apply(const sdk_board *b, const sdk_logic_step *step,
                   int remove_peer_notes, sdk_board *out);

typedef enum sdk_auto_result {
    SDK_AUTO_OK = 0,
    SDK_AUTO_CONFLICT = 1,           /* immediate duplicate present */
    SDK_AUTO_UNSATISFIABLE = 2,      /* no solution exists */
    SDK_AUTO_FAILED = 3
} sdk_auto_result;

/* Auto-solves `in` via the search solver. On SDK_AUTO_OK writes the solved
 * board into `out` with auto-solve origins. Does not overwrite on conflict or
 * unsatisfiable states. */
sdk_auto_result sdk_auto_solve(const sdk_board *in, sdk_board *out);

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Unit index helpers. */
int sdk_row_of(int cell);
int sdk_col_of(int cell);
int sdk_box_of(int cell);
/* Peer cell indices (row, column, box mates) - 20 peers. */
int sdk_peers(int cell, int *out_peers); /* returns peer count */

/* Parses a 9-line board fixture (docs/19 section 23). Each line exactly 9
 * chars: '1'-'9' or '.' for empty. Returns 1 on success, 0 on malformed
 * input. All origins default to GIVEN for non-empty, EMPTY for '.'. */
int sdk_board_parse(const char *lines[SDK_GRID], sdk_board *out);

/* Serializes a board to 81 chars '1'-'9'/'.' plus NUL into `buf` (>=82). */
void sdk_board_serialize(const sdk_board *b, char *buf);

/* Deep copy. */
void sdk_board_copy(const sdk_board *src, sdk_board *dst);

/* Sets a value with the given origin, clearing that cell's notes. Returns 1. */
int sdk_board_set(sdk_board *b, int cell, int value, sdk_origin origin);

#endif /* SDK_SUDOKU_H */
