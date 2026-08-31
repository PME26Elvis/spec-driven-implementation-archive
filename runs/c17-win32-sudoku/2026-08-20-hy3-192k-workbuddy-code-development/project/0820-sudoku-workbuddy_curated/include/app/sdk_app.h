/* sdk_app.h - application state machine + 3 pages (docs/05, docs/06, docs/15).
 *
 * The application is intentionally UI-platform-independent: rendering and
 * input are expressed against a framebuffer (src/ui) and plain coordinates,
 * so the same code runs under the Win32 platform layer (src/platform) AND a
 * headless render harness used for automated visual evidence (G9).
 */
#ifndef SDK_APP_H
#define SDK_APP_H

#include <stdint.h>
#include <stddef.h>

#include "ui/sdk_ui.h"
#include "sudoku/sdk_sudoku.h"
#include "storage/sdk_vault.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* state machine (docs/15)                                             */
/* ------------------------------------------------------------------ */
typedef enum sdk_app_state {
    SDK_APP_BOOT = 0,
    SDK_APP_MENU,
    SDK_APP_PLAYING,
    SDK_APP_COMPLETED,
    SDK_APP_QUIT
} sdk_app_state;

#define SDK_UNDO_CAP 256

typedef struct sdk_app {
    sdk_app_state state;
    sdk_theme_kind theme_kind;
    sdk_theme theme;
    int width, height;
    uint64_t boot_ms;
    int reduced_motion;
    sdk_ripple_mgr *ripples;

    /* current game */
    sdk_difficulty diff;
    sdk_board puzzle;          /* immutable clues */
    sdk_board solution;        /* full solution */
    sdk_board current;         /* working board */
    uint8_t given_mask[SDK_BOARD_CELLS];
    int selected;              /* -1 = none */
    int notes_mode;            /* 0 value, 1 pencil notes */
    int mistakes;
    uint64_t play_start_ms;
    uint64_t play_elapsed_ms;  /* accumulated when paused */
    int paused;

    /* undo / redo (runtime snapshots) */
    sdk_board undo_stack[SDK_UNDO_CAP];
    int       undo_n;
    sdk_board redo_stack[SDK_UNDO_CAP];
    int       redo_n;

    /* menu */
    int menu_diff_idx;         /* 0 easy,1 medium,2 hard */
    double tab_slide;          /* animated difficulty indicator */

    /* completed screen */
    int last_won;
    int last_mistakes;
    int64_t last_elapsed_ms;
    sdk_difficulty last_diff;

    /* transient feedback */
    char toast[96];
    int  toast_kind;           /* 0 info,1 success,2 warn,3 danger */
    uint64_t toast_until;

    /* input mirror */
    int mouse_x, mouse_y, mouse_down;

    /* animation */
    double anim;               /* 0..1 generic transition */
    int animating;

    /* vault (optional; NULL when persistence unavailable) */
    sdk_store *store;
    sdk_vault *vault;
    char vault_path[512];
    int vault_ok;
    int vault_unlocked;          /* 0 until password entered (docs/08) */
    char vault_pw[128];
    char pw_entry[128];          /* on-screen password field buffer */
    int  pw_len;

    int quit_requested;
    void (*request_render)(void *user);
    void *user;
} sdk_app;

/* lifecycle */
sdk_app *sdk_app_create(int width, int height, const char *vault_path,
                         const char *vault_pw, int prod_iters);
void     sdk_app_destroy(sdk_app *a);
void     sdk_app_set_size(sdk_app *a, int w, int h);

/* input (coordinates in framebuffer pixels; vk = Windows virtual key) */
void sdk_app_on_key(sdk_app *a, unsigned int vk, int ch);
void sdk_app_on_mouse_down(sdk_app *a, int x, int y);
void sdk_app_on_mouse_up(sdk_app *a, int x, int y);
void sdk_app_on_mouse_move(sdk_app *a, int x, int y);
void sdk_app_on_timer(sdk_app *a, uint64_t now_ms);

/* render the active page into fb (caller-sized). */
void sdk_app_render(sdk_app *a, sdk_fb *fb);

/* state transitions */
void sdk_app_new_game(sdk_app *a, sdk_difficulty d);
void sdk_app_to_menu(sdk_app *a);
void sdk_app_quit(sdk_app *a);
int  sdk_app_wants_quit(const sdk_app *a);
/* opens/creates the encrypted vault with the given password (docs/08) */
int  sdk_app_unlock(sdk_app *a, const char *pw, int prod_iters);

/* helper: show a toast */
void sdk_app_toast(sdk_app *a, int kind, const char *msg);
/* place a digit at the current selection / clear it (shared by key + UI) */
void sdk_app_place_digit(sdk_app *a, int digit);
void sdk_app_clear_cell(sdk_app *a);

/* ---- page renderers / input (declared for the platform tests) ---- */
void sdk_page_menu_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui);
void sdk_page_menu_input(sdk_app *a, int down, int up, int x, int y);
void sdk_page_play_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui);
void sdk_page_play_input(sdk_app *a, int down, int up, int x, int y);
void sdk_page_completed_render(sdk_app *a, sdk_fb *fb, sdk_ui *ui);
void sdk_page_completed_input(sdk_app *a, int down, int up, int x, int y);

#ifdef __cplusplus
}
#endif
#endif /* SDK_APP_H */
