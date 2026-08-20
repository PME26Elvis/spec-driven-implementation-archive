/* simcheck — headless deterministic simulator + replay validator (doc 07).
 * Builds on the shared production core (scene_parse + sim). No Win32 window. */
#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include "sim.h"
#include "replay.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PB_PHYSICS_VERSION 1

static void print_hex64(uint64_t h, char *out) {
  static const char *d = "0123456789abcdef";
  for (int i = 15; i >= 0; i--) { out[i] = d[h & 0xf]; h >>= 4; }
  out[16] = 0;
}

static int cmp_ball_id(const void *a, const void *b) {
  int ia = ((const Ball*)a)->id, ib = ((const Ball*)b)->id;
  return (ia > ib) - (ia < ib);
}

static void emit_json(const Sim *s, uint64_t scene_hash, int steps_executed, int checkpoint) {
  char hstr[17]; print_hex64(scene_hash, hstr);
  const char *gs = s->game_over ? "GAME_OVER" : (s->tilted ? "TILTED" : "RUNNING");
  /* sort balls by id */
  Ball sorted[PB_MAX_BALLS];
  int nb = 0;
  for (int i = 0; i < s->ball_count; i++) sorted[nb++] = s->balls[i];
  qsort(sorted, nb, sizeof(Ball), cmp_ball_id);

  printf("{\"ok\":true,\"checkpoint\":%s,\"scene_hash\":\"%s\",\"physics_version\":%d,"
         "\"steps_executed\":%d,\"simulation_time\":%.10g,\"score\":%d,"
         "\"combo_multiplier\":%d,\"override_multiplier\":%d,\"active_ball_count\":%d,"
         "\"turns_remaining\":%d,\"game_state\":\"%s\",\"diagnostics\":{"
         "\"bumper_hit\":%ld,\"slingshot_hit\":%ld,\"target_hit\":%ld,\"target_dropped\":%ld,"
         "\"rollover\":%ld,\"spinner_tick\":%ld,\"kickout_capture\":%ld,\"kickout_eject\":%ld,"
         "\"drained\":%ld,\"tilt_started\":%ld,\"tilt_cleared\":%ld,\"nudge\":%ld,"
         "\"actions_executed\":%ld,\"impact_budget_used\":%ld,\"runtime_error\":%d},"
         "\"balls\":[",
         checkpoint ? "true" : "false", hstr, PB_PHYSICS_VERSION,
         steps_executed, s->sim_time, s->score, s->combo_multiplier, s->override_multiplier,
         s->ball_count, s->turns_remaining, gs,
         s->ev_bumper_hit, s->ev_slingshot_hit, s->ev_target_hit, s->ev_target_dropped,
         s->ev_rollover, s->ev_spinner_tick, s->ev_kickout_capture, s->ev_kickout_eject,
         s->ev_drained, s->ev_tilt_started, s->ev_tilt_cleared, s->ev_nudge,
         s->ev_actions_executed, s->impact_budget_used, s->error);
  for (int i = 0; i < nb; i++) {
    const Ball *b = &sorted[i];
    if (i) printf(",");
    printf("{\"id\":%d,\"x\":%.10g,\"y\":%.10g,\"vx\":%.10g,\"vy\":%.10g,\"active\":%d,\"captured\":%d}",
           b->id, b->pos.x, b->pos.y, b->vel.x, b->vel.y, b->active, b->captured);
  }
  printf("]}\n");
  fflush(stdout);
}

int main(int argc, char **argv) {
  const char *scene_path = NULL;
  const char *replay_path = NULL;
  long steps = -1;
  int checkpoint_every = 0;
  int replay_mode = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--headless") == 0 && i + 1 < argc) scene_path = argv[++i];
    else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) steps = strtol(argv[++i], NULL, 10);
    else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) { replay_path = argv[++i]; replay_mode = 1; }
    else if (strcmp(argv[i], "--checkpoint-every") == 0 && i + 1 < argc) checkpoint_every = (int)strtol(argv[++i], NULL, 10);
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("simcheck (pinball headless)\n");
      printf("  --headless <scene.pbt> --steps <N>\n");
      printf("  --headless <scene.pbt> --replay <trace.pbr> [--checkpoint-every <K>]\n");
      return 0;
    } else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 2; }
  }
  if (!scene_path) { fprintf(stderr, "missing --headless <scene>\n"); return 2; }

  FILE *f = fopen(scene_path, "rb");
  if (!f) { fprintf(stderr, "open scene fail: %s\n", scene_path); return 2; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz + 1 : 1);
  size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);

  Scene sc; DiagList d; diag_list_init(&d);
  PbtCode c = parse_scene(buf, rd, &sc, &d);
  if (c != PBT_OK) {
    fprintf(stderr, "parse error: %s\n", pbt_code_name(c));
    for (size_t i = 0; i < d.count; i++)
      fprintf(stderr, "  [%d] %s line=%d %s\n", (int)i, pbt_code_name(d.items[i].code), d.items[i].line, d.items[i].message);
    scene_free(&sc); diag_list_free(&d); free(buf);
    return 1;
  }

  uint64_t scene_hash = scene_fingerprint(&sc);

  Sim sim; sim_init(&sim, &sc); sim_reset(&sim, &sc);

  Replay rp; int rp_ok = 0;
  if (replay_mode) {
    FILE *rf = fopen(replay_path, "rb");
    if (!rf) { fprintf(stderr, "open replay fail: %s\n", replay_path); scene_free(&sc); diag_list_free(&d); free(buf); return 2; }
    fseek(rf, 0, SEEK_END); long rsz = ftell(rf); fseek(rf, 0, SEEK_SET);
    char *rbuf = malloc(rsz ? rsz + 1 : 1); size_t rrd = fread(rbuf, 1, rsz, rf); rbuf[rrd] = 0; fclose(rf);
    if (replay_parse(rbuf, &rp) != 0) {
      fprintf(stderr, "replay parse error\n"); free(rbuf); scene_free(&sc); diag_list_free(&d); free(buf); return 1;
    }
    free(rbuf);
    rp_ok = 1;
    if (rp.has_hash && rp.scene_hash != scene_hash) {
      fprintf(stderr, "replay scene_hash mismatch (replay=%llx, scene=%llx)\n",
              (unsigned long long)rp.scene_hash, (unsigned long long)scene_hash);
      scene_free(&sc); diag_list_free(&d); free(buf); free(rp.ev); return 1;
    }
  }

  long target = steps;
  if (replay_mode) {
    /* determine final step from replay events */
    target = 0;
    for (int i = 0; i < rp.ev_count; i++) if (rp.ev[i].step > target) target = rp.ev[i].step;
    if (target < 0) target = 0;
    target += 1; /* run through last event */
  }
  if (target < 0) target = 1000;

  int left = 0, right = 0, launch = 0;
  int rc = 0;
  for (long st = 0; st < target; st++) {
    if (replay_mode) {
      for (int i = 0; i < rp.ev_count; i++) {
        if (rp.ev[i].step != st) continue;
        switch (rp.ev[i].kind) {
          case 0: left = 1; break; case 1: left = 0; break;
          case 2: right = 1; break; case 3: right = 0; break;
          case 4: launch = 1; break; case 5: launch = 0; break;
        }
      }
      int nl = 0, nr = 0, nu = 0;
      for (int i = 0; i < rp.ev_count; i++) {
        if (rp.ev[i].step != st) continue;
        if (rp.ev[i].kind == 6) nl = 1; else if (rp.ev[i].kind == 7) nr = 1; else if (rp.ev[i].kind == 8) nu = 1;
      }
      sim_input(&sim, left, right, launch, nl, nr, nu);
    }
    int r = sim_step(&sim);
    if (r != 0) { fprintf(stderr, "sim error code %d at step %ld\n", r, st); rc = 1; break; }
    if (checkpoint_every > 0 && ((st + 1) % checkpoint_every == 0)) {
      emit_json(&sim, scene_hash, (int)(st + 1), 1);
    }
  }

  if (rc == 0) emit_json(&sim, scene_hash, (int)target, 0);

  if (rp_ok) replay_free(&rp);
  sim_free(&sim);
  scene_free(&sc); diag_list_free(&d); free(buf);
  return rc;
}
