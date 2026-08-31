/* test_sudoku.c - core Sudoku engine unit tests (docs/07, docs/19).
 * Covers validation, solver, uniqueness, difficulty classification,
 * hint, auto-solve, and technique-band invariants (DIF-01..08, HNT-01..08,
 * SDK-01..20). */
#include "test/sdk_test.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"

#include <stdio.h>
#include <string.h>

/* a well-known minimal-ish puzzle and its unique solution */
static const char *FIXTURE_LINES[9] = {
    "53..7....",
    "6..195...",
    ".98....6.",
    "8...6...3",
    "4..8.3..1",
    "7...2...6",
    ".6....28.",
    "...419..5",
    "....8..79"
};
static const char *FIXTURE_SOL[9] = {
    "534678912",
    "672195348",
    "198342567",
    "859761423",
    "426853791",
    "713924856",
    "961537284",
    "287419635",
    "345286179"
};

static void tc_validate_and_solve(sdk_test_ctx *t) {
    sdk_board b;
    SDK_T_TRUE(t, sdk_board_parse(FIXTURE_LINES, &b));

    /* validate partial: no conflicts in the fixture */
    sdk_validation v;
    sdk_validate(&b, &v);
    SDK_T_EQ_I(t, 0, (int)v.valid_complete);

    /* solve_one must recover the known solution */
    sdk_board sol;
    SDK_T_TRUE(t, sdk_solve_one(&b, &sol));
    char buf[82];
    sdk_board_serialize(&sol, buf);
    char exp[82];
    for (int r = 0; r < 9; ++r) memcpy(exp + r * 9, FIXTURE_SOL[r], 9);
    exp[81] = 0;
    SDK_T_EQ_STR(t, exp, buf);

    /* unique solution */
    SDK_T_TRUE(t, sdk_is_unique(&b));
    int cnt = 0;
    SDK_T_TRUE(t, sdk_count_solutions(&b, 2, &cnt) && cnt == 1);
}

static void tc_conflict_detection(sdk_test_ctx *t) {
    sdk_board b;
    sdk_board_parse(FIXTURE_LINES, &b);
    /* force a duplicate in row 0: set cell 1 to '3' (cell 0 already '5', but
       column 1 has a 3 at row2? craft a direct row duplicate instead) */
    b.cells[1].value = 5;   /* two 5s in row 0 */
    sdk_validation v;
    sdk_validate(&b, &v);
    SDK_T_EQ_I(t, 1, (int)v.cell_conflict[0]);
    SDK_T_EQ_I(t, 1, (int)v.cell_conflict[1]);
    SDK_T_EQ_I(t, 0, (int)v.valid_complete);
    SDK_T_EQ_I(t, 1, (int)sdk_submit_state(&v) < 2); /* not a valid-complete */
}

static void tc_logic_solve_trace(sdk_test_ctx *t) {
    sdk_board b; sdk_board_parse(FIXTURE_LINES, &b);
    sdk_board out; sdk_logic_trace tr;
    SDK_T_TRUE(t, sdk_logic_solve(&b, &out, &tr));
    SDK_T_EQ_I(t, 1, tr.solved);
    SDK_T_EQ_I(t, 0, tr.stalled);
    /* fixture is easy-ish: should be solvable by singles */
    SDK_T_TRUE(t, tr.max_technique <= SDK_TECH_HIDDEN_SINGLE + 1);
    /* determinism: second run identical */
    sdk_board out2; sdk_logic_trace tr2;
    sdk_logic_solve(&b, &out2, &tr2);
    SDK_T_EQ_I(t, tr.logic_score, tr2.logic_score);
    SDK_T_EQ_I(t, tr.max_technique, tr2.max_technique);
    SDK_T_EQ_I(t, tr.step_count, tr2.step_count);
}

static void tc_hint_and_auto(sdk_test_ctx *t) {
    sdk_board b; sdk_board_parse(FIXTURE_LINES, &b);
    sdk_hint h;
    sdk_hint_preview(&b, &h);
    SDK_T_TRUE(t, h.available);
    SDK_T_TRUE(t, h.step.target_cell >= 0);
    SDK_T_TRUE(t, h.has_solution);

    sdk_board after;
    SDK_T_TRUE(t, sdk_hint_apply(&b, &h.step, 1, &after));
    /* applying a placement step must fill the target */
    SDK_T_EQ_I(t, h.step.placed_digit, (int)after.cells[h.step.target_cell].value);

    /* auto-solve completes it */
    sdk_board done;
    sdk_auto_result ar = sdk_auto_solve(&b, &done);
    SDK_T_EQ_I(t, SDK_AUTO_OK, (int)ar);
    sdk_validation v; sdk_validate(&done, &v);
    SDK_T_EQ_I(t, 1, v.valid_complete);
}

/* difficulty-band invariants over a sample of generated puzzles.
 * Uses RANDOM seeds (fixed_seed=0) so the digger can vary solved grids and
 * reach each band quickly (docs/07 section 12 / 27). */
static void tc_difficulty_bands(sdk_test_ctx *t) {
    sdk_difficulty reqs[3] = { SDK_DIFF_EASY, SDK_DIFF_MEDIUM, SDK_DIFF_HARD };
    int counts[3] = { 20, 20, 10 };   /* HARD kept small in the unit suite */
    int fails = 0, accepted = 0;
    for (int di = 0; di < 3; ++di) {
        int got = 0;
        for (int s = 0; s < counts[di] * 3 && got < counts[di]; ++s) {
            sdk_gen_params p; sdk_gen_params_default(&p, reqs[di]);
            p.fixed_seed = 0;   /* let the generator vary seeds */
            sdk_board puzzle, sol; sdk_gen_report rep;
            sdk_generate(&p, &puzzle, &sol, NULL, &rep);
            if (!rep.accepted) continue;
            got++; accepted++;
            sdk_difficulty_result dr;
            sdk_classify_difficulty(&puzzle, &dr);
            if (dr.difficulty != reqs[di]) fails++;
            if (reqs[di] == SDK_DIFF_EASY) {
                if (rep.clue_count < 36 || rep.clue_count > 49) fails++;
                if (rep.logic_score < 20 || rep.logic_score > 120) fails++;
                if (dr.max_technique > SDK_TECH_HIDDEN_SINGLE) fails++;
            } else if (reqs[di] == SDK_DIFF_MEDIUM) {
                if (rep.clue_count < 30 || rep.clue_count > 40) fails++;
                if (rep.logic_score < 70 || rep.logic_score > 260) fails++;
                if (dr.tech_counts[SDK_TECH_NAKED_TRIPLE] || dr.tech_counts[SDK_TECH_HIDDEN_TRIPLE])
                    fails++;  /* MEDIUM must not require T7/T8 */
            } else {
                if (rep.clue_count < 24 || rep.clue_count > 34) fails++;
                if (rep.logic_score < 180 || rep.logic_score > 520) fails++;
                int hard = dr.tech_counts[SDK_TECH_NAKED_TRIPLE] + dr.tech_counts[SDK_TECH_HIDDEN_TRIPLE];
                int mid  = dr.tech_counts[SDK_TECH_NAKED_PAIR] + dr.tech_counts[SDK_TECH_HIDDEN_PAIR];
                if (hard == 0 && mid < 3) fails++;  /* HARD needs T7/T8 or >=3 T5/T6 */
            }
            sdk_validation v; sdk_validate(&sol, &v);
            if (!v.valid_complete) fails++;
        }
        SDK_T_TRUE(t, got >= counts[di]);  /* collected the required sample */
    }
    SDK_T_TRUE(t, fails == 0);
}

static void tc_generator_determinism(sdk_test_ctx *t) {
    sdk_gen_params p1, p2;
    sdk_gen_params_default(&p1, SDK_DIFF_MEDIUM);
    sdk_gen_params_default(&p2, SDK_DIFF_MEDIUM);
    p1.fixed_seed = p2.fixed_seed = 1;
    p1.seed = p2.seed = 424242;
    sdk_board b1, b2, s1, s2; sdk_gen_report r1, r2;
    sdk_generate(&p1, &b1, &s1, NULL, &r1);
    sdk_generate(&p2, &b2, &s2, NULL, &r2);
    SDK_T_EQ_MEM(t, &b1, &b2, sizeof(sdk_board));
    SDK_T_EQ_MEM(t, &s1, &s2, sizeof(sdk_board));
}

static void register_all(void) {
    sdk_test_add("sudoku-validate-and-solve", "SDK-01,SDK-03,SDK-07", tc_validate_and_solve);
    sdk_test_add("sudoku-conflict-detection", "SDK-04,SDK-05", tc_conflict_detection);
    sdk_test_add("sudoku-logic-solve-trace", "SDK-08,SDK-09,SDK-10", tc_logic_solve_trace);
    sdk_test_add("sudoku-hint-and-auto", "HNT-01,HNT-03,HNT-05,SDK-14", tc_hint_and_auto);
    sdk_test_add("sudoku-difficulty-bands", "DIF-01,DIF-02,DIF-03,DIF-04,DIF-05,DIF-06,DIF-07,DIF-08",
                 tc_difficulty_bands);
    sdk_test_add("sudoku-generator-determinism", "SDK-19,G9.1", tc_generator_determinism);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("unit-sudoku", register_all, argc, argv);
}
