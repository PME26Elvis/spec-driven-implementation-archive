/* editor.c — Win32 pinball table editor + playable sandbox.
 * Custom software-rendered UI; no native child controls. Uses GDI only to
 * present the application-owned RGB framebuffer to the HWND.
 *
 * Link against: user32, gdi32, shell32, comdlg32 is NOT used.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include "scene_write.h"
#include "scene_validate.h"
#include "sim.h"
#include "replay.h"
#include "render.h"
#include "png.h"
#include "hash.h"
#include "platform.h"

#ifndef PB_MAX_OBJECTS
#define PB_MAX_OBJECTS 4096
#endif

/* DIB framebuffer presentation (24-bit RGB, top-down). */
#pragma pack(push,1)
typedef struct { BITMAPINFOHEADER h; DWORD masks[3]; } BmiRGB;
#pragma pack(pop)
static BmiRGB a_bmi;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define APP_TITLE L"Pinball Sandbox v1.0.0"
#define APP_VERSION "1.0.0"

/* ------------------------------------------------------------------ */
/* App mode and tool state */
typedef enum { MODE_EDIT, MODE_PLAY, MODE_PREVIEW } AppMode;
typedef enum {
    TOOL_SELECT, TOOL_MOVE, TOOL_WALL, TOOL_RAMP, TOOL_FLIPPER, TOOL_BUMPER,
    TOOL_SLINGSHOT, TOOL_GATE, TOOL_TARGET, TOOL_STANDUP, TOOL_ROLLOVER,
    TOOL_SPINNER, TOOL_KICKOUT, TOOL_SENSOR, TOOL_DRAIN, TOOL_BALL_SPAWN, TOOL_LAUNCHER
} Tool;

#define TOOL_COUNT 17

static const struct { Tool t; const char *name; unsigned char r,g,b; } TOOLS[TOOL_COUNT] = {
    {TOOL_SELECT,      "Select",    255,255,255},
    {TOOL_MOVE,        "Move",      200,200,200},
    {TOOL_WALL,        "Wall",      120,124,140},
    {TOOL_RAMP,        "Ramp",      90,150,170},
    {TOOL_FLIPPER,     "Flipper",   230,230,240},
    {TOOL_BUMPER,      "Bumper",    220,160,60},
    {TOOL_SLINGSHOT,   "Sling",     200,90,90},
    {TOOL_GATE,        "Gate",      150,120,200},
    {TOOL_TARGET,      "DropTgt",   90,200,120},
    {TOOL_STANDUP,     "StandTgt",  120,210,150},
    {TOOL_ROLLOVER,    "Rollover",  210,210,90},
    {TOOL_SPINNER,     "Spinner",   200,160,230},
    {TOOL_KICKOUT,     "Kickout",   120,140,255},
    {TOOL_SENSOR,      "Sensor",    80,200,220},
    {TOOL_DRAIN,       "Drain",     200,70,70},
    {TOOL_BALL_SPAWN,  "Spawn",     70,200,255},
    {TOOL_LAUNCHER,    "Launcher",  150,150,160},
};

/* ------------------------------------------------------------------ */
/* Command history via serialized snapshots */
#define MAX_HISTORY 256
typedef struct {
    char *snapshots[MAX_HISTORY];
    int cap, head, pos; /* head = count bounded by cap; pos = current index */
} History;

static void history_clear(History *h) {
    for (int i=0;i<h->head;i++) free(h->snapshots[i]);
    h->head=0; h->pos=-1;
}
static void history_push(History *h, const Scene *s) {
    char *buf = scene_write(s);
    if (!buf) return;
    if (h->pos+1 < h->head) {
        /* truncate redo branch */
        for (int i=h->pos+1;i<h->head;i++) free(h->snapshots[i]);
        h->head = h->pos+1;
    }
    int slot = h->head % MAX_HISTORY;
    if (h->head >= MAX_HISTORY) {
        free(h->snapshots[slot]);
        h->head = MAX_HISTORY; /* keep cap full */
    }
    h->snapshots[slot] = buf;
    h->head++;
    if (h->head > MAX_HISTORY) {
        /* shift semantics: pos tracks logical index relative to tail */
        /* For simplicity when full, keep head=MAX_HISTORY and advance tail conceptually */
    }
    h->pos = h->head-1;
}
static int history_undo(History *h, Scene *s) {
    if (h->pos <= 0) return 0; /* nothing to undo */
    h->pos--;
    char *buf = h->snapshots[h->pos % MAX_HISTORY];
    DiagList d; diag_list_init(&d);
    Scene tmp; scene_init(&tmp);
    PbtCode c = parse_scene(buf, strlen(buf), &tmp, &d);
    diag_list_free(&d);
    if (c != PBT_OK) return 0;
    scene_free(s); *s = tmp;
    return 1;
}
static int history_redo(History *h, Scene *s) {
    if (h->pos+1 >= h->head) return 0;
    h->pos++;
    char *buf = h->snapshots[h->pos % MAX_HISTORY];
    DiagList d; diag_list_init(&d);
    Scene tmp; scene_init(&tmp);
    PbtCode c = parse_scene(buf, strlen(buf), &tmp, &d);
    diag_list_free(&d);
    if (c != PBT_OK) return 0;
    scene_free(s); *s = tmp;
    return 1;
}
static int history_dirty_to_saved(const History *h) {
    /* If current pos equals last saved pos -> clean. We approximate: position 0 is baseline. */
    return h->pos != 0;
}

/* ------------------------------------------------------------------ */
/* UI theme */
#define PAL_W 92
#define INS_W 220
#define STAT_H 24

typedef struct {
    int x,y,w,h;
    const char *label;
    int active, hover, disabled;
    int id;
} Button;

typedef struct {
    int x,y,w,h;
    char text[128];
    int cursor; /* byte index */
    int focused;
    int numeric; /* 0=string, 1=double, 2=int */
    int invalid;
    double val;
} TextField;

/* ------------------------------------------------------------------ */
/* App state */
typedef struct {
    HWND hwnd;
    HDC hdcMem;
    HBITMAP hbmp, holdbmp;
    int W, H;
    unsigned char *client_rgb; /* W*H*3 */
    Framebuffer fb;

    AppMode mode;
    Tool tool;
    int running; /* play mode advancing */
    int single_step;
    double playback_speed;

    Scene scene;
    Sim sim;
    Sim preview_sim;
    int in_preview;

    History history;
    char file_path[MAX_PATH];
    int dirty;

    /* camera */
    double pan_x, pan_y, zoom;
    int canvas_x, canvas_y, canvas_w, canvas_h;

    /* input / selection */
    int sel[256]; int sel_count; int primary_sel; /* indices in scene.objects */
    int ldown, rdown, mdown;
    int drag_start_x, drag_start_y;
    int dragging; /* moving selection */
    int marquee; int marquee_x0, marquee_y0, marquee_x1, marquee_y1;
    double drag_offs_x[256], drag_offs_y[256]; /* world offset from click */

    /* validation panel */
    DiagList diagnostics;
    int show_validation;

    /* replay */
    Replay replay;
    int recording;
    int playing_replay;
    int replay_step_target;

    /* file picker */
    int show_filepicker;
    int picker_save; /* 1=save, 0=open */
    char picker_dir[MAX_PATH];
    char picker_input[MAX_PATH];

    /* simple periodic timer for play loop */
    UINT_PTR timer_id;
    int timer_ms;
} App;

static App g_app = {0};

/* ------------------------------------------------------------------ */
/* World <-> screen transform */
static double app_scale(App *a) {
    if (a->scene.world_size.x <= 0 || a->scene.world_size.y <= 0) return 1.0;
    double sx = (double)a->canvas_w / a->scene.world_size.x;
    double sy = (double)a->canvas_h / a->scene.world_size.y;
    double base = sx < sy ? sx : sy;
    return base * a->zoom;
}
static void world_to_screen(App *a, double x, double y, int *sx, int *sy) {
    double s = app_scale(a);
    *sx = a->canvas_x + (int)((x - a->pan_x) * s);
    *sy = a->canvas_y + (int)((y - a->pan_y) * s);
}
static void screen_to_world(App *a, int sx, int sy, double *x, double *y) {
    double s = app_scale(a);
    *x = a->pan_x + (sx - a->canvas_x) / s;
    *y = a->pan_y + (sy - a->canvas_y) / s;
}

/* ------------------------------------------------------------------ */
/* Geometry helpers for selection */
static double dist2(double x0,double y0,double x1,double y1){double dx=x1-x0,dy=y1-y0;return dx*dx+dy*dy;}
static double seg_dist2(Vec2 p, Vec2 a, Vec2 b){
    Vec2 ab={b.x-a.x,b.y-a.y}; Vec2 ap={p.x-a.x,p.y-a.y};
    double ab2=ab.x*ab.x+ab.y*ab.y;
    if(ab2<=0) return ap.x*ap.x+ap.y*ap.y;
    double t=(ap.x*ab.x+ap.y*ab.y)/ab2; if(t<0)t=0; if(t>1)t=1;
    double cx=a.x+ab.x*t, cy=a.y+ab.y*t;
    double dx=p.x-cx,dy=p.y-cy; return dx*dx+dy*dy;
}
static int obj_hit(const Obj *o, double x, double y, double tol) {
    switch (o->type) {
        case OBJ_WALL: case OBJ_RAMP: case OBJ_ONE_WAY_GATE: case OBJ_SLINGSHOT: case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER:
            return seg_dist2((Vec2){x,y}, o->u.cap.start, o->u.cap.end) <= (tol+o->u.cap.thickness*0.5)*(tol+o->u.cap.thickness*0.5);
        case OBJ_BUMPER:
            return dist2(x,y,o->u.bumper.center.x,o->u.bumper.center.y) <= (tol+o->u.bumper.radius)*(tol+o->u.bumper.radius);
        case OBJ_SPINNER:
            return seg_dist2((Vec2){x,y}, (Vec2){o->u.spinner.pivot.x-o->u.spinner.half_length,o->u.spinner.pivot.y},
                             (Vec2){o->u.spinner.pivot.x+o->u.spinner.half_length,o->u.spinner.pivot.y}) <= (tol+o->u.spinner.thickness*0.5)*(tol+o->u.spinner.thickness*0.5);
        case OBJ_FLIPPER:
            return seg_dist2((Vec2){x,y}, o->u.flipper.pivot,
                             (Vec2){o->u.flipper.pivot.x+o->u.flipper.length,o->u.flipper.pivot.y}) <= (tol+o->u.flipper.thickness*0.5)*(tol+o->u.flipper.thickness*0.5);
        case OBJ_KICKOUT:
            return dist2(x,y,o->u.kickout.center.x,o->u.kickout.center.y) <= (tol+o->u.kickout.capture_radius)*(tol+o->u.kickout.capture_radius);
        case OBJ_SENSOR: case OBJ_DRAIN:
            return x>=o->u.sensor.x && x<=o->u.sensor.x+o->u.sensor.w && y>=o->u.sensor.y && y<=o->u.sensor.y+o->u.sensor.h;
        case OBJ_BALL_SPAWN:
            return dist2(x,y,o->u.spawn.position.x,o->u.spawn.position.y) <= (tol+ (o->u.spawn.has_ball_radius?o->u.spawn.ball_radius:8))*(tol+(o->u.spawn.has_ball_radius?o->u.spawn.ball_radius:8));
        case OBJ_LAUNCHER: {
            Vec2 d=o->u.launcher.direction; double px=o->u.launcher.position.x, py=o->u.launcher.position.y;
            return seg_dist2((Vec2){x,y}, (Vec2){px,py}, (Vec2){px+d.x*40,py+d.y*40}) <= (tol+6)*(tol+6);
        }
        default: return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Unique IDs */
static int id_exists(const Scene *s, const char *id) {
    for (int i=0;i<s->obj_count;i++) if (strcmp(s->objects[i].id, id)==0) return 1;
    return 0;
}
static void make_id(const Scene *s, const char *prefix, char *out, size_t outsz) {
    for (int n=1;n<9999;n++) {
        snprintf(out, outsz, "%s_%03d", prefix, n);
        if (!id_exists(s, out)) return;
    }
    snprintf(out, outsz, "%s_%d", prefix, (int)GetTickCount());
}

/* ------------------------------------------------------------------ */
/* Default object creation */
static void make_default_obj(Obj *o, Tool t, double x, double y) {
    memset(o,0,sizeof(*o));
    switch(t){
        case TOOL_WALL: o->type=OBJ_WALL; o->u.cap.start=(Vec2){x-40,y}; o->u.cap.end=(Vec2){x+40,y}; o->u.cap.thickness=6; break;
        case TOOL_RAMP: o->type=OBJ_RAMP; o->u.cap.start=(Vec2){x-40,y}; o->u.cap.end=(Vec2){x+40,y}; o->u.cap.thickness=8; break;
        case TOOL_FLIPPER: o->type=OBJ_FLIPPER; o->u.flipper.pivot=(Vec2){x,y}; o->u.flipper.length=70; o->u.flipper.thickness=12; o->u.flipper.rest_angle_deg=0; o->u.flipper.active_angle_deg=30; o->u.flipper.engage_speed_deg_s=900; o->u.flipper.return_speed_deg_s=900; o->u.flipper.input=0; break;
        case TOOL_BUMPER: o->type=OBJ_BUMPER; o->u.bumper.center=(Vec2){x,y}; o->u.bumper.radius=18; o->u.bumper.impulse=400; o->u.bumper.base_score=100; break;
        case TOOL_SLINGSHOT: o->type=OBJ_SLINGSHOT; o->u.cap.start=(Vec2){x-25,y}; o->u.cap.end=(Vec2){x+25,y}; o->u.cap.thickness=8; break;
        case TOOL_GATE: o->type=OBJ_ONE_WAY_GATE; o->u.cap.start=(Vec2){x,y-20}; o->u.cap.end=(Vec2){x,y+20}; o->u.cap.thickness=4; break;
        case TOOL_TARGET: o->type=OBJ_DROP_TARGET; o->u.cap.start=(Vec2){x-15,y}; o->u.cap.end=(Vec2){x+15,y}; o->u.cap.thickness=8; o->u.cap.enabled=1; break;
        case TOOL_STANDUP: o->type=OBJ_STANDUP_TARGET; o->u.cap.start=(Vec2){x-15,y}; o->u.cap.end=(Vec2){x+15,y}; o->u.cap.thickness=8; break;
        case TOOL_ROLLOVER: o->type=OBJ_ROLLOVER; o->u.cap.start=(Vec2){x-20,y}; o->u.cap.end=(Vec2){x+20,y}; o->u.cap.thickness=2; o->u.cap.width=12; break;
        case TOOL_SPINNER: o->type=OBJ_SPINNER; o->u.spinner.pivot=(Vec2){x,y}; o->u.spinner.half_length=35; o->u.spinner.thickness=4; break;
        case TOOL_KICKOUT: o->type=OBJ_KICKOUT; o->u.kickout.center=(Vec2){x,y}; o->u.kickout.capture_radius=16; o->u.kickout.eject_direction=(Vec2){0,-1}; o->u.kickout.eject_speed=500; o->u.kickout.hold_time=0.4; break;
        case TOOL_SENSOR: o->type=OBJ_SENSOR; o->u.sensor.x=x-25; o->u.sensor.y=y-10; o->u.sensor.w=50; o->u.sensor.h=20; break;
        case TOOL_DRAIN: o->type=OBJ_DRAIN; o->u.sensor.x=x-40; o->u.sensor.y=y-10; o->u.sensor.w=80; o->u.sensor.h=20; break;
        case TOOL_BALL_SPAWN: o->type=OBJ_BALL_SPAWN; o->u.spawn.position=(Vec2){x,y}; o->u.spawn.enabled=1; o->u.spawn.has_ball_radius=1; o->u.spawn.ball_radius=8; break;
        case TOOL_LAUNCHER: o->type=OBJ_LAUNCHER; o->u.launcher.position=(Vec2){x,y}; o->u.launcher.direction=(Vec2){0,-1}; o->u.launcher.min_speed=200; o->u.launcher.max_speed=800; o->u.launcher.full_charge_time=1.0; o->u.launcher.spawn_id[0]=0; break;
        default: break;
    }
}

/* ------------------------------------------------------------------ */
/* Layout calculation */
static void calc_layout(App *a) {
    a->canvas_x = PAL_W + 1; a->canvas_y = 0;
    a->canvas_w = a->W - PAL_W - INS_W - 2; if (a->canvas_w<100) a->canvas_w=100;
    a->canvas_h = a->H - STAT_H - 1;
}

/* ------------------------------------------------------------------ */
/* Repaint pipeline: render UI into client_rgb then GDI blit */
static void render_button(Framebuffer *fb, const Button *b) {
    unsigned char br=b->active?90:70, bg=b->active?100:75, bb=b->active?120:95;
    if (b->hover && !b->active) { br=100; bg=110; bb=130; }
    fb_round_rect(fb,b->x,b->y,b->w,b->h,4,br,bg,bb);
    int tx=b->x+4, ty=b->y+(b->h-7)/2;
    fb_text(fb,tx,ty,b->label, b->disabled?150:230, b->disabled?150:230, b->disabled?150:230);
}

static void render_textfield(Framebuffer *fb, TextField *tf) {
    fb_rect(fb,tf->x,tf->y,tf->w,tf->h, tf->focused?220:120, tf->focused?220:120, tf->focused?220:120);
    fb_rect(fb,tf->x+1,tf->y+1,tf->w-2,tf->h-2,30,30,36);
    fb_text(fb,tf->x+4,tf->y+(tf->h-7)/2, tf->text, 255,255,255);
    if (tf->focused) {
        int cx=tf->x+4; /* crude caret */
        for (int i=0;i<tf->cursor && tf->text[i];i++) cx+=6;
        fb_line(fb,cx,tf->y+3,cx,tf->y+tf->h-3,1,255,255,255);
    }
    if (tf->invalid) {
        fb_rect_a(fb,tf->x,tf->y,tf->w,tf->h,255,60,60,0.25);
    }
}

static void draw_panel_bg(Framebuffer *fb, int x, int y, int w, int h) {
    fb_rect(fb,x,y,w,h,36,38,46);
    fb_vgrad(fb,x,y,w,h,44,46,56,32,34,42);
}

static void app_render(App *a) {
    fb_init(&a->fb, a->W, a->H);
    fb_clear(&a->fb, 24,26,32);

    draw_panel_bg(&a->fb, 0,0,PAL_W,a->H-STAT_H);
    draw_panel_bg(&a->fb, a->W-INS_W,0,INS_W,a->H-STAT_H);

    /* palette buttons */
    int y=8;
    for (int i=0;i<TOOL_COUNT;i++){
        Button b = {4,y,PAL_W-8,18, TOOLS[i].name, a->tool==TOOLS[i].t, 0,0, i};
        render_button(&a->fb, &b);
        y+=22;
    }

    /* canvas background */
    fb_rect(&a->fb, a->canvas_x,a->canvas_y,a->canvas_w,a->canvas_h,18,20,28);
    fb_rect(&a->fb, a->canvas_x+1,a->canvas_y+1,a->canvas_w-2,a->canvas_h-2,24,28,38);

    /* world view: render full world into temp FB and blit subrect */
    if (a->scene.world_size.x>0 && a->scene.world_size.y>0) {
        double s = app_scale(a);
        int ww = (int)(a->scene.world_size.x * s);
        int wh = (int)(a->scene.world_size.y * s);
        if (ww>0 && wh>0) {
            Framebuffer wfb; fb_init(&wfb, ww, wh);
            render_scene(&a->scene, (a->mode==MODE_PLAY||a->in_preview)?&a->sim:NULL, &wfb, s);
            /* copy visible portion into canvas */
            int sx0 = (int)(a->pan_x * s);
            int sy0 = (int)(a->pan_y * s);
            for (int cy=0; cy<a->canvas_h; cy++) {
                int wy = sy0 + cy;
                if (wy < 0 || wy >= wh) continue;
                for (int cx=0; cx<a->canvas_w; cx++) {
                    int wx = sx0 + cx;
                    if (wx < 0 || wx >= ww) continue;
                    size_t src = ((size_t)wy*ww + wx)*3;
                    size_t dst = ((size_t)(a->canvas_y+cy)*a->W + (a->canvas_x+cx))*3;
                    a->fb.pix[dst]=wfb.pix[src];
                    a->fb.pix[dst+1]=wfb.pix[src+1];
                    a->fb.pix[dst+2]=wfb.pix[src+2];
                }
            }
            fb_free(&wfb);
        }
    }

    /* selection highlight on canvas */
    if (a->mode==MODE_EDIT && a->sel_count>0) {
        for (int k=0;k<a->sel_count;k++) {
            int idx=a->sel[k]; if (idx<0||idx>=a->scene.obj_count) continue;
            const Obj *o=&a->scene.objects[idx];
            int sx,sy; world_to_screen(a, obj_center_x(o), obj_center_y(o), &sx, &sy);
            fb_circle_a(&a->fb,sx,sy,12,255,220,80,0.4);
        }
    }

    /* marquee */
    if (a->marquee) {
        int x0=a->marquee_x0<a->marquee_x1?a->marquee_x0:a->marquee_x1;
        int y0=a->marquee_y0<a->marquee_y1?a->marquee_y0:a->marquee_y1;
        int w=abs(a->marquee_x1-a->marquee_x0), h=abs(a->marquee_y1-a->marquee_y0);
        fb_rect_a(&a->fb,x0,y0,w,h,100,160,255,0.2);
        fb_rect(&a->fb,x0,y0,w,h,100,160,255);
    }

    /* inspector */
    int ix=a->W-INS_W+4, iy=8;
    char buf[128];
    const char *mode_name = a->mode==MODE_EDIT?"EDIT":(a->mode==MODE_PLAY?"PLAY":"PREVIEW");
    snprintf(buf,sizeof(buf),"Mode: %s", mode_name);
    fb_text(&a->fb,ix,iy,buf,255,255,255); iy+=14;
    snprintf(buf,sizeof(buf),"Objs: %d", a->scene.obj_count);
    fb_text(&a->fb,ix,iy,buf,200,220,255); iy+=14;
    if (a->primary_sel>=0 && a->primary_sel<a->scene.obj_count) {
        const Obj *o=&a->scene.objects[a->primary_sel];
        snprintf(buf,sizeof(buf),"Sel: %s", o->id);
        fb_text(&a->fb,ix,iy,buf,255,220,120); iy+=16;
        snprintf(buf,sizeof(buf),"Type: %s", obj_type_name(o->type));
        fb_text(&a->fb,ix,iy,buf,200,200,200); iy+=14;
        /* show some numeric properties */
        if (o->type==OBJ_BUMPER){
            snprintf(buf,sizeof(buf),"R:%.1f I:%.0f", o->u.bumper.radius, o->u.bumper.impulse);
            fb_text(&a->fb,ix,iy,buf,200,200,200); iy+=14;
        } else if (o->type==OBJ_WALL||o->type==OBJ_RAMP||o->type==OBJ_FLIPPER){
            snprintf(buf,sizeof(buf),"Th:%.1f L:%.1f", o->u.cap.thickness, o->u.flipper.length);
            fb_text(&a->fb,ix,iy,buf,200,200,200); iy+=14;
        }
    }

    /* status bar */
    fb_rect(&a->fb,0,a->H-STAT_H,a->W,STAT_H,44,46,56);
    snprintf(buf,sizeof(buf),"%s | %s | step %llu | score %d | %s",
        a->dirty?"*":"", a->file_path[0]?a->file_path:"(new)",
        (unsigned long long)a->sim.step, a->sim.score,
        a->running?"RUN":"PAUSE");
    fb_text(&a->fb,4,a->H-STAT_H+5,buf,220,220,220);

    /* modal: validation panel overlay */
    if (a->show_validation) {
        int mx=a->canvas_x+40, my=40, mw=a->canvas_w-80, mh=a->canvas_h-80;
        fb_rect_a(&a->fb,0,0,a->W,a->H,0,0,0,0.4);
        fb_rect(&a->fb,mx,my,mw,mh,50,54,66);
        fb_rect(&a->fb,mx+1,my+1,mw-2,mh-2,30,32,40);
        fb_text(&a->fb,mx+8,my+8,"VALIDATION",255,255,255);
        int dy=26;
        for (int i=0;i<(int)a->diagnostics.count && dy+14<mh;i++) {
            const Diag *d=&a->diagnostics.items[i];
            const char *sev = d->severity==SEV_ERROR?"ERR":(d->severity==SEV_WARNING?"WARN":"INFO");
            int r = d->severity==SEV_ERROR?255:(d->severity==SEV_WARNING?220:180);
            int g = d->severity==SEV_ERROR?120:(d->severity==SEV_WARNING?180:180);
            int b = d->severity==SEV_ERROR?120:255;
            snprintf(buf,sizeof(buf),"[%s] %s", sev, d->message);
            fb_text(&a->fb,mx+8,my+dy,buf,r,g,b);
            dy+=12;
        }
    }

    /* modal: file picker overlay */
    if (a->show_filepicker) {
        int mx=a->W/2-200, my=a->H/2-150, mw=400, mh=300;
        fb_rect_a(&a->fb,0,0,a->W,a->H,0,0,0,0.5);
        fb_rect(&a->fb,mx,my,mw,mh,50,54,66);
        fb_rect(&a->fb,mx+1,my+1,mw-2,mh-2,30,32,40);
        fb_text(&a->fb,mx+8,my+8,a->picker_save?"SAVE AS":"OPEN",255,255,255);
        fb_text(&a->fb,mx+8,my+26,a->picker_dir,180,220,255);
        /* list of entries would go here; for brevity render a simple path field */
        fb_rect(&a->fb,mx+8,my+46,mw-16,18,60,62,70);
        fb_text(&a->fb,mx+10,my+50,a->picker_input,255,255,255);
        Button saveb={mx+mw-80,my+mh-30,70,22,"Save",0,0,0,0};
        Button canb={mx+mw-160,my+mh-30,70,22,"Cancel",0,0,0,0};
        render_button(&a->fb,&saveb); render_button(&a->fb,&canb);
    }

    /* copy to client_rgb for GDI */
    memcpy(a->client_rgb, a->fb.pix, (size_t)a->W*a->H*3);
    fb_free(&a->fb);
}

static void blit_to_window(App *a) {
    if (!a->hbmp) return;
    SetDIBits(a->hdcMem, a->hbmp, 0, a->H, a->client_rgb, (BITMAPINFO*)&a_bmi, DIB_RGB_COLORS);
    HDC hdc = GetDC(a->hwnd);
    BitBlt(hdc, 0, 0, a->W, a->H, a->hdcMem, 0, 0, SRCCOPY);
    ReleaseDC(a->hwnd, hdc);
}

/* DIB presentation state (a_bmi) declared near top of file. */
static void resize_framebuffer(App *a, int w, int h) {
    if (w<=0 || h<=0) return;
    if (a->hbmp) { SelectObject(a->hdcMem, a->holdbmp); DeleteObject(a->hbmp); }
    if (a->hdcMem) DeleteDC(a->hdcMem);
    free(a->client_rgb);
    a->W=w; a->H=h;
    a->client_rgb=(unsigned char*)malloc((size_t)w*h*3);
    memset(a->client_rgb, 32, (size_t)w*h*3);
    a->hdcMem=CreateCompatibleDC(NULL);
    a_bmi.h.biSize=sizeof(BITMAPINFOHEADER);
    a_bmi.h.biWidth=w; a_bmi.h.biHeight=-h; /* top-down */
    a_bmi.h.biPlanes=1; a_bmi.h.biBitCount=24; a_bmi.h.biCompression=BI_RGB;
    a_bmi.h.biSizeImage=(DWORD)(w*h*3);
    a->hbmp=CreateDIBSection(a->hdcMem,(BITMAPINFO*)&a_bmi,DIB_RGB_COLORS,NULL,NULL,0);
    a->holdbmp=(HBITMAP)SelectObject(a->hdcMem,a->hbmp);
    calc_layout(a);
}

/* ------------------------------------------------------------------ */
/* Simulation tick */
static void sim_tick(App *a) {
    Sim *sim = a->in_preview ? &a->preview_sim : &a->sim;
    if (a->running || a->single_step) {
        int steps = a->single_step ? 1 : (int)(a->playback_speed);
        if (steps<1) steps=1; if (steps>10) steps=10;
        for (int i=0;i<steps;i++) {
            if (a->recording) sim_recorder_take(sim, NULL, NULL, NULL);
            if (sim_step(sim)!=0) { a->running=0; break; }
        }
        a->single_step=0;
    }
}

/* ------------------------------------------------------------------ */
/* File operations */
static int load_scene(App *a, const char *path) {
    FILE *f=fopen(path,"rb"); if (!f) return 0;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    char *buf=malloc(sz+1); size_t rd=fread(buf,1,sz,f); buf[rd]=0; fclose(f);
    Scene tmp; scene_init(&tmp); DiagList d; diag_list_init(&d);
    PbtCode c=parse_scene(buf,rd,&tmp,&d);
    diag_list_free(&d); free(buf);
    if (c!=PBT_OK) return 0;
    scene_free(&a->scene); a->scene=tmp;
    strncpy(a->file_path,path,sizeof(a->file_path)-1);
    a->dirty=0;
    sim_free(&a->sim); sim_init(&a->sim,&a->scene); sim_reset(&a->sim,&a->scene);
    sim_free(&a->preview_sim);
    history_clear(&a->history); history_push(&a->history,&a->scene);
    return 1;
}
static int save_scene(App *a, const char *path) {
    char *buf=scene_write(&a->scene);
    if (!buf) return 0;
    size_t n=strlen(buf);
    FILE *f=fopen(path,"wb");
    if (!f){free(buf);return 0;}
    size_t wr=fwrite(buf,1,n,f); fclose(f); free(buf);
    if (wr!=n) return 0;
    strncpy(a->file_path,path,sizeof(a->file_path)-1);
    a->dirty=0; return 1;
}

/* ------------------------------------------------------------------ */
/* Validation */
static void validate(App *a) {
    diag_list_free(&a->diagnostics);
    diag_list_init(&a->diagnostics);
    scene_validate(&a->scene, &a->diagnostics);
    a->show_validation=1;
}

/* ------------------------------------------------------------------ */
/* Input helpers */
static void clear_selection(App *a){ a->sel_count=0; a->primary_sel=-1; }
static void select_obj(App *a, int idx, int toggle) {
    if (idx<0 || idx>=a->scene.obj_count) return;
    if (toggle) {
        int found=-1;
        for (int i=0;i<a->sel_count;i++) if (a->sel[i]==idx){found=i;break;}
        if (found>=0){ for(int i=found;i<a->sel_count-1;i++) a->sel[i]=a->sel[i+1]; a->sel_count--; }
        else { if (a->sel_count<256) a->sel[a->sel_count++]=idx; a->primary_sel=idx; }
    } else {
        a->sel_count=0; a->sel[0]=idx; a->sel_count=1; a->primary_sel=idx;
    }
}

static void start_preview(App *a) {
    if (a->in_preview) return;
    sim_free(&a->preview_sim); sim_init(&a->preview_sim,&a->scene); sim_reset(&a->preview_sim,&a->scene);
    a->in_preview=1; a->mode=MODE_PREVIEW; a->running=1;
}
static void stop_preview(App *a) {
    a->in_preview=0; a->mode=MODE_EDIT; a->running=0;
    sim_free(&a->preview_sim);
}
static void start_play(App *a) {
    a->mode=MODE_PLAY; a->running=1;
    sim_free(&a->sim); sim_init(&a->sim,&a->scene); sim_reset(&a->sim,&a->scene);
}
static void stop_play(App *a, int keep_runtime) {
    if (!keep_runtime) { sim_free(&a->sim); sim_init(&a->sim,&a->scene); sim_reset(&a->sim,&a->scene); }
    a->mode=MODE_EDIT; a->running=0;
}

/* ------------------------------------------------------------------ */
/* Object creation / modification */
static void create_obj_at(App *a, Tool t, double x, double y) {
    if (a->scene.obj_count >= PB_MAX_OBJECTS) return;
    Obj o; make_default_obj(&o,t,x,y);
    make_id(&a->scene, obj_type_prefix(o.type), o.id, sizeof(o.id));
    o.layer[0]=0;
    int idx=a->scene.obj_count++;
    a->scene.objects[idx]=o;
    a->dirty=1; history_push(&a->history,&a->scene);
    clear_selection(a); select_obj(a,idx,0);
}
static void delete_selection(App *a) {
    if (a->sel_count==0) return;
    int map[PB_MAX_OBJECTS];
    for (int i=0;i<a->scene.obj_count;i++) map[i]=i;
    /* remove selected indices in descending order */
    for (int k=a->sel_count-1;k>=0;k--) {
        int idx=a->sel[k]; if (idx<0||idx>=a->scene.obj_count) continue;
        scene_remove_object_at(&a->scene, idx);
        for (int i=idx;i<a->scene.obj_count;i++) map[i]=map[i+1];
    }
    a->dirty=1; clear_selection(a); history_push(&a->history,&a->scene);
}
static void duplicate_selection(App *a) {
    if (a->sel_count==0) return;
    int new_sel[256], newc=0;
    for (int k=0;k<a->sel_count;k++) {
        int idx=a->sel[k]; if (idx<0||idx>=a->scene.obj_count) continue;
        if (a->scene.obj_count>=PB_MAX_OBJECTS) break;
        Obj o=a->scene.objects[idx];
        /* offset */
        if (o.type==OBJ_WALL||o.type==OBJ_RAMP||o.type==OBJ_ONE_WAY_GATE||o.type==OBJ_SLINGSHOT||o.type==OBJ_DROP_TARGET||o.type==OBJ_STANDUP_TARGET||o.type==OBJ_ROLLOVER){
            o.u.cap.start.x+=20; o.u.cap.start.y+=20; o.u.cap.end.x+=20; o.u.cap.end.y+=20;
        } else if (o.type==OBJ_BUMPER){ o.u.bumper.center.x+=20; o.u.bumper.center.y+=20;}
        else if (o.type==OBJ_SPINNER){ o.u.spinner.pivot.x+=20; o.u.spinner.pivot.y+=20;}
        else if (o.type==OBJ_FLIPPER){ o.u.flipper.pivot.x+=20; o.u.flipper.pivot.y+=20;}
        else if (o.type==OBJ_KICKOUT){ o.u.kickout.center.x+=20; o.u.kickout.center.y+=20;}
        else if (o.type==OBJ_SENSOR||o.type==OBJ_DRAIN){ o.u.sensor.x+=20; o.u.sensor.y+=20;}
        else if (o.type==OBJ_BALL_SPAWN){ o.u.spawn.position.x+=20; o.u.spawn.position.y+=20;}
        else if (o.type==OBJ_LAUNCHER){ o.u.launcher.position.x+=20; o.u.launcher.position.y+=20;}
        make_id(&a->scene, obj_type_prefix(o.type), o.id, sizeof(o.id));
        int ni=a->scene.obj_count++;
        a->scene.objects[ni]=o;
        new_sel[newc++]=ni;
    }
    clear_selection(a);
    for (int i=0;i<newc;i++) select_obj(a,new_sel[i],1);
    a->dirty=1; history_push(&a->history,&a->scene);
}

/* ------------------------------------------------------------------ */
/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App *a = &g_app;
    switch (msg) {
        case WM_CREATE:
            a->hwnd = hwnd;
            platform_init_ime();
            platform_enable_ime(hwnd, 1);
            SetTimer(hwnd, 1, 16, NULL); /* ~60Hz */
            break;
        case WM_SIZE: {
            RECT r; GetClientRect(hwnd,&r);
            resize_framebuffer(a, r.right-r.left, r.bottom-r.top);
            InvalidateRect(hwnd,NULL,FALSE);
            break;
        }
        case WM_TIMER:
            sim_tick(a);
            app_render(a);
            blit_to_window(a);
            break;
        case WM_LBUTTONDOWN: {
            int x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
            a->ldown=1; a->drag_start_x=x; a->drag_start_y=y;
            SetCapture(hwnd);
            if (x<PAL_W) {
                int idx=(y-8)/22;
                if (idx>=0 && idx<TOOL_COUNT) a->tool=TOOLS[idx].t;
                a->ldown=0;
            } else if (a->mode==MODE_EDIT && x>=a->canvas_x && x<a->canvas_x+a->canvas_w && y>=a->canvas_y && y<a->canvas_y+a->canvas_h) {
                double wx,wy; screen_to_world(a,x,y,&wx,&wy);
                if (a->tool==TOOL_SELECT) {
                    int hit=-1; double tol=8/app_scale(a);
                    for (int i=0;i<a->scene.obj_count;i++) if (obj_hit(&a->scene.objects[i],wx,wy,tol)){hit=i;break;}
                    if (hit>=0) select_obj(a,hit,(GetKeyState(VK_CONTROL)&0x8000)!=0);
                    else { clear_selection(a); a->marquee=1; a->marquee_x0=a->marquee_x1=x; a->marquee_y0=a->marquee_y1=y; }
                } else {
                    create_obj_at(a,a->tool,wx,wy);
                    a->ldown=0;
                }
            }
            break;
        }
        case WM_MOUSEMOVE: {
            int x=GET_X_LPARAM(lParam), y=GET_Y_LPARAM(lParam);
            if (a->ldown && a->tool==TOOL_SELECT && a->mode==MODE_EDIT) {
                if (a->marquee) { a->marquee_x1=x; a->marquee_y1=y; }
                else if (a->sel_count>0 && !a->dragging) {
                    if (abs(x-a->drag_start_x)>3 || abs(y-a->drag_start_y)>3) {
                        a->dragging=1;
                        double wx0,wy0; screen_to_world(a,a->drag_start_x,a->drag_start_y,&wx0,&wy0);
                        for (int k=0;k<a->sel_count;k++) {
                            int idx=a->sel[k]; if (idx<0||idx>=a->scene.obj_count) continue;
                            a->drag_offs_x[k]=wx0; a->drag_offs_y[k]=wy0;
                        }
                    }
                }
                if (a->dragging) {
                    double wx,wy; screen_to_world(a,x,y,&wx,&wy);
                    double dx=wx-a->drag_offs_x[0], dy=wy-a->drag_offs_y[0];
                    /* move all selected by same delta */
                    for (int k=0;k<a->sel_count;k++) {
                        int idx=a->sel[k]; if (idx<0||idx>=a->scene.obj_count) continue;
                        Obj *o=&a->scene.objects[idx];
                        if (o->type==OBJ_WALL||o->type==OBJ_RAMP||o->type==OBJ_ONE_WAY_GATE||o->type==OBJ_SLINGSHOT||o->type==OBJ_DROP_TARGET||o->type==OBJ_STANDUP_TARGET||o->type==OBJ_ROLLOVER){
                            o->u.cap.start.x+=dx; o->u.cap.start.y+=dy;
                            o->u.cap.end.x+=dx; o->u.cap.end.y+=dy;
                        } else if (o->type==OBJ_BUMPER){ o->u.bumper.center.x+=dx; o->u.bumper.center.y+=dy;}
                        else if (o->type==OBJ_SPINNER){ o->u.spinner.pivot.x+=dx; o->u.spinner.pivot.y+=dy;}
                        else if (o->type==OBJ_FLIPPER){ o->u.flipper.pivot.x+=dx; o->u.flipper.pivot.y+=dy;}
                        else if (o->type==OBJ_KICKOUT){ o->u.kickout.center.x+=dx; o->u.kickout.center.y+=dy;}
                        else if (o->type==OBJ_SENSOR||o->type==OBJ_DRAIN){ o->u.sensor.x+=dx; o->u.sensor.y+=dy;}
                        else if (o->type==OBJ_BALL_SPAWN){ o->u.spawn.position.x+=dx; o->u.spawn.position.y+=dy;}
                        else if (o->type==OBJ_LAUNCHER){ o->u.launcher.position.x+=dx; o->u.launcher.position.y+=dy;}
                    }
                    for (int k=0;k<a->sel_count;k++){ a->drag_offs_x[k]+=dx; a->drag_offs_y[k]+=dy; }
                }
            }
            break;
        }
        case WM_LBUTTONUP: {
            a->ldown=0;
            if (a->dragging) { a->dragging=0; a->dirty=1; history_push(&a->history,&a->scene); }
            if (a->marquee) {
                a->marquee=0;
                int x0=a->marquee_x0<a->marquee_x1?a->marquee_x0:a->marquee_x1;
                int y0=a->marquee_y0<a->marquee_y1?a->marquee_y0:a->marquee_y1;
                int x1=a->marquee_x0<a->marquee_x1?a->marquee_x1:a->marquee_x0;
                int y1=a->marquee_y0>a->marquee_y1?a->marquee_y0:a->marquee_y1;
                double wx0,wy0,wx1,wy1; screen_to_world(a,x0,y0,&wx0,&wy0); screen_to_world(a,x1,y1,&wx1,&wy1);
                clear_selection(a);
                for (int i=0;i<a->scene.obj_count;i++) {
                    double cx=obj_center_x(&a->scene.objects[i]), cy=obj_center_y(&a->scene.objects[i]);
                    if (cx>=wx0 && cx<=wx1 && cy>=wy0 && cy<=wy1) select_obj(a,i,1);
                }
            }
            ReleaseCapture();
            break;
        }
        case WM_RBUTTONDOWN: {
            a->rdown=1; SetCapture(hwnd); a->drag_start_x=GET_X_LPARAM(lParam); a->drag_start_y=GET_Y_LPARAM(lParam);
            break;
        }
        case WM_RBUTTONUP: a->rdown=0; ReleaseCapture(); break;
        case WM_MOUSEWHEEL: {
            /* zoom about pointer */
            int x=GET_X_LPARAM(lParam); int y=GET_Y_LPARAM(lParam);
            ScreenToClient(hwnd,(POINT*)&((POINT){x,y}));
            double wx,wy; screen_to_world(a,x,y,&wx,&wy);
            int delta=GET_WHEEL_DELTA_WPARAM(wParam);
            a->zoom *= (delta>0)?1.1:0.9; if (a->zoom<0.2) a->zoom=0.2; if (a->zoom>5.0) a->zoom=5.0;
            double s=app_scale(a);
            a->pan_x = wx - (x-a->canvas_x)/s;
            a->pan_y = wy - (y-a->canvas_y)/s;
            break;
        }
        case WM_KEYDOWN: {
            if ((GetKeyState(VK_CONTROL)&0x8000)) {
                switch (wParam) {
                    case 'N': { scene_init(&a->scene); a->scene.world_size=(Vec2){800,600}; a->file_path[0]=0; a->dirty=0; history_clear(&a->history); history_push(&a->history,&a->scene); break; }
                    case 'O': { OPENFILENAMEA ofn={sizeof(ofn)}; char buf[MAX_PATH]={0}; ofn.hwndOwner=hwnd; ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH; ofn.lpstrFilter="PBT\0*.pbt\0"; ofn.Flags=OFN_FILEMUSTEXIST; if (GetOpenFileNameA(&ofn)) load_scene(a,buf); break; }
                    case 'S': { if (a->file_path[0]) save_scene(a,a->file_path); else {
                        OPENFILENAMEA ofn={sizeof(ofn)}; char buf[MAX_PATH]="table.pbt"; ofn.hwndOwner=hwnd; ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH; ofn.lpstrFilter="PBT\0*.pbt\0"; ofn.Flags=OFN_OVERWRITEPROMPT; if (GetSaveFileNameA(&ofn)) save_scene(a,buf); } break; }
                    case 'Z': if (history_undo(&a->history,&a->scene)) a->dirty=history_dirty_to_saved(&a->history); break;
                    case 'Y': if (history_redo(&a->history,&a->scene)) a->dirty=history_dirty_to_saved(&a->history); break;
                    case 'D': duplicate_selection(a); break;
                    case 'C': /* internal copy not implemented via system clipboard; no-op */ break;
                    case 'V': /* internal paste not implemented */ break;
                }
            } else {
                switch(wParam){
                    case VK_DELETE: delete_selection(a); break;
                    case VK_ESCAPE: if (a->mode==MODE_PREVIEW) stop_preview(a); else { a->running=0; a->show_validation=0; a->show_filepicker=0; } break;
                    case VK_SPACE: {
                        if (a->mode==MODE_EDIT) start_preview(a);
                        else if (a->mode==MODE_PLAY||a->mode==MODE_PREVIEW) sim_input(&a->sim,1,1,1,0,0,0);
                        break;
                    }
                    case 'Z': if (a->mode==MODE_PLAY||a->mode==MODE_PREVIEW) sim_input(&a->sim,1,0,0,0,0,0); break;
                    case VK_OEM_2: /* / key */ if (a->mode==MODE_PLAY||a->mode==MODE_PREVIEW) sim_input(&a->sim,0,1,0,0,0,0); break;
                    case 'P': if (a->mode==MODE_EDIT) start_play(a); else stop_play(a,0); break;
                    case VK_F5: start_play(a); break;
                    case VK_RIGHT: a->single_step=1; break;
                    case 'V': validate(a); break;
                }
            }
            break;
        }
        case WM_KEYUP: {
            if (a->mode==MODE_PLAY || a->mode==MODE_PREVIEW) {
                if (wParam=='Z') sim_input(&a->sim,0,0,0,0,0,0);
                if (wParam==VK_OEM_2) sim_input(&a->sim,0,0,0,0,0,0);
                if (wParam==VK_SPACE) sim_input(&a->sim,0,0,0,0,0,0);
            }
            break;
        }
        case WM_CLOSE: {
            if (a->dirty) {
                int r=MessageBoxA(hwnd,"Save changes before closing?","Unsaved changes",MB_YESNOCANCEL);
                if (r==IDCANCEL) return 0;
                if (r==IDYES) { /* trigger save dialog simplified */ }
            }
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd,1);
            PostQuitMessage(0);
            break;
        default: return DefWindowProc(hwnd,msg,wParam,lParam);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Helpers referenced above (obj_type_name, obj_type_prefix, obj_center_x/y)
 * are provided by types.h / scene.h. */


/* ------------------------------------------------------------------ */
/* Main entry */
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev; (void)lpCmdLine;
    /* Initialize core defaults */
    App *a = &g_app;
    scene_init(&a->scene);
    a->scene.world_size = (Vec2){800,600};
    a->scene.gravity = (Vec2){0,980};
    a->scene.default_ball_radius=8; a->scene.default_ball_mass=1; a->scene.default_ball_restitution=0.6; a->scene.default_ball_friction=0.1; a->scene.default_ball_damping=0;
    a->scene.max_active_balls=16; a->scene.starting_turns=3;
    a->mode=MODE_EDIT; a->tool=TOOL_SELECT; a->zoom=1.0; a->playback_speed=1.0;
    a->primary_sel=-1;
    history_push(&a->history, &a->scene);
    sim_init(&a->sim, &a->scene); sim_reset(&a->sim, &a->scene);
    diag_list_init(&a->diagnostics);

    WNDCLASSEXW wc={0};
    wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName=L"PinballSandbox";
    RegisterClassExW(&wc);

    HWND hwnd=CreateWindowExW(0, L"PinballSandbox", APP_TITLE, WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT, 1280,800, NULL,NULL,hInst,NULL);
    if (!hwnd) return 1;

    MSG msg;
    while (GetMessage(&msg,NULL,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    /* cleanup */
    scene_free(&a->scene); sim_free(&a->sim); sim_free(&a->preview_sim);
    diag_list_free(&a->diagnostics);
    history_clear(&a->history);
    if (a->hdcMem) DeleteDC(a->hdcMem);
    if (a->hbmp) DeleteObject(a->hbmp);
    free(a->client_rgb);
    return (int)msg.wParam;
}
