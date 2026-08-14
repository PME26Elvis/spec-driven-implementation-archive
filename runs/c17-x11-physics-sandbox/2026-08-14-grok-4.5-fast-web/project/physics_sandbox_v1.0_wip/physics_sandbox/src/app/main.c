#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <time.h>
#include "../physics/world.h"
#include "../render/framebuffer.h"
#include "../ui/ui.h"
#include "../ui/ui_core.h"
#include "../scene/scene_io.h"
#include "../scene/undo.h"
#include "../physics/query.h"
#include "../physics/sensor.h"
#include "../diagnostics/trail.h"
#include "../diagnostics/solver_export.h"

static float g_scale = 18.0f;
static float g_ox, g_oy;
static ps_joint *g_mouse_joint = NULL;
static int g_mouse_body = -1;
static int g_tool = 0; /* 0=select 1=circle 2=rect */

static void create_starter_scene(ps_world *w) {
    w->body_count = 0;
    w->joint_count = 0;
    w->next_id = 1;
    /* static floor - real body */
    ps_body *floor = ps_world_create_body(w, PS_BODY_STATIC);
    ps_shape fs = {0};
    fs.type = PS_SHAPE_RECTANGLE;
    fs.density = 0.0f;
    fs.data.rectangle.hx = 45.0f;
    fs.data.rectangle.hy = 1.5f;
    fs.friction = 0.5f;
    fs.restitution = 0.0f;
    ps_body_set_shape(floor, &fs);
    ps_body_set_transform(floor, ps_v2(0.0f, 22.0f), 0.0f);

    /* left and right walls as static bodies */
    ps_body *left = ps_world_create_body(w, PS_BODY_STATIC);
    ps_shape ws = {0};
    ws.type = PS_SHAPE_RECTANGLE;
    ws.data.rectangle.hx = 1.0f; ws.data.rectangle.hy = 25.0f;
    ws.friction = 0.3f;
    ps_body_set_shape(left, &ws);
    ps_body_set_transform(left, ps_v2(-42.0f, 0.0f), 0.0f);

    ps_body *right = ps_world_create_body(w, PS_BODY_STATIC);
    ps_body_set_shape(right, &ws);
    ps_body_set_transform(right, ps_v2(42.0f, 0.0f), 0.0f);

    /* mixed dynamic bodies */
    for (int i = 0; i < 12; i++) {
        ps_body *b = ps_world_create_body(w, PS_BODY_DYNAMIC);
        ps_shape s = {0};
        s.density = 1.0f;
        s.friction = 0.3f;
        s.restitution = 0.25f + 0.05f * (i % 3);
        if (i % 3 == 0) {
            s.type = PS_SHAPE_CIRCLE;
            s.data.circle.radius = 0.6f + 0.15f * (i % 4);
        } else {
            s.type = PS_SHAPE_RECTANGLE;
            s.data.rectangle.hx = 0.7f + 0.1f * (i % 3);
            s.data.rectangle.hy = 0.7f + 0.1f * ((i+1) % 3);
        }
        ps_body_set_shape(b, &s);
        float x = -10.0f + (i % 6) * 3.5f;
        float y = 0.0f + (i / 6) * 4.0f - 8.0f;
        ps_body_set_transform(b, ps_v2(x, y), 0.05f * i);
    }

    /* hanging pendulum via distance joint */
    ps_body *anchor = ps_world_create_body(w, PS_BODY_STATIC);
    ps_shape as = {0};
    as.type = PS_SHAPE_CIRCLE;
    as.data.circle.radius = 0.3f;
    as.density = 0;
    ps_body_set_shape(anchor, &as);
    ps_body_set_transform(anchor, ps_v2(15.0f, -5.0f), 0);

    ps_body *bob = ps_world_create_body(w, PS_BODY_DYNAMIC);
    ps_shape bs2 = {0};
    bs2.type = PS_SHAPE_CIRCLE;
    bs2.density = 1.0f;
    bs2.data.circle.radius = 0.8f;
    bs2.friction = 0.2f;
    bs2.restitution = 0.3f;
    ps_body_set_shape(bob, &bs2);
    ps_body_set_transform(bob, ps_v2(15.0f, 2.0f), 0);

    ps_joint *dj = ps_world_create_joint(w);
    if (dj) {
        ps_joint_init_distance(dj, anchor, bob, anchor->xf.p, bob->xf.p);
    }

    /* simple 5-segment bridge */
    ps_body *prev = NULL;
    for (int i = 0; i < 5; i++) {
        ps_body *seg = ps_world_create_body(w, PS_BODY_DYNAMIC);
        ps_shape ss = {0};
        ss.type = PS_SHAPE_RECTANGLE;
        ss.density = 0.8f;
        ss.data.rectangle.hx = 1.2f;
        ss.data.rectangle.hy = 0.25f;
        ss.friction = 0.4f;
        ps_body_set_shape(seg, &ss);
        ps_body_set_transform(seg, ps_v2(-8.0f + i * 2.6f, 8.0f), 0);
        if (prev) {
            ps_joint *link = ps_world_create_joint(w);
            if (link) {
                ps_vec2 pa = ps_v2(prev->xf.p.x + 1.1f, prev->xf.p.y);
                ps_vec2 pb = ps_v2(seg->xf.p.x - 1.1f, seg->xf.p.y);
                ps_joint_init_distance(link, prev, seg, pa, pb);
            }
        }
        prev = seg;
    }
}

static void world_to_screen(float wx, float wy, int *sx, int *sy) {
    *sx = (int)(g_ox + wx * g_scale);
    *sy = (int)(g_oy + wy * g_scale);
}

static void screen_to_world(int sx, int sy, float *wx, float *wy) {
    *wx = (sx - g_ox) / g_scale;
    *wy = (sy - g_oy) / g_scale;
}

static int pick_body(ps_world *w, float wx, float wy) {
    for (int i = w->body_count - 1; i >= 0; i--) {
        ps_body *b = &w->bodies[i];
        if (b->type != PS_BODY_DYNAMIC) continue;
        ps_vec2 amin, amax;
        ps_shape_compute_aabb(&b->shape, b->xf, &amin, &amax);
        if (wx >= amin.x && wx <= amax.x && wy >= amin.y && wy <= amax.y)
            return i;
    }
    return -1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    Display *dpy = XOpenDisplay(NULL);
    ps_world world;
    ps_world_init(&world);
    create_starter_scene(&world);
    ps_ui_state ui;
    ps_undo_stack undo;
    ps_sensor main_sensor;
    ps_trail_system trails;
    ps_trail_sys_init(&trails);
    ps_sensor_init(&main_sensor, ps_v2(-5,-5), ps_v2(5,5));
    ps_undo_init(&undo);
    ps_ui_init(&ui);
    /* top-level panels */
    int panel_tools = ps_ui_add_panel(&ui, 4, 40, 72, 160, "TOOLS", true);
    int panel_insp = ps_ui_add_panel(&ui, 0, 0, 220, 400, "INSPECTOR", true); /* pos set later */
    /* tool buttons */
    int btn_sel = ps_ui_add_button(&ui, panel_tools, 10, 60, 60, 24, "SEL");
    int btn_cir = ps_ui_add_button(&ui, panel_tools, 10, 90, 60, 24, "CIR");
    int btn_rect = ps_ui_add_button(&ui, panel_tools, 10, 120, 60, 24, "RECT");
    int btn_force = ps_ui_add_button(&ui, panel_tools, 10, 150, 60, 24, "FORCE");
    /* transport */
    int btn_play = ps_ui_add_button(&ui, -1, 10, 0, 70, 24, "PLAY");
    int btn_step = ps_ui_add_button(&ui, -1, 90, 0, 70, 24, "STEP");
    int btn_reset = ps_ui_add_button(&ui, -1, 170, 0, 80, 24, "RESET");
    /* inspector widgets */
    int sl_grav = ps_ui_add_slider(&ui, panel_insp, 20, 50, 180, 16, "GRAVITY Y", 0.f, 20.f, 9.81f);
    int sl_rest = ps_ui_add_slider(&ui, panel_insp, 20, 90, 180, 16, "RESTITUTION", 0.f, 1.f, 0.3f);
    int cb_sleep = ps_ui_add_checkbox(&ui, panel_insp, 20, 130, 180, 16, "ALLOW SLEEP", true);
    int lbl_body = ps_ui_add_label(&ui, panel_insp, 20, 170, 180, 16, "BODY: -");
    int btn_modal = ps_ui_add_button(&ui, panel_insp, 20, 210, 100, 24, "ABOUT");
    int sl_replay = ps_ui_add_slider(&ui, panel_insp, 20, 320, 180, 16, "REPLAY", 0.f, 1.f, 1.f);
    int panel_matrix = ps_ui_add_panel(&ui, 4, 220, 72, 200, "LAYER", true);
    int lbl_mat = ps_ui_add_label(&ui, panel_matrix, 10, 240, 60, 12, "CAT 0-3");
    int cb_c0 = ps_ui_add_checkbox(&ui, panel_matrix, 10, 260, 60, 14, "C0", true);
    int cb_c1 = ps_ui_add_checkbox(&ui, panel_matrix, 10, 280, 60, 14, "C1", true);
    int cb_c2 = ps_ui_add_checkbox(&ui, panel_matrix, 10, 300, 60, 14, "C2", true);
    int cb_c3 = ps_ui_add_checkbox(&ui, panel_matrix, 10, 320, 60, 14, "C3", true);
    /* solver inspector labels in inspector panel */
    int lbl_man = ps_ui_add_label(&ui, panel_insp, 20, 250, 180, 14, "MANIFOLDS: 0");
    int lbl_iter = ps_ui_add_label(&ui, panel_insp, 20, 270, 180, 14, "VEL ITER: 10");
    int lbl_ccd = ps_ui_add_label(&ui, panel_insp, 20, 290, 180, 14, "CCD: ON");
    (void)lbl_mat; (void)cb_c0; (void)cb_c1; (void)cb_c2; (void)cb_c3;
    (void)lbl_man; (void)lbl_iter; (void)lbl_ccd; (void)panel_matrix;


    if (!dpy) {
        fprintf(stderr, "No X11 display. Running headless simulation 3s...\n");
        for (int i = 0; i < 180; i++) ps_world_step(&world, 1.0f/60.0f);
        printf("Headless OK. Bodies=%d Joints=%d sample_y=%.3f\n",
               world.body_count, world.joint_count, world.bodies[3].xf.p.y);
        return 0;
    }

    int screen = DefaultScreen(dpy);
    int win_w = 1100, win_h = 700;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     40, 40, win_w, win_h, 1,
                                     BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XStoreName(dpy, win, "Physics Sandbox v1.0 (C17+X11) — WIP");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | StructureNotifyMask |
                            ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(dpy, win);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    ps_framebuffer fb;
    if (ps_fb_init(&fb, win_w, win_h) != 0) return 1;
    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, screen), DefaultDepth(dpy, screen),
                                ZPixmap, 0, (char *)fb.pixels, win_w, win_h, 32, win_w * 4);
    if (!ximg) return 1;
    ximg->byte_order = ImageByteOrder(dpy);

g_ox = win_w * 0.5f;
    g_oy = 60.0f;

    int running = 1, paused = 1, step_once = 0;
    float sim_speed = 1.0f;
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) running = 0;
            if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape || ks == XK_q) running = 0;
                if (ks == XK_space) paused = !paused;
                if (ks == XK_s) { step_once = 1; paused = 1; }
                if (ks == XK_r) create_starter_scene(&world);
                if (ks == XK_1) sim_speed = 0.25f;
                if (ks == XK_2) sim_speed = 0.5f;
                if (ks == XK_3) sim_speed = 1.0f;
                if (ks == XK_4) sim_speed = 2.0f;
                if (ks == XK_w) {
                    ps_scene_save_json(&world, "/tmp/physics_scene.json");
                    printf("Scene saved to /tmp/physics_scene.json\n");
                }
                if (ks == XK_e) {
                    if (ps_solver_export_trace(&world, "/tmp/solver_trace.txt") == 0)
                        printf("Solver trace exported\n");
                }
                if (ks == XK_z) {
                    if (ps_undo_pop(&undo, &world))
                        printf("Undo\n");
                }
                if (ks == XK_l) {
                    if (ps_scene_load_json(&world, "/tmp/physics_scene.json") == 0)
                        printf("Scene loaded from /tmp/physics_scene.json\n");
                    else
                        printf("Load failed\n");
                }
            }
            if (ev.type == ButtonPress && ev.xbutton.button == 1) {
                int mx = ev.xbutton.x, my = ev.xbutton.y;
                ps_ui_handle_mouse(&ui, mx, my, 1, 0); /* down */
                /* toggle matrix cell */
                {
                    int gx = 12, gy = 340, cs = 12;
                    for (int i=0;i<4;i++) for (int j=0;j<4;j++) {
                        int cx = gx + j*(cs+2), cy = gy + i*(cs+2);
                        if (mx >= cx && mx < cx+cs && my >= cy && my < cy+cs) {
                            bool cur = world.collision_matrix.collide[i][j];
                            ps_matrix_set(&world.collision_matrix, i, j, !cur);
                        }
                    }
                }
                /* replay scrub */
                if (ui.widgets[sl_replay].pressed || ui.widgets[sl_replay].hovered) {
                    int fc = ps_replay_frame_count(&world.replay);
                    if (fc > 0) {
                        int fi = (int)(ui.widgets[sl_replay].value * (fc - 1));
                        if (fi < 0) fi = 0;
                        ps_replay_restore(&world.replay, fi, world.bodies, world.body_count);
                        paused = 1;
                    }
                }

                /* tool / transport actions via widget state */
                if (ui.widgets[btn_play].pressed) { paused = !paused; ui.widgets[btn_play].pressed = false; }
                if (ui.widgets[btn_step].pressed) { step_once = 1; paused = 1; ui.widgets[btn_step].pressed = false; }
                if (ui.widgets[btn_reset].pressed) { create_starter_scene(&world); ui.widgets[btn_reset].pressed = false; }
                if (ui.widgets[btn_sel].pressed) { g_tool = 0; ui.widgets[btn_sel].pressed = false; }
                if (ui.widgets[btn_cir].pressed) { g_tool = 1; ui.widgets[btn_cir].pressed = false; }
                if (ui.widgets[btn_rect].pressed) { g_tool = 2; ui.widgets[btn_rect].pressed = false; }
                if (ui.widgets[btn_force].pressed) { g_tool = 3; ui.widgets[btn_force].pressed = false; }
                if (ui.widgets[btn_modal].pressed) {
                    ps_ui_show_modal(&ui, "ABOUT", "Physics Sandbox v1.0 C17+X11");
                    ui.widgets[btn_modal].pressed = false;
                }
                /* create body */
                if (g_tool == 1 || g_tool == 2) {
                    float wx, wy; screen_to_world(mx, my, &wx, &wy);
                    if (my > 40 && mx > 80 && mx < win_w - 230) {
                        ps_undo_push(&undo, &world);
                        ps_body *nb = ps_world_create_body(&world, PS_BODY_DYNAMIC);
                        if (nb) {
                            ps_shape ns = {0}; ns.density=1; ns.friction=0.3f; ns.restitution=0.2f;
                            if (g_tool==1){ ns.type=PS_SHAPE_CIRCLE; ns.data.circle.radius=0.8f; }
                            else { ns.type=PS_SHAPE_RECTANGLE; ns.data.rectangle.hx=0.8f; ns.data.rectangle.hy=0.8f; }
                            ps_body_set_shape(nb, &ns);
                            ps_body_set_transform(nb, ps_v2(wx,wy), 0);
                        }
                    }
                } else {
                    float wx, wy; screen_to_world(mx, my, &wx, &wy);
                    int idx = pick_body(&world, wx, wy);
                    if (idx >= 0) {
                        g_mouse_body = idx;
                        g_mouse_joint = ps_world_create_joint(&world);
                        if (g_mouse_joint) {
                            ps_joint_init_mouse(g_mouse_joint, &world.bodies[idx], ps_v2(wx,wy));
                            world.bodies[idx].awake = true;
                        }
                        ui.selected_body_id = world.bodies[idx].id;
                        snprintf(ui.widgets[lbl_body].label, 64, "BODY: %u", world.bodies[idx].id);
                        if (g_tool == 3) {
                            ps_body_apply_impulse(&world.bodies[idx], ps_v2(0, -15.0f), world.bodies[idx].xf.p);
                            world.bodies[idx].awake = true;
                        }
                    }
                }
            }
            if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                if (g_mouse_joint) {
                    /* simple remove last joint if it is the mouse one */
                    if (world.joint_count > 0 && &world.joints[world.joint_count-1] == g_mouse_joint)
                        world.joint_count--;
                    g_mouse_joint = NULL;
                    g_mouse_body = -1;
                }
            }
            if (ev.type == MotionNotify && g_mouse_joint) {
                float wx, wy;
                screen_to_world(ev.xmotion.x, ev.xmotion.y, &wx, &wy);
                g_mouse_joint->target = ps_v2(wx, wy);
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double dt_real = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) * 1e-9;
        last = now;

        if (!paused || step_once) {
            ps_world_step(&world, world.time_step * sim_speed);
            ps_sensor_update(&main_sensor, &world);
            {
                ps_vec2 pos[64]; int ids[64]; int n = world.body_count < 64 ? world.body_count : 64;
                for (int i=0;i<n;i++) { pos[i]=world.bodies[i].xf.p; ids[i]=(int)world.bodies[i].id; }
                ps_trail_sys_update(&trails, pos, ids, n);
            }
            step_once = 0;
        }

        /* render */
        ps_fb_clear(&fb, 0xFF0f0f1a);
        /* draw trails */
        for (int ti = 0; ti < trails.count; ti++) {
            ps_trail *tr = &trails.trails[ti];
            for (int k = 1; k < tr->count; k++) {
                int i0 = (tr->head - tr->count + k - 1 + PS_TRAIL_LEN * 2) % PS_TRAIL_LEN;
                int i1 = (tr->head - tr->count + k + PS_TRAIL_LEN * 2) % PS_TRAIL_LEN;
                int x0,y0,x1,y1;
                world_to_screen(tr->pts[i0].x, tr->pts[i0].y, &x0, &y0);
                world_to_screen(tr->pts[i1].x, tr->pts[i1].y, &x1, &y1);
                ps_fb_draw_line(&fb, x0, y0, x1, y1, 0xFF6688aa);
            }
        }
        /* draw bodies */
        for (int i = 0; i < world.body_count; i++) {
            ps_body *b = &world.bodies[i];
            int cx, cy;
            world_to_screen(b->xf.p.x, b->xf.p.y, &cx, &cy);
            uint32_t col = (b->type == PS_BODY_STATIC) ? 0xFF3a3a5c :
                           (b->awake ? 0xFFe94560 : 0xFF6a6a8a);
            if (b->shape.type == PS_SHAPE_CIRCLE) {
                int r = (int)(b->shape.data.circle.radius * g_scale + 0.5f);
                if (r < 1) r = 1;
                ps_fb_draw_circle(&fb, cx, cy, r, col);
            } else {
                int hw = (int)(b->shape.data.rectangle.hx * g_scale + 0.5f);
                int hh = (int)(b->shape.data.rectangle.hy * g_scale + 0.5f);
                ps_fb_fill_rect(&fb, cx - hw, cy - hh, hw*2+1, hh*2+1, col);
            }
        }

        /* draw contact points as red crosshairs from solver manifolds */
        for (int mi = 0; mi < world.solver.manifold_count; mi++) {
            ps_manifold *m = &world.solver.manifolds[mi];
            for (int p = 0; p < m->point_count; p++) {
                int sx, sy;
                world_to_screen(m->points[p].world_point.x, m->points[p].world_point.y, &sx, &sy);
                ps_fb_draw_line(&fb, sx-4, sy, sx+4, sy, 0xFFff2222);
                ps_fb_draw_line(&fb, sx, sy-4, sx, sy+4, 0xFFff2222);
                /* normal */
                int nx = sx + (int)(m->normal.x * 12);
                int ny = sy + (int)(m->normal.y * 12);
                ps_fb_draw_line(&fb, sx, sy, nx, ny, 0xFF22ff22);
            }
        }

        /* status bar */
        ps_fb_fill_rect(&fb, 0, 0, win_w, 28, 0xFF16213e);
        ps_fb_fill_rect(&fb, 0, win_h-32, win_w, 32, 0xFF16213e);
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "BODIES %d  JOINTS %d  SPEED %d",
                     world.body_count, world.joint_count, (int)(sim_speed*100));
            ps_fb_draw_text(&fb, 8, 8, buf, 0xFFaaccff);
            const char *mode = paused ? "PAUSED  SPACE=PLAY  S=STEP  R=RESET" : "RUNNING  SPACE=PAUSE";
            ps_fb_draw_text(&fb, 8, win_h-22, mode, 0xFFcccccc);
        }


                /* update UI layout positions */
        ui.panels[panel_insp].x = win_w - 230;
        ui.panels[panel_insp].y = 40;
        ui.panels[panel_insp].w = 220;
        ui.panels[panel_insp].h = win_h - 80;
        ui.widgets[btn_play].y = win_h - 28;
        ui.widgets[btn_step].y = win_h - 28;
        ui.widgets[btn_reset].y = win_h - 28;
        ui.widgets[btn_play].label[0] = 0;
        snprintf(ui.widgets[btn_play].label, 64, "%s", paused ? "PLAY" : "PAUSE");
        /* apply inspector sliders */
        world.gravity.y = ui.widgets[sl_grav].value;
        world.allow_sleep = ui.widgets[cb_sleep].active;
        snprintf(ui.widgets[lbl_man].label, 64, "MANIFOLDS: %d", world.solver.manifold_count);
        snprintf(ui.widgets[lbl_iter].label, 64, "VEL ITER: %d", world.velocity_iterations);
        ps_ui_update(&ui, (float)dt_real);
        ps_ui_draw(&ui, &fb);
        /* draw 4x4 collision matrix grid in layer panel region */
        {
            int gx = 12, gy = 340, cs = 12;
            for (int i=0;i<4;i++) for (int j=0;j<4;j++) {
                bool on = world.collision_matrix.collide[i][j];
                uint32_t col = on ? 0xFF44aa44 : 0xFF442222;
                ps_fb_fill_rect(&fb, gx + j*(cs+2), gy + i*(cs+2), cs, cs, col);
            }
        }
        /* replay frame count */
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "REPLAY %d", ps_replay_frame_count(&world.replay));
            ps_fb_draw_text(&fb, win_w - 220, win_h - 50, buf, 0xFFaaffaa);
        }

        XPutImage(dpy, win, DefaultGC(dpy, screen), ximg, 0, 0, 0, 0, win_w, win_h);
        XFlush(dpy);

        if (dt_real < 0.012) usleep((useconds_t)((0.012 - dt_real) * 1e6));
    }

    fb.pixels = NULL;
    XDestroyImage(ximg);
    ps_fb_free(&fb);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    printf("Application closed cleanly. Final bodies=%d\n", world.body_count);
    return 0;
}
