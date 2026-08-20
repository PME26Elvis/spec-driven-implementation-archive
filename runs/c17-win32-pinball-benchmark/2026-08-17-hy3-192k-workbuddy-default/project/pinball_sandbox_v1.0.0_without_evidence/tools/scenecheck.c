/* scenecheck — scene semantic validation CLI (doc 31.2). */
#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include "scene_validate.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: scenecheck FILE [FILE ...]\n"); return 2; }
  int overall = 0;
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb");
    if (!f) { fprintf(stderr, "%s: open fail\n", argv[a]); overall = 2; continue; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz ? sz + 1 : 1);
    size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);

    Scene sc; DiagList d; diag_list_init(&d);
    PbtCode c = parse_scene(buf, rd, &sc, &d);
    if (c != PBT_OK) {
      fprintf(stderr, "%s: PARSE %s\n", argv[a], pbt_code_name(c));
      for (size_t i = 0; i < d.count; i++)
        fprintf(stderr, "  [%d] %s line=%d %s\n", (int)i, pbt_code_name(d.items[i].code), d.items[i].line, d.items[i].message);
      scene_free(&sc); diag_list_free(&d); free(buf);
      if (c != PBT_OK) overall = 1;
      continue;
    }
    PbtCode v = scene_validate(&sc, &d);
    int errs = scene_validate_has_error(&sc, &d);
    printf("%s: PARSE_OK validate=%s\n", argv[a], pbt_code_name(v));
    for (size_t i = 0; i < d.count; i++)
      printf("  [%d/%s] %s %s\n", (int)i,
             d.items[i].severity == SEV_WARNING ? "WARN" : "ERR",
             pbt_code_name(d.items[i].code), d.items[i].message);
    if (errs) overall = 1;
    scene_free(&sc); diag_list_free(&d); free(buf);
  }
  return overall;
}
