/* sdk_sudoku_hint.c - Hint preview / apply and Auto Solve (docs/07 sections
 * 16-20, 30).
 *
 * Critical isolation property (docs/07 section 30, docs/19 section 30):
 *   - Hint derives candidates and steps ONLY from the current formal values
 *     via the human-logic solver. It never reads any stored solution.
 *   - Auto Solve fills from the search solver but marks origins as AUTO and
 *     never uses the solution to flag "wrong" player cells.
 *   - The vault/app layer is free to keep a stored solution for generation and
 *     testing; this module is provably independent of it.
 */
#include "sudoku/sdk_sudoku.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Hint                                                                */
/* ------------------------------------------------------------------ */

void sdk_hint_preview(const sdk_board *b, sdk_hint *out) {
    memset(out, 0, sizeof *out);

    /* Step 1: direct conflict validation. */
    out->has_direct_conflict = !sdk_validate_partial(b);

    /* Step 2: at least one solution must exist. A direct conflict implies
     * no solution, so short-circuit. */
    if (out->has_direct_conflict) {
        out->has_solution = 0;
        out->available = 0;
        return;
    }
    {
        sdk_board sol;
        out->has_solution = sdk_solve_one(b, &sol) ? 1 : 0;
    }
    if (!out->has_solution) {
        out->available = 0;
        return;
    }

    /* Steps 3-4: derive candidates from current formal values and find the
     * first logic step following the fixed scan order. */
    {
        sdk_logic_step step;
        if (sdk_logic_find_step(b, NULL, &step)) {
            out->available = 1;
            out->step = step;
        } else {
            out->available = 0;
        }
    }
}

int sdk_hint_apply(const sdk_board *b, const sdk_logic_step *step,
                   int remove_peer_notes, sdk_board *out) {
    int i;
    if (!b || !step || !out) return 0;
    sdk_board_copy(b, out);

    if (step->kind == SDK_STEP_PLACE) {
        int c = step->target_cell;
        if (c < 0 || c >= 81) return 0;
        if (out->cells[c].value != 0) return 0;     /* already filled */
        sdk_board_set(out, c, step->placed_digit, SDK_O_HINT);
        if (remove_peer_notes) {
            int peers[20], pn = sdk_peers(c, peers);
            for (i = 0; i < pn; ++i) {
                int pc = peers[i];
                if (out->cells[pc].value == 0)
                    out->cells[pc].notes &=
                        (uint16_t)(~SDK_CAND_BIT(step->placed_digit) & 0x01FFu);
            }
        }
    } else {
        /* Elimination only removes candidates the user actually holds. */
        for (i = 0; i < step->target_count; ++i) {
            int c = step->target_cells[i];
            if (c < 0 || c >= 81) continue;
            if (out->cells[c].value != 0) continue;
            out->cells[c].notes &=
                (uint16_t)(~step->affected_candidates & 0x01FFu);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Auto Solve                                                          */
/* ------------------------------------------------------------------ */

sdk_auto_result sdk_auto_solve(const sdk_board *in, sdk_board *out) {
    sdk_board sol;
    int i;

    if (!in || !out) return SDK_AUTO_FAILED;

    /* Direct conflict check: never overwrite, report conflicts. */
    if (!sdk_validate_partial(in)) return SDK_AUTO_CONFLICT;

    if (!sdk_solve_one(in, &sol)) return SDK_AUTO_UNSATISFIABLE;

    sdk_board_copy(in, out);
    for (i = 0; i < 81; ++i) {
        if (out->cells[i].value == 0) {
            out->cells[i].value = sol.cells[i].value;
            out->cells[i].origin = (uint8_t)SDK_O_AUTO;
            out->cells[i].notes = 0;
        }
    }
    return SDK_AUTO_OK;
}
