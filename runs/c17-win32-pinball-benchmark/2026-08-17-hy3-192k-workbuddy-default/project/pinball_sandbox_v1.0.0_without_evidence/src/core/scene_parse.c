#include "scene.h"
#include "scene_parse.h"
#include "vec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#define PB_LINE_MAX     1048576   /* 1 MiB hard physical line limit */
#define PB_TOKEN_MAX    4096      /* identifier / string hard limit */
#define PB_ID_HARD      63        /* doc 17.12 */

/* ---------------- low-level checks ---------------- */
static int utf8_valid(const unsigned char *s, size_t n) {
  size_t i = 0;
  while (i < n) {
    unsigned char c = s[i];
    if (c < 0x80) { i++; }
    else if ((c >> 5) == 0x6) {
      if (i + 1 >= n || (s[i+1] & 0xC0) != 0x80) return 0;
      i += 2;
    } else if ((c >> 4) == 0xE) {
      if (i + 2 >= n || (s[i+1]&0xC0)!=0x80 || (s[i+2]&0xC0)!=0x80) return 0;
      i += 3;
    } else if ((c >> 3) == 0x1E) {
      if (i + 3 >= n || (s[i+1]&0xC0)!=0x80 || (s[i+2]&0xC0)!=0x80 || (s[i+3]&0xC0)!=0x80) return 0;
      i += 4;
    } else return 0;
  }
  return 1;
}

/* ---------------- numeric parsing ---------------- */
static PbtCode parse_i64(const char *t, long *out) {
  if (!t || !*t) return PBT_E_TYPE;
  errno = 0;
  char *end;
  long v = strtol(t, &end, 10);
  if (end == t) return PBT_E_TYPE;
  if (*end != 0) {
    /* reject decimals/exponents for int fields */
    return PBT_E_TYPE;
  }
  if (errno == ERANGE) return PBT_E_NUMERIC_OVERFLOW;
  *out = v;
  return PBT_OK;
}
static PbtCode parse_f64(const char *t, double *out) {
  if (!t || !*t) return PBT_E_TYPE;
  /* reject nonfinite spellings */
  if (strcmp(t, "nan")==0 || strcmp(t,"NaN")==0 || strcmp(t,"inf")==0 ||
      strcmp(t,"Inf")==0 || strcmp(t,"infinity")==0 || strcmp(t,"Infinity")==0)
    return PBT_E_NONFINITE;
  char *end;
  double v = strtod(t, &end);
  if (end == t) return PBT_E_TYPE;
  if (*end != 0) return PBT_E_TYPE;
  if (isnan(v)) return PBT_E_NONFINITE;
  if (isinf(v)) return PBT_E_NUMERIC_OVERFLOW;
  *out = v;
  return PBT_OK;
}
static PbtCode parse_bool(const char *t, int *out) {
  if (strcmp(t, "true")==0) { *out = 1; return PBT_OK; }
  if (strcmp(t, "false")==0) { *out = 0; return PBT_OK; }
  return PBT_E_TYPE;
}
/* parse a raw identifier (unquoted, ASCII) */
static PbtCode parse_id(const char *t, char *out, int maxlen) {
  if (!t || !*t) return PBT_E_TYPE;
  if (strlen(t) > (size_t)maxlen) return PBT_E_TOKEN_TOO_LONG;
  /* identifiers: ASCII, no spaces/control, no quotes */
  for (const char *p = t; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x21 || c == '"' || c == '#') return PBT_E_TYPE;
  }
  strcpy(out, t);
  return PBT_OK;
}
/* quoted string with escapes */
static PbtCode parse_string(const char *t, char *out, int maxlen, int *outlen) {
  if (t[0] != '"') return PBT_E_TYPE;
  size_t i = 1, o = 0;
  while (t[i] && t[i] != '"') {
    if (o + 1 > (size_t)maxlen) return PBT_E_TOKEN_TOO_LONG;
    if (t[i] == '\\') {
      i++;
      char c = t[i];
      char m;
      switch (c) {
        case '\\': m = '\\'; break;
        case '"':  m = '"';  break;
        case 'n':  m = '\n'; break;
        case 't':  m = '\t'; break;
        default:   return PBT_E_STRING_ESCAPE;
      }
      out[o++] = m; i++;
    } else {
      out[o++] = t[i++];
    }
  }
  if (t[i] != '"') return PBT_E_STRING_UNTERMINATED;
  /* ensure no following content */
  if (t[i+1] != 0) return PBT_E_SYNTAX;
  out[o] = 0;
  if (outlen) *outlen = (int)o;
  return PBT_OK;
}
/* vector2 (x, y) and tuple4 (x, y, w, h) */
static PbtCode parse_vec2(const char *t, Vec2 *out) {
  /* expect ( a , b ) */
  if (t[0] != '(') return PBT_E_TYPE;
  const char *p = t + 1;
  char buf[PB_TOKEN_MAX]; int bi = 0;
  double vals[2]; int n = 0;
  int depth = 0;
  while (1) {
    char c = *p;
    if (c == 0) break;
    if (c == '(') { depth++; if (depth>1) return PBT_E_TYPE; p++; continue; }
    if (c == ')') {
      if (bi) { buf[bi]=0; if (parse_f64(buf,&vals[n++])!=PBT_OK) return PBT_E_TYPE; bi=0; }
      p++;
      if (*p != 0) return PBT_E_TYPE;
      break;
    }
    if (c == ',') {
      if (!bi) return PBT_E_TYPE;
      buf[bi]=0; if (parse_f64(buf,&vals[n++])!=PBT_OK) return PBT_E_TYPE; bi=0; p++;
      /* skip spaces */
      while (*p==' '||*p=='\t') p++;
      continue;
    }
    if (c==' '||c=='\t') { p++; continue; }
    if (bi >= PB_TOKEN_MAX-1) return PBT_E_TOKEN_TOO_LONG;
    buf[bi++] = c; p++;
  }
  if (n != 2) return PBT_E_TYPE;
  out->x = vals[0]; out->y = vals[1];
  return PBT_OK;
}
static PbtCode parse_tuple4(const char *t, double *x, double *y, double *w, double *h) {
  if (t[0] != '(') return PBT_E_TYPE;
  const char *p = t + 1;
  char buf[PB_TOKEN_MAX]; int bi = 0;
  double vals[4]; int n = 0;
  while (1) {
    char c = *p;
    if (c == 0) break;
    if (c == ')') {
      if (bi) { buf[bi]=0; if (parse_f64(buf,&vals[n++])!=PBT_OK) return PBT_E_TYPE; bi=0; }
      p++; if (*p!=0) return PBT_E_TYPE; break;
    }
    if (c == ',') {
      if (!bi) return PBT_E_TYPE;
      buf[bi]=0; if (parse_f64(buf,&vals[n++])!=PBT_OK) return PBT_E_TYPE; bi=0; p++;
      while (*p==' '||*p=='\t') p++;
      continue;
    }
    if (c==' '||c=='\t') { p++; continue; }
    if (bi >= PB_TOKEN_MAX-1) return PBT_E_TOKEN_TOO_LONG;
    buf[bi++] = c; p++;
  }
  if (n != 4) return PBT_E_TYPE;
  *x=vals[0]; *y=vals[1]; *w=vals[2]; *h=vals[3];
  return PBT_OK;
}

/* ---------------- line splitting ---------------- */
typedef struct { const char *start; int len; int line_no; } Line;
typedef struct { Line *items; int count; int cap; } LineList;

static void push_line(LineList *L, const char *s, int len, int no) {
  if (L->count == L->cap) {
    int nc = L->cap ? L->cap*2 : 256;
    L->items = realloc(L->items, nc*sizeof(Line));
    L->cap = nc;
  }
  L->items[L->count].start = s; L->items[L->count].len = len; L->items[L->count].line_no = no;
  L->count++;
}

/* trim spaces and strip trailing comment; returns 0 if blank/comment-only */
static const char *trim(const char *s, int len, int *outlen) {
  int a = 0, b = len;
  while (a < b && (s[a]==' '||s[a]=='\t')) a++;
  while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r')) b--;
  *outlen = b - a;
  return s + a;
}

/* ---------------- field seen tracking ---------------- */
typedef struct { char keys[48][PB_ID_MAX]; int n; } Seen;
static void seen_add(Seen *s, const char *k) {
  if (s->n >= 48) return;
  for (int i=0;i<s->n;i++) if (strcmp(s->keys[i],k)==0) return; /* dup detection handled outside */
  strcpy(s->keys[s->n++], k);
}
static int seen_has(Seen *s, const char *k) {
  for (int i=0;i<s->n;i++) if (strcmp(s->keys[i],k)==0) return 1;
  return 0;
}

/* ---------------- main parse ---------------- */
static PbtCode g_first = PBT_OK;
static void report(DiagList *d, PbtCode code, int line, const char *msg) {
  if (code == PBT_OK) return;
  if (g_first == PBT_OK) g_first = code;
  diag_push_code(d, code, line, msg);
}

/* field parse helpers: parse and propagate the EXACT error code (doc 31) */
#define PSET_DBL(f)   do { double _v; PbtCode _rc = parse_f64(val,&_v); if (_rc==PBT_OK) (f)=_v; else report(diag,_rc,ln,key); } while(0);
#define PSET_INT(f)   do { long _v; PbtCode _rc = parse_i64(val,&_v); if (_rc==PBT_OK) (f)=(int)_v; else report(diag,_rc,ln,key); } while(0);
#define PSET_BOOL(f)  do { int _v; PbtCode _rc = parse_bool(val,&_v); if (_rc==PBT_OK) (f)=_v; else report(diag,_rc,ln,key); } while(0);
#define PSET_VEC(f)   do { Vec2 _v; PbtCode _rc = parse_vec2(val,&_v); if (_rc==PBT_OK) (f)=_v; else report(diag,_rc,ln,key); } while(0);
#define PSET_TUP4(x,y,w,h) do { double _x,_y,_w,_h; PbtCode _rc=parse_tuple4(val,&_x,&_y,&_w,&_h); if(_rc==PBT_OK){(x)=_x;(y)=_y;(w)=_w;(h)=_h;} else report(diag,_rc,ln,key);} while(0);
#define PSET_STR(f)   do { PbtCode _rc = parse_string(val,(f),PB_NAME_MAX,NULL); if(_rc!=PBT_OK) report(diag,_rc,ln,key); } while(0);
#define PSET_ID(f)    do { PbtCode _rc = parse_id(val,(f),PB_ID_MAX); if(_rc!=PBT_OK) report(diag,_rc,ln,key); } while(0);

static int is_ident_char(char c) {
  if (c>='A'&&c<='Z') return 1;
  if (c>='a'&&c<='z') return 1;
  if (c>='0'&&c<='9') return 1;
  return c=='_';
}

PbtCode parse_scene(const char *text, size_t len, Scene *out, DiagList *diag) {
  g_first = PBT_OK;
  if (diag) diag->count = 0;
  scene_init(out); /* ensure out is valid even on early returns so callers can free safely */
  if (len == 0) { report(diag, PBT_E_EMPTY, 0, "empty input"); return PBT_E_EMPTY; }
  /* NUL check */
  for (size_t i = 0; i < len; i++) {
    if (text[i] == 0) {
      int ln = 1; for (size_t j=0;j<i;j++) if (text[j]=='\n') ln++;
      report(diag, PBT_E_NUL, ln, "embedded NUL"); return PBT_E_NUL;
    }
  }
  /* UTF-8 */
  if (!utf8_valid((const unsigned char*)text, len)) {
    report(diag, PBT_E_UTF8, 1, "invalid UTF-8"); return PBT_E_UTF8;
  }
  /* file size hard limit handled by caller; here soft cap None */

  /* split lines */
  LineList L = {0};
  int line_no = 1;
  const char *p = text;
  const char *sol = text;
  for (size_t i = 0; i <= len; i++) {
    if (i == len || text[i] == '\n') {
      int llen = (int)(&text[i] - sol);
      if (llen > PB_LINE_MAX) {
        report(diag, PBT_E_LINE_TOO_LONG, line_no, "line too long");
        free(L.items);
        return PBT_E_LINE_TOO_LONG;
      }
      push_line(&L, sol, llen, line_no);
      sol = &text[i] + 1;
      line_no++;
    }
  }

  Seen seen; memset(&seen, 0, sizeof(seen));
  unsigned tf = 0; /* bitmask of required table fields seen */

  /* find header (first non-blank, non-comment) */
  int hdr = -1;
  for (int i = 0; i < L.count; i++) {
    int tl; const char *t = trim(L.items[i].start, L.items[i].len, &tl);
    if (tl == 0) continue;
    if (t[0] == '#') continue;
    hdr = i; break;
  }
  if (hdr < 0) { report(diag, PBT_E_EMPTY, 0, "no content"); free(L.items); return PBT_E_EMPTY; }

  int tl; const char *t = trim(L.items[hdr].start, L.items[hdr].len, &tl);
  /* header must be "PINBALL_TABLE <version>" */
  {
    const char *sp = t;
    while (*sp==' '||*sp=='\t') sp++;
    if (strncmp(sp, "PINBALL_TABLE", 13) != 0) {
      report(diag, PBT_E_HEADER, L.items[hdr].line_no, "bad header"); free(L.items); return PBT_E_HEADER;
    }
    sp += 13;
    while (*sp==' '||*sp=='\t') sp++;
    int ver = 0;
    if (sscanf(sp, "%d", &ver) != 1) {
      report(diag, PBT_E_HEADER, L.items[hdr].line_no, "bad header version"); free(L.items); return PBT_E_HEADER;
    }
    if (ver == 1) out->format = 1;
    else if (ver == 2) out->format = 2;
    else { report(diag, PBT_E_VERSION_UNSUPPORTED, L.items[hdr].line_no, "unsupported version"); free(L.items); return PBT_E_VERSION_UNSUPPORTED; }
  }

  /* count/limit caps */
  int obj_limit = 10000, ev_limit = 10000, grp_limit = 2000, layer_limit = 64;

  int i = hdr + 1;
  char cur_section = 0; /* 0 none, 't' table, 'l' layer, 'o' object, 'g' group, 'e' event */
  Obj cur_obj; memset(&cur_obj, 0, sizeof(cur_obj));
  Layer cur_layer; memset(&cur_layer, 0, sizeof(cur_layer));
  Group cur_group; memset(&cur_group, 0, sizeof(cur_group));
  Event cur_event; memset(&cur_event, 0, sizeof(cur_event));

  for (; i < L.count; i++) {
    int ln = L.items[i].line_no;
    const char *line = L.items[i].start;
    int llen = L.items[i].len;
    const char *tlp; int tll = 0;
    tlp = trim(line, llen, &tll);
    if (tll == 0 || tlp[0] == '#') continue;

    /* section header? */
    if (tlp[0] == '[') {
      /* commit the section currently being built before starting a new one */
      if (cur_section=='l') scene_add_layer(out, &cur_layer);
      else if (cur_section=='o') {
        if (out->obj_count < obj_limit) scene_add_object(out, &cur_obj);
        else report(diag,PBT_E_LIMIT_OBJECTS,0,"object limit");
      }
      else if (cur_section=='g') {
        if (out->group_count < grp_limit) scene_add_group(out, &cur_group);
        else report(diag,PBT_E_LIMIT_GROUPS,0,"group limit");
      }
      else if (cur_section=='e') {
        if (out->event_count < ev_limit) scene_add_event(out, &cur_event);
        else report(diag,PBT_E_LIMIT_EVENTS,0,"event limit");
      }
      /* parse [type id] or [table] or [layer id] or [object TYPE ID] or [group id] or [event id] */
      const char *end = tlp + tll;
      const char *q = tlp + 1;
      char head[256]; int hi = 0;
      while (q < end && *q != ']' && hi < 255) head[hi++] = *q++;
      head[hi] = 0;
      if (*q != ']') { report(diag, PBT_E_SYNTAX, ln, "malformed section"); continue; }
      /* tokenize head by spaces; detect oversized tokens (doc 17.12) */
      char tok[4][PB_ID_MAX]; int nt = 0; int tok_overflow = 0;
      const char *w = head;
      while (*w && nt < 4) {
        while (*w==' '||*w=='\t') w++;
        if (!*w) break;
        int k=0; while (*w && *w!=' ' && *w!='\t' && k<PB_ID_HARD) tok[nt][k++]=*w++;
        tok[nt][k]=0;
        if (*w && *w!=' ' && *w!='\t') { tok_overflow=1; while (*w && *w!=' ' && *w!='\t') w++; }
        nt++;
      }
      if (tok_overflow) report(diag, PBT_E_TOKEN_TOO_LONG, ln, "token too long");
      if (nt == 1 && strcmp(tok[0],"table")==0) { cur_section='t'; seen.n=0; continue; }
      if (nt == 2 && strcmp(tok[0],"layer")==0) {
        cur_section='l'; memset(&cur_layer,0,sizeof(cur_layer)); seen.n=0;
        if (strlen(tok[1])>PB_ID_HARD) report(diag,PBT_E_TOKEN_TOO_LONG,ln,"layer id too long");
        strcpy(cur_layer.id, tok[1]);
        cur_layer.visible=1; cur_layer.locked=0; cur_layer.order=out->layer_count;
        if (scene_find_layer(out, cur_layer.id)) report(diag,PBT_E_DUP_ID,ln,"duplicate layer id");
        continue;
      }
      if (nt == 3 && strcmp(tok[0],"object")==0) {
        cur_section='o'; memset(&cur_obj,0,sizeof(cur_obj)); seen.n=0;
        ObjType ot = obj_type_from_name(tok[1]);
        if (ot==OBJ_COUNT) { report(diag,PBT_E_UNKNOWN_OBJECT,ln,"unknown object type"); cur_section=0; continue; }
        if (strlen(tok[2])>PB_ID_HARD) report(diag,PBT_E_TOKEN_TOO_LONG,ln,"object id too long");
        cur_obj.type=ot; strcpy(cur_obj.id, tok[2]); cur_obj.locked=0;
        if (scene_find_object(out, cur_obj.id)) report(diag,PBT_E_DUP_ID,ln,"duplicate object id");
        /* defaults for nested types */
        if (ot==OBJ_BALL_SPAWN){ cur_obj.u.spawn.enabled=1; }
        else if (ot==OBJ_WALL||ot==OBJ_RAMP||ot==OBJ_SLINGSHOT||ot==OBJ_ONE_WAY_GATE||ot==OBJ_DROP_TARGET||ot==OBJ_STANDUP_TARGET||ot==OBJ_ROLLOVER){ cur_obj.u.cap.enabled=1; cur_obj.u.cap.thickness=10; cur_obj.u.cap.restitution=out->default_ball_restitution; cur_obj.u.cap.friction=out->default_ball_friction; }
        else if (ot==OBJ_BUMPER){ cur_obj.u.bumper.enabled=1; cur_obj.u.bumper.radius=30; cur_obj.u.bumper.restitution=1.0; cur_obj.u.bumper.friction=0.0; cur_obj.u.bumper.impulse=500; cur_obj.u.bumper.base_score=100; cur_obj.u.bumper.cooldown=0.10; }
        else if (ot==OBJ_FLIPPER){ cur_obj.u.flipper.enabled=1; cur_obj.u.flipper.length=120; cur_obj.u.flipper.thickness=20; cur_obj.u.flipper.rest_angle_deg=-30; cur_obj.u.flipper.active_angle_deg=30; cur_obj.u.flipper.engage_speed_deg_s=900; cur_obj.u.flipper.return_speed_deg_s=900; cur_obj.u.flipper.restitution=0.5; cur_obj.u.flipper.friction=0.2; cur_obj.u.flipper.input=0; }
        else if (ot==OBJ_SENSOR||ot==OBJ_DRAIN){ cur_obj.u.sensor.enabled=1; cur_obj.u.sensor.debug_visible=0; }
        else if (ot==OBJ_LAUNCHER){ cur_obj.u.launcher.enabled=1; cur_obj.u.launcher.min_speed=400; cur_obj.u.launcher.max_speed=1800; cur_obj.u.launcher.full_charge_time=1.2; cur_obj.u.launcher.charge_curve=0; cur_obj.u.launcher.direction=v2(0,-1); }
        else if (ot==OBJ_SPINNER){ cur_obj.u.spinner.enabled=1; cur_obj.u.spinner.half_length=60; cur_obj.u.spinner.thickness=8; cur_obj.u.spinner.rest_angle_deg=0; cur_obj.u.spinner.angular_damping=1.0; cur_obj.u.spinner.inertia=1.0; cur_obj.u.spinner.restitution=0.5; cur_obj.u.spinner.friction=0.2; cur_obj.u.spinner.score_per_tick=10; cur_obj.u.spinner.tick_angle_deg=30; }
        else if (ot==OBJ_KICKOUT){ cur_obj.u.kickout.enabled=1; cur_obj.u.kickout.capture_radius=24; cur_obj.u.kickout.eject_speed=900; cur_obj.u.kickout.hold_time=0.75; cur_obj.u.kickout.base_score=50; cur_obj.u.kickout.eject_direction=v2(0,-1); }
        continue;
      }
      if (nt == 2 && strcmp(tok[0],"group")==0) {
        cur_section='g'; memset(&cur_group,0,sizeof(cur_group)); seen.n=0;
        if (strlen(tok[1])>PB_ID_HARD) report(diag,PBT_E_TOKEN_TOO_LONG,ln,"group id too long");
        strcpy(cur_group.id, tok[1]);
        if (scene_find_group(out, cur_group.id)) report(diag,PBT_E_DUP_ID,ln,"duplicate group id");
        continue;
      }
      if (nt == 2 && strcmp(tok[0],"event")==0) {
        cur_section='e'; memset(&cur_event,0,sizeof(cur_event)); seen.n=0;
        for (int _a=0;_a<PB_MAX_ACTIONS;_a++) cur_event.actions[_a].type=ACT_COUNT; /* sentinel for "unset" */
        if (strlen(tok[1])>PB_ID_HARD) report(diag,PBT_E_TOKEN_TOO_LONG,ln,"event id too long");
        strcpy(cur_event.id, tok[1]);
        if (scene_find_event(out, cur_event.id)) report(diag,PBT_E_DUP_ID,ln,"duplicate event id");
        continue;
      }
      report(diag, PBT_E_UNKNOWN_SECTION, ln, "unknown section");
      cur_section = 0;
      continue;
    }

    /* key = value */
    const char *eq = memchr(tlp, '=', tll);
    if (!eq) { report(diag, PBT_E_SYNTAX, ln, "malformed key/value"); continue; }
    int klen = (int)(eq - tlp);
    int vlen = tll - klen - 1;
    const char *vp = eq + 1;
    while (klen>0 && (tlp[klen-1]==' '||tlp[klen-1]=='\t')) klen--;
    while (vlen>0 && (*vp==' '||*vp=='\t')) { vp++; vlen--; }
    char key[PB_ID_MAX]; char val[PB_TOKEN_MAX];
    if (klen >= PB_ID_MAX) { report(diag,PBT_E_TOKEN_TOO_LONG,ln,"key too long"); continue; }
    memcpy(key, tlp, klen); key[klen]=0;
    if (vlen >= PB_TOKEN_MAX) { report(diag,PBT_E_TOKEN_TOO_LONG,ln,"value too long"); continue; }
    memcpy(val, vp, vlen); val[vlen]=0;

    /* dispatch by section */
    if (cur_section) {
      if (seen_has(&seen, key)) { report(diag,PBT_E_DUP_KEY,ln,"duplicate key"); continue; }
      seen_add(&seen, key);
    }
    if (cur_section=='t') {
      if (strcmp(key,"name")==0) { PSET_STR(out->name); tf|=1u<<0; }
      else if (strcmp(key,"world_size")==0) { PSET_VEC(out->world_size); tf|=1u<<1; }
      else if (strcmp(key,"gravity")==0) { PSET_VEC(out->gravity); tf|=1u<<2; }
      else if (strcmp(key,"max_active_balls")==0) { PSET_INT(out->max_active_balls); tf|=1u<<3; }
      else if (strcmp(key,"starting_turns")==0) { PSET_INT(out->starting_turns); tf|=1u<<4; }
      else if (strcmp(key,"default_ball_radius")==0) { PSET_DBL(out->default_ball_radius); tf|=1u<<5; }
      else if (strcmp(key,"default_ball_mass")==0) { PSET_DBL(out->default_ball_mass); tf|=1u<<6; }
      else if (strcmp(key,"default_ball_restitution")==0) { PSET_DBL(out->default_ball_restitution); tf|=1u<<7; }
      else if (strcmp(key,"default_ball_friction")==0) { PSET_DBL(out->default_ball_friction); tf|=1u<<8; }
      else if (strcmp(key,"default_ball_damping")==0) { PSET_DBL(out->default_ball_damping); tf|=1u<<9; }
      else if (strcmp(key,"default_ball_max_speed")==0) { PSET_DBL(out->default_ball_max_speed); tf|=1u<<10; }
      else if (strcmp(key,"scene_seed")==0){ char *e; unsigned long long v=strtoull(val,&e,10); if(*e==0) out->scene_seed=v; else report(diag,PBT_E_TYPE,ln,key); out->has_seed=1; }
      else if (strcmp(key,"nudge_impulse")==0) PSET_DBL(out->nudge_impulse)
      else if (strcmp(key,"nudge_tilt_cost")==0) PSET_DBL(out->nudge_tilt_cost)
      else if (strcmp(key,"tilt_threshold")==0) PSET_DBL(out->tilt_threshold)
      else if (strcmp(key,"tilt_decay_per_second")==0) PSET_DBL(out->tilt_decay_per_second)
      else if (strcmp(key,"nudge_cooldown")==0) PSET_DBL(out->nudge_cooldown)
      else { report(diag, PBT_E_SYNTAX, ln, "unknown table field"); }
    }
    else if (cur_section=='l') {
      if (strcmp(key,"name")==0) PSET_STR(cur_layer.name)
      else if (strcmp(key,"visible")==0) PSET_BOOL(cur_layer.visible)
      else if (strcmp(key,"locked")==0) PSET_BOOL(cur_layer.locked)
      else if (strcmp(key,"order")==0) PSET_INT(cur_layer.order)
      else report(diag,PBT_E_SYNTAX,ln,"unknown layer field");
    }
    else if (cur_section=='o') {
      ObjType ot = cur_obj.type;
      if (strcmp(key,"layer")==0) PSET_ID(cur_obj.layer)
      else if (strcmp(key,"locked")==0) PSET_BOOL(cur_obj.locked)
      else {
        switch (ot) {
          case OBJ_BALL_SPAWN:
            if (strcmp(key,"position")==0) PSET_VEC(cur_obj.u.spawn.position)
            else if (strcmp(key,"initial_velocity")==0) PSET_VEC(cur_obj.u.spawn.initial_velocity)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.spawn.enabled)
            else if (strcmp(key,"ball_radius")==0){double v;PbtCode _rc=parse_f64(val,&v);if(_rc==PBT_OK){cur_obj.u.spawn.ball_radius=v;cur_obj.u.spawn.has_ball_radius=1;}else report(diag,_rc,ln,key);}
            else report(diag,PBT_E_SYNTAX,ln,"unknown spawn field");
            break;
          case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
          case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER:
            if (strcmp(key,"start")==0) PSET_VEC(cur_obj.u.cap.start)
            else if (strcmp(key,"end")==0) PSET_VEC(cur_obj.u.cap.end)
            else if (strcmp(key,"thickness")==0) PSET_DBL(cur_obj.u.cap.thickness)
            else if (strcmp(key,"restitution")==0) PSET_DBL(cur_obj.u.cap.restitution)
            else if (strcmp(key,"friction")==0) PSET_DBL(cur_obj.u.cap.friction)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.cap.enabled)
            else if (ot==OBJ_ONE_WAY_GATE && strcmp(key,"allowed_direction")==0) PSET_VEC(cur_obj.u.cap.allowed_direction)
            else if (ot==OBJ_SLINGSHOT && strcmp(key,"impulse")==0) PSET_DBL(cur_obj.u.cap.impulse)
            else if (ot==OBJ_SLINGSHOT && strcmp(key,"base_score")==0) PSET_INT(cur_obj.u.cap.base_score)
            else if (ot==OBJ_SLINGSHOT && strcmp(key,"cooldown")==0) PSET_DBL(cur_obj.u.cap.cooldown)
            else if (ot==OBJ_ROLLOVER && strcmp(key,"width")==0) PSET_DBL(cur_obj.u.cap.width)
            else if (ot==OBJ_ROLLOVER && strcmp(key,"base_score")==0) PSET_INT(cur_obj.u.cap.base_score)
            else if (ot==OBJ_ROLLOVER && strcmp(key,"activation_mode")==0){ /* accepted */ }
            else if ((ot==OBJ_DROP_TARGET||ot==OBJ_STANDUP_TARGET) && strcmp(key,"min_hit_speed")==0) PSET_DBL(cur_obj.u.cap.min_hit_speed)
            else if ((ot==OBJ_DROP_TARGET||ot==OBJ_STANDUP_TARGET) && strcmp(key,"base_score")==0) PSET_INT(cur_obj.u.cap.base_score)
            else if ((ot==OBJ_DROP_TARGET||ot==OBJ_STANDUP_TARGET) && strcmp(key,"cooldown")==0) PSET_DBL(cur_obj.u.cap.cooldown)
            else if (ot==OBJ_DROP_TARGET && strcmp(key,"initially_raised")==0) PSET_BOOL(cur_obj.u.cap.initially_raised)
            else if (ot==OBJ_DROP_TARGET && strcmp(key,"reset_mode")==0){ if(strcmp(val,"MANUAL_EVENT")==0)cur_obj.u.cap.reset_mode=0; else if(strcmp(val,"AFTER_DELAY")==0)cur_obj.u.cap.reset_mode=1; else if(strcmp(val,"ON_NEW_BALL")==0)cur_obj.u.cap.reset_mode=2; else report(diag,PBT_E_TYPE,ln,key);}
            else if (ot==OBJ_DROP_TARGET && strcmp(key,"reset_delay")==0) PSET_DBL(cur_obj.u.cap.reset_delay)
            else report(diag,PBT_E_SYNTAX,ln,"unknown capsule field");
            break;
          case OBJ_BUMPER:
            if (strcmp(key,"center")==0) PSET_VEC(cur_obj.u.bumper.center)
            else if (strcmp(key,"radius")==0) PSET_DBL(cur_obj.u.bumper.radius)
            else if (strcmp(key,"restitution")==0) PSET_DBL(cur_obj.u.bumper.restitution)
            else if (strcmp(key,"friction")==0) PSET_DBL(cur_obj.u.bumper.friction)
            else if (strcmp(key,"impulse")==0) PSET_DBL(cur_obj.u.bumper.impulse)
            else if (strcmp(key,"base_score")==0) PSET_INT(cur_obj.u.bumper.base_score)
            else if (strcmp(key,"cooldown")==0) PSET_DBL(cur_obj.u.bumper.cooldown)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.bumper.enabled)
            else report(diag,PBT_E_SYNTAX,ln,"unknown bumper field");
            break;
          case OBJ_FLIPPER:
            if (strcmp(key,"pivot")==0) PSET_VEC(cur_obj.u.flipper.pivot)
            else if (strcmp(key,"length")==0) PSET_DBL(cur_obj.u.flipper.length)
            else if (strcmp(key,"thickness")==0) PSET_DBL(cur_obj.u.flipper.thickness)
            else if (strcmp(key,"rest_angle_deg")==0) PSET_DBL(cur_obj.u.flipper.rest_angle_deg)
            else if (strcmp(key,"active_angle_deg")==0) PSET_DBL(cur_obj.u.flipper.active_angle_deg)
            else if (strcmp(key,"engage_speed_deg_s")==0) PSET_DBL(cur_obj.u.flipper.engage_speed_deg_s)
            else if (strcmp(key,"return_speed_deg_s")==0) PSET_DBL(cur_obj.u.flipper.return_speed_deg_s)
            else if (strcmp(key,"restitution")==0) PSET_DBL(cur_obj.u.flipper.restitution)
            else if (strcmp(key,"friction")==0) PSET_DBL(cur_obj.u.flipper.friction)
            else if (strcmp(key,"input")==0){ if(strcmp(val,"LEFT_FLIPPER")==0)cur_obj.u.flipper.input=0; else if(strcmp(val,"RIGHT_FLIPPER")==0)cur_obj.u.flipper.input=1; else report(diag,PBT_E_TYPE,ln,key);}
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.flipper.enabled)
            else report(diag,PBT_E_SYNTAX,ln,"unknown flipper field");
            break;
          case OBJ_SENSOR: case OBJ_DRAIN:
            if (strcmp(key,"rect")==0) PSET_TUP4(cur_obj.u.sensor.x,cur_obj.u.sensor.y,cur_obj.u.sensor.w,cur_obj.u.sensor.h)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.sensor.enabled)
            else if (ot==OBJ_SENSOR && strcmp(key,"debug_visible")==0) PSET_BOOL(cur_obj.u.sensor.debug_visible)
            else report(diag,PBT_E_SYNTAX,ln,"unknown sensor/drain field");
            break;
          case OBJ_LAUNCHER:
            if (strcmp(key,"position")==0) PSET_VEC(cur_obj.u.launcher.position)
            else if (strcmp(key,"spawn")==0) PSET_ID(cur_obj.u.launcher.spawn_id)
            else if (strcmp(key,"direction")==0) PSET_VEC(cur_obj.u.launcher.direction)
            else if (strcmp(key,"min_speed")==0) PSET_DBL(cur_obj.u.launcher.min_speed)
            else if (strcmp(key,"max_speed")==0) PSET_DBL(cur_obj.u.launcher.max_speed)
            else if (strcmp(key,"full_charge_time")==0) PSET_DBL(cur_obj.u.launcher.full_charge_time)
            else if (strcmp(key,"charge_curve")==0){ if(strcmp(val,"LINEAR")==0)cur_obj.u.launcher.charge_curve=0; else if(strcmp(val,"EASE")==0)cur_obj.u.launcher.charge_curve=1; else report(diag,PBT_E_TYPE,ln,key);}
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.launcher.enabled)
            else report(diag,PBT_E_SYNTAX,ln,"unknown launcher field");
            break;
          case OBJ_SPINNER:
            if (strcmp(key,"pivot")==0) PSET_VEC(cur_obj.u.spinner.pivot)
            else if (strcmp(key,"half_length")==0) PSET_DBL(cur_obj.u.spinner.half_length)
            else if (strcmp(key,"thickness")==0) PSET_DBL(cur_obj.u.spinner.thickness)
            else if (strcmp(key,"rest_angle_deg")==0) PSET_DBL(cur_obj.u.spinner.rest_angle_deg)
            else if (strcmp(key,"angular_damping")==0) PSET_DBL(cur_obj.u.spinner.angular_damping)
            else if (strcmp(key,"inertia")==0) PSET_DBL(cur_obj.u.spinner.inertia)
            else if (strcmp(key,"restitution")==0) PSET_DBL(cur_obj.u.spinner.restitution)
            else if (strcmp(key,"friction")==0) PSET_DBL(cur_obj.u.spinner.friction)
            else if (strcmp(key,"score_per_tick")==0) PSET_DBL(cur_obj.u.spinner.score_per_tick)
            else if (strcmp(key,"tick_angle_deg")==0) PSET_DBL(cur_obj.u.spinner.tick_angle_deg)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.spinner.enabled)
            else report(diag,PBT_E_SYNTAX,ln,"unknown spinner field");
            break;
          case OBJ_KICKOUT:
            if (strcmp(key,"center")==0) PSET_VEC(cur_obj.u.kickout.center)
            else if (strcmp(key,"capture_radius")==0) PSET_DBL(cur_obj.u.kickout.capture_radius)
            else if (strcmp(key,"eject_direction")==0) PSET_VEC(cur_obj.u.kickout.eject_direction)
            else if (strcmp(key,"eject_speed")==0) PSET_DBL(cur_obj.u.kickout.eject_speed)
            else if (strcmp(key,"hold_time")==0) PSET_DBL(cur_obj.u.kickout.hold_time)
            else if (strcmp(key,"base_score")==0) PSET_INT(cur_obj.u.kickout.base_score)
            else if (strcmp(key,"enabled")==0) PSET_BOOL(cur_obj.u.kickout.enabled)
            else report(diag,PBT_E_SYNTAX,ln,"unknown kickout field");
            break;
          default: report(diag,PBT_E_SYNTAX,ln,"unknown object field"); break;
        }
      }
    }
    else if (cur_section=='g') {
      if (strcmp(key,"name")==0) PSET_STR(cur_group.name)
      else if (strcmp(key,"pivot")==0) PSET_VEC(cur_group.pivot)
      else if (strcmp(key,"member_count")==0){ long v; PbtCode _rc=parse_i64(val,&v); if(_rc==PBT_OK){cur_group.member_count=(int)v; if(v<0)report(diag,PBT_E_NUMERIC_RANGE,ln,"negative member count");} else report(diag,_rc,ln,key); }
      else if (strncmp(key,"member.",7)==0) {
        int idx = atoi(key+7);
        if (idx<0||idx>=PB_MAX_MEMBERS) report(diag,PBT_E_NUMERIC_RANGE,ln,"member index");
        else PSET_ID(cur_group.members[idx]);
      }
      else report(diag,PBT_E_SYNTAX,ln,"unknown group field");
    }
    else if (cur_section=='e') {
      if (strcmp(key,"source")==0) PSET_ID(cur_event.source)
      else if (strcmp(key,"trigger")==0){ TriggerType tr=trigger_from_name(val); if(tr==TRIG_COUNT) report(diag,PBT_E_TYPE,ln,"unknown trigger"); else cur_event.trigger=tr; }
      else if (strcmp(key,"action_count")==0){ long v; PbtCode _rc=parse_i64(val,&v); if(_rc==PBT_OK){cur_event.action_count=(int)v; if(v<0)report(diag,PBT_E_NUMERIC_RANGE,ln,"negative action count");} else report(diag,_rc,ln,key); }
      else if (strncmp(key,"action.",7)==0) {
        int idx = atoi(key+7);
        if (idx<0||idx>=PB_MAX_ACTIONS) { report(diag,PBT_E_NUMERIC_RANGE,ln,"action index"); }
        else {
          char tmp[PB_TOKEN_MAX]; strcpy(tmp,val);
          char *tok0 = strtok(tmp," ");
          if (!tok0) { report(diag,PBT_E_SYNTAX,ln,"malformed action"); }
          else {
            ActionType at = action_from_name(tok0);
            if (at==ACT_COUNT) { report(diag,PBT_E_UNKNOWN_ACTION,ln,tok0); }
            else {
              cur_event.actions[idx].type = at;
              char *param = strtok(NULL," ");
              while (param) {
                char *eqp = strchr(param,'=');
                if (eqp) {
                  *eqp=0; const char *pk=param; const char *pv=eqp+1;
                  if (strcmp(pk,"amount")==0){cur_event.actions[idx].amount=atol(pv);}
                  else if (strcmp(pk,"spawn")==0){strncpy(cur_event.actions[idx].spawn,pv,PB_ID_MAX-1);cur_event.actions[idx].spawn[PB_ID_MAX-1]=0;}
                  else if (strcmp(pk,"count")==0){cur_event.actions[idx].count=atoi(pv);}
                  else if (strcmp(pk,"add_count")==0){cur_event.actions[idx].count=atoi(pv);}
                  else if (strcmp(pk,"target")==0){strncpy(cur_event.actions[idx].target,pv,PB_ID_MAX-1);cur_event.actions[idx].target[PB_ID_MAX-1]=0;}
                  else if (strcmp(pk,"multiplier")==0){cur_event.actions[idx].multiplier=atoi(pv);}
                  else if (strcmp(pk,"duration")==0){cur_event.actions[idx].duration=atof(pv);}
                  else if (strcmp(pk,"dropped")==0){cur_event.actions[idx].dropped=(strcmp(pv,"true")==0)?1:0;}
                }
                param = strtok(NULL," ");
              }
            }
          }
        }
      }
      else report(diag,PBT_E_SYNTAX,ln,"unknown event field");
    }
    else {
      report(diag, PBT_E_SYNTAX, ln, "key/value outside section");
    }
  }

  /* commit sections */
  if (cur_section=='l') scene_add_layer(out, &cur_layer);
  else if (cur_section=='o') {
    if (out->obj_count >= obj_limit) report(diag,PBT_E_LIMIT_OBJECTS,0,"object limit");
    else scene_add_object(out, &cur_obj);
  }
  else if (cur_section=='g') {
    if (out->group_count >= grp_limit) report(diag,PBT_E_LIMIT_GROUPS,0,"group limit");
    else scene_add_group(out, &cur_group);
  }
  else if (cur_section=='e') {
    if (out->event_count >= ev_limit) report(diag,PBT_E_LIMIT_EVENTS,0,"event limit");
    else scene_add_event(out, &cur_event);
  }

  free(L.items);

  /* ---------- reference resolution pass (parse-stage diagnostics) ---------- */
  /* launcher spawn reference */
  for (int i=0;i<out->obj_count;i++) {
    Obj *o=&out->objects[i];
    if (o->type==OBJ_LAUNCHER) {
      if (o->u.launcher.spawn_id[0]==0) { report(diag,PBT_E_MISSING_FIELD,0,"launcher missing spawn"); continue; }
      Obj *sp=scene_find_object(out,o->u.launcher.spawn_id);
      if (!sp) report(diag,PBT_E_REFERENCE_MISSING,0,"launcher spawn missing");
      else if (sp->type!=OBJ_BALL_SPAWN) report(diag,PBT_E_REFERENCE_TYPE,0,"launcher spawn wrong type");
    }
  }
  /* group members: must be objects, not groups; no duplicates; nesting prohibited */
  for (int g=0;g<out->group_count;g++) {
    Group *gr=&out->groups[g];
    for (int m=0;m<gr->member_count;m++) {
      char *mid=gr->members[m];
      if (scene_find_group(out,mid)) report(diag,PBT_E_GROUP_NESTING,0,"group references group");
      else if (!scene_find_object(out,mid)) report(diag,PBT_E_REFERENCE_MISSING,0,"group member missing");
      for (int k=m+1;k<gr->member_count;k++) if (strcmp(gr->members[k],mid)==0) report(diag,PBT_E_GROUP_MEMBERSHIP,0,"duplicate group member");
    }
  }
  /* format-2 requires layer for objects; layer must exist */
  if (out->format==2) {
    if (out->layer_count==0) report(diag,PBT_E_LAYER,0,"no layer in v2");
    for (int i=0;i<out->obj_count;i++) {
      Obj *o=&out->objects[i];
      if (o->layer[0]==0) { report(diag,PBT_E_LAYER,0,"object missing layer"); }
      else if (!scene_find_layer(out,o->layer)) report(diag,PBT_E_LAYER,0,"object layer missing");
    }
  }

  /* ---------- required table fields (doc 06) ---------- */
  if ((tf & 0x7FFu) != 0x7FFu)
    report(diag, PBT_E_MISSING_FIELD, 0, "missing required table field");

  /* ---------- numeric ranges ---------- */
  if (out->max_active_balls < 1) report(diag, PBT_E_NUMERIC_RANGE, 0, "max_active_balls out of range");
  if (out->starting_turns < 0)   report(diag, PBT_E_NUMERIC_RANGE, 0, "starting_turns out of range");

  /* ---------- event action index contiguity & references ---------- */
  for (int e = 0; e < out->event_count; e++) {
    Event *ev = &out->events[e];
    for (int a = 0; a < ev->action_count; a++)
      if (ev->actions[a].type == ACT_COUNT) report(diag, PBT_E_ACTION_INDEX, 0, "action index gap");
    if (ev->source[0] && !scene_find_object(out, ev->source))
      report(diag, PBT_E_REFERENCE_MISSING, 0, "event source missing");
  }

  /* ---------- migration defaults for v1 ---------- */
  if (out->format==1) {
    /* add default gameplay layer; assign all objects to it */
    Layer gl; memset(&gl,0,sizeof(gl));
    strcpy(gl.id,"gameplay"); strcpy(gl.name,"Gameplay"); gl.visible=1; gl.locked=0; gl.order=0;
    if (!scene_find_layer(out,"gameplay")) scene_add_layer(out,&gl);
    for (int i=0;i<out->obj_count;i++) { strcpy(out->objects[i].layer,"gameplay"); out->objects[i].locked=0; }
    if (!out->has_seed) out->scene_seed = 0;
    if (out->nudge_impulse==0) out->nudge_impulse=85;
    if (out->tilt_threshold==0) out->tilt_threshold=3;
    if (out->nudge_tilt_cost==0) out->nudge_tilt_cost=1;
    if (out->tilt_decay_per_second==0) out->tilt_decay_per_second=0.75;
    if (out->nudge_cooldown==0) out->nudge_cooldown=0.08;
    out->format = 2; /* migrated in-memory representation is format 2 */
  }

  return g_first;
}

PbtCode parse_scene_file(const char *path, Scene *out, DiagList *diag) {
  FILE *f = fopen(path, "rb");
  if (!f) { report(diag, PBT_E_HEADER, 0, "cannot open file"); return PBT_E_HEADER; }
  fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  if (sz < 0) { fclose(f); report(diag, PBT_E_HEADER, 0, "seek error"); return PBT_E_HEADER; }
  if ((unsigned long)sz > PB_FILE_MAX) {
    fclose(f); report(diag, PBT_E_LIMIT_FILE, 0, "file too large"); return PBT_E_LIMIT_FILE;
  }
  char *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); report(diag, PBT_E_ALLOCATION, 0, "oom"); return PBT_E_ALLOCATION; }
  size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = 0; fclose(f);
  PbtCode c = parse_scene(buf, rd, out, diag);
  free(buf);
  return c;
}
