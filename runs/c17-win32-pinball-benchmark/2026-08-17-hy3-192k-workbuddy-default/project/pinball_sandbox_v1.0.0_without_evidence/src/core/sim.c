#include "sim.h"
#include "hash.h"
#include "rng.h"
#include "replay.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define PB_DT       (1.0/240.0)
#define PB_EPS      1e-9
#define PB_SLOP     0.02
#define PB_CORR     0.60
#define PB_MAX_IMPACT 16
#define PB_SOLVER_ITERS 8

static double clamp_speed(Vec2 *v, double vmax) {
  double sp = vlen(*v);
  if (sp > vmax && sp > PB_EPS) { double k = vmax/sp; v->x*=k; v->y*=k; return vmax; }
  return sp;
}

static void sim_alloc_cd(Sim *s) {
  if (s->bumper_cd) free(s->bumper_cd);
  s->bumper_cd = calloc((size_t)PB_MAX_BALLS * PB_MAX_RT_OBJ, sizeof(double));
}
static void sim_alloc_rec(Sim *s) {
  if (s->rec_step) free(s->rec_step);
  if (s->rec_kind) free(s->rec_kind);
  s->rec_cap = 1 << 16;
  s->rec_count = 0;
  s->rec_step = malloc(sizeof(int) * s->rec_cap);
  s->rec_kind = malloc(sizeof(int) * s->rec_cap);
}

void sim_init(Sim *s, Scene *scene) {
  memset(s,0,sizeof(*s));
  s->scene=scene; s->next_id=1;
  sim_alloc_cd(s); sim_alloc_rec(s);
}

void sim_free(Sim *s) {
  if (s->bumper_cd) { free(s->bumper_cd); s->bumper_cd = NULL; }
  if (s->rec_step) { free(s->rec_step); s->rec_step = NULL; }
  if (s->rec_kind) { free(s->rec_kind); s->rec_kind = NULL; }
}

void sim_reset(Sim *s, Scene *scene) {
  if (s->bumper_cd) free(s->bumper_cd); s->bumper_cd = NULL;
  if (s->rec_step) free(s->rec_step);
  if (s->rec_kind) free(s->rec_kind);
  memset(s,0,sizeof(*s));
  s->scene=scene; s->next_id=1;
  sim_alloc_cd(s); sim_alloc_rec(s);
  s->turns_remaining = scene?scene->starting_turns:3;
  s->combo_multiplier=1; s->sim_time=0; s->step=0; s->game_over=0;
  if (scene) {
    for (int i=0;i<scene->obj_count && i<PB_MAX_RT_OBJ;i++) {
      Obj *o=&scene->objects[i];
      if (o->type==OBJ_FLIPPER) { s->flippers[i].angle=deg2rad(o->u.flipper.rest_angle_deg); s->flippers[i].ang_vel=0; s->flippers[i].engaged=0; }
      else if (o->type==OBJ_SPINNER) { s->spinners[i].angle=o->u.spinner.rest_angle_deg; s->spinners[i].ang_vel=0; }
    }
    for (int i=0;i<scene->obj_count;i++) {
      Obj *o=&scene->objects[i];
      if (o->type==OBJ_BALL_SPAWN && o->u.spawn.enabled) { sim_spawn_ball(s,o->id); break; }
    }
  }
}

int sim_spawn_ball(Sim *s, const char *spawn_id) {
  if (s->ball_count>=PB_MAX_BALLS) { s->error=(int)RT_E_ACTIVE_BALL_LIMIT; return -1; }
  Obj *o=scene_find_object(s->scene,spawn_id);
  if (!o||o->type!=OBJ_BALL_SPAWN) return -1;
  Ball *b=&s->balls[s->ball_count++];
  memset(b,0,sizeof(*b));
  b->active=1; b->id=s->next_id++;
  b->pos=o->u.spawn.position; b->vel=o->u.spawn.initial_velocity;
  b->radius=o->u.spawn.has_ball_radius?o->u.spawn.ball_radius:s->scene->default_ball_radius;
  b->mass=s->scene->default_ball_mass; b->restitution=s->scene->default_ball_restitution; b->friction=s->scene->default_ball_friction;
  return b->id;
}

/* ---------- TOI primitives (pure) ---------- */
static int sweep_point_segment(Vec2 p, Vec2 v, Vec2 a, Vec2 b, double R, double *out_t, Vec2 *out_n) {
  Vec2 ab=vsub(b,a); Vec2 ap=vsub(p,a);
  double abl2=vdot(ab,ab);
  if (abl2<PB_EPS) { Vec2 d=vsub(p,a); double dist=vlen(d); if(dist<R&&dist>PB_EPS){*out_t=0;*out_n=vscale(d,1.0/dist);return 1;} return 0; }
  Vec2 abn=vscale(ab,1.0/abl2);
  double ap_ab=vdot(ap,abn); Vec2 ap_par=vscale(ab,ap_ab); Vec2 w0=vsub(ap,ap_par);
  double vp_ab=vdot(v,abn); Vec2 v_par=vscale(ab,vp_ab); Vec2 wv=vsub(v,v_par);
  double A=vdot(wv,wv), B=2.0*vdot(w0,wv), C=vdot(w0,w0)-R*R;
  if (A<PB_EPS) { if(C<=0){*out_t=0;Vec2 d=vlen(w0)>PB_EPS?vscale(w0,1.0/vlen(w0)):v2(0,1);*out_n=d;return 1;} return 0; }
  double disc=B*B-4.0*A*C; if(disc<0)return 0; double sq=sqrt(disc);
  double t1=(-B-sq)/(2.0*A), t2=(-B+sq)/(2.0*A);
  double t=(t1>=0.0&&t1<=1.0)?t1:((t2>=0.0&&t2<=1.0)?t2:-1.0);
  if(t<0.0)return 0;
  Vec2 hit=vadd(p,vscale(v,t)); Vec2 proj=vadd(ap_par,vscale(v_par,t)); Vec2 closest=vadd(a,proj);
  Vec2 n=vsub(hit,closest); double nl=vlen(n);
  if(nl<PB_EPS){Vec2 perp=v2(-ab.y,ab.x); n=(vlen(perp)<PB_EPS)?v2(0,1):vscale(perp,1.0/vlen(perp));} else n=vscale(n,1.0/nl);
  *out_t=t; *out_n=n; return 1;
}
static int sweep_point_circle(Vec2 p, Vec2 v, Vec2 c, double R, double *out_t, Vec2 *out_n) {
  Vec2 d=vsub(p,c); double A=vdot(v,v), B=2.0*vdot(d,v), C=vdot(d,d)-R*R;
  if(A<PB_EPS){ if(C<=0){*out_t=0;Vec2 n=vlen(d)>PB_EPS?vscale(d,1.0/vlen(d)):v2(0,1);*out_n=n;return 1;} return 0; }
  double disc=B*B-4.0*A*C; if(disc<0)return 0; double sq=sqrt(disc);
  double t1=(-B-sq)/(2.0*A), t2=(-B+sq)/(2.0*A);
  double t=(t1>=0.0&&t1<=1.0)?t1:((t2>=0.0&&t2<=1.0)?t2:-1.0);
  if(t<0.0)return 0;
  Vec2 hit=vadd(p,vscale(v,t)); Vec2 n=vsub(hit,c); double nl=vlen(n); n=nl>PB_EPS?vscale(n,1.0/nl):v2(0,1);
  *out_t=t; *out_n=n; return 1;
}

/* ---------- response ---------- */
static void resolve_surface(Ball *b, Vec2 n, double e, double mu, Vec2 vs) {
  Vec2 vrel=vsub(b->vel,vs); double vn=vdot(vrel,n);
  if(vn<0){
    double jn=-(1.0+e)*vn*b->mass; b->vel=vadd(b->vel,vscale(n,jn/b->mass));
    vrel=vsub(b->vel,vs); Vec2 tr=vsub(vrel,vscale(n,vdot(vrel,n))); double tl=vlen(tr);
    if(tl>PB_EPS){ Vec2 t=vscale(tr,1.0/tl); double jtd=-vdot(vrel,t)*b->mass; double jt=jtd; double lim=mu*jn;
      if(jt>lim)jt=lim; else if(jt<-lim)jt=-lim; b->vel=vadd(b->vel,vscale(t,jt/b->mass)); }
  }
}
static void correct_pen(Ball *b, Vec2 n, double pen){ double c=(pen-PB_SLOP)>0?(pen-PB_SLOP)*PB_CORR:0; b->pos=vadd(b->pos,vscale(n,c)); }

/* ---------- events ---------- */
static void execute_action(Sim *s, const Action *ac){
  s->ev_actions_executed++;
  switch(ac->type){
    case ACT_ADD_SCORE: { int m=s->override_multiplier>0?s->override_multiplier:(s->combo_multiplier>0?s->combo_multiplier:1); s->score+=ac->amount*m; break; }
    case ACT_SPAWN_BALL: if(ac->spawn[0]){int n=ac->count>0?ac->count:1;for(int i=0;i<n;i++)sim_spawn_ball(s,ac->spawn);} break;
    case ACT_START_MULTIBALL: if(ac->spawn[0]){int n=ac->count>0?ac->count:1;for(int i=0;i<n;i++)sim_spawn_ball(s,ac->spawn);} break;
    case ACT_ENABLE_OBJECT: { Obj*o=scene_find_object(s->scene,ac->target); if(o)o->u.cap.enabled=1; break; }
    case ACT_DISABLE_OBJECT: { Obj*o=scene_find_object(s->scene,ac->target); if(o)o->u.cap.enabled=0; break; }
    case ACT_OPEN_GATE: { Obj*o=scene_find_object(s->scene,ac->target); if(o)o->u.cap.enabled=0; break; }
    case ACT_LIGHT_INDICATOR: break;
    case ACT_RESET_TARGET: { Obj*o=scene_find_object(s->scene,ac->target); if(o&&o->type==OBJ_DROP_TARGET)o->u.cap.enabled=1; break; }
    case ACT_SET_TARGET_DROPPED: { Obj*o=scene_find_object(s->scene,ac->target); if(o)o->u.cap.enabled=ac->dropped?0:1; break; }
    case ACT_EJECT_KICKOUT: if(ac->target[0]){for(int i=0;i<s->scene->obj_count;i++)if(strcmp(s->scene->objects[i].id,ac->target)==0)s->kickouts[i].hold_timer=0;break;}
    case ACT_SET_MULTIPLIER_OVERRIDE: s->override_multiplier=ac->multiplier>0?ac->multiplier:1; s->override_frames=(int)(ac->duration/PB_DT+0.5); break;
    case ACT_CLEAR_TILT: s->tilted=0; s->tilt_accum=0; s->ev_tilt_cleared++; break;
    default: break;
  }
}
static void fire_trigger(Sim *s, const char *src, TriggerType trig){
  Scene *sc=s->scene;
  for(int i=0;i<sc->event_count;i++){ Event *e=&sc->events[i]; if(e->trigger!=trig)continue; if(strcmp(e->source,src)!=0)continue;
    for(int a=0;a<e->action_count;a++) execute_action(s,&e->actions[a]); }
}

/* ---------- apply response for a given object (no re-advance) ---------- */
static void apply_response(Sim *s, Ball *b, int oi, Vec2 n){
  Scene *sc=s->scene; Obj *o=&sc->objects[oi];
  switch(o->type){
    case OBJ_WALL: case OBJ_RAMP: case OBJ_ONE_WAY_GATE: {
      double e=fmin(b->restitution,o->u.cap.restitution); double mu=sqrt(b->friction*o->u.cap.friction);
      resolve_surface(b,n,e,mu,v2(0,0));
      double R=o->u.cap.thickness*0.5+b->radius;
      correct_pen(b,n, vlen(vsub(b->pos,closest_on_segment(b->pos,o->u.cap.start,o->u.cap.end)))-R);
      break; }
    case OBJ_SLINGSHOT: {
      double e=fmin(b->restitution,o->u.cap.restitution); double mu=sqrt(b->friction*o->u.cap.friction);
      Vec2 vrel=vsub(b->vel,v2(0,0)); double vn=vdot(vrel,n); int hit=(vn<0);
      resolve_surface(b,n,e,mu,v2(0,0));
      double R=o->u.cap.thickness*0.5+b->radius;
      correct_pen(b,n, vlen(vsub(b->pos,closest_on_segment(b->pos,o->u.cap.start,o->u.cap.end)))-R);
      if(hit){ b->vel=vadd(b->vel,vscale(n,o->u.cap.impulse/b->mass)); clamp_speed(&b->vel,sc->default_ball_max_speed);
        s->ev_slingshot_hit++; s->score+=(int)o->u.cap.base_score; fire_trigger(s,o->id,TRIG_SLINGSHOT_HIT); }
      break; }
    case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: {
      double e=fmin(b->restitution,o->u.cap.restitution); double mu=sqrt(b->friction*o->u.cap.friction);
      double vn=-vdot(b->vel,n); int hit=(vn>0);
      resolve_surface(b,n,e,mu,v2(0,0));
      double R=o->u.cap.thickness*0.5+b->radius;
      correct_pen(b,n, vlen(vsub(b->pos,closest_on_segment(b->pos,o->u.cap.start,o->u.cap.end)))-R);
      if(hit && vn>=o->u.cap.min_hit_speed){ s->ev_target_hit++; s->score+=(int)o->u.cap.base_score;
        if(o->type==OBJ_DROP_TARGET){o->u.cap.enabled=0; s->ev_target_dropped++;} fire_trigger(s,o->id,TRIG_TARGET_HIT); }
      break; }
    case OBJ_ROLLOVER: {
      s->ev_rollover++; s->score+=(int)o->u.cap.base_score; fire_trigger(s,o->id,TRIG_ROLLOVER_ENTER); break; }
    case OBJ_BUMPER: {
      double e=fmin(b->restitution,o->u.bumper.restitution); double mu=sqrt(b->friction*o->u.bumper.friction);
      Vec2 vrel=vsub(b->vel,v2(0,0)); double vn=vdot(vrel,n); int hit=(vn<0);
      resolve_surface(b,n,e,mu,v2(0,0));
      correct_pen(b,n, vlen(vsub(b->pos,o->u.bumper.center))-o->u.bumper.radius-b->radius);
      if(hit){ b->vel=vadd(b->vel,vscale(n,o->u.bumper.impulse/b->mass)); clamp_speed(&b->vel,sc->default_ball_max_speed);
        double now=s->step*PB_DT; int cd_ok=(now-s->bumper_cd[(b->id%PB_MAX_BALLS)*PB_MAX_RT_OBJ + (oi%PB_MAX_RT_OBJ)])>=o->u.bumper.cooldown;
        if(cd_ok){ s->bumper_cd[(b->id%PB_MAX_BALLS)*PB_MAX_RT_OBJ + (oi%PB_MAX_RT_OBJ)]=now; s->ev_bumper_hit++; s->score+=(int)o->u.bumper.base_score; fire_trigger(s,o->id,TRIG_BUMPER_HIT);} }
      break; }
    case OBJ_FLIPPER: {
      FlipperRT *fr=&s->flippers[oi]; double e=fmin(b->restitution,o->u.flipper.restitution); double mu=sqrt(b->friction*o->u.flipper.friction);
      Vec2 r=vsub(b->pos,o->u.flipper.pivot); Vec2 vs=v2(-fr->ang_vel*r.y, fr->ang_vel*r.x);
      resolve_surface(b,n,e,mu,vs);
      double R=o->u.flipper.thickness*0.5+b->radius;
      Vec2 tip=vadd(o->u.flipper.pivot,v2(cos(fr->angle)*o->u.flipper.length,sin(fr->angle)*o->u.flipper.length));
      correct_pen(b,n, vlen(vsub(b->pos,closest_on_segment(b->pos,o->u.flipper.pivot,tip)))-R);
      break; }
    case OBJ_SPINNER: {
      SpinnerRT *sr=&s->spinners[oi]; double e=fmin(b->restitution,o->u.spinner.restitution); double mu=sqrt(b->friction*o->u.spinner.friction);
      Vec2 r=vsub(b->pos,o->u.spinner.pivot); Vec2 q=v2(-r.y,r.x); Vec2 vs=vscale(q,sr->ang_vel);
      Vec2 vrel=vsub(b->vel,vs); double vn=vdot(vrel,n);
      if(vn<0){ double an=vdot(n,q); double Kn=1.0/b->mass+(an*an)/o->u.spinner.inertia; double jn=-(1.0+e)*vn/Kn;
        b->vel=vadd(b->vel,vscale(n,jn/b->mass)); sr->ang_vel-=jn*an/o->u.spinner.inertia;
        vrel=vsub(b->vel,vscale(q,sr->ang_vel)); Vec2 tr=vsub(vrel,vscale(n,vdot(vrel,n))); double tl=vlen(tr);
        if(tl>PB_EPS){ Vec2 ta=vscale(tr,1.0/tl); double at=vdot(ta,q); double Kt=1.0/b->mass+(at*at)/o->u.spinner.inertia;
          double jtd=-vdot(vrel,ta)/Kt; double lim=mu*jn; double jt=jtd; if(jt>lim)jt=lim; else if(jt<-lim)jt=-lim;
          b->vel=vadd(b->vel,vscale(ta,jt/b->mass)); sr->ang_vel-=jt*at/o->u.spinner.inertia; }
        s->ev_spinner_tick++; fire_trigger(s,o->id,TRIG_SPINNER_TICK);
      }
      double R=o->u.spinner.thickness*0.5+b->radius;
      Vec2 tip=vadd(o->u.spinner.pivot,v2(cos(deg2rad(sr->angle))*o->u.spinner.half_length,sin(deg2rad(sr->angle))*o->u.spinner.half_length));
      correct_pen(b,n, vlen(vsub(b->pos,closest_on_segment(b->pos,o->u.spinner.pivot,tip)))-R);
      break; }
    case OBJ_KICKOUT: {
      b->captured=1; s->kickouts[oi].holding=b->id; s->kickouts[oi].hold_timer=o->u.kickout.hold_time; b->vel=v2(0,0);
      s->ev_kickout_capture++; fire_trigger(s,o->id,TRIG_KICKOUT_CAPTURE); break; }
    case OBJ_SENSOR: { s->ev_rollover++; fire_trigger(s,o->id,TRIG_SENSOR_ENTER); break; }
    case OBJ_DRAIN: { b->drained=1; s->ev_drained++; fire_trigger(s,o->id,TRIG_BALL_DRAINED); break; }
    default: break;
  }
}

/* ---------- step ---------- */
int sim_step(Sim *s){
  if(s->game_over)return 0;
  Scene *sc=s->scene; s->step++; s->sim_time=s->step*PB_DT;
  if(s->override_frames>0){s->override_frames--; if(s->override_frames==0)s->override_multiplier=0;}

  /* mechanism kinematics */
  for(int i=0;i<sc->obj_count && i<PB_MAX_RT_OBJ;i++){
    Obj *o=&sc->objects[i];
    if(o->type==OBJ_FLIPPER){ FlipperRT *fr=&s->flippers[i];
      double target=(fr->engaged?deg2rad(o->u.flipper.active_angle_deg):deg2rad(o->u.flipper.rest_angle_deg));
      double maxd=deg2rad((fr->engaged?o->u.flipper.engage_speed_deg_s:o->u.flipper.return_speed_deg_s))*PB_DT;
      double da=target-fr->angle; double na=(fabs(da)<=maxd)?target:fr->angle+(da>0?1:-1)*maxd;
      fr->ang_vel=(na-fr->angle)/PB_DT; fr->angle=na;
    } else if(o->type==OBJ_SPINNER){ s->spinners[i].ang_vel*=exp(-o->u.spinner.angular_damping*PB_DT); }
  }

  /* per-ball integration with CCD */
  for(int bi=0;bi<s->ball_count;bi++){
    Ball *b=&s->balls[bi];
    if(!b->active||b->drained||b->captured)continue;
    b->vel=vadd(b->vel,vscale(sc->gravity,PB_DT));
    b->vel=vscale(b->vel,exp(-sc->default_ball_damping*PB_DT));
    clamp_speed(&b->vel,sc->default_ball_max_speed);

    double remain=1.0; int impacts=0;
    while(remain>PB_EPS && impacts<PB_MAX_IMPACT){
      double best_t=2.0; int best_obj=-1; Vec2 best_n=v2(0,1);
      for(int oi=0;oi<sc->obj_count;oi++){
        Obj *o=&sc->objects[oi];
        int type_incl=0; double tt; Vec2 nn; int got=0;
        Vec2 disp=vscale(b->vel,remain);
        switch(o->type){
          case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
          case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER:
            if(o->type==OBJ_ROLLOVER){ type_incl=1; }
            else if(!o->u.cap.enabled) break;
            else if(o->type==OBJ_ONE_WAY_GATE){ Vec2 dir=vnorm(o->u.cap.allowed_direction); if(vdot(b->vel,dir)>1e-9)break; type_incl=1; }
            else type_incl=1;
            if(type_incl){ double Rc=(o->type==OBJ_ROLLOVER?o->u.cap.width*0.5:o->u.cap.thickness*0.5); double R=Rc+b->radius; got=sweep_point_segment(b->pos,disp,o->u.cap.start,o->u.cap.end,R,&tt,&nn);}
            break;
          case OBJ_BUMPER: { double R=o->u.bumper.radius+b->radius; got=sweep_point_circle(b->pos,disp,o->u.bumper.center,R,&tt,&nn); break; }
          case OBJ_FLIPPER: { FlipperRT*fr=&s->flippers[oi]; double L=o->u.flipper.length; double R=o->u.flipper.thickness*0.5+b->radius;
            Vec2 tip=vadd(o->u.flipper.pivot,v2(cos(fr->angle)*L,sin(fr->angle)*L)); got=sweep_point_segment(b->pos,disp,o->u.flipper.pivot,tip,R,&tt,&nn); break; }
          case OBJ_SPINNER: { SpinnerRT*sr=&s->spinners[oi]; double L=o->u.spinner.half_length; double R=o->u.spinner.thickness*0.5+b->radius;
            Vec2 tip=vadd(o->u.spinner.pivot,v2(cos(deg2rad(sr->angle))*L,sin(deg2rad(sr->angle))*L)); got=sweep_point_segment(b->pos,disp,o->u.spinner.pivot,tip,R,&tt,&nn); break; }
          case OBJ_KICKOUT: if(o->u.kickout.enabled&&!b->captured){ double R=o->u.kickout.capture_radius; got=sweep_point_circle(b->pos,disp,o->u.kickout.center,R,&tt,&nn);} break;
          case OBJ_SENSOR: case OBJ_DRAIN: {
            double x=o->u.sensor.x-b->radius,y=o->u.sensor.y-b->radius,w=o->u.sensor.w+2*b->radius,h=o->u.sensor.h+2*b->radius;
            Vec2 p=b->pos; double tmin=1.0; int hx=0,hy=0;
            if(fabs(disp.x)>PB_EPS){double a=(x-p.x)/disp.x,b2=(x+w-p.x)/disp.x;double tA=fmin(a,b2);if(tA>=0&&tA<=1&&p.y+disp.y*tA>=y&&p.y+disp.y*tA<=y+h){if(tA<tmin){tmin=tA;hx=1;}}}
            if(fabs(disp.y)>PB_EPS){double a=(y-p.y)/disp.y,b2=(y+h-p.y)/disp.y;double tA=fmin(a,b2);if(tA>=0&&tA<=1&&p.x+disp.x*tA>=x&&p.x+disp.x*tA<=x+w){if(tA<tmin){tmin=tA;hy=1;}}}
            if(hx||hy){tt=tmin;got=2;} break; }
          default: break;
        }
        if(got && tt>=0.0 && tt<=1.0 && tt<best_t){ best_t=tt; best_obj=oi; best_n=nn; }
      }
      if(best_obj<0){ b->pos=vadd(b->pos,vscale(b->vel,remain)); break; }
      b->pos=vadd(b->pos,vscale(b->vel,best_t*remain));
      remain*=(1.0-best_t);
      apply_response(s,b,best_obj,best_n);
      impacts++;
    }
  }

  /* ball-ball */
  for(int iter=0;iter<PB_SOLVER_ITERS;iter++){
    int any=0;
    for(int i=0;i<s->ball_count;i++){ Ball*a=&s->balls[i]; if(!a->active||a->drained||a->captured)continue;
      for(int j=i+1;j<s->ball_count;j++){ Ball*bb=&s->balls[j]; if(!bb->active||bb->drained||bb->captured)continue;
        Vec2 d=vsub(bb->pos,a->pos); double dist=vlen(d); double Rsum=a->radius+bb->radius;
        if(dist<Rsum&&dist>PB_EPS){ Vec2 n=vscale(d,1.0/dist); double invA=1.0/a->mass,invB=1.0/bb->mass;
          Vec2 rv=vsub(bb->vel,a->vel); double vn=vdot(rv,n);
          if(vn<0){ double e=fmin(a->restitution,bb->restitution);
            double j=-(1+e)*vn/(invA+invB); a->vel=vsub(a->vel,vscale(n,j*invA)); bb->vel=vadd(bb->vel,vscale(n,j*invB)); }
          double pen=Rsum-dist; double c=(pen-PB_SLOP)>0?(pen-PB_SLOP)*PB_CORR/(invA+invB):0;
          a->pos=vsub(a->pos,vscale(n,c*invA)); bb->pos=vadd(bb->pos,vscale(n,c*invB)); any=1; }
      } }
    if(!any)break;
  }

  /* kickout ejection */
  for(int i=0;i<sc->obj_count && i<PB_MAX_RT_OBJ;i++){ Obj*o=&sc->objects[i]; if(o->type!=OBJ_KICKOUT)continue; KickoutRT*kr=&s->kickouts[i];
    if(kr->holding){ kr->hold_timer-=PB_DT; if(kr->hold_timer<=0){
      int bi2=-1; for(int k=0;k<s->ball_count;k++)if(s->balls[k].id==kr->holding){bi2=k;break;}
      if(bi2>=0){ Ball*b=&s->balls[bi2]; Vec2 dir=vnorm(o->u.kickout.eject_direction);
        b->pos=vadd(o->u.kickout.center,vscale(dir,o->u.kickout.capture_radius+b->radius+PB_SLOP));
        b->vel=vscale(dir,o->u.kickout.eject_speed); clamp_speed(&b->vel,sc->default_ball_max_speed);
        b->captured=0; kr->holding=0; kr->hold_timer=0; s->ev_kickout_eject++; fire_trigger(s,o->id,TRIG_KICKOUT_EJECT); }
    } }
  }

  /* drain removal & turn resolution */
  int removed=0;
  for(int i=0;i<s->ball_count;i++) if(s->balls[i].drained){s->balls[i].active=0;removed++;}
  if(removed){ int w=0; for(int i=0;i<s->ball_count;i++) if(s->balls[i].active)s->balls[w++]=s->balls[i]; s->ball_count=w; }
  if(s->ball_count==0&&!s->game_over){ s->turns_remaining--;
    if(s->turns_remaining>0){ for(int i=0;i<sc->obj_count;i++){Obj*o=&sc->objects[i];if(o->type==OBJ_BALL_SPAWN&&o->u.spawn.enabled){sim_spawn_ball(s,o->id);break;}}}
    else s->game_over=1;
  }

  for(int i=0;i<s->ball_count;i++){ Ball*b=&s->balls[i];
    if(!isfinite(b->pos.x)||!isfinite(b->pos.y)||!isfinite(b->vel.x)||!isfinite(b->vel.y)){s->error=(int)RT_E_NONFINITE_STATE;return -1;} }
  return 0;
}

void sim_input(Sim *s,int left,int right,int launch,int nl,int nr,int nu){
  (void)launch; Scene*sc=s->scene;
  for(int i=0;i<sc->obj_count && i<PB_MAX_RT_OBJ;i++){ Obj*o=&sc->objects[i];
    if(o->type==OBJ_FLIPPER){ if(o->u.flipper.input==0)s->flippers[i].engaged=left; else s->flippers[i].engaged=right; } }
  if(s->record_enabled){
    int cur[6]={left,right,launch,nl,nr,nu};
    int prev[6]={s->prev_left,s->prev_right,s->prev_launch,s->prev_nl,s->prev_nr,s->prev_nu};
    int down[6]={RPL_L_FLIPPER_DOWN,RPL_R_FLIPPER_DOWN,RPL_LAUNCH_DOWN,RPL_NUDGE_LEFT,RPL_NUDGE_RIGHT,RPL_NUDGE_UP};
    int up[6]  ={RPL_L_FLIPPER_UP,  RPL_R_FLIPPER_UP,  RPL_LAUNCH_UP,  RPL_NUDGE_UP,   RPL_NUDGE_UP,   RPL_NUDGE_UP};
    for(int k=0;k<6;k++){
      if(cur[k]!=prev[k]){
        int kind = cur[k] ? down[k] : up[k];
        if(s->rec_count < s->rec_cap){
          s->rec_step[s->rec_count]= (int)s->step; s->rec_kind[s->rec_count]=kind; s->rec_count++;
        }
      }
    }
    s->prev_left=left; s->prev_right=right; s->prev_launch=launch; s->prev_nl=nl; s->prev_nr=nr; s->prev_nu=nu;
  }
  if(nl||nr||nu){ s->ev_nudge++;
    Vec2 dir=v2((nr?1:0)-(nl?1:0), nu?-1:0); double il=vlen(dir); if(il>PB_EPS)dir=vscale(dir,1.0/il);
    for(int i=0;i<s->ball_count;i++){ Ball*b=&s->balls[i]; if(!b->active||b->drained||b->captured)continue;
      b->vel=vadd(b->vel,vscale(dir,sc->nudge_impulse)); clamp_speed(&b->vel,sc->default_ball_max_speed); }
    s->tilt_accum+=(int)sc->nudge_tilt_cost;
    if(!s->tilted && s->tilt_accum>=(int)sc->tilt_threshold){ s->tilted=1; s->ev_tilt_started++; fire_trigger(s,"",TRIG_TILT_STARTED); }
  }
}

uint64_t sim_fingerprint(const Sim *s){
  uint64_t h; fnv1a64_init(&h);
  fnv1a64_update(&h,&s->score,sizeof(s->score));
  fnv1a64_update(&h,&s->step,sizeof(s->step));
  fnv1a64_update(&h,&s->ball_count,sizeof(s->ball_count));
  fnv1a64_update(&h,&s->turns_remaining,sizeof(s->turns_remaining));
  fnv1a64_update(&h,&s->tilted,sizeof(s->tilted));
  fnv1a64_update(&h,&s->override_multiplier,sizeof(s->override_multiplier));
  for(int i=0;i<s->ball_count;i++){ const Ball*b=&s->balls[i];
    fnv1a64_update(&h,&b->id,sizeof(b->id)); fnv1a64_update(&h,&b->active,1); fnv1a64_update(&h,&b->drained,1); fnv1a64_update(&h,&b->captured,1);
    fnv1a64_update(&h,&b->pos.x,sizeof(b->pos.x)); fnv1a64_update(&h,&b->pos.y,sizeof(b->pos.y));
    fnv1a64_update(&h,&b->vel.x,sizeof(b->vel.x)); fnv1a64_update(&h,&b->vel.y,sizeof(b->vel.y)); }
  Scene*sc=s->scene;
  for(int i=0;i<sc->obj_count && i<PB_MAX_RT_OBJ;i++){ Obj*o=&sc->objects[i];
    if(o->type==OBJ_FLIPPER)fnv1a64_update(&h,&s->flippers[i].angle,sizeof(double));
    else if(o->type==OBJ_SPINNER){fnv1a64_update(&h,&s->spinners[i].angle,sizeof(double));fnv1a64_update(&h,&s->spinners[i].ang_vel,sizeof(double));}
    else if(o->type==OBJ_KICKOUT)fnv1a64_update(&h,&s->kickouts[i].holding,sizeof(s->kickouts[i].holding)); }
  return fnv1a64_final(h);
}

void sim_checkpoint_json(const Sim *s, char *out, size_t outsz){
  int n=snprintf(out,outsz,"{\"step\":%llu,\"score\":%d,\"balls\":%d,\"turns\":%d,\"tilted\":%d,\"override\":%d,\"game_over\":%d",
    (unsigned long long)s->step,s->score,s->ball_count,s->turns_remaining,s->tilted,s->override_multiplier,s->game_over);
  size_t len=strlen(out);
  len+=(size_t)snprintf(out+len,outsz-len,"\",\"ball_list\":[");
  for(int i=0;i<s->ball_count;i++){ const Ball*b=&s->balls[i];
    if(i)len+=(size_t)snprintf(out+len,outsz-len,",");
    len+=(size_t)snprintf(out+len,outsz-len,"{\"id\":%d,\"x\":%.6g,\"y\":%.6g,\"vx\":%.6g,\"vy\":%.6g,\"active\":%d,\"captured\":%d}",
      b->id,b->pos.x,b->pos.y,b->vel.x,b->vel.y,b->active,b->captured); }
  len+=(size_t)snprintf(out+len,outsz-len,"]}");
}

void sim_recorder_enable(Sim *s, int on) {
  s->record_enabled = on ? 1 : 0;
  s->rec_count = 0;
  s->prev_left = s->prev_right = s->prev_launch = 0;
  s->prev_nl = s->prev_nr = s->prev_nu = 0;
}
void sim_recorder_take(Sim *s, int *out_count, const int **out_step, const int **out_kind) {
  *out_count = s->rec_count;
  *out_step = s->rec_step;
  *out_kind = s->rec_kind;
}
