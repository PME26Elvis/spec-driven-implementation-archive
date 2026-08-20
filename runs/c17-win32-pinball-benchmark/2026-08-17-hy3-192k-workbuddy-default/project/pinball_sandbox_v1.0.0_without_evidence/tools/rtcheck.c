/* Round-trip verifier: parse -> write -> parse -> write must be idempotent
   and the canonical text must be byte-identical; fingerprints must match. */
#include "scene.h"
#include "scene_parse.h"
#include "scene_write.h"
#include "hash.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  char *b = malloc(sz+1); size_t rd=fread(b,1,(size_t)sz,f); b[rd]=0; fclose(f);
  *out_len = rd; return b;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr,"usage: rtcheck <scene.pbt>\n"); return 2; }
  for (int i=1;i<argc;i++) {
    size_t len; char *buf = read_all(argv[i], &len);
    if (!buf) { fprintf(stderr,"cannot read %s\n", argv[i]); return 2; }
    Scene s1; scene_init(&s1); DiagList d1; diag_list_init(&d1);
    PbtCode c1 = parse_scene(buf, len, &s1, &d1);
    if (c1 != PBT_OK) {
      fprintf(stderr,"%s: PARSE1 FAIL %s (%d diags)\n", argv[i], pbt_code_name(c1), (int)d1.count);
      scene_free(&s1); diag_list_free(&d1); free(buf); return 1;
    }
    char *t1 = scene_write(&s1);
    Scene s2; scene_init(&s2); DiagList d2; diag_list_init(&d2);
    PbtCode c2 = parse_scene(t1, strlen(t1), &s2, &d2);
    if (c2 != PBT_OK) {
      fprintf(stderr,"%s: PARSE2 FAIL %s\n", argv[i], pbt_code_name(c2));
      scene_free(&s1); scene_free(&s2); free(t1); free(buf); return 1;
    }
    char *t2 = scene_write(&s2);
    uint64_t fp1 = scene_fingerprint(&s1), fp2 = scene_fingerprint(&s2);
    int ok = (strcmp(t1,t2)==0) && (fp1==fp2);
    if (!ok && (strcmp(t1,t2)==0)) {
      fprintf(stderr,"  DEBUG s1 vs s2: fmt=%d/%d has_seed=%d/%d grp=%d/%d ev=%d/%d layer=%d/%d obj=%d/%d\n",
        s1.format,s2.format, s1.has_seed,s2.has_seed, s1.group_count,s2.group_count, s1.event_count,s2.event_count,
        s1.layer_count,s2.layer_count, s1.obj_count,s2.obj_count);
      if (s1.layer_count && s2.layer_count)
        fprintf(stderr,"  layer0 id=%s/%s name=%s/%s vis=%d/%d lock=%d/%d ord=%d/%d\n",
          s1.layers[0].id,s2.layers[0].id, s1.layers[0].name,s2.layers[0].name,
          s1.layers[0].visible,s2.layers[0].visible, s1.layers[0].locked,s2.layers[0].locked,
          s1.layers[0].order,s2.layers[0].order);
      for (int k=0;k<s1.event_count && k<s2.event_count;k++) {
        fprintf(stderr,"  event[%d] id=%s/%s src=%s/%s trig=%d/%d ac=%d/%d\n", k,
          s1.events[k].id,s2.events[k].id, s1.events[k].source,s2.events[k].source,
          (int)s1.events[k].trigger,(int)s2.events[k].trigger, s1.events[k].action_count,s2.events[k].action_count);
        for (int a=0;a<s1.events[k].action_count;a++){
          Action *x=&s1.events[k].actions[a], *y=&s2.events[k].actions[a];
          if (x->type!=y->type || x->amount!=y->amount || strcmp(x->spawn,y->spawn)!=0 ||
              x->count!=y->count || strcmp(x->target,y->target)!=0 || x->multiplier!=y->multiplier ||
              x->duration!=y->duration || x->dropped!=y->dropped)
            fprintf(stderr,"    act[%d] type=%d/%d amt=%ld/%ld spawn=%s/%s cnt=%d/%d tgt=%s/%s mult=%d/%d dur=%g/%g drop=%d/%d\n",
              a,(int)x->type,(int)y->type, x->amount,y->amount, x->spawn,y->spawn, x->count,y->count,
              x->target,y->target, x->multiplier,y->multiplier, x->duration,y->duration, x->dropped,y->dropped);
        }
      }
      for (int k=0;k<s1.obj_count && k<s2.obj_count;k++){
        uint64_t a; fnv1a64_init(&a);
        fnv1a64_update(&a, &s1.objects[k].id, strlen(s1.objects[k].id));
        fnv1a64_update(&a, &s1.objects[k].u, sizeof(s1.objects[k].u));
        a = fnv1a64_final(a);
        uint64_t b; fnv1a64_init(&b);
        fnv1a64_update(&b, &s2.objects[k].id, strlen(s2.objects[k].id));
        fnv1a64_update(&b, &s2.objects[k].u, sizeof(s2.objects[k].u));
        b = fnv1a64_final(b);
        if (a!=b) fprintf(stderr,"  obj[%d] type=%s id=%s union-hash %016llx != %016llx\n",
          k, obj_type_name(s1.objects[k].type), s1.objects[k].id,
          (unsigned long long)a,(unsigned long long)b);
      }
    }
    if (!ok) {
      fprintf(stderr,"%s: ROUNDTRIP MISMATCH (text_equal=%d fp_match=%llu==%llu)\n",
              argv[i], strcmp(t1,t2)==0, (unsigned long long)fp1, (unsigned long long)fp2);
      /* dump diff */
      FILE *df=fopen("rtcheck.diff.txt","w");
      if (df){ fprintf(df,"=== WRITE1 ===\n%s\n=== WRITE2 ===\n%s\n", t1, t2); fclose(df); }
      scene_free(&s1); scene_free(&s2); free(t1); free(t2); free(buf); return 1;
    }
    printf("%s: ROUNDTRIP OK (obj=%d ev=%d layers=%d fp=%016llx)\n",
           argv[i], s1.obj_count, s1.event_count, s1.layer_count, (unsigned long long)fp1);
    scene_free(&s1); scene_free(&s2); free(t1); free(t2); free(buf);
  }
  return 0;
}
