#ifndef PB_REPLAY_H
#define PB_REPLAY_H

#include "scene.h"

/* Replay logical-action kinds (doc 07.5 / 07.22). Stable numbering. */
typedef enum {
  RPL_L_FLIPPER_DOWN = 0,
  RPL_L_FLIPPER_UP,
  RPL_R_FLIPPER_DOWN,
  RPL_R_FLIPPER_UP,
  RPL_LAUNCH_DOWN,
  RPL_LAUNCH_UP,
  RPL_NUDGE_LEFT,
  RPL_NUDGE_RIGHT,
  RPL_NUDGE_UP,
  RPL_KIND_COUNT
} RplKind;

typedef struct {
  int step;
  int kind;
} ReplayEvent;

typedef struct {
  uint64_t scene_hash;
  int      has_hash;
  int      seed;
  int      has_seed;
  int      physics_version;
  int      has_pv;
  ReplayEvent *ev;
  int ev_count, ev_cap;
} Replay;

/* Parse replay text. Returns 0 on success, -1 on malformed header. Caller frees with replay_free. */
int  replay_parse(const char *text, Replay *r);
int  replay_parse_file(const char *path, Replay *r);
void replay_free(Replay *r);

/* Write a replay file (PINBALL_REPLAY 1). Returns 0 on success, -1 on write failure. */
int  replay_write_file(const char *path, uint64_t scene_hash, int seed, int physics_version,
                       const ReplayEvent *ev, int ev_count);

/* Verify a replay against a loaded scene: checks scene fingerprint and required fields.
   Returns PBT_OK (0) if valid, or RPL_E_* on mismatch/error. */
int  replay_verify(const Scene *sc, const Replay *r);

#endif /* PB_REPLAY_H */
