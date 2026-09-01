/* search.c - literal find. */
#include "search.h"
#include "ce_common.h"
#include "utf8.h"

static inline int fold_c(int c, bool fold){
    if(fold && c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static bool matches_at(const char *src, size_t len, size_t pos,
                       const char *needle, size_t nlen, bool fold){
    if(pos + nlen > len) return false;
    for(size_t i = 0; i < nlen; i++){
        if(fold_c((unsigned char)src[pos+i], fold) != fold_c((unsigned char)needle[i], fold)) return false;
    }
    return true;
}

bool md_is_word_char(const char *src, size_t len, size_t pos){
    if(pos >= len) return false;
    unsigned char c = (unsigned char)src[pos];
    if(c < 0x80){
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    }
    /* non-ASCII lead byte: treat as word char */
    return (c & 0xC0) == 0xC0;
}

size_t md_find_all(const char *src, size_t len, const char *needle, size_t nlen,
                   bool case_sensitive, bool whole_word, md_match **out){
    md_match *matches = NULL; size_t count = 0, cap = 0;
    if(nlen == 0){ *out = NULL; return 0; }
    size_t i = 0;
    while(i + nlen <= len){
        /* skip UTF-8 continuation bytes (never a valid match start) */
        if((unsigned char)src[i] >= 0x80 && (unsigned char)src[i] < 0xC0){ i++; continue; }
        if(matches_at(src, len, i, needle, nlen, !case_sensitive)){
            bool ok = true;
            if(whole_word){
                if(i > 0 && md_is_word_char(src, len, i - 1)) ok = false;
                if(ok && i + nlen < len && md_is_word_char(src, len, i + nlen)) ok = false;
            }
            if(ok){
                if(count == cap){ cap = cap ? cap * 2 : 16; matches = ce_realloc(matches, cap * sizeof(md_match)); }
                matches[count].pos = i; matches[count].len = nlen;
                count++;
                i += nlen; /* non-overlapping */
                continue;
            }
        }
        i++;
    }
    *out = matches;
    return count;
}

long md_find_next(const char *src, size_t len, const char *needle, size_t nlen,
                  bool case_sensitive, bool whole_word, size_t from){
    md_match *m = NULL;
    size_t n = md_find_all(src, len, needle, nlen, case_sensitive, whole_word, &m);
    for(size_t i = 0; i < n; i++){
        if(m[i].pos >= from){ long r = (long)m[i].pos; ce_free(m); return r; }
    }
    if(m) ce_free(m);
    return -1;
}

long md_find_prev(const char *src, size_t len, const char *needle, size_t nlen,
                  bool case_sensitive, bool whole_word, size_t from){
    md_match *m = NULL;
    size_t n = md_find_all(src, len, needle, nlen, case_sensitive, whole_word, &m);
    long last = -1;
    for(size_t i = 0; i < n; i++){
        if(m[i].pos <= from) last = (long)m[i].pos;
        else break;
    }
    if(m) ce_free(m);
    return last;
}
