/* sdk_app.c - application core: lifecycle, input dispatch, undo/redo,
 * win detection, and vault persistence (docs/05, docs/06, docs/15, docs/19). */
#include "app/sdk_app.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */
static uint64_t _now(void) { return sdk_monotonic_ms(); }

sdk_app *sdk_app_create(int width, int height, const char *vault_path,
                         const char *vault_pw, int prod_iters) {
    sdk_app *a = (sdk_app *)calloc(1, sizeof *a);
    if (!a) return NULL;
    a->width = width > 0 ? width : 960;
    a->height = height > 0 ? height : 720;
    a->theme_kind = SDK_THEME_DARK;
    sdk_theme_load(a->theme_kind, &a->theme);
    a->boot_ms = _now();
    a->ripples = sdk_ripple_create(64);
    a->state = SDK_APP_MENU;
    a->selected = -1;
    a->menu_diff_idx = 0;
    a->tab_slide = 0.0;
    a->reduced_motion = 0;

    /* optional encrypted persistence (docs/08, docs/19) */
    if (vault_path && vault_path[0]) {
        size_t n = 0;
        while (vault_path[n]) { a->vault_path[n] = vault_path[n]; if (++n >= sizeof a->vault_path) break; }
        a->vault_path[n] = 0;
        a->store = (sdk_store *)calloc(1, sizeof(sdk_store));
        if (vault_pw && vault_pw[0]) {
            sdk_app_unlock(a, vault_pw, prod_iters);
        } else {
            a->vault_unlocked = 0;   /* show on-screen unlock screen */
        }
    }
    return a;
}

void sdk_app_destroy(sdk_app *a) {
    if (!a) return;
    if (a->vault) {
        if (a->store) sdk_vault_save(a->vault, a->store);
        sdk_vault_close(a->vault);
    }
    if (a->store) sdk_store_free(a->store);
    sdk_ripple_destroy(a->ripples);
    free(a);
}

void sdk_app_set_size(sdk_app *a, int w, int h) {
    if (!a) return;
    if (w > 0) a->width = w;
    if (h > 0) a->height = h;
}

/* ------------------------------------------------------------------ */
/* feedback                                                            */
/* ------------------------------------------------------------------ */
void sdk_app_toast(sdk_app *a, int kind, const char *msg) {
    if (!a || !msg) return;
    a->toast_kind = kind;
    size_t n = 0;
    while (msg[n] && n + 1 < sizeof a->toast) { a->toast[n] = msg[n]; ++n; }
    a->toast[n] = 0;
    a->toast_until = _now() + 2600;
}

/* ------------------------------------------------------------------ */
/* undo / redo                                                         */
/* ------------------------------------------------------------------ */
static void _push_undo(sdk_app *a) {
    if (a->undo_n >= SDK_UNDO_CAP) return;
    a->undo_stack[a->undo_n++] = a->current;
    a->redo_n = 0;
}
static void _undo(sdk_app *a) {
    if (a->undo_n <= 0) return;
    if (a->redo_n < SDK_UNDO_CAP) a->redo_stack[a->redo_n++] = a->current;
    a->current = a->undo_stack[--a->undo_n];
}
static void _redo(sdk_app *a) {
    if (a->redo_n <= 0) return;
    if (a->undo_n < SDK_UNDO_CAP) a->undo_stack[a->undo_n++] = a->current;
    a->current = a->redo_stack[--a->redo_n];
}

/* ------------------------------------------------------------------ */
/* game setup / transitions                                            */
/* ------------------------------------------------------------------ */
void sdk_app_new_game(sdk_app *a, sdk_difficulty d) {
    if (!a) return;
    sdk_gen_params p;
    sdk_gen_params_default(&p, d);
    p.fixed_seed = 0;
    p.wall_guard_ms = 0;
    sdk_board sol;
    sdk_gen_report rep;
    sdk_generate(&p, &a->puzzle, &sol, NULL, &rep);
    if (!rep.accepted) {
        sdk_app_toast(a, 3, "GENERATION FAILED");
        return;
    }
    a->solution = sol;
    sdk_board_copy(&a->puzzle, &a->current);
    memset(a->given_mask, 0, sizeof a->given_mask);
    for (int i = 0; i < SDK_BOARD_CELLS; ++i)
        if (a->puzzle.cells[i].value) a->given_mask[i] = 1;
    a->diff = d;
    a->selected = -1;
    a->notes_mode = 0;
    a->mistakes = 0;
    a->undo_n = a->redo_n = 0;
    a->paused = 0;
    a->play_elapsed_ms = 0;
    a->play_start_ms = _now();
    a->anim = 0; a->animating = 1;
    a->state = SDK_APP_PLAYING;
}

void sdk_app_to_menu(sdk_app *a) {
    if (!a) return;
    a->state = SDK_APP_MENU;
    a->selected = -1;
}

void sdk_app_quit(sdk_app *a) {
    if (!a) return;
    a->quit_requested = 1;
    a->state = SDK_APP_QUIT;
}

/* opens or creates the encrypted vault with the given password (docs/08). */
int sdk_app_unlock(sdk_app *a, const char *pw, int prod_iters) {
    if (!a || !pw || !pw[0]) return 0;
    size_t n = 0;
    while (pw[n] && n + 1 < sizeof a->vault_pw) { a->vault_pw[n] = pw[n]; ++n; }
    a->vault_pw[n] = 0;
    if (!a->store) a->store = (sdk_store *)calloc(1, sizeof(sdk_store));
    if (!a->store) return 0;
    if (a->vault) { sdk_vault_close(a->vault); a->vault = NULL; }
    sdk_status st = sdk_vault_open(a->vault_path, pw, &a->vault, a->store);
    if (st == SDK_OK && a->vault) {
        a->vault_ok = 1; a->vault_unlocked = 1;
        a->theme_kind = a->store->settings.theme ? SDK_THEME_LIGHT : SDK_THEME_DARK;
        sdk_theme_load(a->theme_kind, &a->theme);
        a->menu_diff_idx = a->store->settings.last_difficulty % 3;
        a->tab_slide = (double)a->menu_diff_idx;
        return 1;
    }
    /* create fresh vault on first run (file absent). Wrong password on an
     * existing file leaves it intact (create refuses to overwrite). */
    sdk_store_init(a->store);
    a->store->settings.theme = (uint8_t)a->theme_kind;
    a->store->settings.last_difficulty = (uint8_t)a->menu_diff_idx;
    st = sdk_vault_create(a->vault_path, pw, prod_iters ? 1 : 0, a->store, &a->vault);
    if (st == SDK_OK && a->vault) {
        a->vault_ok = 1; a->vault_unlocked = 1;
        sdk_vault_save(a->vault, a->store);
        return 1;
    }
    a->vault_unlocked = 0;
    return 0;
}

int sdk_app_wants_quit(const sdk_app *a) { return a && a->quit_requested; }

/* ------------------------------------------------------------------ */
/* win detection                                                       */
/* ------------------------------------------------------------------ */
static int _clue_count(const sdk_app *a) {
    int n = 0;
    for (int i = 0; i < SDK_BOARD_CELLS; ++i) if (a->given_mask[i]) n++;
    return n;
}

static void _fill_completed(sdk_app *a, sdk_completed_record *rec, int auto_solved) {
    memset(rec, 0, sizeof *rec);
    sdk_vault_new_game_id(rec->id);
    rec->difficulty = (int)a->diff;
    rec->diff_rules_ver = SDK_DIFF_RULES_VERSION;
    rec->gen_format_ver = SDK_GEN_FORMAT_VERSION;
    rec->active_elapsed_ms = (uint64_t)a->last_elapsed_ms;
    uint64_t t = _now();
    rec->created_epoch_ms = t;
    rec->last_played_epoch_ms = t;
    rec->completed_epoch_ms = t;
    rec->completion_class = auto_solved ? 2 : 0;   /* 2 AUTO_SOLVED else UNASSISTED */
    rec->used_auto_solve = auto_solved ? 1 : 0;
    rec->clue_count = _clue_count(a);
    for (int i = 0; i < SDK_BOARD_CELLS; ++i) {
        rec->orig[i]   = a->puzzle.cells[i].value;
        rec->grid[i]   = a->current.cells[i].value;
        rec->origin[i] = a->current.cells[i].origin;
    }
}

static void _check_win(sdk_app *a) {
    sdk_validation v;
    sdk_validate(&a->current, &v);
    if (v.valid_complete) {
        a->last_won = 1;
        a->last_mistakes = a->mistakes;
        a->last_elapsed_ms = (int64_t)(a->play_elapsed_ms +
                              (_now() - a->play_start_ms));
        a->last_diff = a->diff;
        if (a->vault_ok && a->store) {
            sdk_completed_record rec;
            _fill_completed(a, &rec, 0);
            sdk_store_add_completed(a->store, &rec);
            sdk_vault_save(a->vault, a->store);
        }
        a->state = SDK_APP_COMPLETED;
    }
}

/* ------------------------------------------------------------------ */
/* play interactions                                                   */
/* ------------------------------------------------------------------ */
void sdk_app_place_digit(sdk_app *a, int digit) {
    int c = a->selected;
    if (c < 0 || c >= SDK_BOARD_CELLS) return;
    if (a->given_mask[c]) return;                 /* clue locked */
    if (a->notes_mode) {
        /* toggle pencil note */
        _push_undo(a);
        if (a->current.cells[c].value == 0) {
            a->current.cells[c].notes ^= SDK_CAND_BIT(digit);
        }
        return;
    }
    if (a->current.cells[c].value == digit) return; /* no-op */
    _push_undo(a);
    int ok = sdk_board_set(&a->current, c, digit, SDK_O_PLAYER);
    (void)ok;
    /* mistake accounting (docs/07 section 17) */
    if (digit != a->solution.cells[c].value) {
        a->mistakes++;
        sdk_app_toast(a, 2, "WRONG MOVE");
    }
    _check_win(a);
}

void sdk_app_clear_cell(sdk_app *a) {
    int c = a->selected;
    if (c < 0 || c >= SDK_BOARD_CELLS) return;
    if (a->given_mask[c]) return;
    if (a->current.cells[c].value == 0 && a->current.cells[c].notes == 0) return;
    _push_undo(a);
    sdk_board_set(&a->current, c, 0, SDK_O_EMPTY);
}

static void _move_selection(sdk_app *a, int dx, int dy) {
    int c = a->selected < 0 ? 40 : a->selected;
    int r = c / 9, col = c % 9;
    r = (r + dy + 9) % 9; col = (col + dx + 9) % 9;
    a->selected = r * 9 + col;
}

static void _hint(sdk_app *a) {
    sdk_hint h;
    sdk_hint_preview(&a->current, &h);
    if (!h.available) { sdk_app_toast(a, 2, "NO HINT"); return; }
    int c = h.step.target_cell;
    if (c < 0) return;
    if (a->given_mask[c]) return;
    _push_undo(a);
    sdk_board b;
    if (sdk_hint_apply(&a->current, &h.step, 1, &b)) {
        a->current = b;
        sdk_app_toast(a, 1, "HINT APPLIED");
        _check_win(a);
    }
}

static void _auto_solve(sdk_app *a) {
    sdk_board b;
    sdk_auto_result r = sdk_auto_solve(&a->current, &b);
    if (r == SDK_AUTO_OK) {
        _push_undo(a);
        a->current = b;
        sdk_app_toast(a, 1, "AUTO-SOLVED");
        /* auto-solve completes the board */
        a->last_won = 1;
        a->last_mistakes = a->mistakes;
        a->last_elapsed_ms = (int64_t)(a->play_elapsed_ms + (_now() - a->play_start_ms));
        a->last_diff = a->diff;
        if (a->vault_ok && a->store) {
            sdk_completed_record rec;
            _fill_completed(a, &rec, 1);
            sdk_store_add_completed(a->store, &rec);
            sdk_vault_save(a->vault, a->store);
        }
        a->state = SDK_APP_COMPLETED;
    } else if (r == SDK_AUTO_CONFLICT) {
        sdk_app_toast(a, 3, "CONFLICT - UNDO");
    } else {
        sdk_app_toast(a, 3, "UNSOLVABLE");
    }
}

/* ------------------------------------------------------------------ */
/* input dispatch                                                     */
/* ------------------------------------------------------------------ */
void sdk_app_on_key(sdk_app *a, unsigned int vk, int ch) {
    if (!a) return;
    if (a->state == SDK_APP_PLAYING) {
        if (ch >= '1' && ch <= '9') { sdk_app_place_digit(a, ch - '0'); return; }
        if (vk == 0x08 || vk == 0x2E) { sdk_app_clear_cell(a); return; }       /* Backspace/Del */
        if (vk == 0x25 || vk == 0x26 || vk == 0x27 || vk == 0x28) {     /* arrows */
            if (vk == 0x25) _move_selection(a, -1, 0);
            else if (vk == 0x27) _move_selection(a, 1, 0);
            else if (vk == 0x26) _move_selection(a, 0, -1);
            else _move_selection(a, 0, 1);
            return;
        }
        if (ch == 'n' || ch == 'N') { a->notes_mode = a->notes_mode ? 0 : 1; return; }
        if (ch == 'u' || ch == 'U') { _undo(a); return; }
        if (ch == 'r' || ch == 'R') { _redo(a); return; }
        if (ch == 'h' || ch == 'H') { _hint(a); return; }
        if (ch == 'a' || ch == 'A') { _auto_solve(a); return; }
        if (vk == 0x1B) { sdk_app_toast(a, 0, "PAUSED"); a->paused = a->paused ? 0 : 1;
                           if (!a->paused) a->play_start_ms = _now(); return; }
    } else if (a->state == SDK_APP_MENU) {
        if (!a->vault_unlocked && a->vault_path[0]) {
            /* edit password field */
            if (vk == 0x08 || vk == 0x2E) { if (a->pw_len > 0) a->pw_len--; a->pw_entry[a->pw_len] = 0; return; }
            if (ch >= 32 && ch < 127 && a->pw_len < (int)(sizeof a->pw_entry - 1)) {
                a->pw_entry[a->pw_len++] = (char)ch; a->pw_entry[a->pw_len] = 0; return;
            }
            if (vk == 0x0D && a->pw_len > 0) {
                a->pw_entry[a->pw_len] = 0;
                if (sdk_app_unlock(a, a->pw_entry, 0)) { a->pw_len = 0; a->pw_entry[0] = 0; }
                else sdk_app_toast(a, 3, "VAULT ERROR");
            }
            return;
        }
        if (vk == 0x26) { a->menu_diff_idx = (a->menu_diff_idx + 2) % 3; a->tab_slide = a->menu_diff_idx; }
        else if (vk == 0x28) { a->menu_diff_idx = (a->menu_diff_idx + 1) % 3; a->tab_slide = a->menu_diff_idx; }
        else if (vk == 0x0D) { sdk_app_new_game(a, (sdk_difficulty)a->menu_diff_idx); }
    } else if (a->state == SDK_APP_COMPLETED) {
        if (vk == 0x0D) sdk_app_new_game(a, a->last_diff);
        else if (vk == 0x1B) sdk_app_to_menu(a);
    }
}

void sdk_app_on_mouse_down(sdk_app *a, int x, int y) {
    if (!a) return;
    a->mouse_x = x; a->mouse_y = y; a->mouse_down = 1;
    if (a->state == SDK_APP_MENU) sdk_page_menu_input(a, 1, 0, x, y);
    else if (a->state == SDK_APP_PLAYING) sdk_page_play_input(a, 1, 0, x, y);
    else if (a->state == SDK_APP_COMPLETED) sdk_page_completed_input(a, 1, 0, x, y);
}
void sdk_app_on_mouse_up(sdk_app *a, int x, int y) {
    if (!a) return;
    a->mouse_x = x; a->mouse_y = y; a->mouse_down = 0;
    if (a->state == SDK_APP_MENU) sdk_page_menu_input(a, 0, 1, x, y);
    else if (a->state == SDK_APP_PLAYING) sdk_page_play_input(a, 0, 1, x, y);
    else if (a->state == SDK_APP_COMPLETED) sdk_page_completed_input(a, 0, 1, x, y);
}
void sdk_app_on_mouse_move(sdk_app *a, int x, int y) {
    if (!a) return;
    a->mouse_x = x; a->mouse_y = y;
}

void sdk_app_on_timer(sdk_app *a, uint64_t now_ms) {
    if (!a) return;
    if (a->toast_until && now_ms > a->toast_until) { a->toast[0] = 0; a->toast_until = 0; }
    if (a->state == SDK_APP_PLAYING && !a->paused) {
        /* elapsed accrues in render-time; nothing to do here */
    }
    if (a->animating) {
        a->anim += 0.08;
        if (a->anim >= 1.0) { a->anim = 1.0; a->animating = 0; }
    }
    /* tab slide easing toward target */
    double target = (double)a->menu_diff_idx;
    if (a->tab_slide < target) a->tab_slide += 0.12;
    else if (a->tab_slide > target) a->tab_slide -= 0.12;
    if (a->tab_slide < target - 0.001 || a->tab_slide > target + 0.001) {}
    else a->tab_slide = target;
}

/* ------------------------------------------------------------------ */
/* render dispatch                                                     */
/* ------------------------------------------------------------------ */
void sdk_app_render(sdk_app *a, sdk_fb *fb) {
    if (!a || !fb) return;
    sdk_ui ui;
    ui.fb = fb;
    ui.theme = &a->theme;
    ui.mouse_x = a->mouse_x; ui.mouse_y = a->mouse_y;
    ui.mouse_down = a->mouse_down;
    ui.keyboard_focus = a->selected;
    ui.now = _now();
    ui.reduced_motion = a->reduced_motion;

    sdk_clear(fb, a->theme.surface);
    if (a->state == SDK_APP_MENU) sdk_page_menu_render(a, fb, &ui);
    else if (a->state == SDK_APP_PLAYING) sdk_page_play_render(a, fb, &ui);
    else if (a->state == SDK_APP_COMPLETED) sdk_page_completed_render(a, fb, &ui);
    else { /* BOOT/QUIT: simple splash */ }
}
