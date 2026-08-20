#include "types.h"
#include "scene.h"
#include "scene_parse.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: pcheck FILE\n"); return 2; }
  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "open fail\n"); return 2; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz + 1 : 1);
  size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);
  Scene s; DiagList d; diag_list_init(&d);
  PbtCode c = parse_scene(buf, rd, &s, &d);
  printf("%s\n", pbt_code_name(c));
  for (size_t i = 0; i < d.count; i++)
    printf("  [%d] %s line=%d %s\n", (int)i, pbt_code_name(d.items[i].code), d.items[i].line, d.items[i].message);
  scene_free(&s); diag_list_free(&d); free(buf);
  return 0;
}
