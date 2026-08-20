/* detcompare — first-divergence comparator for checkpoint / trace outputs (doc 07.24).
   Reads two line-oriented JSON files (checkpoint JSON Lines) and reports the first
   line (step index) where they differ, with field-level detail. Non-zero on divergence. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal field extractor: for each line, compare the set of "key":"value" or key:value
   tokens. We compare normalized token strings per line for robustness. */
static int load_lines(const char *path, char ***out_lines, int *out_n) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  char *buf = malloc(sz ? sz + 1 : 1);
  size_t rd = fread(buf, 1, sz, f); buf[rd] = 0; fclose(f);
  int cap = 1024, n = 0; char **lines = malloc(sizeof(char*) * cap);
  int start = 0;
  for (long i = 0; i <= rd; i++) {
    if (i == rd || buf[i] == '\n') {
      int len = (int)(i - start);
      if (len > 0 && !(len == 1 && buf[start] == '\r')) {
        char *l = malloc(len + 1);
        memcpy(l, buf + start, len);
        if (len > 0 && l[len-1] == '\r') len--;
        l[len] = 0;
        if (n >= cap) { cap *= 2; lines = realloc(lines, sizeof(char*)*cap); }
        lines[n++] = l;
      }
      start = (int)i + 1;
    }
  }
  free(buf);
  *out_lines = lines; *out_n = n;
  return 0;
}

static int line_eq(const char *a, const char *b) {
  /* compare ignoring whitespace differences */
  int ia = 0, ib = 0;
  for (;;) {
    while (a[ia] == ' ' || a[ia] == '\t') ia++;
    while (b[ib] == ' ' || b[ib] == '\t') ib++;
    if (a[ia] == 0 && b[ib] == 0) return 1;
    if (a[ia] != b[ib]) return 0;
    ia++; ib++;
  }
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: detcompare <a.jsonl> <b.jsonl>\n"); return 2; }
  char **A, **B; int na, nb;
  if (load_lines(argv[1], &A, &na) != 0) { fprintf(stderr, "open fail: %s\n", argv[1]); return 2; }
  if (load_lines(argv[2], &B, &nb) != 0) { fprintf(stderr, "open fail: %s\n", argv[2]); return 2; }
  int mn = na < nb ? na : nb;
  int first_div = -1;
  char *da = NULL, *db = NULL;
  for (int i = 0; i < mn; i++) {
    if (!line_eq(A[i], B[i])) {
      first_div = i;
      da = A[i]; db = B[i];
      break;
    }
  }
  if (first_div < 0 && na != nb) {
    first_div = mn;
    da = na > nb ? A[mn] : (char*)"(missing)";
    db = nb > na ? B[mn] : (char*)"(missing)";
  }
  if (first_div < 0) {
    printf("DETCOMPARE MATCH: %d lines identical\n", mn);
    for (int i = 0; i < na; i++) free(A[i]); for (int i = 0; i < nb; i++) free(B[i]);
    free(A); free(B);
    return 0;
  }
  printf("DETCOMPARE DIVERGENCE at line %d (step index in sequence)\n", first_div);
  printf("  A: %s\n", da);
  printf("  B: %s\n", db);
  for (int i = 0; i < na; i++) free(A[i]); for (int i = 0; i < nb; i++) free(B[i]);
  free(A); free(B);
  return 1;
}
