/* app.h - editor application types and shared declarations. */
#ifndef APP_H
#define APP_H

#include <windows.h>
#include <stdbool.h>
#include "doc.h"
#include "md.h"
#include "history.h"
#include "stats.h"

/* ---------------- colors ---------------- */
typedef struct { uint8_t r, g, b, a; } Color;
static inline uint32_t color_bgra(Color c){
    return ((uint32_t)c.b) | ((uint32_t)c.g << 8) | ((uint32_t)c.r << 16) | ((uint32_t)c.a << 24);
}
static inline Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a){ Color c = {r,g,b,a}; return c; }

typedef struct {
    Color bg, bg2, surface, surface2, panel, border, text, text_dim, text_faint;
    Color accent, accent2, hover, active, sel, sel_text;
    Color code_bg, table_border, table_header, quote_rule;
    Color link, diff_add, diff_del, diff_mod, error, warn, ok;
    Color nav_bg, status_bg, caret, shadow;
} Theme;

extern Theme g_light, g_dark;

/* ---------------- editor modes ---------------- */
#define MODE_SOURCE 0
#define MODE_SPLIT 1
#define MODE_PREVIEW 2
#define MODE_RENDERED 3

/* ---------------- preferences ---------------- */
typedef struct {
    bool dark;
    int font_size;         /* 10..32 */
    int line_spacing;      /* 0 compact, 1 normal, 2 relaxed */
    int default_image_mode;/* 0 relative, 1 embedded */
    bool autosave_enabled;
    int autosave_interval; /* 10..300 s */
    int default_mode;
    bool sync_scroll;
    bool restore_session;
} Prefs;

/* ---------------- document tab ---------------- */
typedef struct {
    md_document doc;
    char *display_name;     /* basename or "Untitled N" */
    int mode;
    double zoom;
    int scroll_y;
    int preview_scroll_y;
    double split_ratio;
    size_t caret;
    size_t sel_start, sel_end;
    bool has_sel;
    /* history */
    md_history *history;
    char *history_path;      /* path to history file, or NULL */
    bool history_dirty;
    /* external-change tracking */
    bool file_exists;
    uint64_t file_mtime;     /* last known */
    char *file_hash;         /* last known content hash (hex) */
    bool external_conflict;  /* keep-current conflict gate */
    bool external_missing;
    /* cached render model */
    md_doc *parsed;
    bool parsed_dirty;
} DocTab;

/* ---------------- tree node ---------------- */
typedef struct TreeNode TreeNode;
struct TreeNode {
    char *name;         /* basename */
    char *relpath;      /* '/'-separated relative to workspace root */
    bool is_dir;
    bool expanded;
    int depth;
    TreeNode **children;
    size_t nchildren, cap;
    TreeNode *parent;
};

/* ---------------- commands ---------------- */
typedef struct {
    const char *id;
    const char *label;
    void (*fn)(void *app);
    bool (*enabled)(void *app);
} Command;

/* ---------------- application ---------------- */
typedef struct {
    HINSTANCE hinst;
    HWND hwnd;
    int width, height;      /* client size (device px) */
    int dpi;                /* current DPI (96 base) */
    double scale;           /* dpi/96 */

    /* framebuffer */
    void *fb;               /* 32-bit BGRA */
    int fb_w, fb_h;
    int fb_stride;
    HBITMAP dib;
    HDC memdc;
    HBITMAP old_bmp;
    HFONT font, font_bold, font_mono, font_ui;
    int font_px;

    Theme *theme;
    Prefs prefs;
    char *prefs_path;

    /* documents / tabs */
    DocTab **tabs;
    size_t ntabs, cap;
    int active;

    /* workspace */
    char *workspace_root;
    TreeNode *tree_root;
    bool sidebar_visible;
    int sidebar_width;
    int sidebar_tab;        /* 0 files, 1 outline */

    /* search */
    bool find_open;
    bool find_replace;
    char find_query[512];
    char find_repl[512];
    bool find_case, find_word;

    /* modal / overlays */
    int modal;              /* 0 none, 1 palette, 2 statistics, 3 history, 4 diff, 5 prefs, 6 about, 7 error, 8 unsaved, 9 recovery, 10 external conflict, 11 image insert, 12 shortcut ref */
    char modal_text[4096];
    /* history modal state */
    int hist_sel;
    int diff_mode;          /* 0 side-by-side, 1 inline */
    int diff_from;
    /* palette */
    char palette_query[256];
    int palette_sel;
    /* statistics */
    md_stats stats;
    /* unsaved-close flow */
    int unsaved_idx;
    /* image resize state */
    int img_resize_active;
    /* drag */
    bool dragging;
    int drag_sel_start, drag_sel_end;
    int drag_target;
    int drag_autoscroll;
    /* animations */
    double nav_scroll;      /* 0..1 frosted progress */
    double modal_anim;      /* 0..1 open progress */
    double capsule_anim;    /* 0..3 mode index */
    /* ripple */
    double ripple_x, ripple_y, ripple_t;
    bool ripple_active;
    /* caret blink */
    double caret_phase;
    /* IME */
    bool ime_composing;
    int ime_comp_len;
    WCHAR ime_comp[256];
    /* recent */
    char **recent_files; size_t nrecent_files;
    char **recent_workspaces; size_t nrecent_ws;
    /* global state */
    uint64_t untitled_counter;
    bool running;
    /* screenshot/automation mode */
    const char *shot_id;      /* if non-NULL, capture and exit */
    char *shot_out;
    int shot_frame;
    /* perf */
    double now;
} App;

/* ---------------- function decls ---------------- */
void render_init(App *a);
void render_resize(App *a, int w, int h);
void app_init(App *a);
void app_run(App *a);
void app_render(App *a);
void app_invalidate(App *a);
void app_open_file(App *a, const wchar_t *path);
void app_open_workspace(App *a, const wchar_t *path);
void app_new_document(App *a);
void app_save(App *a, bool save_as);
void app_close_tab(App *a, int idx);
void app_switch_tab(App *a, int idx);
DocTab *app_active(App *a);
void app_set_mode(App *a, int mode);
void app_reparse(App *a, DocTab *t);
void app_do_command(App *a, const char *id);
void app_insert_text(App *a, DocTab *t, size_t pos, const char *text, size_t len);
void app_delete_range(App *a, DocTab *t, size_t start, size_t end);
void app_find_next(App *a, int dir);
void app_replace_one(App *a);
void app_replace_all(App *a);
void app_toggle_inline(App *a, DocTab *t, const char *open_delim, const char *close_delim, int delim_len);
void app_commit_history(App *a, DocTab *t);
void app_save_history(App *a, DocTab *t);
void app_load_history(App *a, DocTab *t);
void app_autosave(App *a, DocTab *t);
void app_show_modal(App *a, int modal, const char *text);
void app_apply_fmt(App *a, int fmt);
void app_image_insert(App *a, const wchar_t *path);
void app_render_preview(App *a, DocTab *t, int x, int y, int w, int h);
int  app_hit_test_rendered(App *a, DocTab *t, int x, int y, size_t *pos);
void app_click_rendered(App *a, DocTab *t, int x, int y, bool ctrl);
void app_render_source(App *a, DocTab *t, int x, int y, int w, int h);
int  app_source_hit(App *a, DocTab *t, int x, int y, size_t *pos);
void app_key_char(App *a, WPARAM wparam);
void app_key(App *a, WPARAM vk, bool ctrl, bool shift, bool alt);
void app_mouse_down(App *a, int x, int y, int button);
void app_mouse_up(App *a, int x, int y, int button);
void app_mouse_move(App *a, int x, int y);
void app_wheel(App *a, int delta, bool ctrl, int x, int y);
void app_capture_shot(App *a);
void app_setup_screenshot(App *a, const char *id);

/* ui helpers */
void ui_draw_button(App *a, int x, int y, int w, int h, const char *label, bool hover, bool pressed, bool disabled, bool primary);
void ui_draw_capsule(App *a, int x, int y, int w, int h, double frac);
void ui_draw_text(App *a, int x, int y, const char *utf8, Color c, bool bold, int size);
int  ui_text_width(App *a, const char *utf8, bool bold, int size);
void ui_draw_modal_frame(App *a, int x, int y, int w, int h);
void ui_blur_region(App *a, int x, int y, int w, int h, int radius);
void ui_draw_shadow_rect(App *a, int x, int y, int w, int h, int r);
void ui_fill_round(App *a, int x, int y, int w, int h, int r, Color c);
void ui_draw_rect(App *a, int x, int y, int w, int h, Color c);

/* image helper */
void app_draw_image(App *a, int x, int y, int w, int h, const uint8_t *rgba, int iw, int ih);

#endif /* APP_H */
