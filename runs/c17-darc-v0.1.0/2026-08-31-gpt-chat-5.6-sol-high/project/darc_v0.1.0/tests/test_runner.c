#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>

bool test_run_stress=false;
const char *test_root=NULL;
static const char *test_filter_id=NULL;
static unsigned char seen[252];
static int passed=0,failed=0,skipped=0;
static const char *ids[252]={
 "ALG-001",
 "ALG-002",
 "ALG-003",
 "ALG-004",
 "ALG-005",
 "ALG-006",
 "ALG-007",
 "ALG-008",
 "ALG-009",
 "ALG-010",
 "ALG-011",
 "ALG-012",
 "ALG-013",
 "ALG-014",
 "ALG-015",
 "ALG-016",
 "ALG-017",
 "ALG-018",
 "ALG-019",
 "ALG-020",
 "ALG-021",
 "ALG-022",
 "ALG-023",
 "ALG-024",
 "ALG-025",
 "ALG-026",
 "ALG-027",
 "ALG-028",
 "ALG-029",
 "ALG-030",
 "CFG-001",
 "CFG-002",
 "CFG-003",
 "CFG-004",
 "CFG-005",
 "CFG-006",
 "CFG-007",
 "CFG-008",
 "CFG-009",
 "CFG-010",
 "CFG-011",
 "CFG-012",
 "CFG-013",
 "CFG-014",
 "CFG-015",
 "CFG-016",
 "CFG-017",
 "CFG-018",
 "CFG-019",
 "CFG-020",
 "CFG-021",
 "CFG-022",
 "SCN-001",
 "SCN-002",
 "SCN-003",
 "SCN-004",
 "SCN-005",
 "SCN-006",
 "SCN-007",
 "SCN-008",
 "SCN-009",
 "SCN-010",
 "SCN-011",
 "SCN-012",
 "SCN-013",
 "SCN-014",
 "SCN-015",
 "SCN-016",
 "SCN-017",
 "SCN-018",
 "SCN-019",
 "SCN-020",
 "SCN-021",
 "SCN-022",
 "SCN-023",
 "SCN-024",
 "SCN-025",
 "SCN-026",
 "SCN-027",
 "SCN-028",
 "SCN-029",
 "SCN-030",
 "INC-001",
 "INC-002",
 "INC-003",
 "INC-004",
 "INC-005",
 "INC-006",
 "INC-007",
 "INC-008",
 "INC-009",
 "INC-010",
 "INC-011",
 "INC-012",
 "INC-013",
 "INC-014",
 "INC-015",
 "DIF-001",
 "DIF-002",
 "DIF-003",
 "DIF-004",
 "DIF-005",
 "DIF-006",
 "DIF-007",
 "DIF-008",
 "DIF-009",
 "DIF-010",
 "DIF-011",
 "DIF-012",
 "DIF-013",
 "DIF-014",
 "DIF-015",
 "DIF-016",
 "DIF-017",
 "DIF-018",
 "DIF-019",
 "DIF-020",
 "DIF-021",
 "DIF-022",
 "RST-001",
 "RST-002",
 "RST-003",
 "RST-004",
 "RST-005",
 "RST-006",
 "RST-007",
 "RST-008",
 "RST-009",
 "RST-010",
 "RST-011",
 "RST-012",
 "RST-013",
 "RST-014",
 "RST-015",
 "RST-016",
 "RST-017",
 "RST-018",
 "RST-019",
 "RST-020",
 "RST-021",
 "RST-022",
 "RST-023",
 "RST-024",
 "VER-001",
 "VER-002",
 "VER-003",
 "VER-004",
 "VER-005",
 "VER-006",
 "VER-007",
 "VER-008",
 "VER-009",
 "VER-010",
 "VER-011",
 "VER-012",
 "VER-013",
 "VER-014",
 "VER-015",
 "VER-016",
 "VER-017",
 "VER-018",
 "VER-019",
 "VER-020",
 "VER-021",
 "VER-022",
 "VER-023",
 "VER-024",
 "VER-025",
 "VER-026",
 "VER-027",
 "VER-028",
 "GCI-001",
 "GCI-002",
 "GCI-003",
 "GCI-004",
 "GCI-005",
 "GCI-006",
 "GCI-007",
 "GCI-008",
 "GCI-009",
 "GCI-010",
 "GCI-011",
 "GCI-012",
 "GCI-013",
 "GCI-014",
 "GCI-015",
 "CRS-001",
 "CRS-002",
 "CRS-003",
 "CRS-004",
 "CRS-005",
 "CRS-006",
 "CRS-007",
 "CRS-008",
 "CRS-009",
 "CRS-010",
 "CRS-011",
 "CRS-012",
 "CRS-013",
 "CRS-014",
 "CRS-015",
 "CRS-016",
 "CRS-017",
 "CRS-018",
 "CRS-019",
 "FMT-001",
 "FMT-002",
 "FMT-003",
 "FMT-004",
 "FMT-005",
 "FMT-006",
 "FMT-007",
 "FMT-008",
 "FMT-009",
 "FMT-010",
 "SEC-001",
 "SEC-002",
 "SEC-003",
 "SEC-004",
 "SEC-005",
 "SEC-006",
 "SEC-007",
 "SEC-008",
 "STR-001",
 "STR-002",
 "STR-003",
 "STR-004",
 "STR-005",
 "STR-006",
 "STR-007",
 "STR-008",
 "CLI-001",
 "CLI-002",
 "CLI-003",
 "CLI-004",
 "CLI-005",
 "CLI-006",
 "CLI-007",
 "CLI-008",
 "CLI-009",
 "CLI-010",
 "CLI-011",
 "CLI-012",
 "CLI-013",
 "CLI-014",
 "CLI-015",
 "DEL-001",
 "DEL-002",
 "DEL-003",
 "DEL-004",
 "DEL-005",
 "DEL-006"
};
static int id_index(const char *id){for(int i=0;i<252;i++)if(!strcmp(ids[i],id))return i;return -1;}
static bool is_stress_id(const char *id){return !strcmp(id,"SCN-029")||!strcmp(id,"RST-023")||!strncmp(id,"STR-",4);}
bool test_wants(const char *id){if(test_filter_id)return !strcmp(test_filter_id,id);if(!test_run_stress&&is_stress_id(id))return false;return true;}
void test_check(const char *id,bool ok,const char *fmt,...){if(!test_wants(id))return;int k=id_index(id);if(k<0){fprintf(stderr,"INTERNAL unknown catalog id %s\n",id);failed++;return;}if(seen[k]){fprintf(stderr,"INTERNAL duplicate catalog id %s\n",id);failed++;return;}seen[k]=1;if(ok){passed++;printf("PASS %s",id);}else{failed++;printf("FAIL %s",id);}if(fmt&&*fmt){printf(" — ");va_list ap;va_start(ap,fmt);vprintf(fmt,ap);va_end(ap);}putchar('\n');fflush(stdout);}
void test_skip(const char *id,const char *why){if(!test_wants(id))return;int k=id_index(id);if(k<0||seen[k]){failed++;return;}seen[k]=1;skipped++;printf("SKIP %s — %s\n",id,why?why:"");}
char *test_path(const char*a,const char*b){return darc_path_join(a,b);}
int test_write(const char*p,const void*d,size_t n,unsigned mode){char*dir=darc_dirname_dup(p);if(!dir)return -1;if(darc_mkdir_p(dir,0755)&&errno!=EEXIST){free(dir);return -1;}free(dir);int fd=open(p,O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC,(mode_t)mode);if(fd<0)return -1;int rc=darc_write_all(fd,d,n);if(close(fd)&&!rc)rc=-1;return rc;}
int test_read(const char*p,DarcBuf*b){return darc_read_file(p,b,1024u*1024u*1024u);}
int test_mkdir(const char*p){return darc_mkdir_p(p,0755);}
char *test_case_dir(const char*name){static unsigned seq;char z[160];snprintf(z,sizeof z,"%03u-%s",++seq,name);char*p=test_path(test_root,z);if(p){(void)darc_remove_tree(p);if(darc_mkdir_p(p,0755)){free(p);return NULL;}}return p;}
static char *slurp_fd(int fd){DarcBuf b;darc_buf_init(&b);uint8_t tmp[4096];for(;;){ssize_t r=read(fd,tmp,sizeof tmp);if(r<0){if(errno==EINTR)continue;break;}if(!r)break;if(darc_buf_put(&b,tmp,(size_t)r))break;}darc_buf_u8(&b,0);return (char*)b.data;}
TestProc test_exec(const char *const argv[],const char *const envv[]){TestProc p={-1,NULL,NULL};int po[2],pe[2];if(pipe(po)||pipe(pe))return p;pid_t pid=fork();if(pid==0){dup2(po[1],STDOUT_FILENO);dup2(pe[1],STDERR_FILENO);close(po[0]);close(po[1]);close(pe[0]);close(pe[1]);if(envv)for(size_t i=0;envv[i];i++){const char*eq=strchr(envv[i],'=');if(eq){char*k=darc_strndup(envv[i],(size_t)(eq-envv[i]));if(k){if(!strcmp(k,"TEST_DROP_UID")&&geteuid()==0){unsigned long u=strtoul(eq+1,NULL,10);(void)setgid((gid_t)u);(void)setuid((uid_t)u);}else setenv(k,eq+1,1);free(k);}}}execv("./darc",(char *const*)argv);_exit(127);}close(po[1]);close(pe[1]);p.out=slurp_fd(po[0]);p.err=slurp_fd(pe[0]);close(po[0]);close(pe[0]);int st=0;if(pid>0){while(waitpid(pid,&st,0)<0&&errno==EINTR){}if(WIFEXITED(st))p.code=WEXITSTATUS(st);else if(WIFSIGNALED(st))p.code=128+WTERMSIG(st);}return p;}
void test_proc_free(TestProc*p){free(p->out);free(p->err);memset(p,0,sizeof *p);}
bool test_contains(const char*s,const char*n){return s&&n&&strstr(s,n)!=NULL;}
bool test_file_eq(const char*a,const char*b){
 int fa=open(a,O_RDONLY|O_CLOEXEC),fb=open(b,O_RDONLY|O_CLOEXEC);if(fa<0||fb<0){if(fa>=0)close(fa);if(fb>=0)close(fb);return false;}
 struct stat sa,sb;if(fstat(fa,&sa)||fstat(fb,&sb)||sa.st_size!=sb.st_size){close(fa);close(fb);return false;}
 uint8_t xa[65536],xb[65536];bool ok=true;for(;;){ssize_t na=read(fa,xa,sizeof xa),nb=read(fb,xb,sizeof xb);if(na<0&&errno==EINTR)continue;if(nb<0&&errno==EINTR)continue;if(na<0||nb<0||na!=nb){ok=false;break;}if(!na)break;if(memcmp(xa,xb,(size_t)na)){ok=false;break;}}
 close(fa);close(fb);return ok;
}
static int cmp_cstr(const void*x,const void*y){return strcmp(*(char*const*)x,*(char*const*)y);}
static bool tree_rec(const char*a,const char*b,bool meta){DIR*da=opendir(a),*db=opendir(b);if(!da||!db){if(da)closedir(da);if(db)closedir(db);return false;}struct dirent*de;size_t na=0,nb=0;char**aa=NULL,**bb=NULL;while((de=readdir(da)))if(strcmp(de->d_name,".")&&strcmp(de->d_name,"..")){char**v=realloc(aa,(na+1)*sizeof*aa);if(!v)goto bad;aa=v;aa[na++]=strdup(de->d_name);}while((de=readdir(db)))if(strcmp(de->d_name,".")&&strcmp(de->d_name,"..")){char**v=realloc(bb,(nb+1)*sizeof*bb);if(!v)goto bad;bb=v;bb[nb++]=strdup(de->d_name);}closedir(da);closedir(db);da=db=NULL;if(na!=nb)goto bad;if(na>1)qsort(aa,na,sizeof*aa,cmp_cstr);if(nb>1)qsort(bb,nb,sizeof*bb,cmp_cstr);for(size_t i=0;i<na;i++){if(strcmp(aa[i],bb[i]))goto bad;char*pa=test_path(a,aa[i]),*pb=test_path(b,bb[i]);struct stat sa,sb;if(!pa||!pb||lstat(pa,&sa)||lstat(pb,&sb)){free(pa);free(pb);goto bad;}if((sa.st_mode&S_IFMT)!=(sb.st_mode&S_IFMT)){free(pa);free(pb);goto bad;}if(meta&&((sa.st_mode&07777)!=(sb.st_mode&07777))){free(pa);free(pb);goto bad;}bool ok=true;if(S_ISREG(sa.st_mode))ok=test_file_eq(pa,pb);else if(S_ISDIR(sa.st_mode))ok=tree_rec(pa,pb,meta);else if(S_ISLNK(sa.st_mode)){char xa[PATH_MAX],xb[PATH_MAX];ssize_t la=readlink(pa,xa,sizeof xa),lb=readlink(pb,xb,sizeof xb);ok=la>=0&&la==lb&&!memcmp(xa,xb,(size_t)la);}free(pa);free(pb);if(!ok)goto bad;}for(size_t i=0;i<na;i++){free(aa[i]);free(bb[i]);}free(aa);free(bb);return true;
bad:if(da)closedir(da);if(db)closedir(db);for(size_t i=0;i<na;i++)free(aa[i]);for(size_t i=0;i<nb;i++)free(bb[i]);free(aa);free(bb);return false;}
bool test_tree_eq(const char*a,const char*b,bool meta){return tree_rec(a,b,meta);}
static void count_rec(const char*p,int type,size_t*n){DIR*d=opendir(p);if(!d)return;struct dirent*de;while((de=readdir(d))){if(de->d_name[0]=='.')continue;char*q=test_path(p,de->d_name);struct stat st;if(q&&!lstat(q,&st)){if(S_ISDIR(st.st_mode))count_rec(q,type,n);else{DarcRhEnt h;if(!darc_object_header(q,&h)&&(type==0||h.type==type))(*n)++;}}free(q);}closedir(d);}
size_t test_count_objects(const char*repo,int type){char*p=test_path(repo,"objects/sha256");size_t n=0;if(p)count_rec(p,type,&n);free(p);return n;}
int test_head(const char*repo,char out[65]){char*p=test_path(repo,"HEAD");DarcBuf b;int rc=p?test_read(p,&b):-1;free(p);if(rc)return -1;if(b.len==0){out[0]=0;darc_buf_free(&b);return 0;}if(b.len!=65||b.data[64]!='\n'){darc_buf_free(&b);return -1;}memcpy(out,b.data,64);out[64]=0;darc_buf_free(&b);return 0;}
int main(int argc,char**argv){
 bool all=false;
 if(argc==2&&!strcmp(argv[1],"--quick")){test_run_stress=false;}
 else if(argc==2&&!strcmp(argv[1],"--all")){test_run_stress=true;all=true;}
 else if(argc==3&&!strcmp(argv[1],"--case")){if(id_index(argv[2])<0){fprintf(stderr,"unknown catalog id: %s\n",argv[2]);return 2;}test_run_stress=true;test_filter_id=argv[2];}
 else{fprintf(stderr,"usage: %s --quick|--all|--case CATALOG-ID\n",argv[0]);return 2;}
 char templ[PATH_MAX];const char *base=getenv("DARC_TEST_ROOT_BASE");if(!base||!*base)base="/tmp";if(snprintf(templ,sizeof templ,"%s/darc-tests-XXXXXX",base)<0||strlen(templ)>=sizeof templ){fprintf(stderr,"test root path too long\n");return 2;}test_root=mkdtemp(templ);if(!test_root){perror("mkdtemp");return 2;}
 printf("DARC acceptance runner; root=%s; randomized_seed=0x5eedc0de; mode=%s%s%s\n",test_root,all?"all":test_filter_id?"case":"quick",test_filter_id?"; case=":"",test_filter_id?test_filter_id:"");
 if(test_filter_id){
  if(!strncmp(test_filter_id,"RST-",4))test_restore();
  else if(!strncmp(test_filter_id,"STR-",4))test_stress();
  else if(!strncmp(test_filter_id,"SCN-",4))test_scan_only();
  else if(!strncmp(test_filter_id,"VER-",4)||!strncmp(test_filter_id,"GCI-",4)||!strncmp(test_filter_id,"CRS-",4)||!strncmp(test_filter_id,"FMT-",4)||!strncmp(test_filter_id,"SEC-",4)||!strncmp(test_filter_id,"CLI-",4)||!strncmp(test_filter_id,"DEL-",4))test_repository();
  else if(!strncmp(test_filter_id,"ALG-",4))test_algorithms();
  else if(!strncmp(test_filter_id,"CFG-",4))test_config_cli();
  else if(!strncmp(test_filter_id,"INC-",4))test_incremental();
  else if(!strncmp(test_filter_id,"DIF-",4))test_diff();
 }else{test_algorithms();test_config_cli();test_repository();test_incremental();test_diff();test_restore();test_stress();}
 int expected=0;for(int i=0;i<252;i++)if(test_wants(ids[i])){expected++;if(!seen[i]){failed++;fprintf(stderr,"MISSING %s — catalog test was not executed\n",ids[i]);}}
 printf("\nRESULT total_passed=%d total_failed=%d total_skipped=%d catalog_total=%d randomized_seed=0x5eedc0de\n",passed,failed,skipped,expected);
 if(failed){printf("FAILING_OR_MISSING:");for(int i=0;i<252;i++)if(test_wants(ids[i])&&!seen[i])printf(" %s",ids[i]);printf("\n");}
 if(!getenv("DARC_TEST_KEEP"))(void)darc_remove_tree(test_root);
 return failed?1:0;
}
