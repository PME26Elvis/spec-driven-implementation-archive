#include "darc.h"
uint32_t darc_crc32c(const void*vp,size_t n){const uint8_t*p=vp;uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0x82f63b78u&((uint32_t)-(int32_t)(c&1u)));}return ~c;}
