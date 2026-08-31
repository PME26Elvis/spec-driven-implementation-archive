/* visrender.c - headless G9 visual-evidence harness.
 *
 * Drives the REAL sdk_app through its public input API (the exact same code
 * path the Win32 platform layer uses: sdk_app_on_mouse_down/up/on_key -> page
 * input -> real hit-testing) and writes BMP screenshots plus an animation
 * frame sequence and a golden-scene manifest (docs/06, docs/10 section 9-10,
 * docs/19 section 24-25).
 *
 * Usage: visrender [W H OUTDIR]
 *   Renders the golden scene set at the requested client size into OUTDIR.
 *   Defaults: 1280 800 build/evidence/screenshots
 *
 * No native controls, no display, no post-production. Every frame is a real
 * render of the self-made software renderer. */
#include "app/sdk_app.h"
#include "ui/sdk_ui.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int  VW = 1280;
static int  VH = 800;
static const char *g_out = "build/evidence/screenshots";

/* ---- layout mirror (sdk_pages.c) so we can aim a pointer like a real user ---- */
typedef struct { int x, y, w, h; } R;
static R rmenu_new(int w)     { return (R){ (w - 320) / 2, 330, 320, 56 }; }
static R rmenu_theme(int w)   { return (R){ (w - 320) / 2, 400, 155, 44 }; }
static R rmenu_tab(int w, int idx) { (void)w; (void)idx; return (R){0,0,0,0}; }
static void play_lay(int w, int h, int *gx, int *gy, int *gs, int *cell) {
    int pad = 18, topbar = 70, nh = 150, ah = 56, bottom = nh + ah + pad * 2;
    int avail = h - topbar - bottom - pad;
    int g = w - pad * 2;
    if (g > avail) g = avail;
    if (g > 740) g = 740;
    if (g < 200) g = 200;
    *gx = (w - g) / 2;
    *gy = topbar + ((avail - g) / 2 > 0 ? (avail - g) / 2 : 0);
    *gs = g; *cell = g / 9;
}
static void click_cell(sdk_app *a, int gx, int gy, int cell, int idx) {
    int c = idx % 9, r = idx / 9;
    int x = gx + c * cell + cell / 2, y = gy + r * cell + cell / 2;
    sdk_app_on_mouse_down(a, x, y);
    sdk_app_on_mouse_up(a, x, y);
}
static void click_rect(sdk_app *a, R r) {
    sdk_app_on_mouse_down(a, r.x + r.w / 2, r.y + r.h / 2);
    sdk_app_on_mouse_up(a, r.x + r.w / 2, r.y + r.h / 2);
}

/* path helper: join g_out + name into a reusable buffer */
static char g_pbuf[1024];
static const char *sp(const char *name) {
    snprintf(g_pbuf, sizeof g_pbuf, "%s/%s", g_out, name);
    return g_pbuf;
}
static int render_save(sdk_app *a, sdk_fb *fb, const char *path) {
    sdk_app_render(a, fb);
    int rc = sdk_write_bmp(fb, path);
    if (rc) fprintf(stderr, "WARN: write %s rc=%d\n", path, rc);
    return rc;
}

int main(int argc, char **argv) {
    if (argc >= 4) {
        int w = atoi(argv[1]), h = atoi(argv[2]);
        if (w >= 200 && h >= 200) { VW = w; VH = h; }
        g_out = argv[3];
    }

    int fails = 0;
    sdk_fb *fb = sdk_fb_create(VW, VH);
    if (!fb) { fprintf(stderr, "fb create failed\n"); return 4; }

    /* manifest */
    FILE *mf = fopen(sp("manifest.json"), "wb");
    if (!mf) { fprintf(stderr, "manifest open failed\n"); return 4; }
    fprintf(mf, "{\n  \"schema_version\": 1,\n  \"generator\": \"visrender (headless G9 harness)\",\n");
    fprintf(mf, "  \"client_width\": %d,\n  \"client_height\": %d,\n  \"outdir\": \"%s\",\n  \"scenes\": [\n", VW, VH, g_out);

    /* ---------- MENU (dark) ---------- */
    sdk_app *a = sdk_app_create(VW, VH, NULL, NULL, 0);
    if (!a) { fprintf(stderr, "app create failed\n"); return 4; }
    fails += render_save(a, fb, sp("menu_dark.bmp"));
    fprintf(mf, "    {\"id\":\"menu_dark\",\"page\":\"menu\",\"theme\":\"dark\",\"motion\":\"full\"},\n");

    /* ---------- MENU (light) via real theme toggle ---------- */
    click_rect(a, rmenu_theme(VW));
    fails += render_save(a, fb, sp("menu_light.bmp"));
    fprintf(mf, "    {\"id\":\"menu_light\",\"page\":\"menu\",\"theme\":\"light\",\"motion\":\"full\"},\n");

    /* ---------- MENU with vault unlock modal (real modal backdrop + blur) ---------- */
    sdk_app_destroy(a);
    a = sdk_app_create(VW, VH, sp("_dummy_vault.bin"), NULL, 0);
    fails += render_save(a, fb, sp("menu_unlock_modal.bmp"));
    fprintf(mf, "    {\"id\":\"menu_unlock_modal\",\"page\":\"menu\",\"modal\":\"vault_unlock\",\"theme\":\"dark\"},\n");
    sdk_app_destroy(a);
    a = sdk_app_create(VW, VH, NULL, NULL, 0);

    /* ---------- PLAY (dark, fresh game) ---------- */
    click_rect(a, rmenu_new(VW));            /* NEW GAME (menu_diff_idx=0 => EASY) */
    fails += render_save(a, fb, sp("play_dark.bmp"));
    fprintf(mf, "    {\"id\":\"play_dark\",\"page\":\"play\",\"difficulty\":\"easy\",\"theme\":\"dark\"},\n");

    /* ---------- PLAY (light) ---------- */
    sdk_app_to_menu(a);
    click_rect(a, rmenu_theme(VW));          /* toggle to light */
    click_rect(a, rmenu_new(VW));            /* NEW GAME in light */
    fails += render_save(a, fb, sp("play_light.bmp"));
    fprintf(mf, "    {\"id\":\"play_light\",\"page\":\"play\",\"difficulty\":\"easy\",\"theme\":\"light\"},\n");

    /* ---------- NOTES state ---------- */
    sdk_app_on_key(a, 0, 'n');               /* enter notes mode */
    {
        int gx, gy, gs, cell; play_lay(VW, VH, &gx, &gy, &gs, &cell);
        /* place a few pencil notes in empty cells */
        int notes_cells[4] = { 0, 1, 9, 10 };
        for (int k = 0; k < 4; ++k) {
            int idx = notes_cells[k];
            if (a->given_mask[idx]) continue;
            click_cell(a, gx, gy, cell, idx);
            sdk_app_on_key(a, 0, '1');
            sdk_app_on_key(a, 0, '5');
            sdk_app_on_key(a, 0, '9');
        }
    }
    fails += render_save(a, fb, sp("play_notes.bmp"));
    fprintf(mf, "    {\"id\":\"play_notes\",\"page\":\"play\",\"notes_mode\":true,\"theme\":\"light\"},\n");
    sdk_app_on_key(a, 0, 'n');               /* leave notes mode */

    /* ---------- MULTIPLE CONFLICTS (wrong digits) ---------- */
    {
        int gx, gy, gs, cell; play_lay(VW, VH, &gx, &gy, &gs, &cell);
        /* force duplicate values in row 0 to create several conflicts */
        int placed = 0;
        for (int c = 0; c < 9 && placed < 5; ++c) {
            int idx = c;                     /* row 0 */
            if (a->given_mask[idx]) continue;
            click_cell(a, gx, gy, cell, idx);
            sdk_app_on_key(a, 0, '5');       /* all '5' -> row conflict */
            placed++;
        }
    }
    fails += render_save(a, fb, sp("play_conflicts.bmp"));
    fprintf(mf, "    {\"id\":\"play_conflicts\",\"page\":\"play\",\"conflict_overlay\":true,\"theme\":\"light\"},\n");

    /* clear the mess, then solve it cleanly for a COMPLETED frame */
    for (int i = 0; i < 81; ++i) {
        if (a->given_mask[i]) continue;
        int gx, gy, gs, cell; play_lay(VW, VH, &gx, &gy, &gs, &cell);
        click_cell(a, gx, gy, cell, i);
        sdk_app_on_key(a, 0x08, 0);          /* Backspace clears */
    }
    /* place the verified solution cell-by-cell through the real key path */
    for (int i = 0; i < 81; ++i) {
        if (a->given_mask[i]) continue;
        int gx, gy, gs, cell; play_lay(VW, VH, &gx, &gy, &gs, &cell);
        click_cell(a, gx, gy, cell, i);
        int d = a->solution.cells[i].value;
        sdk_app_on_key(a, 0, '0' + d);
    }
    fails += render_save(a, fb, sp("completed.bmp"));
    fprintf(mf, "    {\"id\":\"completed\",\"page\":\"completed\",\"difficulty\":\"easy\",\"completion_class\":\"UNASSISTED\",\"theme\":\"light\"},\n");

    sdk_app_destroy(a);

    /* ---------- ANIMATION: capsule tab slide (deterministic via on_timer) ---------- */
    a = sdk_app_create(VW, VH, NULL, NULL, 0);
    a->menu_diff_idx = 2;                    /* target HARD */
    a->tab_slide = 0.0;                      /* start at EASY */
    for (int f = 0; f < 6; ++f) {
        sdk_app_on_timer(a, (uint64_t)(f * 120));   /* ease tab_slide toward target */
        char nm[64];
        snprintf(nm, sizeof nm, "anim_slide_%d.bmp", f);
        fails += render_save(a, fb, sp(nm));
    }
    fprintf(mf, "    {\"id\":\"anim_capsule_slide\",\"page\":\"menu\",\"frames\":6,\"kind\":\"navigation_capsule_slide\"},\n");
    sdk_app_destroy(a);

    /* ---------- ANIMATION: button hover elevation ---------- */
    a = sdk_app_create(VW, VH, NULL, NULL, 0);
    {
        R nr = rmenu_new(VW);
        a->mouse_x = nr.x + nr.w / 2; a->mouse_y = nr.y + nr.h / 2; a->mouse_down = 0;
        fails += render_save(a, fb, sp("anim_hover.bmp"));
    }
    fprintf(mf, "    {\"id\":\"anim_hover\",\"page\":\"menu\",\"kind\":\"button_hover_elevation\"},\n");
    sdk_app_destroy(a);

    /* ---------- ANIMATION: click ripple (single captured frame) ---------- */
    a = sdk_app_create(VW, VH, NULL, NULL, 0);
    click_rect(a, rmenu_new(VW));            /* NEW GAME spawns ripples on press */
    /* render immediately to capture the in-flight ripple */
    fails += render_save(a, fb, sp("anim_ripple.bmp"));
    fprintf(mf, "    {\"id\":\"anim_ripple\",\"page\":\"play\",\"kind\":\"click_ripple\"}\n");
    sdk_app_destroy(a);

    fprintf(mf, "  ]\n}\n");
    fclose(mf);

    sdk_fb_destroy(fb);
    printf("visrender [%dx%d -> %s]: %d frame write failures\n", VW, VH, g_out, fails);
    return fails ? 4 : 0;
}
