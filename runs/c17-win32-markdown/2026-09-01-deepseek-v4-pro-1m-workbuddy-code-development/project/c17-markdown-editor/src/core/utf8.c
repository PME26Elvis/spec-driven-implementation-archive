/* utf8.c - UTF-8 and grapheme-cluster helpers. */
#include "utf8.h"

int ce_utf8_decode(const uint8_t *s, size_t len, size_t pos, uint32_t *cp, size_t *consumed){
    if(pos >= len) return -1;
    uint8_t b0 = s[pos];
    if(b0 < 0x80){ *cp = b0; *consumed = 1; return 0; }
    uint32_t c; size_t n;
    if((b0 & 0xE0) == 0xC0){ c = b0 & 0x1F; n = 2; }
    else if((b0 & 0xF0) == 0xE0){ c = b0 & 0x0F; n = 3; }
    else if((b0 & 0xF8) == 0xF0){ c = b0 & 0x07; n = 4; }
    else return -1;
    if(pos + n > len) return -1;
    for(size_t i = 1; i < n; i++){
        uint8_t b = s[pos + i];
        if((b & 0xC0) != 0x80) return -1;
        c = (c << 6) | (b & 0x3F);
    }
    /* reject overlong, surrogates, and > U+10FFFF */
    if(n == 2 && c < 0x80) return -1;
    if(n == 3 && c < 0x800) return -1;
    if(n == 4 && c < 0x10000) return -1;
    if(c >= 0xD800 && c <= 0xDFFF) return -1;
    if(c > 0x10FFFF) return -1;
    *cp = c; *consumed = n; return 0;
}

int ce_utf8_encode(uint32_t cp, uint8_t *out){
    if(cp < 0x80){ out[0] = (uint8_t)cp; return 1; }
    if(cp < 0x800){ out[0] = 0xC0 | (cp >> 6); out[1] = 0x80 | (cp & 0x3F); return 2; }
    if(cp < 0x10000){ out[0] = 0xE0 | (cp >> 12); out[1] = 0x80 | ((cp >> 6) & 0x3F); out[2] = 0x80 | (cp & 0x3F); return 3; }
    out[0] = 0xF0 | (cp >> 18); out[1] = 0x80 | ((cp >> 12) & 0x3F); out[2] = 0x80 | ((cp >> 6) & 0x3F); out[3] = 0x80 | (cp & 0x3F); return 4;
}

int ce_utf8_valid(const uint8_t *s, size_t len){
    size_t i = 0;
    while(i < len){
        uint32_t cp; size_t n;
        if(ce_utf8_decode(s, len, i, &cp, &n) != 0) return 0;
        i += n;
    }
    return 1;
}

size_t ce_utf8_prev(const uint8_t *s, size_t pos){
    if(pos == 0) return 0;
    pos--;
    while(pos > 0 && (s[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

size_t ce_utf8_next(const uint8_t *s, size_t len, size_t pos){
    if(pos >= len) return len;
    uint8_t b = s[pos];
    if(b < 0x80) return pos + 1;
    if((b & 0xE0) == 0xC0) return pos + 2;
    if((b & 0xF0) == 0xE0) return pos + 3;
    if((b & 0xF8) == 0xF0) return pos + 4;
    return pos + 1; /* stray byte */
}

size_t ce_utf8_count(const uint8_t *s, size_t len){
    size_t n = 0, i = 0;
    while(i < len){ uint8_t b = s[i]; i += (b < 0x80) ? 1 : ((b & 0xE0) == 0xC0) ? 2 : ((b & 0xF0) == 0xE0) ? 3 : 4; n++; }
    return n;
}

bool ce_is_extend(uint32_t cp){
    return
        (cp >= 0x0300 && cp <= 0x036F) ||
        (cp >= 0x0483 && cp <= 0x0489) ||
        (cp >= 0x0591 && cp <= 0x05BD) ||
        (cp >= 0x0610 && cp <= 0x061A) ||
        (cp >= 0x064B && cp <= 0x065F) ||
        (cp >= 0x06D6 && cp <= 0x06DC) ||
        (cp >= 0x06DF && cp <= 0x06E4) ||
        (cp >= 0x06E7 && cp <= 0x06E8) ||
        (cp >= 0x06EA && cp <= 0x06ED) ||
        (cp >= 0x0711 && cp <= 0x0711) ||
        (cp >= 0x0730 && cp <= 0x074A) ||
        (cp >= 0x07A6 && cp <= 0x07B0) ||
        (cp >= 0x07EB && cp <= 0x07F3) ||
        (cp >= 0x0816 && cp <= 0x0819) ||
        (cp >= 0x081B && cp <= 0x0823) ||
        (cp >= 0x0825 && cp <= 0x0827) ||
        (cp >= 0x0829 && cp <= 0x082D) ||
        (cp >= 0x08D4 && cp <= 0x08E1) ||
        (cp >= 0x08E3 && cp <= 0x0902) ||
        (cp >= 0x093A && cp <= 0x093C) ||
        (cp >= 0x0941 && cp <= 0x0948) ||
        (cp >= 0x094D && cp <= 0x094D) ||
        (cp >= 0x0951 && cp <= 0x0957) ||
        (cp >= 0x0962 && cp <= 0x0963) ||
        (cp >= 0x0981 && cp <= 0x0981) ||
        (cp >= 0x09BC && cp <= 0x09BC) ||
        (cp >= 0x09C1 && cp <= 0x09C4) ||
        (cp >= 0x09CD && cp <= 0x09CD) ||
        (cp >= 0x09E2 && cp <= 0x09E3) ||
        (cp >= 0x0A01 && cp <= 0x0A02) ||
        (cp >= 0x0A3C && cp <= 0x0A3C) ||
        (cp >= 0x0A41 && cp <= 0x0A42) ||
        (cp >= 0x0A47 && cp <= 0x0A48) ||
        (cp >= 0x0A4B && cp <= 0x0A4D) ||
        (cp >= 0x0A70 && cp <= 0x0A71) ||
        (cp >= 0x0A81 && cp <= 0x0A82) ||
        (cp >= 0x0ABC && cp <= 0x0ABC) ||
        (cp >= 0x0AC1 && cp <= 0x0AC5) ||
        (cp >= 0x0AC7 && cp <= 0x0AC8) ||
        (cp >= 0x0ACD && cp <= 0x0ACD) ||
        (cp >= 0x0AE2 && cp <= 0x0AE3) ||
        (cp >= 0x0B01 && cp <= 0x0B01) ||
        (cp >= 0x0B3C && cp <= 0x0B3C) ||
        (cp >= 0x0B3F && cp <= 0x0B3F) ||
        (cp >= 0x0B41 && cp <= 0x0B44) ||
        (cp >= 0x0B4D && cp <= 0x0B4D) ||
        (cp >= 0x0B56 && cp <= 0x0B56) ||
        (cp >= 0x0B82 && cp <= 0x0B82) ||
        (cp >= 0x0BC0 && cp <= 0x0BC0) ||
        (cp >= 0x0BCD && cp <= 0x0BCD) ||
        (cp >= 0x0C3E && cp <= 0x0C40) ||
        (cp >= 0x0C46 && cp <= 0x0C48) ||
        (cp >= 0x0C4A && cp <= 0x0C4D) ||
        (cp >= 0x0C55 && cp <= 0x0C56) ||
        (cp >= 0x0CBC && cp <= 0x0CBC) ||
        (cp >= 0x0CBF && cp <= 0x0CBF) ||
        (cp >= 0x0CC6 && cp <= 0x0CC6) ||
        (cp >= 0x0CCC && cp <= 0x0CCD) ||
        (cp >= 0x0D41 && cp <= 0x0D44) ||
        (cp >= 0x0D4D && cp <= 0x0D4D) ||
        (cp >= 0x0DCA && cp <= 0x0DCA) ||
        (cp >= 0x0DD2 && cp <= 0x0DD4) ||
        (cp >= 0x0DD6 && cp <= 0x0DD6) ||
        (cp >= 0x0E31 && cp <= 0x0E31) ||
        (cp >= 0x0E34 && cp <= 0x0E3A) ||
        (cp >= 0x0E47 && cp <= 0x0E4E) ||
        (cp >= 0x0EB1 && cp <= 0x0EB1) ||
        (cp >= 0x0EB4 && cp <= 0x0EB9) ||
        (cp >= 0x0EBB && cp <= 0x0EBC) ||
        (cp >= 0x0EC8 && cp <= 0x0ECD) ||
        (cp >= 0x0F18 && cp <= 0x0F19) ||
        (cp >= 0x0F35 && cp <= 0x0F35) ||
        (cp >= 0x0F37 && cp <= 0x0F37) ||
        (cp >= 0x0F39 && cp <= 0x0F39) ||
        (cp >= 0x0F71 && cp <= 0x0F7E) ||
        (cp >= 0x0F80 && cp <= 0x0F84) ||
        (cp >= 0x0F86 && cp <= 0x0F87) ||
        (cp >= 0x0F8D && cp <= 0x0F97) ||
        (cp >= 0x0F99 && cp <= 0x0FBC) ||
        (cp >= 0x102D && cp <= 0x1030) ||
        (cp >= 0x1032 && cp <= 0x1037) ||
        (cp >= 0x1039 && cp <= 0x103A) ||
        (cp >= 0x103D && cp <= 0x103E) ||
        (cp >= 0x1058 && cp <= 0x1059) ||
        (cp >= 0x105E && cp <= 0x1060) ||
        (cp >= 0x1082 && cp <= 0x1082) ||
        (cp >= 0x1085 && cp <= 0x1086) ||
        (cp >= 0x108D && cp <= 0x108D) ||
        (cp >= 0x109D && cp <= 0x109D) ||
        (cp >= 0x135D && cp <= 0x135F) ||
        (cp >= 0x1712 && cp <= 0x1714) ||
        (cp >= 0x1732 && cp <= 0x1734) ||
        (cp >= 0x1752 && cp <= 0x1753) ||
        (cp >= 0x1772 && cp <= 0x1773) ||
        (cp >= 0x17B4 && cp <= 0x17B5) ||
        (cp >= 0x17B7 && cp <= 0x17BD) ||
        (cp >= 0x17C6 && cp <= 0x17C6) ||
        (cp >= 0x17C9 && cp <= 0x17D3) ||
        (cp >= 0x17DD && cp <= 0x17DD) ||
        (cp >= 0x180B && cp <= 0x180D) ||
        (cp >= 0x1885 && cp <= 0x1886) ||
        (cp >= 0x18A9 && cp <= 0x18A9) ||
        (cp >= 0x1920 && cp <= 0x1922) ||
        (cp >= 0x1927 && cp <= 0x1928) ||
        (cp >= 0x1932 && cp <= 0x1932) ||
        (cp >= 0x1939 && cp <= 0x193B) ||
        (cp >= 0x1A17 && cp <= 0x1A18) ||
        (cp >= 0x1A1B && cp <= 0x1A1B) ||
        (cp >= 0x1A56 && cp <= 0x1A56) ||
        (cp >= 0x1A58 && cp <= 0x1A5E) ||
        (cp >= 0x1A60 && cp <= 0x1A60) ||
        (cp >= 0x1A62 && cp <= 0x1A62) ||
        (cp >= 0x1A65 && cp <= 0x1A6C) ||
        (cp >= 0x1A73 && cp <= 0x1A7C) ||
        (cp >= 0x1A7F && cp <= 0x1A7F) ||
        (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1B00 && cp <= 0x1B03) ||
        (cp >= 0x1B34 && cp <= 0x1B34) ||
        (cp >= 0x1B36 && cp <= 0x1B3A) ||
        (cp >= 0x1B3C && cp <= 0x1B3C) ||
        (cp >= 0x1B42 && cp <= 0x1B42) ||
        (cp >= 0x1B6B && cp <= 0x1B73) ||
        (cp >= 0x1B80 && cp <= 0x1B81) ||
        (cp >= 0x1BA2 && cp <= 0x1BA5) ||
        (cp >= 0x1BA8 && cp <= 0x1BA9) ||
        (cp >= 0x1BE6 && cp <= 0x1BE6) ||
        (cp >= 0x1BE8 && cp <= 0x1BE9) ||
        (cp >= 0x1BED && cp <= 0x1BED) ||
        (cp >= 0x1BEF && cp <= 0x1BF1) ||
        (cp >= 0x1C2C && cp <= 0x1C33) ||
        (cp >= 0x1C36 && cp <= 0x1C37) ||
        (cp >= 0x1CD0 && cp <= 0x1CD2) ||
        (cp >= 0x1CD4 && cp <= 0x1CE0) ||
        (cp >= 0x1CE2 && cp <= 0x1CE8) ||
        (cp >= 0x1CED && cp <= 0x1CED) ||
        (cp >= 0x1CF4 && cp <= 0x1CF4) ||
        (cp >= 0x1CF8 && cp <= 0x1CF9) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) ||
        (cp >= 0x20D0 && cp <= 0x20FF) ||
        (cp >= 0x2CEF && cp <= 0x2CF1) ||
        (cp >= 0x2D7F && cp <= 0x2D7F) ||
        (cp >= 0x2DE0 && cp <= 0x2DFF) ||
        (cp >= 0x302A && cp <= 0x302D) ||
        (cp >= 0x3099 && cp <= 0x309A) ||
        (cp >= 0xA66F && cp <= 0xA672) ||
        (cp >= 0xA674 && cp <= 0xA67D) ||
        (cp >= 0xA69E && cp <= 0xA69F) ||
        (cp >= 0xA6F0 && cp <= 0xA6F1) ||
        (cp >= 0xA802 && cp <= 0xA802) ||
        (cp >= 0xA806 && cp <= 0xA806) ||
        (cp >= 0xA80B && cp <= 0xA80B) ||
        (cp >= 0xA825 && cp <= 0xA826) ||
        (cp >= 0xA8C4 && cp <= 0xA8C5) ||
        (cp >= 0xA8E0 && cp <= 0xA8F1) ||
        (cp >= 0xA926 && cp <= 0xA92D) ||
        (cp >= 0xA947 && cp <= 0xA951) ||
        (cp >= 0xA980 && cp <= 0xA982) ||
        (cp >= 0xA9B3 && cp <= 0xA9B3) ||
        (cp >= 0xA9B6 && cp <= 0xA9B9) ||
        (cp >= 0xA9BC && cp <= 0xA9BC) ||
        (cp >= 0xAA29 && cp <= 0xAA2E) ||
        (cp >= 0xAA31 && cp <= 0xAA32) ||
        (cp >= 0xAA35 && cp <= 0xAA36) ||
        (cp >= 0xAA43 && cp <= 0xAA43) ||
        (cp >= 0xAA4C && cp <= 0xAA4C) ||
        (cp >= 0xFE00 && cp <= 0xFE0F) ||
        (cp >= 0xFE20 && cp <= 0xFE2F) ||
        (cp >= 0x101FD && cp <= 0x101FD) ||
        (cp >= 0x10A0F && cp <= 0x10A0F) ||
        (cp >= 0x10A3F && cp <= 0x10A3F) ||
        (cp >= 0x1D165 && cp <= 0x1D169) ||
        (cp >= 0x1D16D && cp <= 0x1D182) ||
        (cp >= 0x1D185 && cp <= 0x1D18B) ||
        (cp >= 0x1D1AA && cp <= 0x1D1AD) ||
        (cp >= 0x1D242 && cp <= 0x1D244) ||
        (cp >= 0xE0100 && cp <= 0xE01EF) ||
        /* emoji skin-tone modifiers (E_Modifier, kept with base) */
        (cp >= 0x1F3FB && cp <= 0x1F3FF);
}

bool ce_is_variation_selector(uint32_t cp){
    return (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
}

bool ce_is_zwj(uint32_t cp){ return cp == 0x200D; }

bool ce_is_emoji(uint32_t cp){
    return
        (cp >= 0x1F000 && cp <= 0x1FAFF) ||   /* emoticons, symbols, supplementary */
        (cp >= 0x2600 && cp <= 0x27BF) ||     /* misc symbols, dingbats */
        (cp >= 0x1F1E6 && cp <= 0x1F1FF) ||   /* regional indicators (subset of above) */
        (cp >= 0x2190 && cp <= 0x21FF) ||     /* arrows */
        (cp >= 0x2300 && cp <= 0x23FF) ||     /* misc technical */
        (cp >= 0x2B00 && cp <= 0x2BFF) ||     /* misc symbols arrows */
        (cp >= 0xFE00 && cp <= 0xFE0F) ||     /* variation selectors treated as part of emoji seq */
        (cp == 0x00A9) || (cp == 0x00AE) ||   /* copyright, registered */
        (cp >= 0x1F3FB && cp <= 0x1F3FF);     /* skin tones */
}

size_t ce_grapheme_next(const uint8_t *s, size_t len, size_t pos){
    if(pos >= len) return len;
    /* CR LF pair */
    if(s[pos] == 0x0D && pos + 1 < len && s[pos+1] == 0x0A) return pos + 2;
    uint32_t cp; size_t n;
    if(ce_utf8_decode(s, len, pos, &cp, &n) != 0) return pos + 1;
    size_t i = pos + n;
    for(;;){
        if(i >= len) break;
        uint32_t c2; size_t n2;
        if(ce_utf8_decode(s, len, i, &c2, &n2) != 0) break;
        if(ce_is_extend(c2)){
            i += n2; continue;
        }
        if(ce_is_zwj(c2)){
            /* GB9/GB11: consume ZWJ; if followed by pictographic, keep going */
            size_t j = i + n2;
            uint32_t c3; size_t n3;
            if(j < len && ce_utf8_decode(s, len, j, &c3, &n3) == 0 && ce_is_emoji(c3)){
                i = j + n3;
                continue;
            }
            break;
        }
        break;
    }
    return i;
}

size_t ce_grapheme_prev(const uint8_t *s, size_t pos){
    if(pos == 0) return 0;
    size_t p = ce_utf8_prev(s, pos);
    /* walk backward while the character immediately before p is an extend/VS mark,
     * or a ZWJ that is preceded by an emoji (emoji ZWJ sequence). */
    for(;;){
        if(p == 0) break;
        size_t q = ce_utf8_prev(s, p);
        uint32_t c; size_t nc;
        if(ce_utf8_decode(s, pos, q, &c, &nc) != 0 || q + nc != p) break;
        if(ce_is_extend(c)){ p = q; continue; }
        if(ce_is_zwj(c)){
            size_t r = ce_utf8_prev(s, q);
            uint32_t cr; size_t nr;
            if(r < q && ce_utf8_decode(s, pos, r, &cr, &nr) == 0 && r + nr == q && ce_is_emoji(cr)){
                p = r; continue;
            }
            break;
        }
        break;
    }
    return p;
}

size_t ce_grapheme_count(const uint8_t *s, size_t len){
    size_t n = 0, i = 0;
    while(i < len){ i = ce_grapheme_next(s, len, i); n++; }
    return n;
}
