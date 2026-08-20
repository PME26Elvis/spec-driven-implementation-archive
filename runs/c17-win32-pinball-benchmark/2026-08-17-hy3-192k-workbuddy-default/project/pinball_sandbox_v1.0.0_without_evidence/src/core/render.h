#ifndef PB_RENDER_H
#define PB_RENDER_H

#include "scene.h"
#include "sim.h"

typedef struct {
  int w, h;
  unsigned char *pix;   /* RGB, w*h*3, top-left origin */
} Framebuffer;

void fb_init(Framebuffer *fb, int w, int h);
void fb_free(Framebuffer *fb);
void fb_clear(Framebuffer *fb, unsigned char r, unsigned char g, unsigned char b);

/* pixel setters */
void fb_set(Framebuffer *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b);
void fb_blend(Framebuffer *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b, double a);

/* clipping (stack) */
void fb_clip_push(Framebuffer *fb, int x, int y, int w, int h);
void fb_clip_pop(Framebuffer *fb);

/* primitives */
void fb_rect(Framebuffer *fb, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b);
void fb_rect_a(Framebuffer *fb, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b, double a);
void fb_round_rect(Framebuffer *fb, int x, int y, int w, int h, int rad, unsigned char r, unsigned char g, unsigned char b);
void fb_round_rect_a(Framebuffer *fb, int x, int y, int w, int h, int rad, unsigned char r, unsigned char g, unsigned char b, double a);
void fb_line(Framebuffer *fb, int x0, int y0, int x1, int y1, int thick, unsigned char r, unsigned char g, unsigned char b);
void fb_capsule(Framebuffer *fb, double x0, double y0, double x1, double y1, double radius,
                unsigned char r, unsigned char g, unsigned char b, double alpha);
void fb_circle(Framebuffer *fb, int cx, int cy, int rad, unsigned char r, unsigned char g, unsigned char b);
void fb_circle_a(Framebuffer *fb, int cx, int cy, int rad, unsigned char r, unsigned char g, unsigned char b, double a);
/* vertical gradient fill of a rect */
void fb_vgrad(Framebuffer *fb, int x, int y, int w, int h, unsigned char r0, unsigned char g0, unsigned char b0,
              unsigned char r1, unsigned char g1, unsigned char b1);
/* text (5x7 bitmap font, uppercase; lowercases mapped to upper) */
void fb_text(Framebuffer *fb, int x, int y, const char *s, unsigned char r, unsigned char g, unsigned char b);
void fb_text_a(Framebuffer *fb, int x, int y, const char *s, unsigned char r, unsigned char g, unsigned char b, double a);

/* Render an authored scene + optional runtime state into fb (world->pixel by scale).
   If sim==NULL, only static authored geometry is drawn. */
void render_scene(const Scene *s, const Sim *sim, Framebuffer *fb, double scale);

#endif /* PB_RENDER_H */
