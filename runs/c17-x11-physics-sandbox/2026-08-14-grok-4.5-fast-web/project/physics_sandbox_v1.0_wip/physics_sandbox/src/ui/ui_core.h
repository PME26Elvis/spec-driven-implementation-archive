#ifndef PS_UI_CORE_H
#define PS_UI_CORE_H

#include "../render/framebuffer.h"
#include <stdbool.h>
#include <stdint.h>

#define PS_UI_MAX_WIDGETS 128
#define PS_UI_MAX_PANELS 16

typedef enum {
    PS_W_BUTTON = 0,
    PS_W_SLIDER,
    PS_W_CHECKBOX,
    PS_W_LABEL,
    PS_W_PANEL,
    PS_W_NAV_ITEM
} ps_widget_type;

typedef struct ps_widget {
    ps_widget_type type;
    int id;
    int x, y, w, h;
    bool visible;
    bool enabled;
    bool hovered;
    bool pressed;
    bool active; /* for checkbox / nav */
    char label[64];
    float value;     /* slider 0..1 or numeric */
    float min_v, max_v;
    int parent_panel;
    void *user_data;
} ps_widget;

typedef struct ps_panel {
    int id;
    int x, y, w, h;
    bool visible;
    bool frosted; /* apply blur behind */
    char title[48];
    uint32_t bg_color;
} ps_panel;

typedef struct ps_ui_state {
    ps_widget widgets[PS_UI_MAX_WIDGETS];
    int widget_count;
    ps_panel panels[PS_UI_MAX_PANELS];
    int panel_count;
    int active_nav; /* 0=Sandbox 1=Scenes 2=Diagnostics 3=About */
    float capsule_x; /* animated */
    float capsule_target_x;
    int hot_widget;
    int modal_visible;
    char modal_title[64];
    char modal_message[128];
    int modal_result; /* 0=none 1=ok 2=cancel */
    int selected_body_id; /* for inspector */
} ps_ui_state;

void ps_ui_init(ps_ui_state *ui);
int  ps_ui_add_panel(ps_ui_state *ui, int x, int y, int w, int h, const char *title, bool frosted);
int  ps_ui_add_button(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label);
int  ps_ui_add_slider(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label, float minv, float maxv, float val);
int  ps_ui_add_checkbox(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label, bool checked);
int  ps_ui_add_label(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *text);
void ps_ui_set_nav(ps_ui_state *ui, int index);
void ps_ui_show_modal(ps_ui_state *ui, const char *title, const char *msg);
void ps_ui_handle_mouse(ps_ui_state *ui, int mx, int my, int down, int up);
void ps_ui_update(ps_ui_state *ui, float dt);
void ps_ui_draw(ps_ui_state *ui, ps_framebuffer *fb);

/* blur helper */
void ps_fb_box_blur(ps_framebuffer *fb, int x, int y, int w, int h, int radius);

#endif
