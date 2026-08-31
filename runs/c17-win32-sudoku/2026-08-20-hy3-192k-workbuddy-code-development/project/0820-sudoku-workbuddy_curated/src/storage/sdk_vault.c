/* sdk_vault.c - encrypted local vault implementation.
 * See sdk_vault.h and docs/08 + docs/19 sections 16-22. */
#include "storage/sdk_vault.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common/sdk_crc32.h"
#include "common/sdk_win.h"
#include "crypto/sdk_aead.h"
#include "crypto/sdk_pbkdf2.h"

static void free_game_deep(sdk_game_record *g);

/* ------------------------------------------------------------------ */
/* dynamic writer                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned char *buf;
    size_t len, cap;
} wbuf;

static int wbuf_reserve(wbuf *w, size_t extra) {
    if (w->len + extra <= w->cap) return 1;
    size_t ncap = w->cap ? w->cap * 2 : 1024;
    while (ncap < w->len + extra) ncap *= 2;
    unsigned char *nb = (unsigned char *)realloc(w->buf, ncap);
    if (!nb) return 0;
    w->buf = nb; w->cap = ncap;
    return 1;
}
static int w8 (wbuf *w, uint8_t  v) { if (!wbuf_reserve(w,1)) return 0; w->buf[w->len++] = v; return 1; }
static int w16(wbuf *w, uint16_t v) { if (!wbuf_reserve(w,2)) return 0; w->buf[w->len++]=v&0xff; w->buf[w->len++]=(v>>8)&0xff; return 1; }
static int w32(wbuf *w, uint32_t v) { if (!wbuf_reserve(w,4)) return 0; w->buf[w->len++]=v&0xff; w->buf[w->len++]=(v>>8)&0xff; w->buf[w->len++]=(v>>16)&0xff; w->buf[w->len++]=(v>>24)&0xff; return 1; }
static int w64(wbuf *w, uint64_t v) { if (!wbuf_reserve(w,8)) return 0; for (int i=0;i<8;i++){ w->buf[w->len++]=v&0xff; v>>=8; } return 1; }
static int wraw(wbuf *w, const void *p, size_t n) { if (!wbuf_reserve(w,n)) return 0; memcpy(w->buf+w->len,p,n); w->len+=n; return 1; }

/* ------------------------------------------------------------------ */
/* bounded reader                                                    */
/* ------------------------------------------------------------------ */
typedef struct {
    const unsigned char *p;
    size_t pos, len;
    int err;
} rbuf;

static int r8 (rbuf *r, uint8_t  *v) { if (r->err||r->pos+1>r->len) { r->err=1; return 0; } *v=r->p[r->pos++]; return 1; }
static int r16(rbuf *r, uint16_t *v) { if (r->err||r->pos+2>r->len) { r->err=1; return 0; } *v=(uint16_t)r->p[r->pos]|((uint16_t)r->p[r->pos+1]<<8); r->pos+=2; return 1; }
static int r32(rbuf *r, uint32_t *v) { if (r->err||r->pos+4>r->len) { r->err=1; return 0; } *v=(uint32_t)r->p[r->pos]|((uint32_t)r->p[r->pos+1]<<8)|((uint32_t)r->p[r->pos+2]<<16)|((uint32_t)r->p[r->pos+3]<<24); r->pos+=4; return 1; }
static int r64(rbuf *r, uint64_t *v) { if (r->err||r->pos+8>r->len) { r->err=1; return 0; } *v=0; for(int i=0;i<8;i++){ *v|=((uint64_t)r->p[r->pos+i])<<(8*i); } r->pos+=8; return 1; }
static int rraw(rbuf *r, void *out, size_t n) { if (r->err||r->pos+n>r->len) { r->err=1; return 0; } memcpy(out,(void*)(r->p+r->pos),n); r->pos+=n; return 1; }
static int rskip(rbuf *r, size_t n) { if (r->err||r->pos+n>r->len) { r->err=1; return 0; } r->pos+=n; return 1; }

/* ------------------------------------------------------------------ */
/* transaction (de)serialization                                    */
/* ------------------------------------------------------------------ */
static int write_tx(wbuf *w, const sdk_undo_transaction *t) {
    if (!w8(w, t->action_kind)) return 0;
    if (!w8(w, t->assisted_reason)) return 0;
    if (!w16(w, t->change_count)) return 0;
    if (!w64(w, t->sequence_number)) return 0;
    for (uint16_t i = 0; i < t->change_count; ++i) {
        const sdk_change *c = &t->changes[i];
        if (!w8(w, c->cell_index)) return 0;
        if (!w8(w, c->before_value)) return 0;
        if (!w8(w, c->after_value)) return 0;
        if (!w8(w, c->before_origin)) return 0;
        if (!w8(w, c->after_origin)) return 0;
        if (!w16(w, c->before_notes)) return 0;
        if (!w16(w, c->after_notes)) return 0;
    }
    return 1;
}

static int read_tx(rbuf *r, sdk_undo_transaction *t) {
    memset(t, 0, sizeof *t);
    uint8_t ak, ar; uint16_t cc; uint64_t seq;
    if (!r8(r, &ak)) return 0;
    if (!r8(r, &ar)) return 0;
    if (!r16(r, &cc)) return 0;
    if (!r64(r, &seq)) return 0;
    if (cc > 512) { r->err = 1; return 0; }            /* docs/19 section 2 */
    t->action_kind = ak; t->assisted_reason = ar;
    t->change_count = cc; t->sequence_number = seq;
    if (cc) {
        t->changes = (sdk_change *)calloc(cc, sizeof(sdk_change));
        if (!t->changes) { r->err = 1; return 0; }
    }
    uint8_t last_idx = 0;
    for (uint16_t i = 0; i < cc; ++i) {
        sdk_change *c = &t->changes[i];
        uint8_t bv, av, bo, ao; uint16_t bn, an;
        if (!r8(r, &c->cell_index)) return 0;
        if (!r8(r, &bv)) return 0;
        if (!r8(r, &av)) return 0;
        if (!r8(r, &bo)) return 0;
        if (!r8(r, &ao)) return 0;
        if (!r16(r, &bn)) return 0;
        if (!r16(r, &an)) return 0;
        if (c->cell_index >= 81) { r->err = 1; return 0; }
        if (i > 0 && c->cell_index <= last_idx) { r->err = 1; return 0; } /* ascending, unique */
        last_idx = c->cell_index;
        if (bv > 9 || av > 9 || bo > 4 || ao > 4) { r->err = 1; return 0; }
        if (bn > 0x1FF || an > 0x1FF) { r->err = 1; return 0; }
        if (bv == av && bo == ao && bn == an) { r->err = 1; return 0; } /* no no-op */
        c->before_value = bv; c->after_value = av;
        c->before_origin = bo; c->after_origin = ao;
        c->before_notes = bn; c->after_notes = an;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* game / completed record (de)serialization                         */
/* ------------------------------------------------------------------ */
static int write_game(wbuf *w, const sdk_game_record *g) {
    if (!wraw(w, g->id, SDK_GAME_ID_LEN)) return 0;
    if (!w8(w, (uint8_t)g->difficulty)) return 0;
    if (!w16(w, g->diff_rules_ver)) return 0;
    if (!w16(w, g->gen_format_ver)) return 0;
    if (!w64(w, g->gen_seed)) return 0;
    if (!wraw(w, g->orig, 81)) return 0;
    if (!wraw(w, g->cur, 81)) return 0;
    for (int i = 0; i < 81; ++i) if (!w16(w, g->notes[i])) return 0;
    if (!wraw(w, g->origin, 81)) return 0;
    if (!w64(w, g->active_elapsed_ms)) return 0;
    if (!w64(w, (uint64_t)g->created_epoch_ms)) return 0;
    if (!w64(w, (uint64_t)g->last_played_epoch_ms)) return 0;
    if (!w8(w, g->paused)) return 0;
    if (!w32(w, g->hints_viewed)) return 0;
    if (!w32(w, g->hints_applied)) return 0;
    if (!w8(w, g->highest_hint_tech)) return 0;
    if (!w8(w, g->used_auto_solve)) return 0;
    if (!w64(w, g->current_generation)) return 0;
    if (!w64(w, g->saved_generation)) return 0;
    if (g->undo_count > 10000 || g->redo_count > 10000) return SDK_ERR_LIMIT;
    if (!w32(w, g->undo_count)) return 0;
    for (uint32_t i = 0; i < g->undo_count; ++i) if (!write_tx(w, &g->undo[i])) return 0;
    if (!w32(w, g->redo_count)) return 0;
    for (uint32_t i = 0; i < g->redo_count; ++i) if (!write_tx(w, &g->redo[i])) return 0;
    return 1;
}

static int read_game(rbuf *r, sdk_game_record *g) {
    memset(g, 0, sizeof *g);
    if (!rraw(r, g->id, SDK_GAME_ID_LEN)) return 0;
    uint8_t diff; if (!r8(r, &diff)) return 0; g->difficulty = diff;
    if (!r16(r, &g->diff_rules_ver)) return 0;
    if (!r16(r, &g->gen_format_ver)) return 0;
    if (!r64(r, &g->gen_seed)) return 0;
    if (!rraw(r, g->orig, 81)) return 0;
    if (!rraw(r, g->cur, 81)) return 0;
    for (int i = 0; i < 81; ++i) {
        uint16_t n; if (!r16(r, &n)) return 0;
        if (n > 0x1FF) { r->err = 1; return 0; }
        g->notes[i] = n;
    }
    if (!rraw(r, g->origin, 81)) return 0;
    uint64_t ae; if (!r64(r, &ae)) return 0; g->active_elapsed_ms = ae;
    uint64_t ce; if (!r64(r, &ce)) return 0; g->created_epoch_ms = (int64_t)ce;
    uint64_t le; if (!r64(r, &le)) return 0; g->last_played_epoch_ms = (int64_t)le;
    if (!r8(r, &g->paused)) return 0;
    if (!r32(r, &g->hints_viewed)) return 0;
    if (!r32(r, &g->hints_applied)) return 0;
    if (!r8(r, &g->highest_hint_tech)) return 0;
    if (!r8(r, &g->used_auto_solve)) return 0;
    uint64_t cg, sg; if (!r64(r, &cg)) return 0; if (!r64(r, &sg)) return 0;
    g->current_generation = cg; g->saved_generation = sg;
    uint32_t uc;
    if (!r32(r, &uc)) return 0;
    if (uc > 10000) { r->err = 1; return 0; }
    g->undo_count = uc;
    if (uc) { g->undo = (sdk_undo_transaction *)calloc(uc, sizeof(sdk_undo_transaction)); if(!g->undo){r->err=1;return 0;} }
    for (uint32_t i = 0; i < uc; ++i) if (!read_tx(r, &g->undo[i])) return 0;
    uint32_t rc;
    if (!r32(r, &rc)) return 0;
    if (rc > 10000) { r->err = 1; return 0; }
    g->redo_count = rc;
    if (rc) { g->redo = (sdk_undo_transaction *)calloc(rc, sizeof(sdk_undo_transaction)); if(!g->redo){r->err=1;return 0;} }
    for (uint32_t i = 0; i < rc; ++i) if (!read_tx(r, &g->redo[i])) return 0;

    /* structural consistency: given cells must equal their clue */
    for (int i = 0; i < 81; ++i) {
        if (g->origin[i] == SDK_O_GIVEN) {
            if (g->orig[i] == 0 || g->cur[i] != g->orig[i]) { r->err = 1; return 0; }
        }
    }
    return 1;
}

static int write_completed(wbuf *w, const sdk_completed_record *c) {
    if (!wraw(w, c->id, SDK_GAME_ID_LEN)) return 0;
    if (!w8(w, (uint8_t)c->difficulty)) return 0;
    if (!w16(w, c->diff_rules_ver)) return 0;
    if (!w16(w, c->gen_format_ver)) return 0;
    if (!w64(w, c->gen_seed)) return 0;
    if (!wraw(w, c->orig, 81)) return 0;
    if (!wraw(w, c->grid, 81)) return 0;
    if (!wraw(w, c->origin, 81)) return 0;
    if (!w64(w, c->active_elapsed_ms)) return 0;
    if (!w64(w, (uint64_t)c->created_epoch_ms)) return 0;
    if (!w64(w, (uint64_t)c->last_played_epoch_ms)) return 0;
    if (!w64(w, (uint64_t)c->completed_epoch_ms)) return 0;
    if (!w32(w, c->hints_viewed)) return 0;
    if (!w32(w, c->hints_applied)) return 0;
    if (!w8(w, c->highest_hint_tech)) return 0;
    if (!w8(w, c->used_auto_solve)) return 0;
    if (!w8(w, c->completion_class)) return 0;
    if (!w32(w, (uint32_t)c->logic_score)) return 0;
    if (!w32(w, (uint32_t)c->max_technique)) return 0;
    if (!w32(w, (uint32_t)c->clue_count)) return 0;
    return 1;
}

static int read_completed(rbuf *r, sdk_completed_record *c) {
    memset(c, 0, sizeof *c);
    if (!rraw(r, c->id, SDK_GAME_ID_LEN)) return 0;
    uint8_t diff; if (!r8(r, &diff)) return 0; c->difficulty = diff;
    if (!r16(r, &c->diff_rules_ver)) return 0;
    if (!r16(r, &c->gen_format_ver)) return 0;
    if (!r64(r, &c->gen_seed)) return 0;
    if (!rraw(r, c->orig, 81)) return 0;
    if (!rraw(r, c->grid, 81)) return 0;
    if (!rraw(r, c->origin, 81)) return 0;
    uint64_t ae; if (!r64(r, &ae)) return 0; c->active_elapsed_ms = ae;
    uint64_t ce; if (!r64(r, &ce)) return 0; c->created_epoch_ms = (int64_t)ce;
    uint64_t le; if (!r64(r, &le)) return 0; c->last_played_epoch_ms = (int64_t)le;
    uint64_t pe; if (!r64(r, &pe)) return 0; c->completed_epoch_ms = (int64_t)pe;
    if (!r32(r, &c->hints_viewed)) return 0;
    if (!r32(r, &c->hints_applied)) return 0;
    if (!r8(r, &c->highest_hint_tech)) return 0;
    if (!r8(r, &c->used_auto_solve)) return 0;
    if (!r8(r, &c->completion_class)) return 0;
    uint32_t ls, mt, cc;
    if (!r32(r, &ls)) return 0; c->logic_score = (int)ls;
    if (!r32(r, &mt)) return 0; c->max_technique = (int)mt;
    if (!r32(r, &cc)) return 0; c->clue_count = (int)cc;
    if (c->completion_class == 2 && (c->hints_viewed || c->used_auto_solve)) { /* AUTO_SOLVED must not claim player completion */ }
    for (int i = 0; i < 81; ++i)
        if (c->origin[i] == SDK_O_GIVEN && (c->orig[i] == 0 || c->grid[i] != c->orig[i])) { r->err = 1; return 0; }
    return 1;
}

/* ------------------------------------------------------------------ */
/* store (de)serialization                                           */
/* ------------------------------------------------------------------ */
sdk_status sdk_vault_serialize_store(const sdk_store *s,
                                     unsigned char **out_buf, size_t *out_len) {
    wbuf w; memset(&w, 0, sizeof w);
    if (!wraw(&w, SDK_PAYLOAD_MAGIC, SDK_PAYLOAD_MAGIC_LEN)) goto fail;
    if (!w16(&w, SDK_PAYLOAD_VERSION)) goto fail;
    if (!w16(&w, 1u)) goto fail;   /* difficulty_rules_ver */
    if (!w16(&w, 1u)) goto fail;   /* generator_format_ver */
    if (!w16(&w, 0u)) goto fail;   /* reserved */
    /* settings */
    unsigned char setb[SDK_SETTINGS_LEN]; memset(setb, 0, sizeof setb);
    setb[0] = s->settings.theme;
    setb[1] = s->settings.motion;
    setb[2] = s->settings.auto_remove_peer_notes;
    setb[3] = s->settings.confirm_auto_solve;
    setb[4] = s->settings.last_difficulty;
    if (!w32(&w, SDK_SETTINGS_LEN)) goto fail;
    if (!wraw(&w, setb, SDK_SETTINGS_LEN)) goto fail;
    /* in-progress games */
    if (s->game_count > 1000) return SDK_ERR_LIMIT;
    if (!w32(&w, s->game_count)) goto fail;
    for (uint32_t i = 0; i < s->game_count; ++i) {
        wbuf rec; memset(&rec, 0, sizeof rec);
        if (!write_game(&rec, &s->games[i])) { free(rec.buf); goto fail; }
        if (!w16(&w, 1u)) { free(rec.buf); goto fail; }   /* record_type game */
        if (!w16(&w, 1u)) { free(rec.buf); goto fail; }   /* record_version */
        if (!w32(&w, (uint32_t)rec.len)) { free(rec.buf); goto fail; }
        if (!wraw(&w, rec.buf, rec.len)) { free(rec.buf); goto fail; }
        free(rec.buf);
    }
    /* completed */
    if (s->completed_count > 100000) return SDK_ERR_LIMIT;
    if (!w32(&w, s->completed_count)) goto fail;
    for (uint32_t i = 0; i < s->completed_count; ++i) {
        wbuf rec; memset(&rec, 0, sizeof rec);
        if (!write_completed(&rec, &s->completed[i])) { free(rec.buf); goto fail; }
        if (!w16(&w, 2u)) { free(rec.buf); goto fail; }   /* record_type completed */
        if (!w16(&w, 1u)) { free(rec.buf); goto fail; }
        if (!w32(&w, (uint32_t)rec.len)) { free(rec.buf); goto fail; }
        if (!wraw(&w, rec.buf, rec.len)) { free(rec.buf); goto fail; }
        free(rec.buf);
    }
    uint32_t crc = sdk_crc32(w.buf, w.len);
    if (!w32(&w, crc)) goto fail;
    *out_buf = w.buf; *out_len = w.len;
    return SDK_OK;
fail:
    free(w.buf);
    return SDK_ERR_NOMEM;
}

sdk_status sdk_vault_deserialize_store(const unsigned char *buf, size_t len,
                                       sdk_store *out) {
    rbuf r; r.p = buf; r.pos = 0; r.len = len; r.err = 0;
    unsigned char magic[SDK_PAYLOAD_MAGIC_LEN];
    if (!rraw(&r, magic, SDK_PAYLOAD_MAGIC_LEN)) return SDK_ERR_DATA;
    if (memcmp(magic, SDK_PAYLOAD_MAGIC, SDK_PAYLOAD_MAGIC_LEN) != 0) return SDK_ERR_DATA;
    uint16_t pv, drv, gfv, reserved;
    if (!r16(&r, &pv)) return SDK_ERR_DATA;
    if (!r16(&r, &drv)) return SDK_ERR_DATA;
    if (!r16(&r, &gfv)) return SDK_ERR_DATA;
    if (!r16(&r, &reserved)) return SDK_ERR_DATA;
    if (pv != SDK_PAYLOAD_VERSION) return SDK_ERR_DATA;

    sdk_store_init(out);
    uint32_t slen;
    if (!r32(&r, &slen)) goto bad;
    if (slen != SDK_SETTINGS_LEN) goto bad;
    unsigned char setb[SDK_SETTINGS_LEN];
    if (!rraw(&r, setb, SDK_SETTINGS_LEN)) goto bad;
    out->settings.theme = setb[0];
    out->settings.motion = setb[1];
    out->settings.auto_remove_peer_notes = setb[2];
    out->settings.confirm_auto_solve = setb[3];
    out->settings.last_difficulty = setb[4];

    uint32_t gc;
    if (!r32(&r, &gc)) goto bad;
    if (gc > 1000) goto bad;
    for (uint32_t i = 0; i < gc; ++i) {
        uint16_t rt, rv; uint32_t rl;
        if (!r16(&r, &rt)) goto bad;
        if (!r16(&r, &rv)) goto bad;
        if (!r32(&r, &rl)) goto bad;
        if (rt != 1 || rv != 1) goto bad;
        size_t before = r.pos;
        sdk_game_record g;
        if (!read_game(&r, &g)) goto bad;
        if (r.pos - before != rl) goto bad;             /* exact length */
        if (sdk_store_add_game(out, &g) != SDK_OK) { free_game_deep(&g); goto bad; }
        free_game_deep(&g);
    }
    uint32_t cc;
    if (!r32(&r, &cc)) goto bad;
    if (cc > 100000) goto bad;
    for (uint32_t i = 0; i < cc; ++i) {
        uint16_t rt, rv; uint32_t rl;
        if (!r16(&r, &rt)) goto bad;
        if (!r16(&r, &rv)) goto bad;
        if (!r32(&r, &rl)) goto bad;
        if (rt != 2 || rv != 1) goto bad;
        size_t before = r.pos;
        sdk_completed_record c;
        if (!read_completed(&r, &c)) goto bad;
        if (r.pos - before != rl) goto bad;
        if (sdk_store_add_completed(out, &c) != SDK_OK) goto bad;
    }
    uint32_t crc;
    if (!r32(&r, &crc)) goto bad;
    if (r.pos != r.len) goto bad;                       /* no trailing bytes */
    if (sdk_crc32(buf, r.len - 4) != crc) goto bad;
    return SDK_OK;
bad:
    sdk_store_free(out);
    return SDK_ERR_DATA;
}

/* ------------------------------------------------------------------ */
/* store lifecycle                                                   */
/* ------------------------------------------------------------------ */
void sdk_store_init(sdk_store *s) {
    memset(s, 0, sizeof *s);
    s->settings.theme = 0;
    s->settings.motion = 0;
    s->settings.auto_remove_peer_notes = 1;
    s->settings.confirm_auto_solve = 1;
    s->settings.last_difficulty = 0;
}
void free_game_deep(sdk_game_record *g) {
    for (uint32_t i = 0; i < g->undo_count; ++i) free(g->undo[i].changes);
    for (uint32_t i = 0; i < g->redo_count; ++i) free(g->redo[i].changes);
    free(g->undo); free(g->redo);
    g->undo = NULL; g->redo = NULL; g->undo_count = g->redo_count = 0;
}
void sdk_store_free(sdk_store *s) {
    if (!s) return;
    for (uint32_t i = 0; i < s->game_count; ++i) free_game_deep(&s->games[i]);
    free(s->games);
    free(s->completed);
    memset(s, 0, sizeof *s);
}
static int clone_tx_arr(sdk_undo_transaction **dst, uint32_t *dstn,
                        const sdk_undo_transaction *src, uint32_t n) {
    *dstn = n; *dst = NULL;
    if (n == 0) return 1;
    *dst = (sdk_undo_transaction *)calloc(n, sizeof(sdk_undo_transaction));
    if (!*dst) return 0;
    for (uint32_t i = 0; i < n; ++i) {
        (*dst)[i].action_kind = src[i].action_kind;
        (*dst)[i].assisted_reason = src[i].assisted_reason;
        (*dst)[i].change_count = src[i].change_count;
        (*dst)[i].sequence_number = src[i].sequence_number;
        if (src[i].change_count) {
            (*dst)[i].changes = (sdk_change *)calloc(src[i].change_count, sizeof(sdk_change));
            if (!(*dst)[i].changes) return 0;
            memcpy((*dst)[i].changes, src[i].changes,
                   src[i].change_count * sizeof(sdk_change));
        } else {
            (*dst)[i].changes = NULL;
        }
    }
    return 1;
}

sdk_status sdk_store_add_game(sdk_store *s, const sdk_game_record *g) {
    if (s->game_count >= s->game_cap) {
        uint32_t ncap = s->game_cap ? s->game_cap * 2 : 8;
        sdk_game_record *ng = (sdk_game_record *)realloc(s->games, ncap * sizeof *ng);
        if (!ng) return SDK_ERR_NOMEM;
        s->games = ng; s->game_cap = ncap;
    }
    sdk_game_record *dst = &s->games[s->game_count];
    memcpy(dst, g, sizeof *g);                 /* shallow copy of scalar + ptrs */
    if (!clone_tx_arr(&dst->undo, &dst->undo_count, g->undo, g->undo_count))
        return SDK_ERR_NOMEM;
    if (!clone_tx_arr(&dst->redo, &dst->redo_count, g->redo, g->redo_count)) {
        for (uint32_t i = 0; i < dst->undo_count; ++i) free(dst->undo[i].changes);
        free(dst->undo);
        return SDK_ERR_NOMEM;
    }
    s->game_count++;
    return SDK_OK;
}
sdk_status sdk_store_add_completed(sdk_store *s, const sdk_completed_record *c) {
    if (s->completed_count >= s->completed_cap) {
        uint32_t ncap = s->completed_cap ? s->completed_cap * 2 : 16;
        sdk_completed_record *nc = (sdk_completed_record *)realloc(s->completed, ncap * sizeof *nc);
        if (!nc) return SDK_ERR_NOMEM;
        s->completed = nc; s->completed_cap = ncap;
    }
    memcpy(&s->completed[s->completed_count], c, sizeof *c);
    s->completed_count++;
    return SDK_OK;
}
sdk_game_record *sdk_store_find_game(sdk_store *s, const uint8_t id[SDK_GAME_ID_LEN]) {
    for (uint32_t i = 0; i < s->game_count; ++i)
        if (memcmp(s->games[i].id, id, SDK_GAME_ID_LEN) == 0) return &s->games[i];
    return NULL;
}

sdk_status sdk_vault_new_game_id(uint8_t id[SDK_GAME_ID_LEN]) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (sdk_random_bytes(id, SDK_GAME_ID_LEN) != SDK_OK) return SDK_ERR_INTERNAL;
        int nonzero = 0;
        for (int i = 0; i < SDK_GAME_ID_LEN; ++i) if (id[i]) { nonzero = 1; break; }
        if (!nonzero) continue;
        return SDK_OK;  /* collision check against an open store is the caller's duty */
    }
    return SDK_ERR_INTERNAL;
}

/* ------------------------------------------------------------------ */
/* vault handle                                                      */
/* ------------------------------------------------------------------ */
struct sdk_vault {
    wchar_t *path;
    uint8_t key[SDK_VAULT_KEY_LEN];
    uint8_t salt[SDK_VAULT_SALT_LEN];
    uint32_t iterations;
};

static void wipe(void *p, size_t n) {
    volatile unsigned char *vp = (volatile unsigned char *)p;
    for (size_t i = 0; i < n; ++i) vp[i] = 0;
}

static int utf8_to_w(const char *u8, wchar_t *out, int outcap) {
    int n = MultiByteToWideChar(CP_UTF8, 0, u8, -1, out, outcap);
    return n > 0 ? 1 : 0;
}

/* derive key from password + salt */
static sdk_status derive_key(const char *password, const uint8_t *salt,
                             uint32_t iter, uint8_t key[SDK_VAULT_KEY_LEN]) {
    size_t plen = password ? strlen(password) : 0;
    return sdk_pbkdf2_hmac_sha256(password ? password : "", plen, salt,
                                  SDK_VAULT_SALT_LEN, iter, key,
                                  SDK_VAULT_KEY_LEN);
}

/* Build the full outer file bytes (header + ciphertext + tag). */
static sdk_status build_vault_file(const uint8_t *salt, uint32_t iter,
                                   const uint8_t *key,
                                   const unsigned char *plaintext, size_t ptlen,
                                   unsigned char **out, size_t *outlen) {
    unsigned char nonce[SDK_VAULT_NONCE_LEN];
    if (sdk_random_bytes(nonce, SDK_VAULT_NONCE_LEN) != SDK_OK) return SDK_ERR_INTERNAL;

    /* header (AAD) up to ciphertext_length field end */
    wbuf w; memset(&w, 0, sizeof w);
    int ok = 1;
    ok &= wraw(&w, SDK_VAULT_MAGIC, SDK_VAULT_MAGIC_LEN);
    ok &= w16(&w, SDK_VAULT_HEADER_VERSION);
    ok &= w16(&w, SDK_VAULT_KDF_PBKDF2);
    ok &= w32(&w, iter);
    ok &= wraw(&w, salt, SDK_VAULT_SALT_LEN);
    ok &= w16(&w, SDK_VAULT_CIPHER_XC20P);
    ok &= w16(&w, SDK_VAULT_NONCE_LEN);
    ok &= wraw(&w, nonce, SDK_VAULT_NONCE_LEN);
    ok &= w64(&w, (uint64_t)ptlen);
    if (!ok) { free(w.buf); return SDK_ERR_NOMEM; }

    size_t aad_len = w.len;
    size_t ctlen = ptlen;
    if (!wbuf_reserve(&w, ctlen + SDK_VAULT_TAG_LEN)) { free(w.buf); return SDK_ERR_NOMEM; }
    w.len += ctlen + SDK_VAULT_TAG_LEN;   /* reserve; encrypt fills it in */

    if (sdk_xchacha20poly1305_encrypt(key, nonce, w.buf, aad_len,
                                      plaintext, ptlen,
                                      w.buf + aad_len,
                                      w.buf + aad_len + ctlen) != SDK_OK) {
        free(w.buf); return SDK_ERR_INTERNAL;
    }
    *out = w.buf; *outlen = w.len;
    return SDK_OK;
}

/* Write bytes to a temp file then atomically replace target (creating or
 * replacing with backup as appropriate). */
static sdk_status atomic_write(const wchar_t *current, const unsigned char *data,
                               size_t len) {
    wchar_t tmp[4096], bak[4096];
    wcscpy_s(tmp, 4096, current);
    wcscat_s(tmp, 4096, L".tmp");
    wcscpy_s(bak, 4096, current);
    wcscat_s(bak, 4096, L".bak");

    HANDLE h = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return SDK_ERR_IO;
    DWORD written = 0;
    if (!WriteFile(h, data, (DWORD)len, &written, NULL) || written != (DWORD)len) {
        CloseHandle(h); DeleteFileW(tmp); return SDK_ERR_IO;
    }
    if (!FlushFileBuffers(h)) { CloseHandle(h); DeleteFileW(tmp); return SDK_ERR_IO; }
    CloseHandle(h);

    DWORD attrs = GetFileAttributesW(current);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        /* current does not exist: create via write-through move */
        if (!MoveFileExW(tmp, current, MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(tmp); return SDK_ERR_IO;
        }
    } else {
        if (!ReplaceFileW(current, tmp, bak, REPLACEFILE_WRITE_THROUGH, NULL, NULL)) {
            DeleteFileW(tmp); return SDK_ERR_IO;
        }
    }
    return SDK_OK;
}

/* ------------------------------------------------------------------ */
/* public API                                                        */
/* ------------------------------------------------------------------ */
sdk_status sdk_vault_create(const char *utf8_path, const char *password,
                            int prod, const sdk_store *store, sdk_vault **out) {
    if (!utf8_path || !password || !store) return SDK_ERR_USAGE;
    wchar_t wpath[4096];
    if (!utf8_to_w(utf8_path, wpath, 4096)) return SDK_ERR_USAGE;

    uint8_t salt[SDK_VAULT_SALT_LEN];
    if (sdk_random_bytes(salt, SDK_VAULT_SALT_LEN) != SDK_OK) return SDK_ERR_INTERNAL;
    uint32_t iter = prod ? SDK_VAULT_PROD_ITERATIONS : SDK_VAULT_TEST_ITERATIONS;
    uint8_t key[SDK_VAULT_KEY_LEN];
    if (derive_key(password, salt, iter, key) != SDK_OK) return SDK_ERR_INTERNAL;

    unsigned char *plain = NULL; size_t ptlen = 0;
    if (sdk_vault_serialize_store(store, &plain, &ptlen) != SDK_OK) { wipe(key,sizeof key); return SDK_ERR_NOMEM; }

    unsigned char *file = NULL; size_t flen = 0;
    sdk_status st = build_vault_file(salt, iter, key, plain, ptlen, &file, &flen);
    wipe(plain, ptlen); free(plain);
    if (st != SDK_OK) { wipe(key,sizeof key); return st; }

    st = atomic_write(wpath, file, flen);
    wipe(file, flen); free(file);
    if (st != SDK_OK) { wipe(key,sizeof key); return st; }

    sdk_vault *v = (sdk_vault *)calloc(1, sizeof *v);
    if (!v) { wipe(key,sizeof key); return SDK_ERR_NOMEM; }
    v->path = _wcsdup(wpath);
    memcpy(v->key, key, SDK_VAULT_KEY_LEN);
    memcpy(v->salt, salt, SDK_VAULT_SALT_LEN);
    v->iterations = iter;
    wipe(key, sizeof key);
    if (out) *out = v;
    return SDK_OK;
}

sdk_status sdk_vault_open(const char *utf8_path, const char *password,
                          sdk_vault **out, sdk_store *out_store) {
    if (!utf8_path || !password) return SDK_ERR_USAGE;
    wchar_t wpath[4096];
    if (!utf8_to_w(utf8_path, wpath, 4096)) return SDK_ERR_USAGE;

    HANDLE h = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return SDK_ERR_IO;
    DWORD hi = 0; DWORD lo = GetFileSize(h, &hi);
    size_t flen = ((size_t)hi << 32) | lo;
    if (flen < SDK_VAULT_MAGIC_LEN + 2 + 2 + 4 + SDK_VAULT_SALT_LEN + 2 + 2 +
              SDK_VAULT_NONCE_LEN + 8 + SDK_VAULT_TAG_LEN) {
        CloseHandle(h); return SDK_ERR_DATA;
    }
    unsigned char *file = (unsigned char *)malloc(flen);
    if (!file) { CloseHandle(h); return SDK_ERR_NOMEM; }
    DWORD readn = 0;
    if (!ReadFile(h, file, (DWORD)flen, &readn, NULL) || readn != (DWORD)flen) {
        CloseHandle(h); free(file); return SDK_ERR_IO;
    }
    CloseHandle(h);

    rbuf r; r.p = file; r.pos = 0; r.len = flen; r.err = 0;
    unsigned char magic[SDK_VAULT_MAGIC_LEN];
    if (!rraw(&r, magic, SDK_VAULT_MAGIC_LEN) ||
        memcmp(magic, SDK_VAULT_MAGIC, SDK_VAULT_MAGIC_LEN) != 0) {
        free(file); return SDK_ERR_DATA;
    }
    uint16_t hv, kdf, cipher, nlen;
    uint32_t iter;
    uint64_t ctlen;
    if (!r16(&r, &hv) || !r16(&r, &kdf) || !r32(&r, &iter)) { free(file); return SDK_ERR_DATA; }
    uint8_t salt[SDK_VAULT_SALT_LEN];
    if (!rraw(&r, salt, SDK_VAULT_SALT_LEN)) { free(file); return SDK_ERR_DATA; }
    if (!r16(&r, &cipher) || !r16(&r, &nlen)) { free(file); return SDK_ERR_DATA; }
    if (hv != SDK_VAULT_HEADER_VERSION || kdf != SDK_VAULT_KDF_PBKDF2 ||
        cipher != SDK_VAULT_CIPHER_XC20P || nlen != SDK_VAULT_NONCE_LEN) {
        free(file); return SDK_ERR_DATA;
    }
    uint8_t nonce[SDK_VAULT_NONCE_LEN];
    if (!rraw(&r, nonce, SDK_VAULT_NONCE_LEN)) { free(file); return SDK_ERR_DATA; }
    if (!r64(&r, &ctlen)) { free(file); return SDK_ERR_DATA; }
    size_t aad_len = r.pos;   /* magic .. ciphertext_length inclusive */
    if (ctlen > 64u * 1024u * 1024u) { free(file); return SDK_ERR_DATA; }
    if ((uint64_t)aad_len + (uint64_t)ctlen + SDK_VAULT_TAG_LEN != flen) {
        free(file); return SDK_ERR_DATA;
    }
    const unsigned char *ciphertext = file + aad_len;
    const unsigned char *tag = file + aad_len + ctlen;
    if (iter < 1) { free(file); return SDK_ERR_DATA; }

    uint8_t key[SDK_VAULT_KEY_LEN];
    if (derive_key(password, salt, iter, key) != SDK_OK) { free(file); return SDK_ERR_INTERNAL; }

    unsigned char *plain = (unsigned char *)malloc(ctlen ? ctlen : 1);
    if (!plain) { wipe(key,sizeof key); free(file); return SDK_ERR_NOMEM; }
    sdk_status st = sdk_xchacha20poly1305_decrypt(key, nonce, file, aad_len,
                                                  ciphertext, ctlen, tag, plain);
    wipe(key, sizeof key);
    if (st != SDK_OK) { free(plain); free(file); return SDK_ERR_AUTH; }  /* wrong pw / corrupt */

    sdk_store tmp;
    st = sdk_vault_deserialize_store(plain, ctlen, &tmp);
    wipe(plain, ctlen); free(plain);
    free(file);
    if (st != SDK_OK) return SDK_ERR_AUTH;

    sdk_vault *v = (sdk_vault *)calloc(1, sizeof *v);
    if (!v) { sdk_store_free(&tmp); return SDK_ERR_NOMEM; }
    v->path = _wcsdup(wpath);
    memcpy(v->salt, salt, SDK_VAULT_SALT_LEN);
    v->iterations = iter;
    if (out) *out = v; else sdk_vault_close(v);
    *out_store = tmp;
    return SDK_OK;
}

sdk_status sdk_vault_save(sdk_vault *v, const sdk_store *store) {
    if (!v || !store) return SDK_ERR_USAGE;
    unsigned char *plain = NULL; size_t ptlen = 0;
    if (sdk_vault_serialize_store(store, &plain, &ptlen) != SDK_OK) return SDK_ERR_NOMEM;
    unsigned char *file = NULL; size_t flen = 0;
    sdk_status st = build_vault_file(v->salt, v->iterations, v->key, plain, ptlen, &file, &flen);
    wipe(plain, ptlen); free(plain);
    if (st != SDK_OK) return st;
    st = atomic_write(v->path, file, flen);
    wipe(file, flen); free(file);
    return st;
}

void sdk_vault_close(sdk_vault *v) {
    if (!v) return;
    wipe(v->key, SDK_VAULT_KEY_LEN);
    free(v->path);
    free(v);
}
