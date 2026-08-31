/* sdk_ui_theme.c - theme tokens (docs/06 §23, §18, docs/20). */
#include "ui/sdk_ui.h"

void sdk_theme_load(sdk_theme_kind k, sdk_theme *out) {
    out->kind = k;
    if (k == SDK_THEME_DARK) {
        out->surface        = sdk_color_make(18,20,28,255);
        out->surface_raised = sdk_color_make(34,38,52,255);
        out->surface_sunken = sdk_color_make(12,14,20,255);
        out->text_primary   = sdk_color_make(235,238,245,255);
        out->text_secondary = sdk_color_make(150,156,172,255);
        out->text_on_accent = sdk_color_make(255,255,255,255);
        out->accent         = sdk_color_make(94,129,244,255);
        out->accent_hover   = sdk_color_make(118,150,255,255);
        out->danger         = sdk_color_make(232,84,84,255);
        out->success        = sdk_color_make(74,196,122,255);
        out->warning        = sdk_color_make(232,184,84,255);
        out->focus          = sdk_color_make(120,170,255,255);
        out->given_fg       = sdk_color_make(235,238,245,255);
        out->player_fg      = sdk_color_make(120,190,255,255);
        out->note_fg        = sdk_color_make(150,170,210,255);
        out->conflict_fg    = sdk_color_make(245,120,120,255);
        out->sel_bg         = sdk_color_make(70,110,200,90);
        out->peer_bg        = sdk_color_make(60,70,100,70);
        out->same_bg        = sdk_color_make(90,120,180,70);
        out->backdrop       = sdk_color_make(8,10,16,170);
        out->shadow         = sdk_color_make(0,0,0,120);
    } else {
        out->surface        = sdk_color_make(244,246,250,255);
        out->surface_raised = sdk_color_make(255,255,255,255);
        out->surface_sunken = sdk_color_make(226,230,238,255);
        out->text_primary   = sdk_color_make(24,28,38,255);
        out->text_secondary = sdk_color_make(96,104,120,255);
        out->text_on_accent = sdk_color_make(255,255,255,255);
        out->accent         = sdk_color_make(46,96,220,255);
        out->accent_hover   = sdk_color_make(34,80,200,255);
        out->danger         = sdk_color_make(200,40,40,255);
        out->success        = sdk_color_make(30,150,80,255);
        out->warning        = sdk_color_make(190,130,20,255);
        out->focus          = sdk_color_make(40,110,230,255);
        out->given_fg       = sdk_color_make(24,28,38,255);
        out->player_fg      = sdk_color_make(30,90,200,255);
        out->note_fg        = sdk_color_make(110,130,160,255);
        out->conflict_fg    = sdk_color_make(210,40,40,255);
        out->sel_bg         = sdk_color_make(120,160,255,110);
        out->peer_bg        = sdk_color_make(180,200,235,120);
        out->same_bg        = sdk_color_make(150,190,255,120);
        out->backdrop       = sdk_color_make(20,24,32,150);
        out->shadow         = sdk_color_make(0,0,0,70);
    }
}
