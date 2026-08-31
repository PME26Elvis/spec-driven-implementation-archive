#include "darc.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>

int darc_last_error=0; char darc_last_code[64]="";
static long alloc_count=0, write_count=0, fsync_count=0, rename_count=0;

static long env_long(const char *n){ const char *s=getenv(n); if(!s||!*s) return -1; char *e=NULL; errno=0; long v=strtol(s,&e,10); return errno||!e||*e? -1:v; }
void darc_fault_reset(void){ alloc_count=write_count=fsync_count=rename_count=0; }
void *darc_malloc(size_t n){ if(n==0)n=1; long fail=env_long("DARC_FAULT_ALLOC_N"); alloc_count++; if(fail>0&&alloc_count==fail){errno=ENOMEM;return NULL;} return malloc(n); }
void *darc_calloc(size_t n,size_t s){ if(s&&n>SIZE_MAX/s){errno=ENOMEM;return NULL;} size_t z=n*s; void *p=darc_malloc(z); if(p) memset(p,0,z); return p; }
void *darc_realloc(void *p,size_t n){ if(n==0)n=1; long fail=env_long("DARC_FAULT_ALLOC_N"); alloc_count++; if(fail>0&&alloc_count==fail){errno=ENOMEM;return NULL;} return realloc(p,n); }
char *darc_strdup(const char *s){ return darc_strndup(s,strlen(s)); }
char *darc_strndup(const char *s,size_t n){ char *p=darc_malloc(n+1); if(!p)return NULL; memcpy(p,s,n);p[n]=0;return p; }

void darc_buf_init(DarcBuf *b){ memset(b,0,sizeof(*b)); }
void darc_buf_free(DarcBuf *b){ free(b->data);memset(b,0,sizeof(*b)); }
int darc_buf_reserve(DarcBuf *b,size_t add){ if(add>SIZE_MAX-b->len)return -1; size_t need=b->len+add;if(need<=b->cap)return 0;size_t c=b->cap?b->cap:256;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}uint8_t *p=darc_realloc(b->data,c);if(!p)return -1;b->data=p;b->cap=c;return 0; }
int darc_buf_put(DarcBuf *b,const void *p,size_t n){ if(darc_buf_reserve(b,n))return -1;memcpy(b->data+b->len,p,n);b->len+=n;return 0; }
int darc_buf_u8(DarcBuf *b,uint8_t x){return darc_buf_put(b,&x,1);} 
int darc_buf_u16(DarcBuf *b,uint16_t x){uint8_t p[2]={(uint8_t)x,(uint8_t)(x>>8)};return darc_buf_put(b,p,2);} 
int darc_buf_u32(DarcBuf *b,uint32_t x){uint8_t p[4]={(uint8_t)x,(uint8_t)(x>>8),(uint8_t)(x>>16),(uint8_t)(x>>24)};return darc_buf_put(b,p,4);} 
int darc_buf_u64(DarcBuf *b,uint64_t x){uint8_t p[8];for(int i=0;i<8;i++)p[i]=(uint8_t)(x>>(8*i));return darc_buf_put(b,p,8);} 
int darc_buf_i64(DarcBuf *b,int64_t x){return darc_buf_u64(b,(uint64_t)x);} 
int darc_buf_bytestr(DarcBuf *b,const void *p,size_t n){if(darc_buf_u64(b,(uint64_t)n))return -1;return darc_buf_put(b,p,n);} 
uint16_t darc_rd_u16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
uint32_t darc_rd_u32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
uint64_t darc_rd_u64(const uint8_t*p){uint64_t x=0;for(int i=7;i>=0;i--)x=(x<<8)|p[i];return x;}
int64_t darc_rd_i64(const uint8_t*p){return (int64_t)darc_rd_u64(p);} 

void darc_cid_hex(const DarcCid*c,char out[65]){static const char h[]="0123456789abcdef";for(int i=0;i<32;i++){out[2*i]=h[c->b[i]>>4];out[2*i+1]=h[c->b[i]&15];}out[64]=0;}
static int hx(int c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
int darc_hex_cid(const char*s,DarcCid*c){if(strlen(s)!=64)return -1;for(int i=0;i<32;i++){int a=hx((unsigned char)s[2*i]),b=hx((unsigned char)s[2*i+1]);if(a<0||b<0)return -1;c->b[i]=(uint8_t)((a<<4)|b);}return 0;}
int darc_cid_cmp(const DarcCid*a,const DarcCid*b){return memcmp(a->b,b->b,32);}int darc_cid_eq(const DarcCid*a,const DarcCid*b){return !memcmp(a->b,b->b,32);} 
void darc_object_cid(uint8_t type,const void *payload,size_t len,DarcCid*out){DarcSha256 s;static const uint8_t pre[5]={'D','A','R','C',0};uint8_t v[2]={1,0};darc_sha256_init(&s);darc_sha256_update(&s,pre,5);darc_sha256_update(&s,&type,1);darc_sha256_update(&s,v,2);darc_sha256_update(&s,payload,len);darc_sha256_final(&s,out->b);} 

int darc_valid_utf8(const uint8_t*s,size_t n){size_t i=0;while(i<n){uint8_t c=s[i++];if(c<0x80)continue;unsigned need;uint32_t cp;if((c&0xe0)==0xc0){need=1;cp=c&0x1f;if(cp<2)return 0;}else if((c&0xf0)==0xe0){need=2;cp=c&0x0f;}else if((c&0xf8)==0xf0){need=3;cp=c&7;if(cp>4)return 0;}else return 0;if(i+need>n)return 0;for(unsigned j=0;j<need;j++){uint8_t d=s[i++];if((d&0xc0)!=0x80)return 0;cp=(cp<<6)|(d&0x3f);}if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(need==2&&cp<0x800)||(need==3&&cp<0x10000))return 0;}return 1;}
char *darc_display_bytes(const uint8_t*s,size_t n){DarcBuf b;darc_buf_init(&b);for(size_t i=0;i<n;){uint8_t c=s[i];if(c>=0x20&&c!=0x7f&&c!='\\'&&c!='\n'&&c!='\r'&&c!='\t'){size_t adv=1;if(c>=0x80){if(!darc_valid_utf8(s+i,n-i)){char z[5];snprintf(z,sizeof z,"\\x%02x",c);darc_buf_put(&b,z,4);i++;continue;} /* valid suffix; copy one sequence */ if((c&0xe0)==0xc0)adv=2;else if((c&0xf0)==0xe0)adv=3;else adv=4;}darc_buf_put(&b,s+i,adv);i+=adv;}else{char z[5];snprintf(z,sizeof z,"\\x%02x",c);darc_buf_put(&b,z,4);i++;}}darc_buf_u8(&b,0);return (char*)b.data;}
char *darc_json_escape_bytes(const uint8_t*s,size_t n){DarcBuf b;darc_buf_init(&b);darc_buf_u8(&b,'"');for(size_t i=0;i<n;i++){uint8_t c=s[i];if(c=='"'||c=='\\'){darc_buf_u8(&b,'\\');darc_buf_u8(&b,c);}else if(c=='\b')darc_buf_put(&b,"\\b",2);else if(c=='\f')darc_buf_put(&b,"\\f",2);else if(c=='\n')darc_buf_put(&b,"\\n",2);else if(c=='\r')darc_buf_put(&b,"\\r",2);else if(c=='\t')darc_buf_put(&b,"\\t",2);else if(c<0x20){char z[7];snprintf(z,sizeof z,"\\u%04x",c);darc_buf_put(&b,z,6);}else darc_buf_u8(&b,c);}darc_buf_put(&b,"\"",2);return (char*)b.data;}

char *darc_path_join(const char*a,const char*b){size_t na=strlen(a),nb=strlen(b);int slash=na&&a[na-1]!='/';if(na>SIZE_MAX-nb-2)return NULL;char*p=darc_malloc(na+nb+(size_t)slash+1);if(!p)return NULL;memcpy(p,a,na);if(slash)p[na++]='/';memcpy(p+na,b,nb);p[na+nb]=0;return p;}
char *darc_dirname_dup(const char*p){const char*s=strrchr(p,'/');if(!s)return darc_strdup(".");if(s==p)return darc_strdup("/");return darc_strndup(p,(size_t)(s-p));}
char *darc_basename_dup(const char*p){size_t n=strlen(p);while(n>1&&p[n-1]=='/')n--;size_t i=n;while(i&&p[i-1]!='/')i--;return darc_strndup(p+i,n-i);}
int darc_mkdir_p(const char*path,mode_t mode){char*p=darc_strdup(path);if(!p)return -1;for(char*q=p+1;*q;q++)if(*q=='/'){*q=0;if(mkdir(p,mode)&&errno!=EEXIST){free(p);return -1;}*q='/';}int r=mkdir(p,mode);if(r&&errno==EEXIST)r=0;free(p);return r;}
int darc_read_file(const char*path,DarcBuf*out,size_t limit){darc_buf_init(out);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;uint8_t tmp[65536];for(;;){ssize_t r=read(fd,tmp,sizeof tmp);if(r<0){if(errno==EINTR)continue;close(fd);darc_buf_free(out);return -1;}if(!r)break;if((size_t)r>limit-out->len){close(fd);darc_buf_free(out);errno=EFBIG;return -1;}if(darc_buf_put(out,tmp,(size_t)r)){close(fd);darc_buf_free(out);errno=ENOMEM;return -1;}}close(fd);return 0;}
int darc_write_all(int fd,const void*buf,size_t n){const uint8_t*p=buf;while(n){write_count++;long fail=env_long("DARC_FAULT_WRITE_N");if(fail>0&&write_count==fail){errno=ENOSPC;return -1;}ssize_t w=write(fd,p,n);if(w<0){if(errno==EINTR)continue;return -1;}if(w==0){errno=EIO;return -1;}p+=w;n-=(size_t)w;}return 0;}
int darc_fsync_fd(int fd){fsync_count++;long fail=env_long("DARC_FAULT_FSYNC_N");if(fail>0&&fsync_count==fail){errno=EIO;return -1;}return fsync(fd);} 
int darc_rename_path(const char*a,const char*b){rename_count++;long fail=env_long("DARC_FAULT_RENAME_N");if(fail>0&&rename_count==fail){errno=EIO;return -1;}return rename(a,b);} 
int darc_atomic_write_checkpoint(const char*path,const void*data,size_t n,mode_t mode,const char*checkpoint){
 char*dir=darc_dirname_dup(path);if(!dir)return -1;
 size_t np=strlen(path);if(np>SIZE_MAX-80){free(dir);errno=ENAMETOOLONG;return -1;}
 char*tmp=darc_malloc(np+80);if(!tmp){free(dir);errno=ENOMEM;return -1;}
 snprintf(tmp,np+80,"%s.tmp.%ld.%llu",path,(long)getpid(),(unsigned long long)(uintptr_t)data);
 int fd=open(tmp,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,mode);if(fd<0){free(tmp);free(dir);return -1;}
 int ok=0,saved=0;
 if(darc_write_all(fd,data,n)||darc_fsync_fd(fd)){ok=-1;saved=errno;}
 if(close(fd)&&!ok){ok=-1;saved=errno;}
 if(ok){(void)unlink(tmp);errno=saved;free(tmp);free(dir);return -1;}
 if(checkpoint)darc_checkpoint(checkpoint);
 if(darc_rename_path(tmp,path)){saved=errno;(void)unlink(tmp);free(tmp);free(dir);errno=saved;return -1;}
 int dfd=open(dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(dfd<0){saved=errno;free(tmp);free(dir);errno=saved;return -1;}
 if(darc_fsync_fd(dfd)){saved=errno;close(dfd);free(tmp);free(dir);errno=saved;return -1;}
 if(close(dfd)){saved=errno;free(tmp);free(dir);errno=saved;return -1;}
 free(tmp);free(dir);return 0;
}
int darc_atomic_write(const char*path,const void*data,size_t n,mode_t mode){return darc_atomic_write_checkpoint(path,data,n,mode,NULL);}
int darc_remove_tree(const char*path){struct stat st;if(lstat(path,&st))return errno==ENOENT?0:-1;if(!S_ISDIR(st.st_mode)||S_ISLNK(st.st_mode))return unlink(path);DIR*d=opendir(path);if(!d)return -1;struct dirent*de;int rc=0;while((de=readdir(d))){if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;char*p=darc_path_join(path,de->d_name);if(!p||darc_remove_tree(p)){rc=-1;free(p);break;}free(p);}closedir(d);if(!rc)rc=rmdir(path);return rc;}
int64_t darc_stat_mtime_ns(const struct stat*st){return (int64_t)st->st_mtim.tv_sec*1000000000LL+st->st_mtim.tv_nsec;}
int64_t darc_stat_ctime_ns(const struct stat*st){return (int64_t)st->st_ctim.tv_sec*1000000000LL+st->st_ctim.tv_nsec;}
void darc_error(int exit_code,const char*code,const char*fmt,...){darc_last_error=exit_code;snprintf(darc_last_code,sizeof darc_last_code,"%s",code);fprintf(stderr,"%s: ",code);va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);} 
void darc_checkpoint(const char*name){const char *s=getenv("DARC_TEST_CRASH_CHECKPOINT");if(s&&strcmp(s,name)==0){fflush(NULL);_exit(99);} }
