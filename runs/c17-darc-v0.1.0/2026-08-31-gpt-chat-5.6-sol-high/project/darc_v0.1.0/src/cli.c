#include "darc.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

static void usage(FILE *f){
 fprintf(f,
 "DARC deterministic deduplicating archive %s\n"
 "Usage: darc [global-options] <command> [command-options]\n\n"
 "Global options:\n"
 "  --repo PATH              repository directory (default .darc)\n"
 "  --config PATH            JSON/YAML configuration file\n"
 "  --format text|json|ndjson\n"
 "  --color auto|always|never\n"
 "  --quiet | --verbose\n"
 "  --help | --version\n\n"
 "Commands:\n"
 "  init PATH\n"
 "  snapshot create SOURCE... [--name NAME] [--parent SNAPSHOT] [--timestamp NS]\n"
 "  snapshot list\n"
 "  snapshot show SNAPSHOT\n"
 "  snapshot delete SNAPSHOT --yes [--dry-run]\n"
 "  snapshot diff OLD NEW [--path PATH] [--format text|json|ndjson|svg]\n"
 "  restore SNAPSHOT --to PATH [--path PATH] [--overwrite never|files|all]\n"
 "  verify [--level quick|full|scrub] [--repair] [--format text|json|ndjson|svg]\n"
 "  gc [--dry-run] [--repack-parity]\n"
 "  stats [--format text|json|ndjson|svg]\n"
 "  index rebuild\n"
 "  repo inspect\n"
 "  config validate FILE\n",DARC_VERSION);
}
static int usage_error(const char *msg){darc_error(DARC_E_USAGE,"E_USAGE","%s",msg);return DARC_E_USAGE;}
static int parse_i64(const char*s,int64_t*out){if(!s||!*s)return -1;char*e=NULL;errno=0;long long v=strtoll(s,&e,10);if(errno||!e||*e)return -1;*out=(int64_t)v;return 0;}
static int fmt_value(const char*s){if(!strcmp(s,"text"))return 0;if(!strcmp(s,"json"))return 1;if(!strcmp(s,"ndjson"))return 2;if(!strcmp(s,"svg"))return 3;return -1;}
static int color_value(const char*s){if(!strcmp(s,"auto"))return 0;if(!strcmp(s,"always"))return 1;if(!strcmp(s,"never"))return 2;return -1;}
static int overwrite_value(const char*s){if(!strcmp(s,"never"))return 0;if(!strcmp(s,"files"))return 1;if(!strcmp(s,"all"))return 2;return -1;}
static int level_value(const char*s){if(!strcmp(s,"quick"))return 0;if(!strcmp(s,"full"))return 1;if(!strcmp(s,"scrub"))return 2;return -1;}
static const char *opt_value(int argc,char**argv,int *i,const char*arg,const char*name){
 size_t n=strlen(name);if(!strncmp(arg,name,n)&&arg[n]=='=')return arg+n+1;
 if(!strcmp(arg,name)){if(*i+1>=argc)return NULL;(*i)++;return argv[*i];}return (const char*)-1;
}
static int recalc(DarcConfig*c){DarcBuf b,p;if(darc_config_normalize(c,&b,false))return -1;darc_buf_free(&b);if(darc_config_normalize(c,&p,true))return -1;darc_buf_free(&p);return 0;}
static const char*config_code(const char*e){if(strstr(e,"E_CONFIG_FORMAT"))return "E_CONFIG_FORMAT";if(strstr(e," at line "))return "E_CONFIG_PARSE";return "E_CONFIG_SCHEMA";}
static void command_help(const char*cmd,const char*sub){
 if(!strcmp(cmd,"init"))printf("Usage: darc init PATH\nCreate a new repository only in a missing or empty target.\nExample: darc init backup.darc\n");
 else if(!strcmp(cmd,"snapshot")&&sub&&!strcmp(sub,"create"))printf("Usage: darc snapshot create SOURCE... [--name NAME] [--parent SNAPSHOT] [--timestamp NS]\nCreates and atomically publishes a snapshot.\nExample: darc --repo repo snapshot create data --name nightly\n");
 else if(!strcmp(cmd,"snapshot")&&sub&&!strcmp(sub,"list"))printf("Usage: darc snapshot list\nList published snapshots newest-first.\nExample: darc --repo repo snapshot list\n");
 else if(!strcmp(cmd,"snapshot")&&sub&&!strcmp(sub,"show"))printf("Usage: darc snapshot show SNAPSHOT\nShow snapshot metadata; selector may be HEAD, CID/prefix, or unique name.\nExample: darc --repo repo snapshot show HEAD\n");
 else if(!strcmp(cmd,"snapshot")&&sub&&!strcmp(sub,"delete"))printf("Usage: darc snapshot delete SNAPSHOT --yes [--dry-run]\nDESTRUCTIVE: removes only the published ref; objects remain until GC. --yes is mandatory except dry-run.\nExample: darc --repo repo snapshot delete deadbeef --dry-run\n");
 else if(!strcmp(cmd,"snapshot")&&sub&&!strcmp(sub,"diff"))printf("Usage: darc snapshot diff OLD NEW [--path PATH] [--format text|json|ndjson|svg]\nCompare canonical path/content/metadata semantics.\nExample: darc --repo repo snapshot diff OLD HEAD --format svg\n");
 else if(!strcmp(cmd,"restore"))printf("Usage: darc restore SNAPSHOT --to PATH [--path PATH] [--overwrite never|files|all]\nWrites only beneath --to and verifies every restored file.\nExample: darc --repo repo restore HEAD --to restored\n");
 else if(!strcmp(cmd,"verify"))printf("Usage: darc verify [--level quick|full|scrub] [--repair] [--format text|json|ndjson|svg]\n--repair MUTATES only safe repairable repository state after integrity checks.\nExample: darc --repo repo verify --level scrub --repair\n");
 else if(!strcmp(cmd,"gc"))printf("Usage: darc gc [--dry-run] [--repack-parity]\nDESTRUCTIVE without --dry-run: removes unreachable canonical objects after journaled reachability analysis.\nExample: darc --repo repo gc --dry-run\n");
 else if(!strcmp(cmd,"stats"))printf("Usage: darc stats [--format text|json|ndjson|svg]\nReport repository storage/dedup/compression statistics.\nExample: darc --repo repo stats --format svg\n");
 else if(!strcmp(cmd,"index")&&sub&&!strcmp(sub,"rebuild"))printf("Usage: darc index rebuild\nRebuild derived chunks.idx from canonical object headers.\nExample: darc --repo repo index rebuild\n");
 else if(!strcmp(cmd,"repo")&&sub&&!strcmp(sub,"inspect"))printf("Usage: darc repo inspect\nInspect repository format, refs, objects, and derived index state.\nExample: darc --repo repo repo inspect\n");
 else if(!strcmp(cmd,"config")&&sub&&!strcmp(sub,"validate"))printf("Usage: darc config validate FILE\nParse JSON/YAML, validate schema, normalize, and report hashes.\nExample: darc config validate examples/config.yaml\n");
 else usage(stdout);
}
static int load_cfg(const char*repo,const char*path,bool use_repo,DarcConfig*c){
 darc_config_defaults(c);
 if(use_repo){
  int rr=darc_repo_check(repo);
  if(rr){
   darc_config_free(c);
   if(rr==DARC_E_UNSUPPORTED)darc_error(rr,"E_UNSUPPORTED_VERSION","repository format is unsupported");
   else if(rr==DARC_E_REPO)darc_error(rr,"E_REPO_NOT_FOUND","repository not found or FORMAT is missing");
   else darc_error(rr,"E_IO","cannot read repository FORMAT");
   return rr;
  }
  if(darc_config_apply_repo_defaults(repo,c)){darc_config_free(c);darc_error(DARC_E_REPO,"E_REPO_FORMAT","cannot load repository defaults");return DARC_E_REPO;}
 }
 if(path){DarcBuf b;char err[512];if(darc_config_load_file(path,c,&b,err,sizeof err)){darc_config_free(c);darc_error(DARC_E_USAGE,config_code(err),"%s",err);return DARC_E_USAGE;}darc_buf_free(&b);}return 0;
}
static void emit_config(const DarcConfig*c,const DarcBuf*n){char h[65],p[65];darc_cid_hex(&c->config_hash,h);darc_cid_hex(&c->profile_hash,p);if(c->output_format==1){fwrite(n->data,1,n->len,stdout);printf("\n");fprintf(stderr,"config_hash=%s profile_hash=%s\n",h,p);}else if(c->output_format==2){printf("{\"record\":\"config\",\"config_hash\":\"%s\",\"profile_hash\":\"%s\",\"normalized\":",h,p);fwrite(n->data,1,n->len,stdout);printf("}\n");}else{printf("Normalized configuration:\n");fwrite(n->data,1,n->len,stdout);printf("\nconfig_hash: %s\nprofile_hash: %s\n",h,p);}}
static int require_format(const DarcConfig*c,bool svg_ok){if(c->output_format<0||c->output_format>3||(!svg_ok&&c->output_format==3)){darc_error(DARC_E_USAGE,"E_CONFIG_FORMAT_FOR_COMMAND","output format is not supported by this command");return DARC_E_USAGE;}return 0;}
static int recover_before_write(const char*repo){int rc=darc_recover_journals(repo);if(rc){if(rc==DARC_E_LOCKED){darc_error(DARC_E_LOCKED,"E_REPO_LOCKED","repository writer lock is held");return DARC_E_LOCKED;}darc_error(DARC_E_IO,"E_RECOVERY","repository journal recovery failed");return DARC_E_IO;}return 0;}
static int final_rc(int rc){if(rc<0)rc=DARC_E_IO;if(rc&&darc_last_error==0){const char*code="E_OPERATION_FAILED";switch(rc){case DARC_E_REPO:code="E_REPO";break;case DARC_E_NOTFOUND:code="E_NOT_FOUND";break;case DARC_E_IO:code="E_IO";break;case DARC_E_CORRUPT:code="E_REPO_CORRUPT";break;case DARC_E_UNRECOVERABLE:code="E_UNRECOVERABLE";break;case DARC_E_RESTORE:code="E_RESTORE_CONFLICT";break;case DARC_E_LOCKED:code="E_REPO_LOCKED";break;case DARC_E_UNSUPPORTED:code="E_UNSUPPORTED";break;default:break;}darc_error(rc,code,"command failed");}return rc;}

int darc_cli(int argc,char**argv){
 const char*repo=".darc",*cfgpath=NULL;int gfmt=-1,gcolor=-1;bool gquiet=false,gverbose=false,qset=false,vset=false;int i=1;
 darc_last_error=0;darc_last_code[0]=0;
 while(i<argc&&argv[i][0]=='-'){
  const char*a=argv[i],*v;if(!strcmp(a,"--help")||!strcmp(a,"-h")){usage(stdout);return 0;}if(!strcmp(a,"--version")){printf("darc %s\n",DARC_VERSION);return 0;}
  v=opt_value(argc,argv,&i,a,"--repo");if(v!=(const char*)-1){if(!v)return usage_error("--repo requires PATH");repo=v;i++;continue;}
  v=opt_value(argc,argv,&i,a,"--config");if(v!=(const char*)-1){if(!v)return usage_error("--config requires PATH");cfgpath=v;i++;continue;}
  v=opt_value(argc,argv,&i,a,"--format");if(v!=(const char*)-1){if(!v||(gfmt=fmt_value(v))<0||gfmt==3)return usage_error("global --format requires text|json|ndjson");i++;continue;}
  v=opt_value(argc,argv,&i,a,"--color");if(v!=(const char*)-1){if(!v||(gcolor=color_value(v))<0)return usage_error("--color requires auto|always|never");i++;continue;}
  if(!strcmp(a,"--quiet")){gquiet=true;qset=true;i++;continue;}if(!strcmp(a,"--verbose")){gverbose=true;vset=true;i++;continue;}if(a[0]=='-')return usage_error("unknown global option");break;
 }
 if(i>=argc){usage(stderr);return usage_error("missing command");}
 const char*cmd=argv[i++];
 if(i<argc&&!strcmp(argv[i],"--help")){command_help(cmd,NULL);return 0;}
 if(!strcmp(cmd,"snapshot")&&i+1<argc&&!strcmp(argv[i+1],"--help")){command_help(cmd,argv[i]);return 0;}
 if((!strcmp(cmd,"index")||!strcmp(cmd,"repo")||!strcmp(cmd,"config"))&&i+1<argc&&!strcmp(argv[i+1],"--help")){command_help(cmd,argv[i]);return 0;}
 if(!strcmp(cmd,"config")){
  if(i>=argc||strcmp(argv[i++],"validate")||i>=argc) return usage_error("usage: darc config validate FILE");
  const char*file=argv[i++];
  if(i!=argc) return usage_error("unexpected config validate argument");
  DarcConfig c;darc_config_defaults(&c);DarcBuf n;char err[512];if(darc_config_load_file(file,&c,&n,err,sizeof err)){darc_config_free(&c);darc_error(DARC_E_USAGE,config_code(err),"%s",err);return DARC_E_USAGE;}if(gfmt>=0)c.output_format=gfmt;if(gcolor>=0)c.color=gcolor;if(qset)c.quiet=gquiet;if(vset)c.verbose=gverbose;if(recalc(&c)){darc_buf_free(&n);darc_config_free(&c);return final_rc(DARC_E_INTERNAL);}darc_buf_free(&n);if(darc_config_normalize(&c,&n,false)){darc_config_free(&c);return final_rc(DARC_E_INTERNAL);}emit_config(&c,&n);darc_buf_free(&n);darc_config_free(&c);return 0;
 }
 if(!strcmp(cmd,"init")){
  if(i>=argc) return usage_error("usage: darc init PATH");
  const char*path=argv[i++];
  if(i!=argc) return usage_error("unexpected init argument");
  DarcConfig c;int rc=load_cfg(repo,cfgpath,false,&c);if(rc)return rc;if(gfmt>=0)c.output_format=gfmt;if(gcolor>=0)c.color=gcolor;if(qset)c.quiet=gquiet;if(vset)c.verbose=gverbose;recalc(&c);rc=require_format(&c,false);if(!rc){int ir=darc_repo_init(path,&c);rc=ir?DARC_E_IO:0;}if(!rc&&!c.quiet){if(c.output_format==1)printf("{\"status\":\"initialized\",\"repository\":\"%s\",\"format_version\":1}\n",path);else if(c.output_format==2)printf("{\"record\":\"result\",\"status\":\"initialized\",\"repository\":\"%s\"}\n",path);else printf("Initialized DARC repository at %s (format v1)\n",path);}darc_config_free(&c);return final_rc(rc);
 }
 DarcConfig c;int rc=load_cfg(repo,cfgpath,true,&c);if(rc)return rc;if(gfmt>=0)c.output_format=gfmt;if(gcolor>=0)c.color=gcolor;if(qset)c.quiet=gquiet;if(vset)c.verbose=gverbose;
 if(!strcmp(cmd,"snapshot")){
  if(i>=argc){darc_config_free(&c);return usage_error("snapshot requires subcommand");}const char*sub=argv[i++];
  if(!strcmp(sub,"create")){
   char**src=darc_calloc((size_t)(argc-i+1),sizeof*src);size_t ns=0;if(!src){darc_config_free(&c);return final_rc(DARC_E_IO);}for(;i<argc;i++){const char*a=argv[i],*v;
    v=opt_value(argc,argv,&i,a,"--name");if(v!=(const char*)-1){if(!v){free(src);darc_config_free(&c);return usage_error("--name requires NAME");}free(c.snapshot_name);c.snapshot_name=darc_strdup(v);continue;}
    v=opt_value(argc,argv,&i,a,"--parent");if(v!=(const char*)-1){if(!v){free(src);darc_config_free(&c);return usage_error("--parent requires SNAPSHOT");}free(c.snapshot_parent);c.snapshot_parent=darc_strdup(v);continue;}
    v=opt_value(argc,argv,&i,a,"--timestamp");if(v!=(const char*)-1){int64_t z;if(!v||parse_i64(v,&z)){free(src);darc_config_free(&c);return usage_error("--timestamp requires integer NS");}c.timestamp_set=true;c.timestamp_ns=z;continue;}
    if(a[0]=='-'){free(src);darc_config_free(&c);return usage_error("unknown snapshot create option");}src[ns++]=argv[i];}
   if(!ns){free(src);darc_config_free(&c);return usage_error("snapshot create requires SOURCE");}recalc(&c);rc=require_format(&c,false);if(!rc)rc=recover_before_write(repo);if(!rc)rc=darc_cmd_snapshot_create(repo,&c,src,ns);free(src);
  }else if(!strcmp(sub,"list")){if(i!=argc)rc=DARC_E_USAGE;else {recalc(&c);rc=require_format(&c,false);if(!rc)rc=darc_cmd_snapshot_list(repo,&c);}}
  else if(!strcmp(sub,"show")){if(i+1!=argc)rc=DARC_E_USAGE;else {recalc(&c);rc=require_format(&c,false);if(!rc)rc=darc_cmd_snapshot_show(repo,&c,argv[i]);}}
  else if(!strcmp(sub,"delete")){if(i>=argc){rc=DARC_E_USAGE;}else{const char*sel=argv[i++];bool yes=false,dry=false;for(;i<argc;i++){if(!strcmp(argv[i],"--yes"))yes=true;else if(!strcmp(argv[i],"--dry-run"))dry=true;else{rc=DARC_E_USAGE;break;}}if(!rc){if(!dry&&!yes)rc=DARC_E_USAGE;else {recalc(&c);rc=require_format(&c,false);if(!rc&&!dry)rc=recover_before_write(repo);if(!rc)rc=darc_cmd_snapshot_delete(repo,&c,sel,yes,dry);}}}}
  else if(!strcmp(sub,"diff")){if(i+2>argc){rc=DARC_E_USAGE;}else{const char*a=argv[i++],*b=argv[i++];for(;i<argc;i++){const char*x=argv[i],*v=opt_value(argc,argv,&i,x,"--path");if(v!=(const char*)-1){if(!v){rc=DARC_E_USAGE;break;}free(c.diff_path);c.diff_path=darc_strdup(v);continue;}v=opt_value(argc,argv,&i,x,"--format");if(v!=(const char*)-1){int f=!v?-1:fmt_value(v);if(f<0){rc=DARC_E_USAGE;break;}c.output_format=f;continue;}rc=DARC_E_USAGE;break;}if(!rc){recalc(&c);rc=darc_cmd_diff(repo,&c,a,b);}}}
  else rc=DARC_E_USAGE;
 }else if(!strcmp(cmd,"restore")){
  if(i>=argc)rc=DARC_E_USAGE;else{const char*sel=argv[i++],*to=NULL,*path=NULL;for(;i<argc;i++){const char*a=argv[i],*v=opt_value(argc,argv,&i,a,"--to");if(v!=(const char*)-1){if(!v){rc=DARC_E_USAGE;break;}to=v;continue;}v=opt_value(argc,argv,&i,a,"--path");if(v!=(const char*)-1){if(!v){rc=DARC_E_USAGE;break;}path=v;continue;}v=opt_value(argc,argv,&i,a,"--overwrite");if(v!=(const char*)-1){int z=!v?-1:overwrite_value(v);if(z<0){rc=DARC_E_USAGE;break;}c.overwrite=z;continue;}rc=DARC_E_USAGE;break;}if(!rc&&!to)rc=DARC_E_USAGE;if(!rc){recalc(&c);rc=require_format(&c,false);if(!rc)rc=darc_cmd_restore(repo,&c,sel,to,path);}}
 }else if(!strcmp(cmd,"verify")){
  for(;i<argc;i++){const char*a=argv[i],*v=opt_value(argc,argv,&i,a,"--level");if(v!=(const char*)-1){int z=!v?-1:level_value(v);if(z<0){rc=DARC_E_USAGE;break;}c.verify_level=z;continue;}v=opt_value(argc,argv,&i,a,"--format");if(v!=(const char*)-1){int z=!v?-1:fmt_value(v);if(z<0){rc=DARC_E_USAGE;break;}c.output_format=z;continue;}if(!strcmp(a,"--repair")){c.verify_repair=true;continue;}rc=DARC_E_USAGE;break;}if(!rc){if(c.verify_repair)rc=recover_before_write(repo);if(!rc){recalc(&c);rc=darc_cmd_verify(repo,&c);}}
 }else if(!strcmp(cmd,"gc")){
  for(;i<argc;i++){if(!strcmp(argv[i],"--dry-run"))c.gc_dry_run=true;else if(!strcmp(argv[i],"--repack-parity"))c.gc_repack_parity=true;else{rc=DARC_E_USAGE;break;}}if(!rc){recalc(&c);rc=require_format(&c,false);if(!rc&&!c.gc_dry_run)rc=recover_before_write(repo);if(!rc)rc=darc_cmd_gc(repo,&c);}
 }else if(!strcmp(cmd,"stats")){
  for(;i<argc;i++){const char*v=opt_value(argc,argv,&i,argv[i],"--format");if(v!=(const char*)-1){int z=!v?-1:fmt_value(v);if(z<0){rc=DARC_E_USAGE;break;}c.output_format=z;}else{rc=DARC_E_USAGE;break;}}if(!rc){recalc(&c);rc=darc_cmd_stats(repo,&c);}
 }else if(!strcmp(cmd,"index")){
  if(i+1!=argc||strcmp(argv[i],"rebuild"))rc=DARC_E_USAGE;else{rc=recover_before_write(repo);if(!rc){int fd;rc=darc_lock_acquire(repo,&fd);if(!rc){rc=darc_index_rebuild(repo)?DARC_E_IO:0;darc_lock_release(repo,fd);if(!rc)printf("Index rebuilt successfully\n");}}}
 }else if(!strcmp(cmd,"repo")){
  if(i+1!=argc||strcmp(argv[i],"inspect"))rc=DARC_E_USAGE;else{recalc(&c);rc=require_format(&c,false);if(!rc)rc=darc_cmd_repo_inspect(repo,&c);}
 }else rc=DARC_E_USAGE;
 darc_config_free(&c);if(rc==DARC_E_USAGE&&!darc_last_error)darc_error(DARC_E_USAGE,"E_USAGE","invalid command arguments; use --help");return final_rc(rc);
}
