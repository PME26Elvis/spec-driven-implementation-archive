#include "darc.h"
#include <stdlib.h>
#include <string.h>
static uint64_t hcid(const DarcCid*c){uint64_t x=darc_rd_u64(c->b)^darc_rd_u64(c->b+8)^darc_rd_u64(c->b+16)^darc_rd_u64(c->b+24);x^=x>>33;x*=UINT64_C(0xff51afd7ed558ccd);x^=x>>33;x*=UINT64_C(0xc4ceb9fe1a85ec53);x^=x>>33;return x;}
static size_t pow2(size_t x){size_t p=8;while(p<x&&p<=SIZE_MAX/2)p*=2;return p;}
int darc_rh_init(DarcRhMap*m,size_t cap){cap=pow2(cap?cap:8);m->tab=darc_calloc(cap,sizeof *m->tab);if(!m->tab)return -1;m->cap=cap;m->n=0;return 0;}
void darc_rh_free(DarcRhMap*m){free(m->tab);memset(m,0,sizeof *m);}
static int put_raw(DarcRhMap*m,DarcRhEnt e){size_t mask=m->cap-1,pos=(size_t)hcid(&e.cid)&mask;e.used=1;e.dib=0;for(size_t steps=0;steps<m->cap;steps++,e.dib++){DarcRhEnt*slot=&m->tab[pos];if(!slot->used){*slot=e;m->n++;return 0;}if(darc_cid_eq(&slot->cid,&e.cid)){uint32_t d=slot->dib;*slot=e;slot->dib=d;return 0;}if(slot->dib<e.dib){DarcRhEnt tmp=*slot;*slot=e;e=tmp;}pos=(pos+1)&mask;}return -1;}
static int resize(DarcRhMap*m,size_t cap){DarcRhMap n;if(darc_rh_init(&n,cap))return -1;for(size_t i=0;i<m->cap;i++)if(m->tab[i].used&&put_raw(&n,m->tab[i])){darc_rh_free(&n);return -1;}free(m->tab);*m=n;return 0;}
int darc_rh_put(DarcRhMap*m,const DarcRhEnt*e){if(!m->cap&&darc_rh_init(m,8))return -1;if((m->n+1)*10>m->cap*7&&resize(m,m->cap*2))return -1;return put_raw(m,*e);}
DarcRhEnt*darc_rh_get(DarcRhMap*m,const DarcCid*c){if(!m->cap)return NULL;size_t mask=m->cap-1,pos=(size_t)hcid(c)&mask;uint32_t dib=0;for(size_t s=0;s<m->cap;s++,dib++){DarcRhEnt*e=&m->tab[pos];if(!e->used||e->dib<dib)return NULL;if(darc_cid_eq(&e->cid,c))return e;pos=(pos+1)&mask;}return NULL;}
int darc_rh_del(DarcRhMap*m,const DarcCid*c){DarcRhEnt*e=darc_rh_get(m,c);if(!e)return -1;size_t pos=(size_t)(e-m->tab),mask=m->cap-1;for(;;){size_t next=(pos+1)&mask;if(!m->tab[next].used||m->tab[next].dib==0){memset(&m->tab[pos],0,sizeof m->tab[pos]);break;}m->tab[pos]=m->tab[next];m->tab[pos].dib--;pos=next;}m->n--;return 0;}
