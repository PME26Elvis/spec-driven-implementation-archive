/* replay.c — replay parse / write / verify (doc 07). */
#include "replay.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int kind_from_name(const char *a) {
  if (!strcmp(a, "LEFT_FLIPPER_DOWN")) return RPL_L_FLIPPER_DOWN;
  if (!strcmp(a, "LEFT_FLIPPER_UP"))   return RPL_L_FLIPPER_UP;
  if (!strcmp(a, "RIGHT_FLIPPER_DOWN"))return RPL_R_FLIPPER_DOWN;
  if (!strcmp(a, "RIGHT_FLIPPER_UP"))  return RPL_R_FLIPPER_UP;
  if (!strcmp(a, "LAUNCH_DOWN"))       return RPL_LAUNCH_DOWN;
  if (!strcmp(a, "LAUNCH_UP"))         return RPL_LAUNCH_UP;
  if (!strcmp(a, "NUDGE_LEFT"))        return RPL_NUDGE_LEFT;
  if (!strcmp(a, "NUDGE_RIGHT"))       return RPL_NUDGE_RIGHT;
  if (!strcmp(a, "NUDGE_UP"))          return RPL_NUDGE_UP;
  return -1;
}

static const char *kind_to_name(int k) {
  switch (k) {
    case RPL_L_FLIPPER_DOWN: return "LEFT_FLIPPER_DOWN";
    case RPL_L_FLIPPER_UP:   return "LEFT_FLIPPER_UP";
    case RPL_R_FLIPPER_DOWN: return "RIGHT_FLIPPER_DOWN";
    case RPL_R_FLIPPER_UP:   return "RIGHT_FLIPPER_UP";
    case RPL_LAUNCH_DOWN:    return "LAUNCH_DOWN";
    case RPL_LAUNCH_UP:      return "LAUNCH_UP";
    case RPL_NUDGE_LEFT:     return "NUDGE_LEFT";
    case RPL_NUDGE_RIGHT:    return "NUDGE_RIGHT";
    case RPL_NUDGE_UP:       return "NUDGE_UP";
    default: return "UNKNOWN";
  }
}

void replay_free(Replay *r) {
  if (r->ev) free(r->ev);
  r->ev = NULL; r->ev_count = r->ev_cap = 0;
}

int replay_parse(const char *text, Replay *r) {
  memset(r, 0, sizeof(*r));
  r->ev_cap = 256; r->ev = malloc(sizeof(ReplayEvent) * r->ev_cap);
  const char *p = text;
  int seen_header = 0;
  int last_step = -1;
  while (*p) {
    while (*p == '\r' || *p == '\n') p++;
    if (!*p) break;
    const char *sol = p;
    while (*p && *p != '\n') p++;
    int len = (int)(p - sol);
    char buf[1024]; int bl = 0;
    for (int i = 0; i < len && bl < (int)sizeof(buf) - 1; i++) {
      char c = sol[i];
      if (c == '\r') continue;
      buf[bl++] = c;
    }
    buf[bl] = 0;
    while (bl > 0 && (buf[bl-1]==' '||buf[bl-1]=='\t')) buf[--bl]=0;
    if (buf[0] == '#' || buf[0] == 0) continue;
    if (!seen_header) {
      if (strncmp(buf, "PINBALL_REPLAY", 14) == 0) { seen_header = 1; continue; }
      return -1;
    }
    if (strncmp(buf, "scene_hash", 10) == 0) {
      const char *eq = strchr(buf, '='); if (eq) { r->scene_hash = strtoull(eq+1, NULL, 16); r->has_hash = 1; }
    } else if (strncmp(buf, "seed", 4) == 0) {
      const char *eq = strchr(buf, '='); if (eq) { r->seed = (int)strtol(eq+1, NULL, 10); r->has_seed = 1; }
    } else if (strncmp(buf, "physics_version", 15) == 0) {
      const char *eq = strchr(buf, '='); if (eq) { r->physics_version = (int)strtol(eq+1, NULL, 10); r->has_pv = 1; }
    } else if (strncmp(buf, "STEP", 4) == 0) {
      int step; char action[64];
      if (sscanf(buf, "STEP %d %63s", &step, action) == 2) {
        int kind = kind_from_name(action);
        if (kind < 0) return -1; /* unknown action */
        if (step < last_step) return -1; /* non-increasing step index */
        last_step = step;
        if (r->ev_count >= r->ev_cap) { r->ev_cap *= 2; r->ev = realloc(r->ev, sizeof(ReplayEvent)*r->ev_cap); }
        r->ev[r->ev_count].step = step; r->ev[r->ev_count].kind = kind; r->ev_count++;
      }
    }
  }
  if (!seen_header) return -1;
  return 0;
}

int replay_parse_file(const char *path, Replay *r) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz + 1 : 1);
  size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);
  int rc = replay_parse(buf, r);
  free(buf);
  return rc;
}

int replay_write_file(const char *path, uint64_t scene_hash, int seed, int physics_version,
                      const ReplayEvent *ev, int ev_count) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  char hx[17]; static const char *d = "0123456789abcdef";
  for (int i = 15; i >= 0; i--) { hx[i] = d[scene_hash & 0xf]; scene_hash >>= 4; } hx[16] = 0;
  if (fprintf(f, "PINBALL_REPLAY 1\n") < 0) { fclose(f); return -1; }
  if (fprintf(f, "scene_hash = %s\n", hx) < 0) { fclose(f); return -1; }
  if (fprintf(f, "seed = %d\n", seed) < 0) { fclose(f); return -1; }
  if (fprintf(f, "physics_version = %d\n", physics_version) < 0) { fclose(f); return -1; }
  for (int i = 0; i < ev_count; i++) {
    if (fprintf(f, "STEP %d %s\n", ev[i].step, kind_to_name(ev[i].kind)) < 0) { fclose(f); return -1; }
  }
  if (fclose(f) != 0) return -1;
  return 0;
}

int replay_verify(const Scene *sc, const Replay *r) {
  if (!r->has_hash) return RPL_E_FORMAT;
  uint64_t h = scene_fingerprint(sc);
  if (h != r->scene_hash) return RPL_E_SCENE_MISMATCH;
  if (r->has_pv && r->physics_version != 1) return RPL_E_VERSION;
  return PBT_OK;
}
