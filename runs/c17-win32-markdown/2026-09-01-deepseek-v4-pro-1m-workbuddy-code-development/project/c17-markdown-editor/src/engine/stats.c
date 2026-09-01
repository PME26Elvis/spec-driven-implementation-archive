/* stats.c - document statistics and deterministic word counting. */
#include "stats.h"
#include "ce_common.h"
#include "utf8.h"

size_t md_count_chars(const char *s, size_t len){
    return ce_utf8_count((const uint8_t*)s, len);
}

/* CJK ideograph ranges per spec. */
static bool is_cjk(uint32_t cp){
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0x20000 && cp <= 0x2FA1F);
}

static bool is_ascii_word(uint32_t cp){
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
           (cp >= '0' && cp <= '9') || cp == '_';
}

size_t md_count_words(const char *s, size_t len){
    size_t count = 0;
    size_t i = 0;
    while(i < len){
        uint32_t cp; size_t n;
        if(ce_utf8_decode((const uint8_t*)s, len, i, &cp, &n) != 0){ i++; continue; }
        if(is_cjk(cp)){
            count++; i += n;
            continue;
        }
        if(is_ascii_word(cp)){
            /* contiguous ASCII word run */
            size_t j = i;
            while(j < len){
                uint32_t c2; size_t n2;
                if(ce_utf8_decode((const uint8_t*)s, len, j, &c2, &n2) != 0) break;
                if(is_ascii_word(c2)){ j += n2; continue; }
                /* apostrophe/hyphen inside a token when surrounded by ASCII word chars */
                if((c2 == '\'' || c2 == '-') && j > i){
                    size_t k = j + n2;
                    if(k < len){
                        uint32_t c3; size_t n3;
                        if(ce_utf8_decode((const uint8_t*)s, len, k, &c3, &n3) == 0 && is_ascii_word(c3)){
                            j += n2; continue;
                        }
                    }
                }
                break;
            }
            count++; i = j;
            continue;
        }
        if(cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp < 0x80){
            /* whitespace or ASCII punctuation/control: separator */
            i += n;
            continue;
        }
        /* non-ASCII, non-CJK run: one token */
        size_t j = i + n;
        while(j < len){
            uint32_t c2; size_t n2;
            if(ce_utf8_decode((const uint8_t*)s, len, j, &c2, &n2) != 0) break;
            if(c2 < 0x80 || is_cjk(c2)) break;
            if(c2 == ' ' || c2 == '\t' || c2 == '\n' || c2 == '\r') break;
            j += n2;
        }
        count++; i = j;
    }
    return count;
}

/* count rendered chars + words by walking the inline tree */
static void walk_inlines(const md_inline *inl, size_t *chars, size_t *words){
    for(size_t k = 0; k < 1; k++){
        switch(inl->type){
            case MD_INL_TEXT:
                if(inl->text){ *chars += ce_utf8_count((const uint8_t*)inl->text, strlen(inl->text)); }
                break;
            case MD_INL_CODE:
                if(inl->text){ *chars += ce_utf8_count((const uint8_t*)inl->text, strlen(inl->text)); }
                break;
            case MD_INL_EMPH:
            case MD_INL_STRONG:
            case MD_INL_STRIKE:
            case MD_INL_LINK:
            case MD_INL_IMAGE:
                for(size_t i = 0; i < inl->nchildren; i++) walk_inlines(inl->children[i], chars, words);
                break;
            case MD_INL_AUTOLINK:
                if(inl->url){ *chars += ce_utf8_count((const uint8_t*)inl->url, strlen(inl->url)); }
                break;
            case MD_INL_SOFTBREAK:
            case MD_INL_HARDBREAK:
                (*chars)++;
                break;
            case MD_INL_HTML:
                break;
        }
        (void)words;
    }
}

static void walk_block_count(const md_block *b, md_stats *s){
    switch(b->type){
        case MD_BLOCK_PARAGRAPH: s->paragraphs++; break;
        case MD_BLOCK_HEADING: s->headings++; break;
        case MD_BLOCK_CODE: s->code_blocks++; break;
        case MD_BLOCK_TABLE: break;
        default: break;
    }
    for(size_t i = 0; i < b->ninlines; i++){
        const md_inline *inl = b->inlines[i];
        if(inl->type == MD_INL_IMAGE) s->images++;
        else if(inl->type == MD_INL_LINK || inl->type == MD_INL_AUTOLINK) s->links++;
    }
    for(size_t i = 0; i < b->nchildren; i++) walk_block_count(b->children[i], s);
}

void md_stats_compute(const char *src, size_t len, md_doc *doc, md_stats *out){
    memset(out, 0, sizeof(*out));
    md_doc *own = NULL;
    if(!doc){ own = md_parse(src, len); doc = own; }

    /* raw chars: code points, CRLF normalized to one */
    {
        size_t i = 0;
        while(i < len){
            if(src[i] == '\r' && i + 1 < len && src[i+1] == '\n'){ out->raw_chars++; i += 2; continue; }
            uint32_t cp; size_t n;
            if(ce_utf8_decode((const uint8_t*)src, len, i, &cp, &n) == 0){ i += n; } else i++;
            out->raw_chars++;
        }
    }
    /* lines */
    {
        bool nonempty_line = false;
        out->total_lines = 0; out->nonempty_lines = 0;
        for(size_t k = 0; k <= len; k++){
            bool at_nl = (k == len) || src[k] == '\n';
            if(at_nl){
                out->total_lines++;
                if(nonempty_line) out->nonempty_lines++;
                nonempty_line = false;
                continue;
            }
            if(src[k] != ' ' && src[k] != '\t' && src[k] != '\r') nonempty_line = true;
        }
        if(len == 0){ out->total_lines = 0; out->nonempty_lines = 0; }
    }

    /* recursive walk: structure counts, rendered chars, and per-block word counts */
    md_block **stack = NULL; size_t sn = 0, scap = 0;
    #define PUSH(b) do{ if(sn==scap){scap=scap?scap*2:32; stack=ce_realloc(stack,scap*sizeof(md_block*));} stack[sn++]=(b);}while(0)
    for(size_t i = 0; i < doc->nblocks; i++) PUSH(doc->blocks[i]);
    while(sn){
        md_block *b = stack[--sn];
        walk_block_count(b, out);
        for(size_t j = 0; j < b->ninlines; j++)
            walk_inlines(b->inlines[j], &out->rendered_chars, &out->word_count);
        if(b->ninlines){
            char *pt = md_block_plaintext(b);
            out->word_count += md_count_words(pt, strlen(pt));
            ce_free(pt);
        }
        for(size_t c = 0; c < b->nchildren; c++) PUSH(b->children[c]);
    }
    ce_free(stack);
    #undef PUSH

    if(own) md_free(own);
}
