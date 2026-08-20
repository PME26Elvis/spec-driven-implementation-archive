#include "scene_write.h"
#include "scene.h"
#include "scene_parse.h"   /* PB_FILE_MAX */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

/* ---------------- growable text buffer ---------------- */
typedef struct { char *p; size_t len, cap; } Buf;
static void buf_init(Buf *b){ b->p=NULL; b->len=0; b->cap=0; }
static int buf_need(Buf *b, size_t add){
  if (b->len+add+1 > b->cap){
    size_t nc = b->cap? b->cap*2 : 4096;
    while (nc < b->len+add+1) nc*=2;
    char *np = realloc(b->p, nc);
    if (!np) return -1;
    b->p=np; b->cap=nc;
  }
  return 0;
}
static void buf_puts(Buf *b, const char *s){ size_t n=strlen(s); if(buf_need(b,n))return; memcpy(b->p+b->len,s,n); b->len+=n; b->p[b->len]=0; }
static void buf_putc(Buf *b, char c){ if(buf_need(b,1))return; b->p[b->len++]=c; b->p[b->len]=0; }
static void buf_printf(Buf *b, const char *fmt, ...){
  va_list ap; va_start(ap,fmt);
  char tmp[512]; int n=vsnprintf(tmp,sizeof(tmp),fmt,ap); va_end(ap);
  if (n<0) return;
  if ((size_t)n < sizeof(tmp)) buf_puts(b,tmp);
  else { char *big=malloc((size_t)n+1); if(big){ va_start(ap,fmt); vsnprintf(big,(size_t)n+1,fmt,ap); va_end(ap); buf_puts(b,big); free(big);} }
}

/* round-trippable double */
static void fmt_dbl(Buf *b, double v){
  if (v==0.0) { buf_puts(b,"0"); return; }
  char tmp[32];
  snprintf(tmp,sizeof(tmp),"%.17g",v);
  buf_puts(b,tmp);
}
static void fmt_vec(Buf *b, Vec2 v){ buf_putc(b,'('); fmt_dbl(b,v.x); buf_puts(b,", "); fmt_dbl(b,v.y); buf_putc(b,')'); }
static void fmt_rect(Buf *b, double x,double y,double w,double h){
  buf_putc(b,'('); fmt_dbl(b,x); buf_puts(b,", "); fmt_dbl(b,y); buf_puts(b,", "); fmt_dbl(b,w); buf_puts(b,", "); fmt_dbl(b,h); buf_putc(b,')');
}
static void fmt_str(Buf *b, const char *s){
  buf_putc(b,'"');
  for (const char *p=s; p&&*p; p++){
    char c=*p;
    if (c=='"'||c=='\\') { buf_putc(b,'\\'); buf_putc(b,c); }
    else if (c=='\n') { buf_puts(b,"\\n"); }
    else if (c=='\t') { buf_puts(b,"\\t"); }
    else buf_putc(b,c);
  }
  buf_putc(b,'"');
}
static void fmt_bool(Buf *b, int v){ buf_puts(b, v?"true":"false"); }

static const char *reset_mode_name(int m){ return m==1?"AFTER_DELAY":(m==2?"ON_NEW_BALL":"MANUAL_EVENT"); }
static const char *charge_curve_name(int c){ return c==1?"EASE":"LINEAR"; }
static const char *flipper_input_name(int i){ return i==1?"RIGHT_FLIPPER":"LEFT_FLIPPER"; }

/* ---------------- canonical writer ---------------- */
char *scene_write(const Scene *s){
  Buf b; buf_init(&b);

  buf_puts(&b,"PINBALL_TABLE 2\n\n");

  /* table */
  buf_puts(&b,"[table]\n");
  buf_puts(&b,"name = "); fmt_str(&b,s->name); buf_putc(&b,'\n');
  buf_puts(&b,"world_size = "); fmt_vec(&b,s->world_size); buf_putc(&b,'\n');
  buf_puts(&b,"gravity = "); fmt_vec(&b,s->gravity); buf_putc(&b,'\n');
  buf_printf(&b,"max_active_balls = %d\n", s->max_active_balls);
  buf_printf(&b,"starting_turns = %d\n", s->starting_turns);
  buf_puts(&b,"default_ball_radius = "); fmt_dbl(&b,s->default_ball_radius); buf_putc(&b,'\n');
  buf_puts(&b,"default_ball_mass = "); fmt_dbl(&b,s->default_ball_mass); buf_putc(&b,'\n');
  buf_puts(&b,"default_ball_restitution = "); fmt_dbl(&b,s->default_ball_restitution); buf_putc(&b,'\n');
  buf_puts(&b,"default_ball_friction = "); fmt_dbl(&b,s->default_ball_friction); buf_putc(&b,'\n');
  buf_puts(&b,"default_ball_damping = "); fmt_dbl(&b,s->default_ball_damping); buf_putc(&b,'\n');
  buf_puts(&b,"default_ball_max_speed = "); fmt_dbl(&b,s->default_ball_max_speed); buf_putc(&b,'\n');
  buf_printf(&b,"scene_seed = %llu\n", (unsigned long long)s->scene_seed);
  buf_puts(&b,"nudge_impulse = "); fmt_dbl(&b,s->nudge_impulse); buf_putc(&b,'\n');
  buf_puts(&b,"nudge_tilt_cost = "); fmt_dbl(&b,s->nudge_tilt_cost); buf_putc(&b,'\n');
  buf_puts(&b,"tilt_threshold = "); fmt_dbl(&b,s->tilt_threshold); buf_putc(&b,'\n');
  buf_puts(&b,"tilt_decay_per_second = "); fmt_dbl(&b,s->tilt_decay_per_second); buf_putc(&b,'\n');
  buf_puts(&b,"nudge_cooldown = "); fmt_dbl(&b,s->nudge_cooldown); buf_putc(&b,'\n');

  /* layers by order */
  {
    int *idx = malloc(sizeof(int)* (s->layer_count>0?s->layer_count:1));
    for (int i=0;i<s->layer_count;i++) idx[i]=i;
    for (int i=0;i<s->layer_count;i++)
      for (int j=i+1;j<s->layer_count;j++)
        if (s->layers[idx[j]].order < s->layers[idx[i]].order){ int t=idx[i]; idx[i]=idx[j]; idx[j]=t; }
    for (int k=0;k<s->layer_count;k++){
      const Layer *L=&s->layers[idx[k]];
      buf_printf(&b,"\n[layer %s]\n", L->id);
      buf_puts(&b,"name = "); fmt_str(&b,L->name); buf_putc(&b,'\n');
      buf_puts(&b,"visible = "); fmt_bool(&b,L->visible); buf_putc(&b,'\n');
      buf_puts(&b,"locked = "); fmt_bool(&b,L->locked); buf_putc(&b,'\n');
      buf_printf(&b,"order = %d\n", L->order);
    }
    free(idx);
  }

  /* objects in authored stable order */
  for (int i=0;i<s->obj_count;i++){
    const Obj *o=&s->objects[i];
    buf_printf(&b,"\n[object %s %s]\n", obj_type_name(o->type), o->id);
    switch (o->type){
      case OBJ_BALL_SPAWN:
        buf_puts(&b,"position = "); fmt_vec(&b,o->u.spawn.position); buf_putc(&b,'\n');
        buf_puts(&b,"initial_velocity = "); fmt_vec(&b,o->u.spawn.initial_velocity); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.spawn.enabled); buf_putc(&b,'\n');
        if (o->u.spawn.has_ball_radius){ buf_puts(&b,"ball_radius = "); fmt_dbl(&b,o->u.spawn.ball_radius); buf_putc(&b,'\n'); }
        break;
      case OBJ_WALL: case OBJ_RAMP:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.cap.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.cap.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.cap.friction); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_ONE_WAY_GATE:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.cap.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"allowed_direction = "); fmt_vec(&b,o->u.cap.allowed_direction); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.cap.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.cap.friction); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_SLINGSHOT:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.cap.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.cap.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.cap.friction); buf_putc(&b,'\n');
        buf_puts(&b,"impulse = "); fmt_dbl(&b,o->u.cap.impulse); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.cap.base_score);
        buf_puts(&b,"cooldown = "); fmt_dbl(&b,o->u.cap.cooldown); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_DROP_TARGET:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.cap.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.cap.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.cap.friction); buf_putc(&b,'\n');
        buf_puts(&b,"min_hit_speed = "); fmt_dbl(&b,o->u.cap.min_hit_speed); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.cap.base_score);
        buf_puts(&b,"cooldown = "); fmt_dbl(&b,o->u.cap.cooldown); buf_putc(&b,'\n');
        buf_puts(&b,"initially_raised = "); fmt_bool(&b,o->u.cap.initially_raised); buf_putc(&b,'\n');
        buf_puts(&b,"reset_mode = "); buf_puts(&b, reset_mode_name(o->u.cap.reset_mode)); buf_putc(&b,'\n');
        buf_puts(&b,"reset_delay = "); fmt_dbl(&b,o->u.cap.reset_delay); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_STANDUP_TARGET:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.cap.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.cap.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.cap.friction); buf_putc(&b,'\n');
        buf_puts(&b,"min_hit_speed = "); fmt_dbl(&b,o->u.cap.min_hit_speed); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.cap.base_score);
        buf_puts(&b,"cooldown = "); fmt_dbl(&b,o->u.cap.cooldown); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_ROLLOVER:
        buf_puts(&b,"start = "); fmt_vec(&b,o->u.cap.start); buf_putc(&b,'\n');
        buf_puts(&b,"end = "); fmt_vec(&b,o->u.cap.end); buf_putc(&b,'\n');
        buf_puts(&b,"width = "); fmt_dbl(&b,o->u.cap.width); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.cap.base_score);
        buf_puts(&b,"activation_mode = "); buf_puts(&b,"ON_ENTER"); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.cap.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_BUMPER:
        buf_puts(&b,"center = "); fmt_vec(&b,o->u.bumper.center); buf_putc(&b,'\n');
        buf_puts(&b,"radius = "); fmt_dbl(&b,o->u.bumper.radius); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.bumper.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.bumper.friction); buf_putc(&b,'\n');
        buf_puts(&b,"impulse = "); fmt_dbl(&b,o->u.bumper.impulse); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.bumper.base_score);
        buf_puts(&b,"cooldown = "); fmt_dbl(&b,o->u.bumper.cooldown); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.bumper.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_FLIPPER:
        buf_puts(&b,"pivot = "); fmt_vec(&b,o->u.flipper.pivot); buf_putc(&b,'\n');
        buf_puts(&b,"length = "); fmt_dbl(&b,o->u.flipper.length); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.flipper.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"rest_angle_deg = "); fmt_dbl(&b,o->u.flipper.rest_angle_deg); buf_putc(&b,'\n');
        buf_puts(&b,"active_angle_deg = "); fmt_dbl(&b,o->u.flipper.active_angle_deg); buf_putc(&b,'\n');
        buf_puts(&b,"engage_speed_deg_s = "); fmt_dbl(&b,o->u.flipper.engage_speed_deg_s); buf_putc(&b,'\n');
        buf_puts(&b,"return_speed_deg_s = "); fmt_dbl(&b,o->u.flipper.return_speed_deg_s); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.flipper.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.flipper.friction); buf_putc(&b,'\n');
        buf_puts(&b,"input = "); buf_puts(&b, flipper_input_name(o->u.flipper.input)); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.flipper.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_SENSOR:
        buf_puts(&b,"rect = "); fmt_rect(&b,o->u.sensor.x,o->u.sensor.y,o->u.sensor.w,o->u.sensor.h); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.sensor.enabled); buf_putc(&b,'\n');
        buf_puts(&b,"debug_visible = "); fmt_bool(&b,o->u.sensor.debug_visible); buf_putc(&b,'\n');
        break;
      case OBJ_DRAIN:
        buf_puts(&b,"rect = "); fmt_rect(&b,o->u.sensor.x,o->u.sensor.y,o->u.sensor.w,o->u.sensor.h); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.sensor.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_LAUNCHER:
        buf_puts(&b,"position = "); fmt_vec(&b,o->u.launcher.position); buf_putc(&b,'\n');
        buf_puts(&b,"spawn = "); buf_puts(&b,o->u.launcher.spawn_id); buf_putc(&b,'\n');
        buf_puts(&b,"direction = "); fmt_vec(&b,o->u.launcher.direction); buf_putc(&b,'\n');
        buf_puts(&b,"min_speed = "); fmt_dbl(&b,o->u.launcher.min_speed); buf_putc(&b,'\n');
        buf_puts(&b,"max_speed = "); fmt_dbl(&b,o->u.launcher.max_speed); buf_putc(&b,'\n');
        buf_puts(&b,"full_charge_time = "); fmt_dbl(&b,o->u.launcher.full_charge_time); buf_putc(&b,'\n');
        buf_puts(&b,"charge_curve = "); buf_puts(&b, charge_curve_name(o->u.launcher.charge_curve)); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.launcher.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_SPINNER:
        buf_puts(&b,"pivot = "); fmt_vec(&b,o->u.spinner.pivot); buf_putc(&b,'\n');
        buf_puts(&b,"half_length = "); fmt_dbl(&b,o->u.spinner.half_length); buf_putc(&b,'\n');
        buf_puts(&b,"thickness = "); fmt_dbl(&b,o->u.spinner.thickness); buf_putc(&b,'\n');
        buf_puts(&b,"rest_angle_deg = "); fmt_dbl(&b,o->u.spinner.rest_angle_deg); buf_putc(&b,'\n');
        buf_puts(&b,"angular_damping = "); fmt_dbl(&b,o->u.spinner.angular_damping); buf_putc(&b,'\n');
        buf_puts(&b,"inertia = "); fmt_dbl(&b,o->u.spinner.inertia); buf_putc(&b,'\n');
        buf_puts(&b,"restitution = "); fmt_dbl(&b,o->u.spinner.restitution); buf_putc(&b,'\n');
        buf_puts(&b,"friction = "); fmt_dbl(&b,o->u.spinner.friction); buf_putc(&b,'\n');
        buf_puts(&b,"score_per_tick = "); fmt_dbl(&b,o->u.spinner.score_per_tick); buf_putc(&b,'\n');
        buf_puts(&b,"tick_angle_deg = "); fmt_dbl(&b,o->u.spinner.tick_angle_deg); buf_putc(&b,'\n');
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.spinner.enabled); buf_putc(&b,'\n');
        break;
      case OBJ_KICKOUT:
        buf_puts(&b,"center = "); fmt_vec(&b,o->u.kickout.center); buf_putc(&b,'\n');
        buf_puts(&b,"capture_radius = "); fmt_dbl(&b,o->u.kickout.capture_radius); buf_putc(&b,'\n');
        buf_puts(&b,"eject_direction = "); fmt_vec(&b,o->u.kickout.eject_direction); buf_putc(&b,'\n');
        buf_puts(&b,"eject_speed = "); fmt_dbl(&b,o->u.kickout.eject_speed); buf_putc(&b,'\n');
        buf_puts(&b,"hold_time = "); fmt_dbl(&b,o->u.kickout.hold_time); buf_putc(&b,'\n');
        buf_puts(&b,"base_score = "); buf_printf(&b,"%ld\n", o->u.kickout.base_score);
        buf_puts(&b,"enabled = "); fmt_bool(&b,o->u.kickout.enabled); buf_putc(&b,'\n');
        break;
      default: break;
    }
    buf_puts(&b,"layer = "); buf_puts(&b,o->layer); buf_putc(&b,'\n');
    buf_puts(&b,"locked = "); fmt_bool(&b,o->locked); buf_putc(&b,'\n');
  }

  /* groups in authored stable order */
  for (int i=0;i<s->group_count;i++){
    const Group *g=&s->groups[i];
    buf_printf(&b,"\n[group %s]\n", g->id);
    buf_puts(&b,"name = "); fmt_str(&b,g->name); buf_putc(&b,'\n');
    buf_puts(&b,"pivot = "); fmt_vec(&b,g->pivot); buf_putc(&b,'\n');
    buf_printf(&b,"member_count = %d\n", g->member_count);
    for (int m=0;m<g->member_count;m++)
      buf_printf(&b,"member.%d = %s\n", m, g->members[m]);
  }

  /* events in authored stable order */
  for (int i=0;i<s->event_count;i++){
    const Event *e=&s->events[i];
    buf_printf(&b,"\n[event %s]\n", e->id);
    buf_puts(&b,"source = "); buf_puts(&b,e->source); buf_putc(&b,'\n');
    buf_puts(&b,"trigger = "); buf_puts(&b, trigger_name(e->trigger)); buf_putc(&b,'\n');
    buf_printf(&b,"action_count = %d\n", e->action_count);
    for (int a=0;a<e->action_count && a<PB_MAX_ACTIONS;a++){
      buf_printf(&b,"action.%d = %s", a, action_name(e->actions[a].type));
      const Action *ac=&e->actions[a];
      switch (ac->type){
        case ACT_ADD_SCORE: buf_printf(&b," amount=%ld", ac->amount); break;
        case ACT_SPAWN_BALL: buf_printf(&b," spawn=%s", ac->spawn); if(ac->count) buf_printf(&b," count=%d", ac->count); break;
        case ACT_ENABLE_OBJECT: case ACT_DISABLE_OBJECT: case ACT_RESET_TARGET: case ACT_EJECT_KICKOUT:
          buf_printf(&b," target=%s", ac->target); break;
        case ACT_OPEN_GATE: case ACT_LIGHT_INDICATOR:
          buf_printf(&b," target=%s", ac->target); if(ac->duration) buf_printf(&b," duration=%.6g", ac->duration); break;
        case ACT_START_MULTIBALL: buf_printf(&b," spawn=%s", ac->spawn); if(ac->count) buf_printf(&b," add_count=%d", ac->count); break;
        case ACT_SET_MULTIPLIER_OVERRIDE: buf_printf(&b," multiplier=%d", ac->multiplier); if(ac->duration) buf_printf(&b," duration=%.6g", ac->duration); break;
        case ACT_SET_TARGET_DROPPED: buf_printf(&b," target=%s dropped=%s", ac->target, ac->dropped?"true":"false"); break;
        case ACT_CLEAR_TILT: break;
        default: break;
      }
      buf_putc(&b,'\n');
    }
  }

  buf_putc(&b,'\n');

  char *out = b.p;
  if (!out) { out = malloc(1); out[0]=0; }
  return out;
}

/* ---------------- atomic file save ---------------- */
int scene_write_file(const Scene *s, const char *path){
  char *text = scene_write(s);
  if (!text) return -1;
  size_t n = strlen(text);
  /* temp path = path + ".tmp" */
  size_t plen = strlen(path);
  char *tmp = malloc(plen+8);
  if (!tmp){ free(text); return -1; }
  memcpy(tmp,path,plen); strcpy(tmp+plen,".tmp");

  FILE *f = fopen(tmp,"wb");
  if (!f){ free(tmp); free(text); return (int)PBT_E_ALLOCATION; }
  size_t w = fwrite(text,1,n,f);
  fflush(f);
  int ferr = ferror(f);
  int closeerr = fclose(f);
  free(text);
  if (w!=n || ferr || closeerr){ free(tmp); return (int)IO_E_WRITE; }
  if (rename(tmp,path)!=0){ free(tmp); return (int)IO_E_RENAME; }
  free(tmp);
  return 0;
}
