#define _POSIX_C_SOURCE 200809L
#include "cvc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

void *cvc_xmalloc(size_t n){ if(!n)n=1; void*p=malloc(n); if(!p){perror("malloc");exit(2);} return p; }
void *cvc_xcalloc(size_t n,size_t s){ if(!n)n=1; if(!s)s=1; void*p=calloc(n,s); if(!p){perror("calloc");exit(2);} return p; }
void *cvc_xrealloc(void*p,size_t n){ if(!n)n=1; void*q=realloc(p,n); if(!q){perror("realloc");exit(2);} return q; }
char *cvc_xstrdup(const char*s){size_t n=strlen(s);char*r=cvc_xmalloc(n+1);memcpy(r,s,n+1);return r;}
char *cvc_xstrndup(const char*s,size_t n){char*r=cvc_xmalloc(n+1);memcpy(r,s,n);r[n]=0;return r;}
int cvc_errorf(const char*fmt,...){va_list ap;va_start(ap,fmt);fprintf(stderr,"cvc: ");vfprintf(stderr,fmt,ap);fprintf(stderr,"\n");va_end(ap);return -1;}
void cvc_warnf(const char*fmt,...){va_list ap;va_start(ap,fmt);fprintf(stderr,"cvc: warning: ");vfprintf(stderr,fmt,ap);fprintf(stderr,"\n");va_end(ap);}

void cvc_buf_init(CvcBuf*b){b->data=NULL;b->len=b->cap=0;}
void cvc_buf_free(CvcBuf*b){free(b->data);b->data=NULL;b->len=b->cap=0;}
void cvc_buf_reserve(CvcBuf*b,size_t add){ if(add>SIZE_MAX-b->len){fprintf(stderr,"cvc: buffer overflow\n");exit(2);} size_t need=b->len+add; if(need<=b->cap)return; size_t c=b->cap?b->cap:128; while(c<need){ if(c>SIZE_MAX/2){c=need;break;} c*=2;} b->data=cvc_xrealloc(b->data,c);b->cap=c;}
void cvc_buf_append(CvcBuf *b,const void *p,size_t n){
    if(n==0)return;
    if(!p)abort();
    cvc_buf_reserve(b,n);
    if(!b->data)abort();
    memcpy(b->data+b->len,p,n);
    b->len+=n;
}
void cvc_buf_putc(CvcBuf*b,unsigned char c){cvc_buf_append(b,&c,1);}
void cvc_buf_printf(CvcBuf *b,const char *fmt,...){
    va_list ap,aq;
    va_start(ap,fmt);
    va_copy(aq,ap);
    int n=vsnprintf(NULL,0,fmt,aq);
    va_end(aq);
    if(n<0){va_end(ap);return;}
    size_t z=(size_t)n;
    cvc_buf_reserve(b,z+1u); /* vsnprintf also writes its trailing NUL. */
    if(!b->data)abort();
    (void)vsnprintf((char*)b->data+b->len,z+1u,fmt,ap);
    b->len+=z;
    va_end(ap);
}

void cvc_strvec_init(CvcStrVec*v){v->v=NULL;v->n=v->cap=0;}
void cvc_strvec_free(CvcStrVec*v){for(size_t i=0;i<v->n;i++)free(v->v[i]);free(v->v);v->v=NULL;v->n=v->cap=0;}
void cvc_strvec_push(CvcStrVec*v,const char*s){cvc_strvec_pushn(v,s,strlen(s));}
void cvc_strvec_pushn(CvcStrVec*v,const char*s,size_t n){if(v->n==v->cap){v->cap=v->cap?v->cap*2:8;v->v=cvc_xrealloc(v->v,v->cap*sizeof(*v->v));}v->v[v->n++]=cvc_xstrndup(s,n);}
static int cmp_str_unsigned(const void*a,const void*b){const unsigned char*x=(const unsigned char*)*(const char*const*)a,*y=(const unsigned char*)*(const char*const*)b;while(*x&&*y){if(*x!=*y)return *x<*y?-1:1;x++;y++;}return *x?1:*y?-1:0;}
void cvc_strvec_sort(CvcStrVec *v){
    if(v->n>1)qsort(v->v,v->n,sizeof(*v->v),cmp_str_unsigned);
}
int cvc_strvec_contains(const CvcStrVec*v,const char*s){for(size_t i=0;i<v->n;i++)if(strcmp(v->v[i],s)==0)return 1;return 0;}

int cvc_utf8_valid(const unsigned char*s,size_t n){size_t i=0;while(i<n){unsigned c=s[i++];if(c<=0x7f)continue;if(c>=0xc2&&c<=0xdf){if(i>=n||(s[i]&0xc0)!=0x80)return 0;i++;continue;}if(c>=0xe0&&c<=0xef){if(i+1>=n)return 0;unsigned c1=s[i],c2=s[i+1];if((c1&0xc0)!=0x80||(c2&0xc0)!=0x80)return 0;if(c==0xe0&&c1<0xa0)return 0;if(c==0xed&&c1>=0xa0)return 0;i+=2;continue;}if(c>=0xf0&&c<=0xf4){if(i+2>=n)return 0;unsigned c1=s[i],c2=s[i+1],c3=s[i+2];if((c1&0xc0)!=0x80||(c2&0xc0)!=0x80||(c3&0xc0)!=0x80)return 0;if(c==0xf0&&c1<0x90)return 0;if(c==0xf4&&c1>=0x90)return 0;i+=3;continue;}return 0;}return 1;}
int cvc_name_component_valid(const unsigned char*s,size_t n){if(!n||!cvc_utf8_valid(s,n))return 0;if((n==1&&s[0]=='.')||(n==2&&s[0]=='.'&&s[1]=='.'))return 0;for(size_t i=0;i<n;i++)if(s[i]==0||s[i]=='/'||(s[i]>=1&&s[i]<=0x1f)||s[i]==0x7f)return 0;return 1;}
int cvc_repo_path_valid(const char*s){if(!s||!*s||s[0]=='/')return 0;size_t n=strlen(s);if(!cvc_utf8_valid((const unsigned char*)s,n))return 0;size_t st=0;for(size_t i=0;i<=n;i++){unsigned char c=(unsigned char)s[i];if(c>=1&&c<=0x1f)return 0;if(c==0x7f)return 0;if(c=='/'||c==0){if(!cvc_name_component_valid((const unsigned char*)s+st,i-st))return 0;st=i+1;}}return 1;}
int cvc_branch_name_valid(const char*s){if(!s)return 0;size_t n=strlen(s);if(n<1||n>128||s[0]=='-'||s[0]=='/'||s[n-1]=='/'||s[n-1]=='.')return 0;if(strcmp(s,"HEAD")==0||strstr(s,"..")||strchr(s,'\\'))return 0;if(!cvc_utf8_valid((const unsigned char*)s,n))return 0;size_t st=0;for(size_t i=0;i<=n;i++){unsigned char c=(unsigned char)s[i];if((c>=1&&c<=0x1f)||c==0x7f)return 0;if(c=='/'||c==0){size_t m=i-st;if(!m||(m==1&&s[st]=='.')||(m==2&&s[st]=='.'&&s[st+1]=='.'))return 0;st=i+1;}}return 1;}
int cvc_ascii_hex(const char*s,size_t n){for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return 0;}return 1;}
int cvc_parse_u64_canon(const char*s,uint64_t*out){if(!s||!s[0]||s[0]=='0')return 0;uint64_t x=0;for(size_t i=0;s[i];i++){unsigned c=(unsigned)(s[i]-'0');if(c>9)return 0;if(x>(UINT64_MAX-c)/10)return 0;x=x*10+c;}*out=x;return 1;}
int cvc_parse_i64_timestamp(const char*s,int64_t*out){if(!s||!*s)return 0;int neg=0;size_t i=0;if(s[0]=='-'){neg=1;i=1;if(s[i]=='0'||!s[i])return 0;}else if(s[0]=='+')return 0;if(!neg&&s[0]=='0'){if(s[1])return 0;*out=0;return 1;}uint64_t x=0;for(;s[i];i++){if(s[i]<'0'||s[i]>'9')return 0;unsigned d=(unsigned)(s[i]-'0');uint64_t lim=neg?(uint64_t)INT64_MAX+1u:(uint64_t)INT64_MAX;if(x>(lim-d)/10)return 0;x=x*10+d;}if(neg){if(x==(uint64_t)INT64_MAX+1u)*out=INT64_MIN;else*out=-(int64_t)x;}else*out=(int64_t)x;return 1;}

char*cvc_path_join(const char*a,const char*b){size_t na=strlen(a),nb=strlen(b);size_t slash=(na&&a[na-1]!='/')?1u:0u;char*r=cvc_xmalloc(na+slash+nb+1);memcpy(r,a,na);if(slash)r[na++]='/';memcpy(r+na,b,nb+1);return r;}
char*cvc_path_dirname(const char*p){const char*s=strrchr(p,'/');if(!s)return cvc_xstrdup(".");if(s==p)return cvc_xstrdup("/");return cvc_xstrndup(p,(size_t)(s-p));}
char*cvc_path_basename_dup(const char*p){const char*s=strrchr(p,'/');return cvc_xstrdup(s?s+1:p);}
int cvc_mkdir_p(const char*path,mode_t mode){char*t=cvc_xstrdup(path);size_t n=strlen(t);for(size_t i=1;i<=n;i++)if(t[i]=='/'||t[i]==0){char save=t[i];t[i]=0;if(*t&&mkdir(t,mode)<0&&errno!=EEXIST){free(t);return -1;}t[i]=save;}free(t);return 0;}
int cvc_write_all(int fd,const void*p,size_t n){const unsigned char*s=p;while(n){ssize_t w=write(fd,s,n);if(w<0){if(errno==EINTR)continue;return -1;}if(w==0){errno=EIO;return -1;}s+=(size_t)w;n-=(size_t)w;}return 0;}
static int read_fd_all(int fd,CvcBuf*out){unsigned char tmp[32768];for(;;){ssize_t r=read(fd,tmp,sizeof(tmp));if(r<0){if(errno==EINTR)continue;return -1;}if(!r)return 0;cvc_buf_append(out,tmp,(size_t)r);}}
int cvc_read_file(const char*path,CvcBuf*out){int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;cvc_buf_init(out);int rc=read_fd_all(fd,out);int e=errno;if(close(fd)<0&&rc==0){rc=-1;e=errno;}errno=e;if(rc<0)cvc_buf_free(out);return rc;}
int cvc_read_file_nofollow(const char*path,CvcBuf*out){int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;struct stat st;if(fstat(fd,&st)<0||!S_ISREG(st.st_mode)){int e=errno?errno:EINVAL;close(fd);errno=e;return -1;}cvc_buf_init(out);int rc=read_fd_all(fd,out);int e=errno;if(close(fd)<0&&rc==0){rc=-1;e=errno;}errno=e;if(rc<0)cvc_buf_free(out);return rc;}
int cvc_fsync_parent(const char*path){char*d=cvc_path_dirname(path);int fd=open(d,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);free(d);if(fd<0)return -1;int rc=fsync(fd),e=errno;if(close(fd)<0&&rc==0){rc=-1;e=errno;}errno=e;return rc;}
int cvc_atomic_write(const char*path,const void*data,size_t len,mode_t mode){char*d=cvc_path_dirname(path),*base=cvc_path_basename_dup(path);size_t nd=strlen(d),nb=strlen(base);char*tmp=cvc_xmalloc(nd+nb+32);snprintf(tmp,nd+nb+32,"%s/.%s.tmp.XXXXXX",d,base);int fd=mkstemp(tmp);if(fd<0){free(d);free(base);free(tmp);return -1;}int rc=0,e=0;if(fchmod(fd,mode)<0||cvc_write_all(fd,data,len)<0||fsync(fd)<0)rc=-1,e=errno;if(close(fd)<0&&rc==0)rc=-1,e=errno;if(rc==0&&rename(tmp,path)<0)rc=-1,e=errno;if(rc==0&&cvc_fsync_parent(path)<0)rc=-1,e=errno;if(rc<0)unlink(tmp);free(d);free(base);free(tmp);errno=e;return rc;}
int cvc_lstat_exists(const char*path){struct stat st;return lstat(path,&st)==0;}
int cvc_is_real_dir(const char*path){struct stat st;return lstat(path,&st)==0&&S_ISDIR(st.st_mode);}
int cvc_is_real_regular(const char*path){struct stat st;return lstat(path,&st)==0&&S_ISREG(st.st_mode);}
int cvc_remove_tree_nofollow(const char*path){struct stat st;if(lstat(path,&st)<0){return errno==ENOENT?0:-1;}if(S_ISDIR(st.st_mode)){DIR*d=opendir(path);if(!d)return -1;struct dirent*de;int rc=0;while((de=readdir(d))){if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;char*q=cvc_path_join(path,de->d_name);if(cvc_remove_tree_nofollow(q)<0)rc=-1;free(q);if(rc<0)break;}int e=errno;closedir(d);if(rc<0){errno=e;return -1;}return rmdir(path);}return unlink(path);}
char*cvc_getcwd_alloc(void){size_t n=256;for(;;){char*p=cvc_xmalloc(n);if(getcwd(p,n))return p;int e=errno;free(p);if(e!=ERANGE){errno=e;return NULL;}n*=2;}}
char*cvc_abspath_lexical(const char*path){if(path[0]=='/')return cvc_xstrdup(path);char*c=cvc_getcwd_alloc();if(!c)return NULL;char*r=cvc_path_join(c,path);free(c);return r;}
int cvc_path_is_prefix(const char*a,const char*p){size_t n=strlen(a);return strncmp(a,p,n)==0&&(p[n]==0||p[n]=='/');}
void cvc_hex_encode(const uint8_t*in,size_t n,char*out){static const char h[]="0123456789abcdef";for(size_t i=0;i<n;i++){out[2*i]=h[in[i]>>4];out[2*i+1]=h[in[i]&15];}out[2*n]=0;}
int cvc_hex_decode_32(const char*hex,uint8_t out[32]){if(strlen(hex)!=64||!cvc_ascii_hex(hex,64))return 0;for(int i=0;i<32;i++){unsigned a=(unsigned)(hex[2*i]),b=(unsigned)(hex[2*i+1]);a=(a<='9'?a-'0':(a|32)-'a'+10);b=(b<='9'?b-'0':(b|32)-'a'+10);out[i]=(uint8_t)((a<<4)|b);}return 1;}
int cvc_bytes_cmp(const void*ap,const void*bp,size_t n){const unsigned char*a=ap,*b=bp;for(size_t i=0;i<n;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
void cvc_put_u32be(CvcBuf*b,uint32_t x){unsigned char p[4]={(unsigned char)(x>>24),(unsigned char)(x>>16),(unsigned char)(x>>8),(unsigned char)x};cvc_buf_append(b,p,4);}
void cvc_put_u64be(CvcBuf*b,uint64_t x){unsigned char p[8];for(int i=7;i>=0;i--){p[i]=(unsigned char)x;x>>=8;}cvc_buf_append(b,p,8);}
uint32_t cvc_get_u32be(const unsigned char*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
uint64_t cvc_get_u64be(const unsigned char*p){uint64_t x=0;for(int i=0;i<8;i++)x=(x<<8)|p[i];return x;}
