/* test_e2e.c - G5 end-to-end suite for the Sudoku application.
 *
 * Drives the real sdk_app exclusively through its PUBLIC INPUT API
 * (docs/05, docs/06, docs/15, docs/21):
 *   - sdk_app_on_key      (keyboard: digits, Enter, undo/redo, auto-solve)
 *   - sdk_app_on_mouse_*  (pointer dispatch path)
 * Selection is assigned via the public `selected` field exactly as a click
 * would set it. Exercises the canonical acceptance scenarios end-to-end.
 *
 * Entry point: wmain -> sdk_test_main("e2e", register_all, ...).
 */
#include "test/sdk_test.h"
#include "app/sdk_app.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"

#include <stdio.h>
#include <string.h>

static int solve_all(sdk_app *a) {
    for (int i = 0; i < SDK_BOARD_CELLS; ++i) {
        if (a->given_mask[i]) continue;
        a->selected = i;
        sdk_app_on_key(a, 0, '0' + a->solution.cells[i].value);
    }
    return a->state == SDK_APP_COMPLETED;
}

static int first_empty(const sdk_app *a) {
    for (int i = 0; i < SDK_BOARD_CELLS; ++i)
        if (!a->given_mask[i]) return i;
    return -1;
}

/* docs/21 scenario: start from menu via Enter, solve via keyboard, then
 * restart from the completed screen via Enter. */
static void te_keyboard_play(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    SDK_T_EQ_I(t, (int)SDK_APP_MENU, (int)a->state);

    sdk_app_on_key(a, 0x0D, 0);                 /* Enter -> new game */
    SDK_T_EQ_I(t, (int)SDK_APP_PLAYING, (int)a->state);

    /* exercise the pointer dispatch path (no crash, valid state retained) */
    sdk_app_on_mouse_move(a, 120, 120);
    sdk_app_on_mouse_down(a, 120, 120);
    sdk_app_on_mouse_up(a, 120, 120);
    SDK_T_EQ_I(t, (int)SDK_APP_PLAYING, (int)a->state);

    SDK_T_TRUE(t, solve_all(a));
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);

    sdk_app_on_key(a, 0x0D, 0);                 /* Enter on completed -> new game */
    SDK_T_EQ_I(t, (int)SDK_APP_PLAYING, (int)a->state);
    sdk_app_destroy(a);
}

/* docs/21 scenario: a wrong move is counted but does not complete; the board
 * can still be corrected to a win. */
static void te_mistake_then_win(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    sdk_app_new_game(a, SDK_DIFF_EASY);
    int c = first_empty(a);
    SDK_T_TRUE(t, c >= 0);
    a->selected = c;
    int sol = a->solution.cells[c].value;
    int wrong = (sol == 9) ? 8 : 9;

    sdk_app_on_key(a, 0, '0' + wrong);          /* wrong digit via keyboard */
    SDK_T_EQ_I(t, 1, a->mistakes);
    SDK_T_EQ_I(t, (int)SDK_APP_PLAYING, (int)a->state);

    sdk_app_on_key(a, 0x08, 0);                 /* Backspace -> clear */
    SDK_T_EQ_I(t, 0, (int)a->current.cells[c].value);

    SDK_T_TRUE(t, solve_all(a));
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);
    SDK_T_EQ_I(t, 1, a->mistakes);              /* the single wrong move remains */
    sdk_app_destroy(a);
}

/* docs/21 scenario: hard puzzle solved end-to-end via the auto-solve key. */
static void te_auto_complete(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    sdk_app_new_game(a, SDK_DIFF_HARD);
    sdk_app_on_key(a, 0, 'a');                  /* auto-solve */
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);
    sdk_app_destroy(a);
}

static void register_all(void) {
    sdk_test_add("e2e.keyboard_play", "E2E-01", te_keyboard_play);
    sdk_test_add("e2e.mistake_then_win", "E2E-02", te_mistake_then_win);
    sdk_test_add("e2e.auto_complete", "E2E-03", te_auto_complete);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("e2e", register_all, argc, argv);
}
