#include "sha256.h"
#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, int n){ return (x >> n) | (x << (32 - n)); }

void sha256_init(Sha256 *s){
    s->state[0]=0x6a09e667; s->state[1]=0xbb67ae85; s->state[2]=0x3c6ef372; s->state[3]=0xa54ff53a;
    s->state[4]=0x510e527f; s->state[5]=0x9b05688c; s->state[6]=0x1f83d9ab; s->state[7]=0x5be0cd19;
    s->bitlen=0; s->buflen=0;
}

static void sha256_block(Sha256 *s, const uint8_t p[64]){    uint32_t w[64]; int i;
    uint32_t a,b,c,d,e,f,g,h;
    for(i=0;i<16;i++){
        w[i]=(uint32_t)p[i*4]<<24 | (uint32_t)p[i*4+1]<<16 | (uint32_t)p[i*4+2]<<8 | p[i*4+3];
    }
    for(i=16;i<64;i++){
        uint32_t s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    a=s->state[0]; b=s->state[1]; c=s->state[2]; d=s->state[3];
    e=s->state[4]; f=s->state[5]; g=s->state[6]; h=s->state[7];
    for(i=0;i<64;i++){
        uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t t1=h+S1+ch+K[i]+w[i];
        uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        uint32_t t2=S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->state[0]+=a; s->state[1]+=b; s->state[2]+=c; s->state[3]+=d;
    s->state[4]+=e; s->state[5]+=f; s->state[6]+=g; s->state[7]+=h;
}

void sha256_update(Sha256 *s, const void *data, size_t len){
    const uint8_t *p=data;
    s->bitlen += (uint64_t)len * 8;
    while(len){
        size_t take = 64 - s->buflen;
        if(take > len) take = len;
        memcpy(s->buffer + s->buflen, p, take);
        s->buflen += take; p += take; len -= take;
        if(s->buflen == 64){
            sha256_block(s, s->buffer);
            s->buflen = 0;
        }
    }
}

void sha256_final(Sha256 *s, uint8_t out[32]){
    /* Capture message bit length before any padding byte is appended.
       Padding is assembled into a local block buffer and fed to
       sha256_block directly, so that no incremental sha256_update call
       during finalization can perturb the compression state. */
    uint64_t bitlen = s->bitlen;
    size_t i;
    /* total message bytes buffered = s->buflen (0..63) */
    unsigned char blk[128];
    memcpy(blk, s->buffer, s->buflen);
    size_t bl = s->buflen;
    blk[bl++] = 0x80;
    /* pad until (bl % 64) == 56 */
    while((bl % 64) != 56) blk[bl++] = 0x00;
    /* append 64-bit big-endian bit length */
    for(i=0;i<8;i++) blk[bl++] = (uint8_t)(bitlen >> (56 - 8*i));
    /* process complete blocks */
    size_t nblocks = bl / 64;
    for(i=0;i<nblocks;i++) sha256_block(s, blk + i*64);
    for(i=0;i<8;i++){
        out[i*4+0]=(uint8_t)(s->state[i]>>24);
        out[i*4+1]=(uint8_t)(s->state[i]>>16);
        out[i*4+2]=(uint8_t)(s->state[i]>>8);
        out[i*4+3]=(uint8_t)(s->state[i]);
    }
}

void sha256_one(const void *data, size_t len, uint8_t out[32]){
    Sha256 s; sha256_init(&s); sha256_update(&s, data, len); sha256_final(&s, out);
}

void sha256_to_hex(const uint8_t digest[32], char out[65]){
    static const char hexd[]="0123456789abcdef";
    int i;
    for(i=0;i<32;i++){ out[i*2]=hexd[digest[i]>>4]; out[i*2+1]=hexd[digest[i]&0xf]; }
    out[64]='\0';
}

int sha256_from_hex(const char *h, uint8_t out[32]){
    int i;
    for(i=0;i<32;i++){
        int hi=h[i*2], lo=h[i*2+1];
        int hv, lv;
        if(hi>='0'&&hi<='9') hv=hi-'0';
        else if(hi>='a'&&hi<='f') hv=hi-'a'+10;
        else if(hi>='A'&&hi<='F') hv=hi-'A'+10;
        else return -1;
        if(lo>='0'&&lo<='9') lv=lo-'0';
        else if(lo>='a'&&lo<='f') lv=lo-'a'+10;
        else if(lo>='A'&&lo<='F') lv=lo-'A'+10;
        else return -1;
        out[i]=(uint8_t)((hv<<4)|lv);
    }
    return 0;
}

int sha256_is_hex64(const char *s){
    int i;
    for(i=0;i<64;i++){
        char c=s[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return 0;
    }
    return s[64]=='\0';
}
