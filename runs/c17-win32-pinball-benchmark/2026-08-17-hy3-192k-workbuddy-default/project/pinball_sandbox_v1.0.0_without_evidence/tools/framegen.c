/* framegen — render deterministic simulation frames to PNG (doc 10 visual evidence).
 * Uses the shared software renderer. No Win32 window / GDI drawing. */
#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include "sim.h"
#include "render.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *scene_path = NULL, *out_path = NULL;
  long steps = 0;
  double scale = 0; int width = 900;
  int checkpoint = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--scene") && i+1 < argc) scene_path = argv[++i];
    else if (!strcmp(argv[i], "--out") && i+1 < argc) out_path = argv[++i];
    else if (!strcmp(argv[i], "--steps") && i+1 < argc) steps = strtol(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--scale") && i+1 < argc) scale = strtod(argv[++i], NULL);
    else if (!strcmp(argv[i], "--width") && i+1 < argc) width = (int)strtol(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--checkpoint")) checkpoint = 1;
    else if (!strcmp(argv[i], "-h")) { printf("framegen --scene S --out O.png [--steps N] [--scale S] [--width W]\n"); return 0; }
  }
  if (!scene_path || !out_path) { fprintf(stderr, "usage: framegen --scene S --out O.png [--steps N]\n"); return 2; }

  FILE *f = fopen(scene_path, "rb");
  if (!f) { fprintf(stderr, "open fail\n"); return 2; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz+1 : 1); size_t rd = fread(buf,1,sz,f); buf[rd]=0; fclose(f);

  Scene sc; DiagList d; diag_list_init(&d);
  PbtCode c = parse_scene(buf, rd, &sc, &d);
  if (c != PBT_OK) { fprintf(stderr, "parse error: %s\n", pbt_code_name(c)); scene_free(&sc); diag_list_free(&d); free(buf); return 1; }

  if (scale <= 0) scale = (double)width / sc.world_size.x;
  int W = (int)(sc.world_size.x * scale);
  int H = (int)(sc.world_size.y * scale);

  Sim sim; sim_init(&sim, &sc); sim_reset(&sim, &sc);
  for (long st = 0; st < steps; st++) {
    if (sim_step(&sim) != 0) { fprintf(stderr, "sim error at step %ld\n", st); break; }
  }

  Framebuffer fb; fb_init(&fb, W, H);
  render_scene(&sc, &sim, &fb, scale);
  int rc = png_write_rgb(out_path, W, H, fb.pix);
  fb_free(&fb); sim_free(&sim); scene_free(&sc); diag_list_free(&d); free(buf);
  if (rc != 0) { fprintf(stderr, "png write failed: %s\n", out_path); return 1; }
  if (!checkpoint) printf("frame written: %s (%dx%d, steps=%ld)\n", out_path, W, H, steps);
  return 0;
}
