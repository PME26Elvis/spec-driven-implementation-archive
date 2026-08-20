/* replaycheck — replay validator (doc 07.16). Verifies scene fingerprint, executes
   the replay headlessly, prints a deterministic summary, and returns non-zero on failure. */
#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include "sim.h"
#include "replay.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_hex64(uint64_t h, char *out) {
  static const char *d = "0123456789abcdef";
  for (int i = 15; i >= 0; i--) { out[i] = d[h & 0xf]; h >>= 4; }
  out[16] = 0;
}

int main(int argc, char **argv) {
  const char *scene_path = NULL, *replay_path = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--scene") && i+1 < argc) scene_path = argv[++i];
    else if (!strcmp(argv[i], "--replay") && i+1 < argc) replay_path = argv[++i];
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      printf("replaycheck --scene <scene.pbt> --replay <trace.pbr>\n"); return 0;
    }
  }
  if (!scene_path || !replay_path) { fprintf(stderr, "usage: replaycheck --scene <s> --replay <r>\n"); return 2; }

  FILE *f = fopen(scene_path, "rb");
  if (!f) { fprintf(stderr, "open scene fail\n"); return 2; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz + 1 : 1); size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);

  Scene sc; DiagList d; diag_list_init(&d);
  PbtCode c = parse_scene(buf, rd, &sc, &d);
  if (c != PBT_OK) { fprintf(stderr, "parse error: %s\n", pbt_code_name(c)); scene_free(&sc); diag_list_free(&d); free(buf); return 1; }
  uint64_t scene_hash = scene_fingerprint(&sc);

  Replay rp;
  if (replay_parse_file(replay_path, &rp) != 0) { fprintf(stderr, "replay parse error\n"); scene_free(&sc); diag_list_free(&d); free(buf); return 1; }
  int v = replay_verify(&sc, &rp);
  if (v != PBT_OK) { fprintf(stderr, "replay verify FAILED: %s\n", pbt_code_name((PbtCode)v)); replay_free(&rp); scene_free(&sc); diag_list_free(&d); free(buf); return 1; }

  Sim sim; sim_init(&sim, &sc); sim_reset(&sim, &sc);
  int left = 0, right = 0, launch = 0;
  long target = 0;
  for (int i = 0; i < rp.ev_count; i++) if (rp.ev[i].step > target) target = rp.ev[i].step;
  target += 1;
  int rc = 0;
  for (long st = 0; st < target; st++) {
    for (int i = 0; i < rp.ev_count; i++) {
      if (rp.ev[i].step != st) continue;
      switch (rp.ev[i].kind) {
        case RPL_L_FLIPPER_DOWN: left = 1; break; case RPL_L_FLIPPER_UP: left = 0; break;
        case RPL_R_FLIPPER_DOWN: right = 1; break; case RPL_R_FLIPPER_UP: right = 0; break;
        case RPL_LAUNCH_DOWN: launch = 1; break; case RPL_LAUNCH_UP: launch = 0; break;
      }
    }
    int nl = 0, nr = 0, nu = 0;
    for (int i = 0; i < rp.ev_count; i++) {
      if (rp.ev[i].step != st) continue;
      if (rp.ev[i].kind == RPL_NUDGE_LEFT) nl = 1; else if (rp.ev[i].kind == RPL_NUDGE_RIGHT) nr = 1; else if (rp.ev[i].kind == RPL_NUDGE_UP) nu = 1;
    }
    sim_input(&sim, left, right, launch, nl, nr, nu);
    if (sim_step(&sim) != 0) { fprintf(stderr, "sim error at step %ld\n", st); rc = 1; break; }
  }
  char h[17]; print_hex64(scene_hash, h);
  printf("REPLAY PASS\n");
  printf("scene_hash=%s\n", h);
  printf("final_step=%ld\n", target);
  printf("score=%d\n", sim.score);
  printf("active_ball_count=%d\n", sim.ball_count);
  printf("turns_remaining=%d\n", sim.turns_remaining);
  printf("game_state=%s\n", sim.game_over ? "GAME_OVER" : (sim.tilted ? "TILTED" : "RUNNING"));
  printf("events bumper=%ld slingshot=%ld target_hit=%ld target_dropped=%ld rollover=%ld spinner=%ld kickout_cap=%ld kickout_ej=%ld drained=%ld nudge=%ld\n",
         sim.ev_bumper_hit, sim.ev_slingshot_hit, sim.ev_target_hit, sim.ev_target_dropped, sim.ev_rollover,
         sim.ev_spinner_tick, sim.ev_kickout_capture, sim.ev_kickout_eject, sim.ev_drained, sim.ev_nudge);
  printf("runtime_fingerprint=%llu\n", (unsigned long long)sim_fingerprint(&sim));
  replay_free(&rp); sim_free(&sim); scene_free(&sc); diag_list_free(&d); free(buf);
  return rc;
}
