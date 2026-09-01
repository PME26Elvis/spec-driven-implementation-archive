#include "diff.h"
#include "utf8.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int diff_split_lines(const uint8_t *data, size_t len,
                     size_t **line_offsets, size_t **line_lengths, size_t *count){
    size_t cap = 16, n = 0;
    size_t *off = (size_t*)malloc(cap*sizeof(size_t));
    size_t *len_ = (size_t*)malloc(cap*sizeof(size_t));
    size_t i = 0;
    if(!off || !len_){ free(off); free(len_); *line_offsets=NULL; *line_lengths=NULL; *count=0; return -1; }
    while(i < len){
        if(n == cap){
            size_t ncap = cap * 2;
            size_t *no = (size_t*)realloc(off, ncap*sizeof(size_t));
            size_t *nl = (size_t*)realloc(len_, ncap*sizeof(size_t));
            if(!no || !nl){
                /* free only the pointers that were NOT already realloc'd away */
                if(!no) free(off); else free(no);
                if(!nl) free(len_); else free(nl);
                *line_offsets=NULL;*line_lengths=NULL;*count=0; return -1;
            }
            off=no; len_=nl; cap=ncap;
        }
        off[n] = i;
        size_t j = i;
        while(j < len && data[j] != '\n') j++;
        if(j < len) j++; /* include '\n' */
        len_[n] = j - i;
        n++;
        i = j;
    }
    *line_offsets = off;
    *line_lengths = len_;
    *count = n;
    return 0;
}

static int lines_equal(const uint8_t *ad, const size_t *ao, const size_t *al, size_t ai,
                       const uint8_t *bd, const size_t *bo, const size_t *bl, size_t bi){
    if(al[ai] != bl[bi]) return 0;
    return memcmp(ad+ao[ai], bd+bo[bi], al[ai]) == 0;
}

int diff_myers(const uint8_t *old_data, const size_t *old_off, const size_t *old_len, size_t old_n,
               const uint8_t *new_data, const size_t *new_off, const size_t *new_len, size_t new_n,
               DiffEdit **edits, size_t *n_edits){
    size_t maxd = old_n + new_n;
    size_t vsize = 2*maxd + 3;
    size_t off = maxd + 1;
    int *v = (int*)malloc(vsize * sizeof(int));
    if(!v) return -1;
    int *snaps = (int*)malloc((maxd+1) * vsize * sizeof(int));
    if(!snaps){ free(v); return -1; }
    for(size_t i=0;i<vsize;i++) v[i]=0;
    size_t D = maxd;
    int found = 0;
    for(size_t d=0; d<=maxd; d++){
        memcpy(snaps + d*vsize, v, vsize*sizeof(int));
        for(int k=(int)(0-d); k<=(int)d; k+=2){
            int x;
            size_t idx = off + (size_t)k;
            if(k == -(int)d || (k != (int)d && v[idx-1] < v[idx+1])){
                x = v[idx+1];            /* take DOWN (insert) */
            } else {
                x = v[idx-1] + 1;        /* take RIGHT (delete) */
            }
            int y = x - k;
            while((size_t)x < old_n && (size_t)y < new_n &&
                  lines_equal(old_data,old_off,old_len,(size_t)x,new_data,new_off,new_len,(size_t)y)){
                x++; y++;
            }
            v[idx] = x;
            if((size_t)x >= old_n && (size_t)y >= new_n){
                D = d; found = 1;
                break;
            }
        }
        if(found) break;
    }
    if(!found){ free(v); free(snaps); *edits=NULL; *n_edits=0; return 0; }

    /* Backtrack from (old_n, new_n) building ops in reverse. */
    DiffEdit *rev = NULL; size_t rev_n=0, rev_cap=0;
    size_t x = old_n, y = new_n;
    for(size_t d=D; d>0; d--){
        int *vd  = snaps + d*vsize;
        int k = (int)x - (int)y;
        size_t idx = off + (size_t)k;
        int prevk, came_down;
        if(k == -(int)d || (k != (int)d && vd[idx-1] < vd[idx+1])){
            prevk = k + 1; came_down = 1;   /* came by DOWN = insert */
        } else {
            prevk = k - 1; came_down = 0;   /* came by RIGHT = delete */
        }
        int prevx = vd[off + (size_t)prevk];
        int prevy = prevx - prevk;
        /* snake back */
        while(x > (size_t)prevx && y > (size_t)prevy){
            if(rev_n==rev_cap){ rev_cap=rev_cap?rev_cap*2:16; DiffEdit*nr=(DiffEdit*)realloc(rev,rev_cap*sizeof(DiffEdit)); if(!nr) goto oom; rev=nr; }
            rev[rev_n].op=0; rev[rev_n].old_line=x-1; rev[rev_n].new_line=y-1; rev[rev_n].count=1; rev_n++;
            x--; y--;
        }
        if(came_down){
            if(rev_n==rev_cap){ rev_cap=rev_cap?rev_cap*2:16; DiffEdit*nr=(DiffEdit*)realloc(rev,rev_cap*sizeof(DiffEdit)); if(!nr) goto oom; rev=nr; }
            rev[rev_n].op=2; rev[rev_n].old_line=x; rev[rev_n].new_line=y-1; rev[rev_n].count=1; rev_n++;
            y--;
        } else {
            if(rev_n==rev_cap){ rev_cap=rev_cap?rev_cap*2:16; DiffEdit*nr=(DiffEdit*)realloc(rev,rev_cap*sizeof(DiffEdit)); if(!nr) goto oom; rev=nr; }
            rev[rev_n].op=1; rev[rev_n].old_line=x-1; rev[rev_n].new_line=y; rev[rev_n].count=1; rev_n++;
            x--;
        }
    }
    /* Reverse to forward order and consolidate runs of same op. */
    DiffEdit *fwd = (DiffEdit*)malloc(rev_n * sizeof(DiffEdit));
    if(!fwd){ free(rev); goto oom; }
    size_t fn = 0;
    for(size_t i=rev_n; i>0; i--){
        DiffEdit e = rev[i-1];
        /* Consolidate: if last op equals and lines are contiguous, merge.
           For forward building, track expected continuation. Simpler:
           append and then merge adjacent same-op runs. */
        if(fn>0 && fwd[fn-1].op==e.op){
            if(e.op==0){ /* keep */ if(fwd[fn-1].old_line+fwd[fn-1].count==e.old_line && fwd[fn-1].new_line+fwd[fn-1].count==e.new_line){ fwd[fn-1].count++; continue; } }
            else if(e.op==1){ if(fwd[fn-1].old_line+fwd[fn-1].count==e.old_line && fwd[fn-1].new_line==e.new_line){ fwd[fn-1].count++; continue; } }
            else { /* insert */ if(fwd[fn-1].new_line+fwd[fn-1].count==e.new_line && fwd[fn-1].old_line==e.old_line){ fwd[fn-1].count++; continue; } }
        }
        fwd[fn++]=e;
    }
    /* Snakes produce long keep runs; that's fine. */
    free(rev);
    /* Re-shrink to fn */
    *n_edits = fn;
    *edits = fwd;
    free(v); free(snaps);
    return 0;
oom:
    free(v); free(snaps); free(rev); *edits=NULL; *n_edits=0;
    return -1;
}

void diff_count(const DiffEdit *edits, size_t n, size_t *ins, size_t *del){
    size_t i, is=0, ds=0;
    for(i=0;i<n;i++){
        if(edits[i].op==1) ds += edits[i].count;
        else if(edits[i].op==2) is += edits[i].count;
    }
    if(ins) *ins=is;
    if(del) *del=ds;
}

static void out_push(uint8_t **buf, size_t *len, size_t *cap, const char *s, size_t n){
    if(*len + n + 1 > *cap){
        size_t nc = *cap ? *cap*2 : 64;
        while(nc < *len+n+1) nc*=2;
        uint8_t *nb=(uint8_t*)realloc(*buf,nc); if(!nb) return;
        *buf=nb; *cap=nc;
    }
    memcpy(*buf+*len, s, n); *len+=n; (*buf)[*len]=0;
}
static void out_byte(uint8_t **buf,size_t *len,size_t *cap,uint8_t c){ out_push(buf,len,cap,(const char*)&c,1); }

char *diff_render_line(const uint8_t *line, size_t len){
    uint8_t *buf=NULL; size_t l=0,c=0;
    size_t i=0;
    while(i<len){
        uint8_t b = line[i];
        if(b==0x09){ out_push(&buf,&l,&c,"\\t",2); i++; continue; }
        if(b==0x0D){ out_push(&buf,&l,&c,"\\r",2); i++; continue; }
        if(b==0x08){ out_push(&buf,&l,&c,"\\b",2); i++; continue; }
        if(b==0x0C){ out_push(&buf,&l,&c,"\\f",2); i++; continue; }
        if(b=='\\'){ out_push(&buf,&l,&c,"\\\\",2); i++; continue; }
        if(b>=0x20 && b<=0x7E){ out_byte(&buf,&l,&c,b); i++; continue; }
        uint32_t cp;
        int k = utf8_decode(line+i, len-i, &cp);
        if(k>0 && cp>=0x80){
            out_push(&buf,&l,&c,(const char*)(line+i),(size_t)k);
            i+=(size_t)k; continue;
        }
        char hx[8];
        snprintf(hx,sizeof hx,"\\x%02X", b);
        out_push(&buf,&l,&c,hx,4);
        i++;
    }
    if(!buf){ buf=(uint8_t*)calloc(1,1); }
    return (char*)buf;
}
