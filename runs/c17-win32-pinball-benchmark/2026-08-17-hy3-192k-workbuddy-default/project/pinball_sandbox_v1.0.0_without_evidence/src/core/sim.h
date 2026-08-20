#ifndef PB_SIM_H
#define PB_SIM_H

#include "scene.h"
#include "vec.h"

#define PB_MAX_BALLS 256
#define PB_MAX_RT_OBJ 4096

/* Runtime ball state (doc 15). No pointer fields; deterministic layout. */
typedef struct {
  int    active;       /* 1 = in play */
  int    drained;      /* 1 = drained this step (pending removal) */
  int    captured;     /* 1 = held by a kickout */
  int    id;           /* stable runtime id, 1-based */
  Vec2   pos, vel;
  double radius, mass, restitution, friction;
  double capture_timer; /* remaining hold time if captured */
  double nudge_cooldown; /* per-ball nudge cooldown timer */
} Ball;

/* Per-object runtime state for mechanisms. */
typedef struct {
  double angle;        /* current angle (rad) */
  double ang_vel;      /* current angular velocity (rad/s) */
  int    engaged;      /* input pressed */
} FlipperRT;

typedef struct {
  double angle;        /* unwrapped cumulative angle (deg) for tick detection */
  double ang_vel;      /* rad/s */
} SpinnerRT;

typedef struct {
  int    holding;      /* ball id currently captured, 0 if none */
  double hold_timer;
  int    eject_blocked_count;
} KickoutRT;

typedef struct {
  Scene *scene;
  Ball   balls[PB_MAX_BALLS];
  int    ball_count;
  int    next_id;
  int    score;
  int    combo_multiplier;     /* current combo multiplier */
  int    override_multiplier;  /* override from events, 0 = none */
  int    override_frames;      /* remaining override frames, 0 = none */
  int    turns_remaining;
  int    balls_lost_this_turn;
  int    tilt_accum;           /* nudge tilt accumulator */
  int    tilted;               /* 1 = tilt engaged */
  double sim_time;
  uint64_t step;
  int    game_over;

  /* deterministic event/diagnostic counters (doc 07.10) */
  long   ev_bumper_hit, ev_slingshot_hit, ev_target_hit, ev_target_dropped;
  long   ev_rollover, ev_spinner_tick, ev_kickout_capture, ev_kickout_eject;
  long   ev_drained, ev_tilt_started, ev_tilt_cleared, ev_nudge;
  long   ev_actions_executed;
  long   impact_budget_used;

  /* mechanism runtime */
  FlipperRT flippers[PB_MAX_RT_OBJ];
  SpinnerRT spinners[PB_MAX_RT_OBJ];
  KickoutRT kickouts[PB_MAX_RT_OBJ];
  /* per (ball,object) cooldown timestamps for bumper/slingshot (heap-allocated) */
  double   *bumper_cd;

  int     error;       /* RT_E_* if non-finite or other runtime error */

  /* optional replay recorder (doc 07.7) */
  int     record_enabled;
  int     rec_cap, rec_count;
  int    *rec_step;    /* malloc'd */
  int    *rec_kind;
  int     prev_left, prev_right, prev_launch, prev_nl, prev_nr, prev_nu;
} Sim;

void sim_init(Sim *s, Scene *scene);
/* Free heap resources owned by a Sim (does not free the scene). */
void sim_free(Sim *s);
/* Reset to a fresh deterministic game session (doc 07.9). */
void sim_reset(Sim *s, Scene *scene);
/* Advance one fixed step (doc 15). Returns 0 on success, negative on error. */
int  sim_step(Sim *s);
/* Spawn a ball at a given spawn object id; returns ball index or -1. */
int  sim_spawn_ball(Sim *s, const char *spawn_id);

/* Input edges (doc 15.38). */
void sim_input(Sim *s, int left_flipper, int right_flipper, int launch, int nudge_left, int nudge_right, int nudge_up);

/* Replay recorder control (doc 07.7). When enabled, sim_input captures logical
   action edges into the in-memory buffer. */
void sim_recorder_enable(Sim *s, int on);
void sim_recorder_take(Sim *s, int *out_count, const int **out_step, const int **out_kind);

/* Deterministic runtime fingerprint (doc 07.17/26). Excludes pointers/time. */
uint64_t sim_fingerprint(const Sim *s);

/* JSON line for a checkpoint (doc 07.12/13). Caller prints. */
void sim_checkpoint_json(const Sim *s, char *out, size_t outsz);

#endif
