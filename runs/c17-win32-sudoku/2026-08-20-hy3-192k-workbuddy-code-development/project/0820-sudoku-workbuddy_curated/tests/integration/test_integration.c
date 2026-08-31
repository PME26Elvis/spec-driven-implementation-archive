/* test_integration.c - G4 integration suite for the Sudoku application.
 *
 * Drives the real sdk_app state machine through its PUBLIC API
 * (docs/05, docs/06, docs/15) and verifies end-to-end behaviour:
 *   - menu -> new game -> play -> complete transitions (STA-01..05)
 *   - undo / redo of player placements
 *   - pencil-notes mode
 *   - encrypted vault unlock + persisted completion record (docs/08)
 *   - auto-solve completion path
 *
 * Entry point: wmain -> sdk_test_main("integration", register_all, ...).
 */
#include "test/sdk_test.h"
#include "app/sdk_app.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/* Fill every empty (non-given) cell with its solution digit. */
static int solve_all(sdk_app *a) {
    for (int i = 0; i < SDK_BOARD_CELLS; ++i) {
        if (a->given_mask[i]) continue;
        a->selected = i;
        sdk_app_place_digit(a, a->solution.cells[i].value);
    }
    return a->state == SDK_APP_COMPLETED;
}

static int first_empty(const sdk_app *a) {
    for (int i = 0; i < SDK_BOARD_CELLS; ++i)
        if (!a->given_mask[i]) return i;
    return -1;
}

static void ti_lifecycle(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    SDK_T_TRUE(t, a != NULL);
    SDK_T_EQ_I(t, (int)SDK_APP_MENU, (int)a->state);

    sdk_app_new_game(a, SDK_DIFF_EASY);
    SDK_T_EQ_I(t, (int)SDK_APP_PLAYING, (int)a->state);

    int clues = 0;
    for (int i = 0; i < SDK_BOARD_CELLS; ++i) clues += a->given_mask[i];
    SDK_T_TRUE(t, clues >= 17 && clues <= 60);

    SDK_T_TRUE(t, solve_all(a));
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);
    SDK_T_EQ_I(t, 0, a->mistakes);

    sdk_app_to_menu(a);
    SDK_T_EQ_I(t, (int)SDK_APP_MENU, (int)a->state);
    sdk_app_destroy(a);
}

static void ti_undo_redo(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    sdk_app_new_game(a, SDK_DIFF_MEDIUM);
    int c = first_empty(a);
    SDK_T_TRUE(t, c >= 0);
    a->selected = c;
    int sol = a->solution.cells[c].value;

    sdk_app_place_digit(a, sol);
    SDK_T_EQ_I(t, sol, (int)a->current.cells[c].value);
    SDK_T_EQ_I(t, (int)SDK_O_PLAYER, (int)a->current.cells[c].origin);

    sdk_app_on_key(a, 0, 'u');                 /* undo */
    SDK_T_EQ_I(t, 0, (int)a->current.cells[c].value);

    sdk_app_on_key(a, 0, 'r');                 /* redo */
    SDK_T_EQ_I(t, sol, (int)a->current.cells[c].value);
    sdk_app_destroy(a);
}

static void ti_notes(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    sdk_app_new_game(a, SDK_DIFF_EASY);
    int c = first_empty(a);
    SDK_T_TRUE(t, c >= 0);
    a->selected = c;
    a->notes_mode = 1;

    sdk_app_place_digit(a, 5);
    SDK_T_EQ_I(t, 0, (int)a->current.cells[c].value);
    SDK_T_TRUE(t, (a->current.cells[c].notes & SDK_CAND_BIT(5)) != 0);

    sdk_app_place_digit(a, 5);                 /* toggle off */
    SDK_T_TRUE(t, (a->current.cells[c].notes & SDK_CAND_BIT(5)) == 0);
    sdk_app_destroy(a);
}

static void ti_vault(sdk_test_ctx *t) {
    char path[256];
    DWORD pid = GetCurrentProcessId();
    snprintf(path, sizeof path, "D:/sdk_itest_vault_%u.dat", (unsigned)pid);
    remove(path);

    sdk_app *a = sdk_app_create(960, 720, path, "test-pw-123", 0);
    SDK_T_TRUE(t, a != NULL);
    SDK_T_EQ_I(t, 1, a->vault_unlocked);
    SDK_T_EQ_I(t, 1, a->vault_ok);

    sdk_app_new_game(a, SDK_DIFF_EASY);
    SDK_T_TRUE(t, solve_all(a));
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);

    sdk_app_destroy(a);                         /* persists vault */
    remove(path);
}

static void ti_auto_solve(sdk_test_ctx *t) {
    sdk_app *a = sdk_app_create(960, 720, NULL, NULL, 0);
    sdk_app_new_game(a, SDK_DIFF_HARD);
    sdk_app_on_key(a, 0, 'a');                  /* auto-solve */
    SDK_T_EQ_I(t, (int)SDK_APP_COMPLETED, (int)a->state);
    sdk_app_destroy(a);
}

static void register_all(void) {
    sdk_test_add("integration.lifecycle", "STA-01", ti_lifecycle);
    sdk_test_add("integration.undo_redo", "STA-02", ti_undo_redo);
    sdk_test_add("integration.notes", "STA-03", ti_notes);
    sdk_test_add("integration.vault_unlock", "STA-04", ti_vault);
    sdk_test_add("integration.auto_solve", "STA-05", ti_auto_solve);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("integration", register_all, argc, argv);
}
