/* history.c - persistent version history. */
#include "history.h"
#include "ce_common.h"
#include "buf.h"
#include "lzss.h"
#include "sha256.h"
#include "diff.h"

#define MAGIC "MDHV01"

md_history *md_history_create(void){
    md_history *h = ce_calloc(1, sizeof(md_history));
    h->next_id = 1;
    h->snapshot_interval = 20;
    h->max_versions = 200;
    h->max_payload = 64u * 1024u * 1024u;
    return h;
}

void md_history_free(md_history *h){
    if(!h) return;
    for(size_t i = 0; i < h->n; i++){
        if(h->v[i].payload) ce_free(h->v[i].payload);
        if(h->v[i].cached) ce_free(h->v[i].cached);
    }
    if(h->v) ce_free(h->v);
    ce_free(h);
}

size_t md_history_count(const md_history *h){ return h ? h->n : 0; }

/* ---- delta serialize (line edit script + inserted lines) ---- */

/* split with newline included in each line (last line may omit '\n') */
static size_t split_nl(const char *s, size_t len, size_t **starts, size_t **lens){
    size_t cap = 64, n = 0;
    size_t *st = ce_malloc(cap * sizeof(size_t));
    size_t *ln = ce_malloc(cap * sizeof(size_t));
    size_t i = 0;
    while(i < len){
        size_t start = i;
        while(i < len && s[i] != '\n') i++;
        size_t end = (i < len) ? i + 1 : i;  /* include newline */
        if(n == cap){ cap *= 2; st = ce_realloc(st, cap * sizeof(size_t)); ln = ce_realloc(ln, cap * sizeof(size_t)); }
        st[n] = start; ln[n] = end - start;
        n++;
        if(i >= len) break;
        i++;
    }
    *starts = st; *lens = ln;
    return n;
}

static unsigned char *delta_encode(const char *parent, size_t plen,
                                   const char *child, size_t clen, size_t *out_len){
    md_edit *ed = NULL;
    size_t ne = md_diff_script(parent, plen, child, clen, &ed);
    /* serialize: type byte per op; for ins, len + bytes */
    ce_buf b; ce_buf_init(&b);
    /* placeholder for op count */
    ce_buf_append(&b, "\0\0\0\0", 4);
    size_t *cst = NULL, *cln = NULL;
    split_nl(child, clen, &cst, &cln);
    size_t bi = 0;
    for(size_t i = 0; i < ne; i++){
        unsigned char t = (unsigned char)ed[i].type;
        ce_buf_append_c(&b, (char)t);
        if(t == 2){
            /* ins: store child line bytes */
            uint32_t l = (uint32_t)cln[bi];
            unsigned char lb[4] = { l & 0xFF, (l>>8)&0xFF, (l>>16)&0xFF, (l>>24)&0xFF };
            ce_buf_append(&b, lb, 4);
            ce_buf_append(&b, child + cst[bi], l);
            bi++;
        }
    }
    ce_free(cst); ce_free(cln); ce_free(ed);
    /* write op count */
    uint32_t nc = (uint32_t)ne;
    unsigned char hb[4] = { nc & 0xFF, (nc>>8)&0xFF, (nc>>16)&0xFF, (nc>>24)&0xFF };
    memcpy(b.data, hb, 4);
    *out_len = b.len;
    return (unsigned char*)ce_buf_detach(&b);
}

static char *delta_decode(const char *parent, size_t plen,
                          const unsigned char *payload, size_t plen2, size_t *out_len){
    if(plen2 < 4) return NULL;
    uint32_t nc = payload[0] | (payload[1]<<8) | (payload[2]<<16) | ((uint32_t)payload[3]<<24);
    size_t *pst = NULL, *pln = NULL;
    size_t np = split_nl(parent, plen, &pst, &pln);
    ce_buf b; ce_buf_init(&b);
    size_t pi = 0;   /* parent line index */
    size_t pos = 4;
    bool ok = true;
    for(uint32_t i = 0; i < nc; i++){
        if(pos >= plen2){ ok = false; break; }
        unsigned char t = payload[pos++];
        if(t == 0){  /* equal: copy parent line */
            if(pi >= np){ ok = false; break; }
            ce_buf_append(&b, parent + pst[pi], pln[pi]);
            pi++;
        } else if(t == 1){ /* del */
            if(pi >= np){ ok = false; break; }
            pi++;
        } else if(t == 2){ /* ins */
            if(pos + 4 > plen2){ ok = false; break; }
            uint32_t l = payload[pos] | (payload[pos+1]<<8) | (payload[pos+2]<<16) | ((uint32_t)payload[pos+3]<<24);
            pos += 4;
            if(pos + l > plen2){ ok = false; break; }
            ce_buf_append(&b, payload + pos, l);
            pos += l;
        } else { ok = false; break; }
    }
    ce_free(pst); ce_free(pln);
    if(!ok){ ce_buf_free(&b); return NULL; }
    *out_len = b.len;
    return ce_buf_detach(&b);
}

/* compress payload if beneficial; sets *compressed */
static unsigned char *maybe_compress(const unsigned char *data, size_t len, size_t *out_len, bool *compressed){
    size_t clen = 0;
    unsigned char *c = ce_lzss_compress(data, len, &clen);
    if(c && clen < len){
        *out_len = clen; *compressed = true;
        return c;
    }
    if(c) ce_free(c);
    unsigned char *raw = ce_malloc(len ? len : 1);
    memcpy(raw, data, len);
    *out_len = len; *compressed = false;
    return raw;
}

static unsigned char *maybe_decompress(const unsigned char *data, size_t len, bool compressed, size_t *out_len){
    if(!compressed){
        unsigned char *r = ce_malloc(len ? len : 1);
        memcpy(r, data, len);
        *out_len = len;
        return r;
    }
    return ce_lzss_decompress(data, len, out_len);
}

int md_history_add(md_history *h, const char *content, size_t len, uint64_t timestamp){
    md_version *v;
    if(h->n == h->cap){ h->cap = h->cap ? h->cap * 2 : 32; h->v = ce_realloc(h->v, h->cap * sizeof(md_version)); }
    v = &h->v[h->n];
    memset(v, 0, sizeof(*v));
    v->id = h->next_id++;
    v->timestamp = timestamp;
    v->parent = (h->n > 0) ? h->v[h->n - 1].id : 0;

    bool snapshot = (h->n == 0) || ((h->n % h->snapshot_interval) == 0);
    v->is_snapshot = snapshot;

    if(snapshot){
        v->payload = maybe_compress((const unsigned char*)content, len, &v->payload_len, &v->compressed);
    } else {
        /* reconstruct parent to build a delta */
        size_t plen = 0;
        char *pcontent = md_history_get(h, h->n - 1, &plen);
        if(pcontent){
            size_t dlen = 0;
            unsigned char *d = delta_encode(pcontent, plen, content, len, &dlen);
            v->payload = maybe_compress(d, dlen, &v->payload_len, &v->compressed);
            ce_free(d);
            ce_free(pcontent);
        } else {
            /* parent unavailable: fall back to snapshot */
            v->is_snapshot = true;
            v->payload = maybe_compress((const unsigned char*)content, len, &v->payload_len, &v->compressed);
        }
    }
    /* cache content */
    v->cached = ce_strndup(content, len);
    v->cached_len = len;
    h->total_payload += v->payload_len;
    h->n++;
    return (int)(h->n - 1);
}

char *md_history_get(md_history *h, size_t index, size_t *out_len){
    if(index >= h->n) return NULL;
    md_version *v = &h->v[index];
    if(v->corrupt) return NULL;
    if(v->cached){
        if(out_len) *out_len = v->cached_len;
        return ce_strdup(v->cached);
    }
    /* find nearest preceding snapshot */
    size_t snap = index;
    while(snap > 0 && !h->v[snap].is_snapshot) snap--;
    md_version *sv = &h->v[snap];
    size_t slen = 0;
    unsigned char *sraw = maybe_decompress(sv->payload, sv->payload_len, sv->compressed, &slen);
    if(!sraw){ v->corrupt = true; return NULL; }
    char *cur = ce_strndup((const char*)sraw, slen);
    ce_free(sraw);
    size_t curlen = slen;
    bool ok = true;
    for(size_t k = snap + 1; k <= index; k++){
        md_version *kv = &h->v[k];
        size_t rlen = 0;
        unsigned char *d = maybe_decompress(kv->payload, kv->payload_len, kv->compressed, &rlen);
        if(!d){ ok = false; break; }
        size_t nextlen = 0;
        char *next = delta_decode(cur, curlen, d, rlen, &nextlen);
        ce_free(d);
        if(!next){ ok = false; break; }
        ce_free(cur);
        cur = next; curlen = nextlen;
    }
    if(!ok){ ce_free(cur); v->corrupt = true; return NULL; }
    v->cached = ce_strndup(cur, curlen);
    v->cached_len = curlen;
    if(out_len) *out_len = curlen;
    return cur;
}

void md_history_pin(md_history *h, size_t index, bool pinned){
    if(index < h->n) h->v[index].pinned = pinned;
}

bool md_history_delete(md_history *h, size_t index){
    if(index >= h->n) return false;
    md_version *v = &h->v[index];
    if(v->payload) ce_free(v->payload);
    if(v->cached) ce_free(v->cached);
    memmove(&h->v[index], &h->v[index + 1], (h->n - index - 1) * sizeof(md_version));
    h->n--;
    return true;
}

bool md_history_prune(md_history *h){
    bool blocked = false;
    /* prune oldest while over limits; never delete pinned */
    while(h->n > h->max_versions || h->total_payload > h->max_payload){
        if(h->n == 0) break;
        size_t victim = SIZE_MAX;
        for(size_t i = 0; i < h->n; i++){
            if(!h->v[i].pinned){ victim = i; break; }
        }
        if(victim == SIZE_MAX){ blocked = true; break; }
        md_history_delete(h, victim);
        /* recompute total_payload */
        h->total_payload = 0;
        for(size_t i = 0; i < h->n; i++) h->total_payload += h->v[i].payload_len;
    }
    return blocked;
}

unsigned char *md_history_serialize(md_history *h, size_t *out_len){
    ce_buf b; ce_buf_init(&b);
    ce_buf_append(&b, MAGIC, 6);
    uint32_t nc = (uint32_t)h->n;
    unsigned char hb[4] = { nc & 0xFF, (nc>>8)&0xFF, (nc>>16)&0xFF, (nc>>24)&0xFF };
    ce_buf_append(&b, hb, 4);
    for(size_t i = 0; i < h->n; i++){
        md_version *v = &h->v[i];
        ce_buf rec; ce_buf_init(&rec);
        ce_buf_append(&rec, &v->id, 8);
        ce_buf_append(&rec, &v->timestamp, 8);
        ce_buf_append(&rec, &v->parent, 8);
        unsigned char flags = (v->compressed ? 1 : 0) | (v->is_snapshot ? 2 : 0) | (v->pinned ? 4 : 0);
        ce_buf_append_c(&rec, (char)flags);
        uint32_t pl = (uint32_t)v->payload_len;
        unsigned char pb[4] = { pl & 0xFF, (pl>>8)&0xFF, (pl>>16)&0xFF, (pl>>24)&0xFF };
        ce_buf_append(&rec, pb, 4);
        ce_buf_append(&rec, v->payload, v->payload_len);
        /* sha256 of rec */
        uint8_t sha[32];
        ce_sha256_hash(rec.data, rec.len, sha);
        ce_buf_append(&rec, sha, 32);
        /* append rec length + rec */
        uint32_t rl = (uint32_t)rec.len;
        unsigned char rb[4] = { rl & 0xFF, (rl>>8)&0xFF, (rl>>16)&0xFF, (rl>>24)&0xFF };
        ce_buf_append(&b, rb, 4);
        ce_buf_append(&b, rec.data, rec.len);
        ce_buf_free(&rec);
    }
    *out_len = b.len;
    return (unsigned char*)ce_buf_detach(&b);
}

md_history *md_history_load(const unsigned char *data, size_t len){
    if(len < 10 || memcmp(data, MAGIC, 6) != 0) return NULL;
    uint32_t nc = data[6] | (data[7]<<8) | (data[8]<<16) | ((uint32_t)data[9]<<24);
    md_history *h = md_history_create();
    size_t pos = 10;
    for(uint32_t i = 0; i < nc; i++){
        if(pos + 4 > len) break;
        uint32_t rl = data[pos] | (data[pos+1]<<8) | (data[pos+2]<<16) | ((uint32_t)data[pos+3]<<24);
        pos += 4;
        if(pos + rl > len || rl < 33) break;
        const unsigned char *rec = data + pos;
        pos += rl;
        /* verify sha (last 32 bytes) */
        uint8_t sha[32];
        ce_sha256_hash(rec, rl - 32, sha);
        bool ok = (memcmp(sha, rec + rl - 32, 32) == 0);
        md_version v; memset(&v, 0, sizeof(v));
        memcpy(&v.id, rec, 8);
        memcpy(&v.timestamp, rec + 8, 8);
        memcpy(&v.parent, rec + 16, 8);
        unsigned char flags = rec[24];
        v.compressed = (flags & 1) != 0;
        v.is_snapshot = (flags & 2) != 0;
        v.pinned = (flags & 4) != 0;
        uint32_t pl = rec[25] | (rec[26]<<8) | (rec[27]<<16) | ((uint32_t)rec[28]<<24);
        v.payload_len = pl;
        v.payload = ce_malloc(pl ? pl : 1);
        memcpy(v.payload, rec + 29, pl);
        v.corrupt = !ok;
        if(h->n == h->cap){ h->cap = h->cap ? h->cap * 2 : 32; h->v = ce_realloc(h->v, h->cap * sizeof(md_version)); }
        h->v[h->n++] = v;
        if(v.id >= h->next_id) h->next_id = v.id + 1;
        h->total_payload += pl;
    }
    return h;
}
