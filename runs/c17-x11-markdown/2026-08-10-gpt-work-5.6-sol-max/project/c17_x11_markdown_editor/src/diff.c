#include "mdedit/diff.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *data;
    size_t len;
    size_t offset;
} DiffSlice;

typedef struct {
    DiffSlice *items;
    size_t len;
    size_t cap;
} SliceVec;

typedef struct {
    MdDiffKind *items;
    size_t len;
    size_t cap;
} KindVec;

static void diff_error(char *error,size_t cap,const char *message) {
    if (error!=NULL&&cap!=0U) (void)snprintf(error,cap,"%s",message);
}

static bool reserve(void **items,size_t *cap,size_t needed,size_t item_size) {
    if (needed<=*cap) return true;
    size_t next=*cap==0U?16U:*cap;
    while (next<needed) { if (next>SIZE_MAX/2U) return false; next*=2U; }
    size_t bytes=0U; if (!md_size_mul(next,item_size,&bytes)) return false;
    void *p=realloc(*items,bytes); if (p==NULL) return false;
    *items=p; *cap=next; return true;
}

static bool slices_push(SliceVec *v,DiffSlice item) {
    if (!reserve((void **)&v->items,&v->cap,v->len+1U,sizeof(*v->items))) return false;
    v->items[v->len++]=item; return true;
}

static bool kinds_push(KindVec *v,MdDiffKind kind) {
    if (!reserve((void **)&v->items,&v->cap,v->len+1U,sizeof(*v->items))) return false;
    v->items[v->len++]=kind; return true;
}

static bool slice_equal(DiffSlice a,DiffSlice b) {
    return a.len==b.len&&(a.len==0U||memcmp(a.data,b.data,a.len)==0);
}

static bool split_lines(const char *s,size_t len,SliceVec *out) {
    size_t at=0U;
    while (at<len) {
        size_t end=at;
        while (end<len&&s[end]!='\n') ++end;
        if (end<len) ++end;
        if (!slices_push(out,(DiffSlice){s+at,end-at,at})) return false;
        at=end;
    }
    return true;
}

static bool token_class(uint32_t cp,int *class_out) {
    if (md_unicode_is_space(cp)) *class_out=0;
    else if ((cp>='A'&&cp<='Z')||(cp>='a'&&cp<='z')||(cp>='0'&&cp<='9')||cp=='_') *class_out=1;
    else if (cp>=0x80U) *class_out=2;
    else *class_out=3;
    return true;
}

static bool split_tokens(const char *s,size_t len,SliceVec *out) {
    size_t at=0U;
    while (at<len) {
        size_t start=at,next=at; uint32_t cp=0U;
        if (!md_utf8_decode(s,len,&next,&cp)) return false;
        int cls=0; (void)token_class(cp,&cls);
        at=next;
        if (cls==2) {
            at=md_grapheme_next(s,len,start);
        } else {
            while (at<len) {
                size_t look=at; uint32_t other=0U;
                if (!md_utf8_decode(s,len,&look,&other)) return false;
                int other_cls=0; (void)token_class(other,&other_cls);
                if (other_cls!=cls||cls==2) break;
                at=look;
            }
        }
        if (!slices_push(out,(DiffSlice){s+start,at-start,start})) return false;
    }
    return true;
}

void md_diff_init(MdDiff *diff) { memset(diff,0,sizeof(*diff)); }

void md_diff_free(MdDiff *diff) { free(diff->hunks); memset(diff,0,sizeof(*diff)); }

static bool diff_hunk(MdDiff *out,MdDiffKind kind,size_t a_start,size_t a_count,
                      size_t b_start,size_t b_count) {
    if (out->count>0U) {
        MdDiffHunk *last=&out->hunks[out->count-1U];
        if (last->kind==kind&&last->a_start+last->a_count==a_start&&
            last->b_start+last->b_count==b_start) {
            last->a_count+=a_count; last->b_count+=b_count; return true;
        }
    }
    if (!reserve((void **)&out->hunks,&out->cap,out->count+1U,sizeof(*out->hunks))) return false;
    out->hunks[out->count++]=(MdDiffHunk){kind,a_start,a_count,b_start,b_count};
    return true;
}

static void free_trace(ptrdiff_t **trace,size_t count) {
    for (size_t i=0U;i<count;++i) free(trace[i]);
    free(trace);
}

static bool myers(const SliceVec *a,const SliceVec *b,MdDiff *out,
                  char *error,size_t error_cap) {
    md_diff_free(out);
    size_t max=0U;
    if (!md_size_add(a->len,b->len,&max)||max>(size_t)PTRDIFF_MAX/2U) {
        diff_error(error,error_cap,"Diff input is too large"); return false;
    }
    if (max==0U) return true;
    size_t width=max*2U+3U;
    ptrdiff_t *v=malloc(width*sizeof(*v));
    ptrdiff_t **trace=calloc(max+1U,sizeof(*trace));
    if (v==NULL||trace==NULL) { free(v); free(trace); diff_error(error,error_cap,"Out of memory computing diff"); return false; }
    for (size_t i=0U;i<width;++i) v[i]=-1;
    ptrdiff_t offset=(ptrdiff_t)max+1;
    v[(size_t)(offset+1)]=0;
    size_t found=0U;
    bool done=false;
    for (size_t d=0U;d<=max&&!done;++d) {
        ptrdiff_t dd=(ptrdiff_t)d;
        for (ptrdiff_t k=-dd;k<=dd;k+=2) {
            ptrdiff_t x;
            if (k==-dd||(k!=dd&&v[(size_t)(offset+k-1)]<v[(size_t)(offset+k+1)]))
                x=v[(size_t)(offset+k+1)];
            else x=v[(size_t)(offset+k-1)]+1;
            ptrdiff_t y=x-k;
            while (x<(ptrdiff_t)a->len&&y<(ptrdiff_t)b->len&&x>=0&&y>=0&&
                  slice_equal(a->items[(size_t)x],b->items[(size_t)y])) { ++x; ++y; }
            v[(size_t)(offset+k)]=x;
            if (x>=(ptrdiff_t)a->len&&y>=(ptrdiff_t)b->len) { found=d; done=true; break; }
        }
        trace[d]=malloc(width*sizeof(**trace));
        if (trace[d]==NULL) {
            free(v); free_trace(trace,d); diff_error(error,error_cap,"Out of memory tracing diff"); return false;
        }
        memcpy(trace[d],v,width*sizeof(*v));
    }
    free(v);
    KindVec reverse={0};
    ptrdiff_t x=(ptrdiff_t)a->len,y=(ptrdiff_t)b->len;
    for (size_t d=found;d>0U;--d) {
        ptrdiff_t *prev=trace[d-1U];
        ptrdiff_t k=x-y,dd=(ptrdiff_t)d,prev_k;
        if (k==-dd||(k!=dd&&prev[(size_t)(offset+k-1)]<prev[(size_t)(offset+k+1)])) prev_k=k+1;
        else prev_k=k-1;
        ptrdiff_t prev_x=prev[(size_t)(offset+prev_k)];
        ptrdiff_t prev_y=prev_x-prev_k;
        while (x>prev_x&&y>prev_y) {
            if (!kinds_push(&reverse,MD_DIFF_EQUAL)) goto oom;
            --x; --y;
        }
        if (x==prev_x) { if (!kinds_push(&reverse,MD_DIFF_INSERT)) goto oom; --y; }
        else { if (!kinds_push(&reverse,MD_DIFF_DELETE)) goto oom; --x; }
    }
    while (x>0&&y>0) { if (!kinds_push(&reverse,MD_DIFF_EQUAL)) goto oom; --x; --y; }
    while (x>0) { if (!kinds_push(&reverse,MD_DIFF_DELETE)) goto oom; --x; }
    while (y>0) { if (!kinds_push(&reverse,MD_DIFF_INSERT)) goto oom; --y; }
    {
        size_t ai=0U,bi=0U;
        for (size_t i=reverse.len;i>0U;--i) {
            MdDiffKind kind=reverse.items[i-1U];
            size_t ac=kind==MD_DIFF_INSERT?0U:1U;
            size_t bc=kind==MD_DIFF_DELETE?0U:1U;
            if (!diff_hunk(out,kind,ai,ac,bi,bc)) goto oom;
            ai+=ac; bi+=bc;
        }
    }
    free(reverse.items); free_trace(trace,found+1U); return true;
oom:
    free(reverse.items); free_trace(trace,found+1U); md_diff_free(out);
    diff_error(error,error_cap,"Out of memory constructing diff"); return false;
}

bool md_diff_lines(const char *a,size_t a_len,const char *b,size_t b_len,
                   MdDiff *out,char *error,size_t error_cap) {
    SliceVec av={0},bv={0};
    bool ok=split_lines(a,a_len,&av)&&split_lines(b,b_len,&bv)&&myers(&av,&bv,out,error,error_cap);
    if (!ok&&error!=NULL&&error[0]=='\0') diff_error(error,error_cap,"Out of memory splitting diff lines");
    free(av.items); free(bv.items); return ok;
}

bool md_diff_tokens(const char *a,size_t a_len,const char *b,size_t b_len,
                    MdDiff *out,char *error,size_t error_cap) {
    SliceVec av={0},bv={0};
    bool ok=split_tokens(a,a_len,&av)&&split_tokens(b,b_len,&bv)&&myers(&av,&bv,out,error,error_cap);
    if (!ok&&error!=NULL&&error[0]=='\0') diff_error(error,error_cap,"Out of memory splitting diff tokens");
    free(av.items); free(bv.items); return ok;
}

static bool put_u32(MdBytes *out,uint32_t value) {
    uint8_t b[4]={(uint8_t)value,(uint8_t)(value>>8U),(uint8_t)(value>>16U),(uint8_t)(value>>24U)};
    return md_bytes_append(out,b,sizeof(b));
}

static bool put_u64(MdBytes *out,uint64_t value) {
    uint8_t b[8]; for (size_t i=0U;i<8U;++i) b[i]=(uint8_t)(value>>(i*8U));
    return md_bytes_append(out,b,sizeof(b));
}

static bool get_u32(const uint8_t *p,size_t len,size_t *at,uint32_t *value) {
    if (*at+4U>len) return false;
    *value=(uint32_t)p[*at]|((uint32_t)p[*at+1U]<<8U)|((uint32_t)p[*at+2U]<<16U)|((uint32_t)p[*at+3U]<<24U);
    *at+=4U; return true;
}

static bool get_u64(const uint8_t *p,size_t len,size_t *at,uint64_t *value) {
    if (*at+8U>len) return false;
    uint64_t v=0U; for (size_t i=0U;i<8U;++i) v|=(uint64_t)p[*at+i]<<(i*8U);
    *at+=8U; *value=v; return true;
}

bool md_delta_encode(const char *base,size_t base_len,const char *target,size_t target_len,
                     MdBytes *out,char *error,size_t error_cap) {
    SliceVec av={0},bv={0}; MdDiff diff; md_diff_init(&diff); out->len=0U;
    if (!split_lines(base,base_len,&av)||!split_lines(target,target_len,&bv)||
        !myers(&av,&bv,&diff,error,error_cap)) goto fail;
    if (!md_bytes_append(out,"MDD1",4U)||!put_u64(out,(uint64_t)target_len)||
        !put_u32(out,(uint32_t)diff.count)) goto oom;
    for (size_t i=0U;i<diff.count;++i) {
        MdDiffHunk h=diff.hunks[i]; uint8_t kind=(uint8_t)h.kind;
        if (!md_bytes_append(out,&kind,1U)) goto oom;
        if (h.kind==MD_DIFF_INSERT) {
            size_t start=h.b_start<bv.len?bv.items[h.b_start].offset:target_len;
            size_t end=h.b_start+h.b_count<bv.len?bv.items[h.b_start+h.b_count].offset:target_len;
            if (!put_u64(out,(uint64_t)(end-start))||!md_bytes_append(out,target+start,end-start)) goto oom;
        } else {
            size_t start=h.a_start<av.len?av.items[h.a_start].offset:base_len;
            size_t end=h.a_start+h.a_count<av.len?av.items[h.a_start+h.a_count].offset:base_len;
            if (!put_u64(out,(uint64_t)start)||!put_u64(out,(uint64_t)(end-start))) goto oom;
        }
    }
    free(av.items); free(bv.items); md_diff_free(&diff); return true;
oom:
    diff_error(error,error_cap,"Out of memory encoding delta");
fail:
    out->len=0U; free(av.items); free(bv.items); md_diff_free(&diff); return false;
}

bool md_delta_apply(const char *base,size_t base_len,const uint8_t *delta,size_t delta_len,
                    MdBuf *out,char *error,size_t error_cap) {
    size_t at=0U; uint64_t target_len=0U; uint32_t count=0U; out->len=0U;
    if (delta_len<4U||memcmp(delta,"MDD1",4U)!=0) goto malformed;
    at=4U;
    if (!get_u64(delta,delta_len,&at,&target_len)||target_len>SIZE_MAX||
        !get_u32(delta,delta_len,&at,&count)||!md_buf_reserve(out,(size_t)target_len)) goto malformed;
    out->data[0]='\0';
    for (uint32_t i=0U;i<count;++i) {
        if (at>=delta_len) goto malformed;
        uint8_t kind=delta[at++];
        if (kind==(uint8_t)MD_DIFF_INSERT) {
            uint64_t bytes=0U; if (!get_u64(delta,delta_len,&at,&bytes)||bytes>delta_len-at) goto malformed;
            if (!md_buf_append(out,(const char *)delta+at,(size_t)bytes)) goto oom;
            at+=(size_t)bytes;
        } else if (kind==(uint8_t)MD_DIFF_EQUAL||kind==(uint8_t)MD_DIFF_DELETE) {
            uint64_t start=0U,bytes=0U;
            if (!get_u64(delta,delta_len,&at,&start)||!get_u64(delta,delta_len,&at,&bytes)||
                start>base_len||bytes>base_len-(size_t)start) goto malformed;
            if (kind==(uint8_t)MD_DIFF_EQUAL&&!md_buf_append(out,base+(size_t)start,(size_t)bytes)) goto oom;
        } else goto malformed;
    }
    if (at!=delta_len||out->len!=(size_t)target_len) goto malformed;
    return true;
oom:
    diff_error(error,error_cap,"Out of memory applying delta"); out->len=0U; return false;
malformed:
    diff_error(error,error_cap,"Malformed or inconsistent delta record"); out->len=0U; return false;
}

static uint16_t lz_hash3(const uint8_t *p) {
    return (uint16_t)(((uint32_t)p[0]*251U+(uint32_t)p[1]*31U+(uint32_t)p[2])&0xffffU);
}

bool md_lzss_compress(const uint8_t *input,size_t len,MdBytes *out) {
    out->len=0U;
    size_t *prev=len==0U?NULL:malloc(len*sizeof(*prev));
    size_t *head=malloc(65536U*sizeof(*head));
    if ((len!=0U&&prev==NULL)||head==NULL) { free(prev); free(head); return false; }
    for (size_t i=0U;i<65536U;++i) head[i]=SIZE_MAX;
    size_t pos=0U;
    while (pos<len) {
        size_t flag_pos=out->len; uint8_t flags=0U;
        if (!md_bytes_append(out,&flags,1U)) goto oom;
        for (unsigned token=0U;token<8U&&pos<len;++token) {
            size_t best_len=0U,best_distance=0U;
            if (pos+3U<=len) {
                uint16_t hash=lz_hash3(input+pos); size_t candidate=head[hash]; unsigned checked=0U;
                while (candidate!=SIZE_MAX&&pos>candidate&&pos-candidate<=4096U&&checked<96U) {
                    size_t n=0U,limit=MD_MIN((size_t)18U,len-pos);
                    while (n<limit&&input[candidate+n]==input[pos+n]) ++n;
                    if (n>=3U&&n>best_len) { best_len=n; best_distance=pos-candidate; if (n==limit) break; }
                    candidate=prev[candidate]; ++checked;
                }
            }
            size_t consumed=1U;
            if (best_len>=3U) {
                uint16_t code=(uint16_t)(((best_distance-1U)<<4U)|(best_len-3U));
                uint8_t bytes[2]={(uint8_t)code,(uint8_t)(code>>8U)};
                if (!md_bytes_append(out,bytes,2U)) goto oom;
                flags|=(uint8_t)(1U<<token); consumed=best_len;
            } else if (!md_bytes_append(out,input+pos,1U)) goto oom;
            for (size_t add=0U;add<consumed;++add) {
                size_t here=pos+add;
                if (here+3U<=len) {
                    uint16_t hash=lz_hash3(input+here); prev[here]=head[hash]; head[hash]=here;
                } else prev[here]=SIZE_MAX;
            }
            pos+=consumed;
        }
        out->data[flag_pos]=flags;
    }
    free(prev); free(head); return true;
oom:
    free(prev); free(head); out->len=0U; return false;
}

bool md_lzss_decompress(const uint8_t *input,size_t len,size_t expected_len,MdBytes *out,
                        char *error,size_t error_cap) {
    out->len=0U;
    if (!md_bytes_reserve(out,expected_len)) { diff_error(error,error_cap,"Out of memory decompressing LZSS"); return false; }
    size_t at=0U;
    while (at<len&&out->len<expected_len) {
        uint8_t flags=input[at++];
        for (unsigned token=0U;token<8U&&out->len<expected_len;++token) {
            if ((flags&(uint8_t)(1U<<token))!=0U) {
                if (at+2U>len) goto malformed;
                uint16_t code=(uint16_t)input[at]|((uint16_t)input[at+1U]<<8U); at+=2U;
                size_t distance=((size_t)code>>4U)+1U,match=(size_t)(code&0x0fU)+3U;
                if (distance==0U||distance>4096U||distance>out->len||match>expected_len-out->len) goto malformed;
                for (size_t i=0U;i<match;++i) {
                    uint8_t byte=out->data[out->len-distance];
                    if (!md_bytes_append(out,&byte,1U)) goto oom2;
                }
            } else {
                if (at>=len||!md_bytes_append(out,input+at,1U)) goto malformed;
                ++at;
            }
        }
    }
    if (at!=len||out->len!=expected_len) goto malformed;
    return true;
oom2:
    diff_error(error,error_cap,"Out of memory decompressing LZSS"); out->len=0U; return false;
malformed:
    diff_error(error,error_cap,"Malformed LZSS stream"); out->len=0U; return false;
}

