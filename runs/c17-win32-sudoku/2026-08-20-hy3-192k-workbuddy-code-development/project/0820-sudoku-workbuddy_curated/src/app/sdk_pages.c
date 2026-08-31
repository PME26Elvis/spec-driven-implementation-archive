/* sdk_pages.c - 3 page renderers + input (docs/05, docs/06, docs/15).
 * Layout is computed by shared helpers so rendering and hit-testing agree. */
#include "app/sdk_app.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* shared small helpers ------------------------------------------------- */
static void _draw_digit(sdk_fb *fb, int cx, int cy, int d, sdk_color c) {
    char s[2] = { (char)('0' + d), 0 };
    int w = sdk_text_width(s);
    sdk_draw_text(fb, cx - w / 2, cy - SDK_FONT_H / 2, s, c);
}

static char _fmt_time[32];
static const char *_fmt_ms(int64_t ms) {
    long long s = ms / 1000;
    int m = (int)(s / 60), sec = (int)(s % 60);
    _snprintf(_fmt_time, sizeof _fmt_time, "%02d:%02d", m, sec);
    return _fmt_time;
}

/* ------------------------------------------------------------------ */
/* MENU                                                                */
/* ------------------------------------------------------------------ */
enum { M_BTN_NEW = 1, M_BTN_THEME = 2, M_BTN_QUIT = 3, M_TAB0 = 10, M_TAB1 = 11, M_TAB2 = 12,
       M_UNLOCK = 20, M_SKIP = 21 };

static sdk_rect _menu_pw_rect(int w) { return (sdk_rect){ (w - 360) / 2, 360, 360, 44 }; }
static sdk_rect _menu_unlock_rect(int w) { return (sdk_rect){ (w - 360) / 2, 420, 175, 48 }; }
static sdk_rect _menu_skip_rect(int w) { return (sdk_rect){ (w - 360) / 2 + 185, 420, 175, 48 }; }

static sdk_rect _menu_tab_rect(int w, int idx) {
    int tw = 360, th = 44, x = (w - tw) / 2, y = 250;
    int cw = tw / 3;
    return (sdk_rect){ x + idx * cw, y, cw, th };
}
static sdk_rect _menu_new_rect(int w) { return (sdk_rect){ (w - 320) / 2, 330, 320, 56 }; }
static sdk_rect _menu_theme_rect(int w) { return (sdk_rect){ (w - 320) / 2, 400, 155, 44 }; }
static sdk_rect _menu_quit_rect(int w) { return (sdk_rect){ (w + 10) / 2, 400, 155, 44 }; }

void sdk_page_menu_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui) {
    int w = fb->width, h = fb->height;
    sdk_rect top = { 0, 0, w, 96 };
    sdk_gradient_v(fb, top, a->theme.accent, a->theme.surface);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 24, w, 40 }, "SUDOKU", a->theme.text_on_accent, 1);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 64, w, 20 },
                       "A SELF-MADE RENDERER / ENCRYPTED VAULT", a->theme.text_secondary, 1);

    /* on-screen vault unlock (docs/08) */
    if (!a->vault_unlocked && a->vault_path[0]) {
        sdk_rect full = { 0, 0, w, h };
        sdk_color dim = a->theme.backdrop; dim.a = 150;
        sdk_fill_rect(fb, full, dim);
        sdk_rect panel = { (w - 420) / 2, 280, 420, 240 };
        sdk_draw_panel(ui, panel, 1);
        sdk_draw_text_rect(fb, (sdk_rect){ panel.x, panel.y + 18, panel.w, 28 },
                           "UNLOCK VAULT", a->theme.text_primary, 1);
        sdk_rect pr = _menu_pw_rect(w);
        sdk_draw_input(ui, pr, a->pw_entry, 1, 1, "PASSWORD");
        sdk_rect ur = _menu_unlock_rect(w);
        sdk_draw_button(ui, M_UNLOCK, ur, "UNLOCK", 1, 0, 0, 0, 1, a->ripples);
        sdk_rect sr = _menu_skip_rect(w);
        sdk_draw_button(ui, M_SKIP, sr, "SKIP", 1, 0, 0, 0, 0, a->ripples);
        if (a->toast[0]) {
            sdk_rect tr2 = { (w - 320) / 2, h - 70, 320, 44 };
            sdk_draw_toast(ui, tr2, a->toast, a->toast_kind);
        }
        return;
    }

    /* difficulty capsule tabs */
    sdk_rect tabs = { (w - 360) / 2, 250, 360, 44 };
    const char *labels[3] = { "EASY", "MEDIUM", "HARD" };
    sdk_draw_capsule_tabs(ui, tabs, labels, 3, a->tab_slide, -1);

    /* new game (accent) */
    sdk_rect nr = _menu_new_rect(w);
    sdk_draw_button(ui, M_BTN_NEW, nr, "NEW GAME", 1, 0, 0, 0, 1, a->ripples);

    /* theme + quit */
    sdk_rect tr = _menu_theme_rect(w);
    sdk_draw_button(ui, M_BTN_THEME, tr, a->theme_kind ? "LIGHT" : "DARK", 1, 0, 0, 0, 0, a->ripples);
    sdk_rect qr = _menu_quit_rect(w);
    sdk_draw_button(ui, M_BTN_QUIT, qr, "QUIT", 1, 0, 0, 0, 2, a->ripples);

    /* stats from vault */
    int completed = a->store ? a->store->completed_count : 0;
    char buf[64];
    _snprintf(buf, sizeof buf, "COMPLETED GAMES: %d", completed);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 480, w, 24 }, buf, a->theme.text_secondary, 1);
    const char *dname[3] = { "EASY", "MEDIUM", "HARD" };
    _snprintf(buf, sizeof buf, "MODE: %s", dname[a->menu_diff_idx]);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 510, w, 24 }, buf, a->theme.text_secondary, 1);

    /* toast */
    if (a->toast[0]) {
        sdk_rect tr2 = { (w - 320) / 2, h - 70, 320, 44 };
        sdk_draw_toast(ui, tr2, a->toast, a->toast_kind);
    }
}

void sdk_page_menu_input(sdk_app *a, int down, int up, int x, int y) {
    if (down) sdk_ripple_spawn(a->ripples, 0, x, y, sdk_monotonic_ms());
    int w = a->width;
    /* unlock screen takes priority when active */
    if (!a->vault_unlocked && a->vault_path[0]) {
        if (down) {
            if (sdk_hit(_menu_pw_rect(w), x, y)) return;  /* focus field */
        }
        if (up) {
            if (sdk_hit(_menu_unlock_rect(w), x, y)) {
                if (a->pw_len > 0) {
                    a->pw_entry[a->pw_len] = 0;
                    if (sdk_app_unlock(a, a->pw_entry, 0)) { a->pw_len = 0; a->pw_entry[0] = 0; }
                    else sdk_app_toast(a, 3, "VAULT ERROR");
                }
                return;
            }
            if (sdk_hit(_menu_skip_rect(w), x, y)) {
                a->vault_unlocked = 1; a->vault_ok = 0; return;
            }
        }
        return;
    }
    if (!up) return;
    if (sdk_hit(_menu_new_rect(w), x, y)) { sdk_app_new_game(a, (sdk_difficulty)a->menu_diff_idx); return; }
    if (sdk_hit(_menu_theme_rect(w), x, y)) {
        a->theme_kind = a->theme_kind ? SDK_THEME_DARK : SDK_THEME_LIGHT;
        sdk_theme_load(a->theme_kind, &a->theme);
        return;
    }
    if (sdk_hit(_menu_quit_rect(w), x, y)) { sdk_app_quit(a); return; }
    for (int i = 0; i < 3; ++i)
        if (sdk_hit(_menu_tab_rect(w, i), x, y)) { a->menu_diff_idx = i; a->tab_slide = (double)i; return; }
}

/* ------------------------------------------------------------------ */
/* PLAY                                                                */
/* ------------------------------------------------------------------ */
enum { P_NUM0 = 100, P_UNDO = 200, P_REDO, P_NOTES, P_ERASE, P_HINT, P_AUTO, P_MENU };

typedef struct {
    sdk_rect grid; int cell;
    sdk_rect nums[9];
    sdk_rect undo, redo, notes, erase, hint, auto_, menu;
} play_layout;

static play_layout _play_layout(int w, int h) {
    play_layout L;
    int pad = 18, topbar = 70;
    int numpad_h = 150, actions_h = 56;
    int bottom = numpad_h + actions_h + pad * 2;
    int avail = h - topbar - bottom - pad;
    int gs = w - pad * 2;
    if (gs > avail) gs = avail;
    if (gs > 740) gs = 740;
    if (gs < 200) gs = 200;
    int gx = (w - gs) / 2;
    int gy = topbar + ((avail - gs) / 2 > 0 ? (avail - gs) / 2 : 0);
    L.grid = (sdk_rect){ gx, gy, gs, gs };
    L.cell = gs / 9;

    int ny = gy + gs + pad;
    int nw = (w - pad * 2 - 8 * 8) / 9;
    for (int i = 0; i < 9; ++i)
        L.nums[i] = (sdk_rect){ pad + i * (nw + 8), ny, nw, numpad_h - 90 };
    int ay = ny + (numpad_h - 90) + 12;
    int aw = (w - pad * 2 - 6 * 10) / 7;
    L.undo  = (sdk_rect){ pad,                 ay, aw, actions_h };
    L.redo  = (sdk_rect){ pad + 1*(aw+10),     ay, aw, actions_h };
    L.notes = (sdk_rect){ pad + 2*(aw+10),     ay, aw, actions_h };
    L.erase = (sdk_rect){ pad + 3*(aw+10),     ay, aw, actions_h };
    L.hint  = (sdk_rect){ pad + 4*(aw+10),     ay, aw, actions_h };
    L.auto_ = (sdk_rect){ pad + 5*(aw+10),     ay, aw, actions_h };
    L.menu  = (sdk_rect){ pad + 6*(aw+10),     ay, aw, actions_h };
    return L;
}

static int _cell_from_point(const play_layout *L, int x, int y) {
    if (!sdk_rect_contains(L->grid, x, y)) return -1;
    int col = (x - L->grid.x) / L->cell;
    int row = (y - L->grid.y) / L->cell;
    if (col < 0 || col > 8 || row < 0 || row > 8) return -1;
    return row * 9 + col;
}

static void _draw_board(sdk_app *a, sdk_fb *fb, sdk_ui *ui, const play_layout *L) {
    sdk_rect g = L->grid;
    /* panel behind */
    sdk_rect panel = { g.x - 10, g.y - 10, g.w + 20, g.h + 20 };
    sdk_draw_panel(ui, panel, 1);

    int sel = a->selected;
    int selval = (sel >= 0) ? a->current.cells[sel].value : 0;

    for (int i = 0; i < 81; ++i) {
        int r = i / 9, c = i % 9;
        sdk_rect cr = { g.x + c * L->cell, g.y + r * L->cell, L->cell, L->cell };
        int given = a->given_mask[i];
        int v = a->current.cells[i].value;
        sdk_color bg = given ? a->theme.surface_raised : a->theme.surface;
        if (i == sel) bg = a->theme.sel_bg;
        else if (selval && v == selval && v != 0) bg = a->theme.same_bg;
        sdk_fill_rect(fb, cr, bg);
        if (v) {
            sdk_color fg = given ? a->theme.given_fg
                          : (a->current.cells[i].origin == SDK_O_HINT ? a->theme.accent
                             : a->theme.player_fg);
            _draw_digit(fb, cr.x + L->cell / 2, cr.y + L->cell / 2, v, fg);
        } else if (a->current.cells[i].notes) {
            /* pencil notes 3x3 */
            for (int d = 1; d <= 9; ++d) {
                if (!(a->current.cells[i].notes & SDK_CAND_BIT(d))) continue;
                int nr = (d - 1) / 3, nc = (d - 1) % 3;
                int nx = cr.x + nc * (L->cell / 3) + L->cell / 6;
                int ny = cr.y + nr * (L->cell / 3) + L->cell / 6;
                char s[2] = { (char)('0' + d), 0 };
                sdk_draw_text(fb, nx - 2, ny - 3, s, a->theme.note_fg);
            }
        }
    }

    /* conflict overlay (docs/07 section 3) */
    sdk_validation vd;
    sdk_validate(&a->current, &vd);
    for (int i = 0; i < 81; ++i) {
        if (vd.cell_conflict[i]) {
            int r = i / 9, c = i % 9;
            sdk_rect cr = { g.x + c * L->cell, g.y + r * L->cell, L->cell, L->cell };
            sdk_color cf = a->theme.conflict_fg; cf.a = 70;
            sdk_fill_rect(fb, cr, cf);
        }
    }

    /* grid lines */
    for (int k = 0; k <= 9; ++k) {
        int thick = (k % 3 == 0) ? 2 : 1;
        int off = k * L->cell;
        sdk_color lc = a->theme.text_secondary; lc.a = (k % 3 == 0) ? 200 : 90;
        /* vertical */
        sdk_rect vl = { g.x + off, g.y, thick, g.h };
        sdk_fill_rect(fb, vl, lc);
        /* horizontal */
        sdk_rect hl = { g.x, g.y + off, g.w, thick };
        sdk_fill_rect(fb, hl, lc);
    }
}

void sdk_page_play_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui) {
    int w = fb->width, h = fb->height;
    play_layout L = _play_layout(w, h);

    /* top bar */
    char buf[64];
    _snprintf(buf, sizeof buf, "DIFFICULTY: %s",
              a->diff == SDK_DIFF_EASY ? "EASY" : a->diff == SDK_DIFF_MEDIUM ? "MEDIUM" : "HARD");
    sdk_draw_text(fb, 18, 24, buf, a->theme.text_primary);
    int64_t el = a->play_elapsed_ms + (a->paused ? 0 : (sdk_monotonic_ms() - a->play_start_ms));
    _snprintf(buf, sizeof buf, "TIME %s", _fmt_ms(el));
    sdk_draw_text_rect(fb, (sdk_rect){ w / 2 - 80, 24, 160, 20 }, buf, a->theme.text_primary, 1);
    _snprintf(buf, sizeof buf, "MISTAKES %d", a->mistakes);
    sdk_draw_text_rect(fb, (sdk_rect){ w - 200, 24, 180, 20 }, buf, a->theme.warning, 2);

    _draw_board(a, fb, ui, &L);

    /* number pad */
    for (int d = 1; d <= 9; ++d)
        sdk_draw_button(ui, P_NUM0 + d, L.nums[d - 1], (char[]){ (char)('0' + d), 0 }, 1, 0, 0, 0, 0, a->ripples);
    /* action row */
    sdk_draw_button(ui, P_UNDO,  L.undo,  "UNDO",  a->undo_n > 0, 0, 0, 0, 0, a->ripples);
    sdk_draw_button(ui, P_REDO,  L.redo,  "REDO",  a->redo_n > 0, 0, 0, 0, 0, a->ripples);
    sdk_draw_button(ui, P_NOTES, L.notes, a->notes_mode ? "NOTES*" : "NOTES", 1, 0, a->notes_mode, 0, a->notes_mode ? 1 : 0, a->ripples);
    sdk_draw_button(ui, P_ERASE, L.erase, "ERASE", 1, 0, 0, 0, 0, a->ripples);
    sdk_draw_button(ui, P_HINT,  L.hint,  "HINT",  1, 0, 0, 0, 1, a->ripples);
    sdk_draw_button(ui, P_AUTO,  L.auto_, "AUTO",  1, 0, 0, 0, 1, a->ripples);
    sdk_draw_button(ui, P_MENU,  L.menu,  "MENU",  1, 0, 0, 0, 2, a->ripples);

    if (a->toast[0]) {
        sdk_rect tr2 = { (w - 360) / 2, h - 36, 360, 30 };
        sdk_draw_toast(ui, tr2, a->toast, a->toast_kind);
    }
}

void sdk_page_play_input(sdk_app *a, int down, int up, int x, int y) {
    if (down) sdk_ripple_spawn(a->ripples, 0, x, y, sdk_monotonic_ms());
    play_layout L = _play_layout(a->width, a->height);
    if (down) {
        int c = _cell_from_point(&L, x, y);
        if (c >= 0) { a->selected = c; return; }
    }
    if (!up) return;
    /* buttons */
    for (int d = 1; d <= 9; ++d)
        if (sdk_hit(L.nums[d - 1], x, y)) { if (a->selected < 0) a->selected = 40; sdk_app_place_digit(a, d); return; }
    if (sdk_hit(L.undo, x, y))  { /* undo via key path */ sdk_app_on_key(a, 0, 'u'); return; }
    if (sdk_hit(L.redo, x, y))  { sdk_app_on_key(a, 0, 'r'); return; }
    if (sdk_hit(L.notes, x, y)) { a->notes_mode = a->notes_mode ? 0 : 1; return; }
    if (sdk_hit(L.erase, x, y)) { sdk_app_clear_cell(a); return; }
    if (sdk_hit(L.hint, x, y))  { sdk_app_on_key(a, 0, 'h'); return; }
    if (sdk_hit(L.auto_, x, y)) { sdk_app_on_key(a, 0, 'a'); return; }
    if (sdk_hit(L.menu, x, y))  { sdk_app_to_menu(a); return; }
}

/* ------------------------------------------------------------------ */
/* COMPLETED                                                           */
/* ------------------------------------------------------------------ */
enum { C_AGAIN = 1, C_MENU = 2 };

void sdk_page_completed_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui) {
    int w = fb->width;
    sdk_rect top = { 0, 0, w, 200 };
    sdk_gradient_v(fb, top, a->theme.success, a->theme.surface);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 70, w, 50 }, "SOLVED!", a->theme.text_on_accent, 1);

    const char *dname[3] = { "EASY", "MEDIUM", "HARD" };
    char buf[64];
    _snprintf(buf, sizeof buf, "DIFFICULTY: %s", dname[a->last_diff]);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 250, w, 28 }, buf, a->theme.text_primary, 1);
    _snprintf(buf, sizeof buf, "TIME: %s", _fmt_ms(a->last_elapsed_ms));
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 290, w, 28 }, buf, a->theme.text_primary, 1);
    _snprintf(buf, sizeof buf, "MISTAKES: %d", a->last_mistakes);
    sdk_draw_text_rect(fb, (sdk_rect){ 0, 330, w, 28 }, buf, a->theme.warning, 1);

    sdk_rect again = { (w - 320) / 2, 400, 320, 56 };
    sdk_draw_button(ui, C_AGAIN, again, "PLAY AGAIN", 1, 0, 0, 0, 1, a->ripples);
    sdk_rect menu = { (w - 320) / 2, 470, 320, 48 };
    sdk_draw_button(ui, C_MENU, menu, "MENU", 1, 0, 0, 0, 0, a->ripples);
}

void sdk_page_completed_input(sdk_app *a, int down, int up, int x, int y) {
    if (down) sdk_ripple_spawn(a->ripples, 0, x, y, sdk_monotonic_ms());
    if (!up) return;
    int w = a->width;
    sdk_rect again = { (w - 320) / 2, 400, 320, 56 };
    sdk_rect menu = { (w - 320) / 2, 470, 320, 48 };
    if (sdk_hit(again, x, y)) { sdk_app_new_game(a, a->last_diff); return; }
    if (sdk_hit(menu, x, y)) { sdk_app_to_menu(a); return; }
}
