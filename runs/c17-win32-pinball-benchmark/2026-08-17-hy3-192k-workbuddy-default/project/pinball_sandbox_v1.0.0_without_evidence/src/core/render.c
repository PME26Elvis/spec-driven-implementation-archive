/* render.c — software renderer (doc 02). Framebuffer + primitives + scene render. */
#include "render.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---------------- framebuffer ---------------- */
void fb_init(Framebuffer *fb, int w, int h) {
  fb->w = w; fb->h = h;
  fb->pix = (unsigned char*)malloc((size_t)w * h * 3);
  fb_clear(fb, 0, 0, 0);
}
void fb_free(Framebuffer *fb) { if (fb->pix) { free(fb->pix); fb->pix = NULL; } }
void fb_clear(Framebuffer *fb, unsigned char r, unsigned char g, unsigned char b) {
  for (int i = 0; i < fb->w * fb->h; i++) { fb->pix[i*3]=r; fb->pix[i*3+1]=g; fb->pix[i*3+2]=b; }
}

typedef struct { int x, y, w, h; } Clip;
static Clip g_clip[32]; static int g_clip_n = 0;
static Clip fb_clip_rect(const Framebuffer *fb) {
  if (g_clip_n <= 0) return (Clip){0,0,fb->w,fb->h};
  return g_clip[g_clip_n-1];
}
void fb_clip_push(Framebuffer *fb, int x, int y, int w, int h) {
  Clip c = (g_clip_n>0)?g_clip[g_clip_n-1]:(Clip){0,0,fb->w,fb->h};
  int x0 = x<c.x?c.x:x, y0 = y<c.y?c.y:y;
  int x1 = x+w > c.x+c.w ? c.x+c.w : x+w;
  int y1 = y+h > c.y+c.h ? c.y+c.h : y+h;
  if (x1<x0) x1=x0; if (y1<y0) y1=y0;
  if (g_clip_n < 32) g_clip[g_clip_n++] = (Clip){x0,y0,x1-x0,y1-y0};
}
void fb_clip_pop(Framebuffer *fb) { (void)fb; if (g_clip_n>0) g_clip_n--; }

static int in_clip(const Framebuffer *fb, int x, int y) {
  Clip c = fb_clip_rect(fb);
  return x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h;
}

void fb_set(Framebuffer *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b) {
  if (!in_clip(fb,x,y)) return;
  if (x<0||y<0||x>=fb->w||y>=fb->h) return;
  size_t i = ((size_t)y*fb->w + x)*3;
  fb->pix[i]=r; fb->pix[i+1]=g; fb->pix[i+2]=b;
}
void fb_blend(Framebuffer *fb, int x, int y, unsigned char r, unsigned char g, unsigned char b, double a) {
  if (a>=1.0) { fb_set(fb,x,y,r,g,b); return; }
  if (a<=0.0) return;
  if (!in_clip(fb,x,y)) return;
  if (x<0||y<0||x>=fb->w||y>=fb->h) return;
  size_t i = ((size_t)y*fb->w + x)*3;
  double da = 1.0-a;
  fb->pix[i]   = (unsigned char)(r*a + fb->pix[i]*da);
  fb->pix[i+1] = (unsigned char)(g*a + fb->pix[i+1]*da);
  fb->pix[i+2] = (unsigned char)(b*a + fb->pix[i+2]*da);
}

/* ---------------- primitives ---------------- */
void fb_rect(Framebuffer *fb, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b) {
  Clip c = fb_clip_rect(fb);
  int x0=x<c.x?c.x:x, y0=y<c.y?c.y:y;
  int x1=x+w>c.x+c.w?c.x+c.w:x+w, y1=y+h>c.y+c.h?c.y+c.h:y+h;
  for (int yy=y0; yy<y1; yy++) for (int xx=x0; xx<x1; xx++) fb_set(fb,xx,yy,r,g,b);
}
void fb_rect_a(Framebuffer *fb, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b, double a) {
  Clip c = fb_clip_rect(fb);
  int x0=x<c.x?c.x:x, y0=y<c.y?c.y:y;
  int x1=x+w>c.x+c.w?c.x+c.w:x+w, y1=y+h>c.y+c.h?c.y+c.h:y+h;
  for (int yy=y0; yy<y1; yy++) for (int xx=x0; xx<x1; xx++) fb_blend(fb,xx,yy,r,g,b,a);
}
void fb_round_rect(Framebuffer *fb, int x, int y, int w, int h, int rad, unsigned char r, unsigned char g, unsigned char b) {
  fb_round_rect_a(fb,x,y,w,h,rad,r,g,b,1.0);
}
void fb_round_rect_a(Framebuffer *fb, int x, int y, int w, int h, int rad, unsigned char r, unsigned char g, unsigned char b, double a) {
  if (rad<0) rad=0; if (rad>w/2) rad=w/2; if (rad>h/2) rad=h/2;
  Clip c = fb_clip_rect(fb);
  int x0=x<c.x?c.x:x, y0=y<c.y?c.y:y;
  int x1=x+w>c.x+c.w?c.x+c.w:x+w, y1=y+h>c.y+c.h?c.y+c.h:y+h;
  for (int yy=y0; yy<y1; yy++) for (int xx=x0; xx<x1; xx++) {
    int inside=1;
    if (xx<x+rad && yy<y+rad) { int dx=xx-(x+rad), dy=yy-(y+rad); if (dx*dx+dy*dy>(rad*rad)) inside=0; }
    else if (xx>x+w-rad && yy<y+rad) { int dx=xx-(x+w-rad), dy=yy-(y+rad); if (dx*dx+dy*dy>(rad*rad)) inside=0; }
    else if (xx<x+rad && yy>y+h-rad) { int dx=xx-(x+rad), dy=yy-(y+h-rad); if (dx*dx+dy*dy>(rad*rad)) inside=0; }
    else if (xx>x+w-rad && yy>y+h-rad) { int dx=xx-(x+w-rad), dy=yy-(y+h-rad); if (dx*dx+dy*dy>(rad*rad)) inside=0; }
    if (inside) fb_blend(fb,xx,yy,r,g,b,a);
  }
}
void fb_line(Framebuffer *fb, int x0, int y0, int x1, int y1, int thick, unsigned char r, unsigned char g, unsigned char b) {
  int dx = x1-x0, dy = y1-y0;
  int steps = (int)sqrt((double)dx*dx+dy*dy);
  if (steps < 1) steps = 1;
  double rad = thick/2.0;
  for (int i=0;i<=steps;i++){
    double t=(double)i/steps;
    int cx=(int)(x0+dx*t), cy=(int)(y0+dy*t);
    fb_circle_a(fb,cx,cy,(int)(rad>0?rad:1),r,g,b,1.0);
  }
}
void fb_capsule(Framebuffer *fb, double x0, double y0, double x1, double y1, double radius,
                unsigned char r, unsigned char g, unsigned char b, double alpha) {
  int steps = (int)sqrt((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0)) + 1;
  if (steps < 1) steps = 1;
  for (int i=0;i<=steps;i++){
    double t=(double)i/steps;
    int cx=(int)(x0+(x1-x0)*t), cy=(int)(y0+(y1-y0)*t);
    fb_circle_a(fb,cx,cy,(int)(radius>0?radius:1),r,g,b,alpha);
  }
}
void fb_circle(Framebuffer *fb, int cx, int cy, int rad, unsigned char r, unsigned char g, unsigned char b) {
  fb_circle_a(fb,cx,cy,rad,r,g,b,1.0);
}
void fb_circle_a(Framebuffer *fb, int cx, int cy, int rad, unsigned char r, unsigned char g, unsigned char b, double a) {
  if (rad<0) rad=0;
  int x0=cx-rad, y0=cy-rad, x1=cx+rad+1, y1=cy+rad+1;
  Clip c = fb_clip_rect(fb);
  if (x0<c.x) x0=c.x; if (y0<c.y) y0=c.y; if (x1>c.x+c.w) x1=c.x+c.w; if (y1>c.y+c.h) y1=c.y+c.h;
  int rr=rad*rad;
  for (int yy=y0; yy<y1; yy++) for (int xx=x0; xx<x1; xx++) {
    int dx=xx-cx, dy=yy-cy;
    if (dx*dx+dy*dy<=rr) fb_blend(fb,xx,yy,r,g,b,a);
  }
}
void fb_vgrad(Framebuffer *fb, int x, int y, int w, int h, unsigned char r0, unsigned char g0, unsigned char b0,
              unsigned char r1, unsigned char g1, unsigned char b1) {
  if (h<=0) return;
  Clip c = fb_clip_rect(fb);
  int x0=x<c.x?c.x:x, y0=y<c.y?c.y:y;
  int x1=x+w>c.x+c.w?c.x+c.w:x+w, y1=y+h>c.y+c.h?c.y+c.h:y+h;
  for (int yy=y0; yy<y1; yy++) {
    double t=(double)(yy-y)/h; if (t<0)t=0; if(t>1)t=1;
    unsigned char rr=(unsigned char)(r0+(r1-r0)*t), gg=(unsigned char)(g0+(g1-g0)*t), bb=(unsigned char)(b0+(b1-b0)*t);
    for (int xx=x0; xx<x1; xx++) fb_set(fb,xx,yy,rr,gg,bb);
  }
}

/* ---------------- text (5x7) ---------------- */
/* Each glyph: 7 rows of 5 bits (bit4 = leftmost). Covers space..'_' subset + a few symbols. */
static const unsigned char FONT[96][7] = {
  [32]={{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0}}, /* space */
  ['0']={{14,17,17,17,14},{17,17,17,17,17},{17,17,17,17,14},{14,16,16,17,17},{17,17,17,17,17},{17,17,17,17,14},{14,0,0,0,0}},
  ['1']={{4,12,4,4,14},{4,4,4,4,14},{4,4,4,4,4},{4,4,4,4,14},{4,4,4,4,4},{4,4,4,4,14},{4,4,4,4,4}},
  ['2']={{14,17,1,2,4},{4,8,8,16,16},{16,16,8,4,2},{2,4,4,8,8},{8,8,16,16,16},{16,16,8,4,2},{14,17,17,14,0}},
  ['3']={{14,17,1,1,14},{17,1,1,17,14},{1,2,4,8,16},{16,16,8,4,2},{17,1,1,17,14},{17,1,1,17,14},{14,17,17,14,0}},
  ['4']={{2,6,10,18,31},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2}},
  ['5']={{31,16,16,16,14},{16,16,16,16,17},{1,1,1,1,14},{16,8,4,2,1},{1,1,1,1,17},{17,1,1,1,17},{14,17,17,14,0}},
  ['6']={{14,17,16,16,14},{16,16,16,16,17},{17,16,16,16,14},{17,16,8,4,2},{17,1,1,1,17},{17,1,1,1,17},{14,17,17,14,0}},
  ['7']={{31,1,2,4,4},{4,4,8,8,8},{8,8,8,16,16},{8,8,16,16,16},{8,8,8,4,4},{4,4,4,4,4},{4,4,4,4,4}},
  ['8']={{14,17,17,17,14},{17,1,1,17,14},{14,17,1,1,14},{14,17,1,1,17},{14,17,1,1,17},{14,17,17,17,14},{14,17,17,17,14}},
  ['9']={{14,17,17,17,14},{17,1,1,17,14},{14,17,1,1,14},{16,8,4,4,8},{16,16,16,16,17},{17,16,16,17,14},{14,17,17,14,0}},
  ['A']={{4,10,17,17,31},{17,17,17,17,31},{10,10,17,17,17},{4,4,10,10,10},{4,4,10,10,10},{17,17,17,17,31},{17,17,17,17,31}},
  ['B']={{30,17,17,17,30},{17,17,17,17,30},{14,17,17,17,14},{14,17,17,17,14},{14,17,17,17,14},{17,17,17,17,30},{30,17,17,17,30}},
  ['C']={{14,17,16,16,16},{17,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{17,16,16,16,16},{14,17,16,16,16}},
  ['D']={{30,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{30,17,17,17,17}},
  ['E']={{31,16,16,16,16},{16,16,16,16,16},{14,16,16,16,14},{14,16,16,16,14},{14,16,16,16,16},{16,16,16,16,16},{31,16,16,16,16}},
  ['F']={{31,16,16,16,16},{16,16,16,16,16},{14,16,16,16,14},{14,16,16,16,16},{14,16,16,16,16},{14,16,16,16,16},{14,16,16,16,16}},
  ['G']={{14,17,16,16,16},{17,16,16,16,16},{16,16,16,16,19},{16,16,16,16,19},{16,16,16,16,19},{17,16,16,16,17},{14,17,16,16,17}},
  ['H']={{17,17,17,17,17},{17,17,17,17,17},{31,17,17,17,31},{31,17,17,17,31},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17}},
  ['I']={{14,4,4,4,14},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{14,4,4,4,14}},
  ['J']={{7,2,2,2,4},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2},{2,2,2,2,2},{17,2,2,2,2},{7,2,2,2,4}},
  ['K']={{17,17,17,17,17},{17,17,10,10,4},{17,17,10,10,4},{14,14,4,4,4},{14,14,4,4,4},{17,17,10,10,4},{17,17,17,17,17}},
  ['L']={{16,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{16,16,16,16,16},{31,16,16,16,16}},
  ['M']={{17,17,31,31,17},{17,17,31,31,17},{17,27,27,21,17},{17,21,21,21,17},{17,21,21,21,17},{17,17,17,17,17},{17,17,17,17,17}},
  ['N']={{17,17,17,17,17},{17,25,25,21,17},{17,25,21,21,17},{17,21,21,21,17},{17,21,21,17,17},{17,17,17,17,17},{17,17,17,17,17}},
  ['O']={{14,17,17,17,14},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{14,17,17,17,14}},
  ['P']={{30,17,17,17,30},{17,17,17,17,30},{14,16,16,16,14},{14,16,16,16,16},{14,16,16,16,16},{14,16,16,16,16},{14,16,16,16,16}},
  ['Q']={{14,17,17,17,14},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,21,19},{17,17,17,17,17},{14,17,17,17,14}},
  ['R']={{30,17,17,17,30},{17,17,17,17,30},{14,16,16,16,14},{14,16,20,20,20},{14,16,16,16,14},{14,16,16,16,16},{14,16,16,16,16}},
  ['S']={{14,17,16,16,14},{17,16,16,16,17},{17,1,1,1,1},{1,2,4,8,16},{16,16,8,4,2},{17,1,1,1,17},{14,17,17,14,0}},
  ['T']={{31,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4}},
  ['U']={{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{14,17,17,17,14}},
  ['V']={{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{17,17,17,17,17},{10,10,17,17,17},{10,10,10,10,10},{4,4,4,4,4}},
  ['W']={{17,17,17,17,17},{17,17,17,17,17},{17,17,21,21,17},{17,21,21,21,17},{17,21,21,21,17},{17,27,27,21,17},{17,17,17,17,17}},
  ['X']={{17,17,17,17,17},{10,10,10,10,10},{4,4,4,4,4},{4,4,4,4,4},{10,10,10,10,10},{17,17,17,17,17},{17,17,17,17,17}},
  ['Y']={{17,17,17,17,17},{10,10,17,17,17},{10,10,10,10,10},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4},{4,4,4,4,4}},
  ['Z']={{31,1,1,1,1},{1,2,4,8,16},{2,4,4,8,8},{4,4,8,8,16},{8,8,16,16,16},{16,16,16,16,16},{31,16,16,16,16}},
  [':']={{0,0,2,0,0},{0,0,2,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,2,0,0},{0,0,2,0,0},{0,0,0,0,0}},
  ['-']={{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{31,31,31,31,31},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0}},
  ['.']={{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,0,0,0,0},{0,2,2,0,0},{0,2,2,0,0}},
  ['/']={{1,1,2,4,8},{2,2,4,8,16},{2,4,4,8,8},{4,4,8,8,16},{8,8,16,16,16},{8,16,16,16,16},{16,16,16,16,16}},
  ['(']={{2,4,8,8,8},{4,8,8,0,0},{8,8,0,0,0},{8,8,0,0,0},{8,8,0,0,0},{4,8,8,0,0},{2,4,8,8,8}},
  [')']={{8,4,2,2,2},{8,8,4,0,0},{8,8,0,0,0},{8,8,0,0,0},{8,8,0,0,0},{8,8,4,0,0},{8,4,2,2,2}},
  ['%']={{14,17,1,2,4},{17,16,8,8,16},{1,2,4,8,16},{2,4,8,8,16},{4,8,16,16,17},{8,8,16,1,1},{4,8,16,17,14}},
};
static int glyph_for(char c) {
  int u = (unsigned char)c;
  if (u >= 'a' && u <= 'z') u -= 32;   /* lowercase -> uppercase glyph */
  if (u >= 96) u = 0;                  /* out of table -> blank */
  return u;
}
void fb_text(Framebuffer *fb, int x, int y, const char *s, unsigned char r, unsigned char g, unsigned char b) {
  fb_text_a(fb,x,y,s,r,g,b,1.0);
}
void fb_text_a(Framebuffer *fb, int x, int y, const char *s, unsigned char r, unsigned char g, unsigned char b, double a) {
  int cx=x;
  for (const char *p=s; *p; p++) {
    int gi = glyph_for(*p);
    const unsigned char *gl = FONT[gi];
    for (int row=0;row<7;row++) {
      unsigned char bits = gl[row];
      for (int col=0;col<5;col++) {
        if (bits & (1<<(4-col))) fb_blend(fb, cx+col, y+row, r,g,b,a);
      }
    }
    cx += 6;
  }
}

/* ---------------- scene render ---------------- */
static void draw_capsule(Framebuffer *fb, double x0,double y0,double x1,double y1,double thick,double scale,
                         unsigned char r,unsigned char g,unsigned char b,double a){
  fb_capsule(fb, x0*scale, y0*scale, x1*scale, y1*scale, (thick*0.5+1)*scale, r,g,b,a);
}

void render_scene(const Scene *s, const Sim *sim, Framebuffer *fb, double scale) {
  fb_clear(fb, 18, 20, 28);                       /* dark canvas */
  /* world border */
  fb_rect_a(fb, 0,0, fb->w, fb->h, 40,44,56, 1.0);
  int bx=(int)(s->world_size.x*scale), by=(int)(s->world_size.y*scale);
  fb_rect(fb, 1,1, bx-2, by-2, 24,28,38);

  for (int i=0;i<s->obj_count;i++){
    const Obj *o=&s->objects[i];
    switch(o->type){
      case OBJ_WALL: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,120,124,140,1.0); break;
      case OBJ_RAMP: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,90,150,170,1.0); break;
      case OBJ_ONE_WAY_GATE: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,150,120,200, o->u.cap.enabled?1.0:0.3); break;
      case OBJ_SLINGSHOT: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,200,90,90,1.0); break;
      case OBJ_DROP_TARGET: if(o->u.cap.enabled) draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,90,200,120,1.0);
                            else draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,60,90,70,0.6); break;
      case OBJ_STANDUP_TARGET: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.thickness,scale,120,210,150,1.0); break;
      case OBJ_ROLLOVER: draw_capsule(fb,o->u.cap.start.x,o->u.cap.start.y,o->u.cap.end.x,o->u.cap.end.y,o->u.cap.width,scale,210,210,90,0.8); break;
      case OBJ_BUMPER: fb_circle_a(fb,(int)(o->u.bumper.center.x*scale),(int)(o->u.bumper.center.y*scale),(int)(o->u.bumper.radius*scale),220,160,60,1.0);
                        fb_circle_a(fb,(int)(o->u.bumper.center.x*scale),(int)(o->u.bumper.center.y*scale),(int)(o->u.bumper.radius*scale*0.5),255,220,150,1.0); break;
      case OBJ_SPINNER: {
        double ang=0; if(sim) ang=deg2rad(sim->spinners[i].angle);
        double L=o->u.spinner.half_length*scale;
        double dx=cos(ang)*L, dy=sin(ang)*L;
        fb_line(fb,(int)(o->u.spinner.pivot.x*scale-dx),(int)(o->u.spinner.pivot.y*scale-dy),
                    (int)(o->u.spinner.pivot.x*scale+dx),(int)(o->u.spinner.pivot.y*scale+dy), (int)(o->u.spinner.thickness*scale), 200,160,230);
        break; }
      case OBJ_FLIPPER: {
        double ang=0; if(sim) ang=sim->flippers[i].angle;
        double L=o->u.flipper.length*scale;
        double dx=cos(ang)*L, dy=sin(ang)*L;
        double th=o->u.flipper.thickness*0.5*scale;
        fb_capsule(fb,o->u.flipper.pivot.x*scale,o->u.flipper.pivot.y*scale,
                       o->u.flipper.pivot.x*scale+dx,o->u.flipper.pivot.y*scale+dy, th+1, 230,230,240,1.0);
        fb_circle(fb,(int)(o->u.flipper.pivot.x*scale),(int)(o->u.flipper.pivot.y*scale),(int)th, 180,180,200);
        break; }
      case OBJ_KICKOUT: fb_circle_a(fb,(int)(o->u.kickout.center.x*scale),(int)(o->u.kickout.center.y*scale),(int)(o->u.kickout.capture_radius*scale),120,140,255, sim&&sim->kickouts[i].holding?0.95:0.5); break;
      case OBJ_SENSOR: fb_rect_a(fb,(int)(o->u.sensor.x*scale),(int)(o->u.sensor.y*scale),(int)(o->u.sensor.w*scale),(int)(o->u.sensor.h*scale),80,200,220,0.35); break;
      case OBJ_DRAIN: fb_rect_a(fb,(int)(o->u.sensor.x*scale),(int)(o->u.sensor.y*scale),(int)(o->u.sensor.w*scale),(int)(o->u.sensor.h*scale),200,70,70,0.8); break;
      case OBJ_BALL_SPAWN: fb_circle_a(fb,(int)(o->u.spawn.position.x*scale),(int)(o->u.spawn.position.y*scale),(int)(o->u.spawn.has_ball_radius?o->u.spawn.ball_radius*scale:8),70,200,255,0.7); break;
      case OBJ_LAUNCHER: {
        Vec2 d=o->u.launcher.direction; double px=o->u.launcher.position.x, py=o->u.launcher.position.y;
        int ex=(int)(px+d.x*40*scale), ey=(int)(py+d.y*40*scale);
        fb_line(fb,(int)(px*scale),(int)(py*scale),ex,ey,(int)(6*scale),150,150,160); break; }
      default: break;
    }
  }
  /* balls */
  if (sim) {
    for (int i=0;i<sim->ball_count;i++){
      const Ball *b=&sim->balls[i];
      if (!b->active) continue;
      int r=(int)(b->radius*scale);
      fb_circle(fb,(int)(b->pos.x*scale),(int)(b->pos.y*scale), r>1?r:2, 240,240,255);
      fb_circle_a(fb,(int)(b->pos.x*scale),(int)(b->pos.y*scale), (r>2?r-2:1), 180,190,255,0.6);
    }
  }
  /* HUD */
  char buf[64];
  int hy = fb->h - 16;
  snprintf(buf, sizeof(buf), "SCORE %d", sim? sim->score : 0);
  fb_text(fb, 8, 8, buf, 255,255,255);
  if (sim) {
    snprintf(buf, sizeof(buf), "STEP %llu", (unsigned long long)sim->step);
    fb_text(fb, 8, 18, buf, 200,220,255);
    snprintf(buf, sizeof(buf), "BALLS %d  TURN %d", sim->ball_count, sim->turns_remaining);
    fb_text(fb, 8, 28, buf, 200,220,255);
    if (sim->tilted) { fb_text(fb, fb->w-90, 8, "TILT", 255,90,90); }
    (void)hy;
  }
}
