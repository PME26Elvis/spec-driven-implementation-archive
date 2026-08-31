#include "test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static TestProc cfg(const char *p){const char *a[]={"./darc","config","validate",p,NULL};return test_exec(a,NULL);}
static char *putcase(const char *dir,const char *name,const void *p,size_t n){char *f=test_path(dir,name);if(f&&test_write(f,p,n,0644)){}return f;}
static bool hashok(const TestProc *p){return p->code==0&&test_contains(p->out,"d3944066428c9b9ad143668ff0bf2ef59230f808402454b9efb71dadf921cbd9")&&test_contains(p->out,"971dd56be6ab862b4739c195c162413d4410351a05b0a70c9efa56ac20037d2f");}

void test_config_cli(void){
 TestProc p=cfg("examples/config.json");test_check("CFG-001",hashok(&p),"example JSON validates and emits normative hashes");test_proc_free(&p);
 p=cfg("examples/config.yaml");test_check("CFG-002",hashok(&p),"example YAML validates to same hashes");test_proc_free(&p);
 p=cfg("tests/fixtures/config_permuted.json");test_check("CFG-003",hashok(&p),"recursive JSON key permutation preserves normalized hash");test_proc_free(&p);
 p=cfg("tests/fixtures/config_comments.yaml");test_check("CFG-004",hashok(&p),"YAML comments/whitespace preserve normalized hash");test_proc_free(&p);
 char *d=test_case_dir("config"),*repo=test_path(d,"repo"),*src=test_path(d,"src"),*cf=test_path(d,"json-output.json");test_mkdir(src);{ const char *txt="{\"output\":{\"format\":\"json\"}}\n"; test_write(cf,txt,strlen(txt),0644); }const char *ai[]={"./darc","--config",cf,"--format","text","init",repo,NULL};p=test_exec(ai,NULL);test_check("CFG-005",p.code==0&&test_contains(p.out,"Initialized DARC repository")&&!test_contains(p.out,"{\"status\""),"CLI output format overrides explicit config");test_proc_free(&p);
 char *f=putcase(d,"unknown.json","{\"chunking\":{\"avgg_bytes\":65536}}\n",strlen("{\"chunking\":{\"avgg_bytes\":65536}}\n"));p=cfg(f);test_check("CFG-006",p.code==2&&test_contains(p.err,"E_CONFIG_SCHEMA"),"unknown nested key rejected");test_proc_free(&p);free(f);
 f=putcase(d,"dup.json","{\"output\":{\"quiet\":false,\"quiet\":true}}\n",strlen("{\"output\":{\"quiet\":false,\"quiet\":true}}\n"));p=cfg(f);test_check("CFG-007",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"duplicate JSON key rejected");test_proc_free(&p);free(f);
 f=putcase(d,"trailing.json","{\"output\":{\"quiet\":false,}}\n",strlen("{\"output\":{\"quiet\":false,}}\n"));p=cfg(f);test_check("CFG-008",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE")&&test_contains(p.err,"line"),"JSON trailing comma rejected with location");test_proc_free(&p);free(f);
 f=putcase(d,"comment.json","{\"output\":{/*no*/\"quiet\":false}}\n",strlen("{\"output\":{/*no*/\"quiet\":false}}\n"));p=cfg(f);test_check("CFG-009",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"JSON comments rejected");test_proc_free(&p);free(f);
 const char *pair="{\"snapshot\":{\"name\":\"ok-\\uD83D\\uDE80\"}}\n";f=putcase(d,"pair.json",pair,strlen(pair));p=cfg(f);test_check("CFG-010",p.code==0&&test_contains(p.out,"ok-")&&test_contains(p.out,"🚀"),"valid JSON surrogate pair decoded to supplementary UTF-8");test_proc_free(&p);free(f);
 const char *lone="{\"snapshot\":{\"name\":\"bad-\\uD83D\"}}\n";f=putcase(d,"lone.json",lone,strlen(lone));p=cfg(f);test_check("CFG-011",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"lone surrogate rejected");test_proc_free(&p);free(f);
 const unsigned char inv[]={ '{','"','s','n','a','p','s','h','o','t','"',':','{','"','n','a','m','e','"',':','"',0xc3,0x28,'"','}','}','\n'};f=putcase(d,"utf8.json",inv,sizeof inv);p=cfg(f);test_check("CFG-012",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"malformed UTF-8 rejected safely");test_proc_free(&p);free(f);
 const char *yes="repository:\n  parity_enabled: yes\n";f=putcase(d,"yes.yaml",yes,strlen(yes));p=cfg(f);test_check("CFG-013",p.code==2&&test_contains(p.err,"E_CONFIG_SCHEMA"),"YAML yes is string, not coerced boolean");test_proc_free(&p);free(f);
 const char *anchor="output: &a\n  quiet: false\ncopy: *a\n";f=putcase(d,"anchor.yaml",anchor,strlen(anchor));p=cfg(f);test_check("CFG-014",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"YAML anchors/aliases rejected");test_proc_free(&p);free(f);
 const char *tab="output:\n\tquiet: false\n";f=putcase(d,"tab.yaml",tab,strlen(tab));p=cfg(f);test_check("CFG-015",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE")&&test_contains(p.err,"line"),"YAML tab indentation rejected with location");test_proc_free(&p);free(f);
 const char *ydup="output:\n  quiet: false\n  quiet: true\n";f=putcase(d,"dup.yaml",ydup,strlen(ydup));p=cfg(f);test_check("CFG-016",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"duplicate YAML key rejected");test_proc_free(&p);free(f);
 const char *ord="{\"chunking\":{\"min_bytes\":65536,\"avg_bytes\":65536}}\n";f=putcase(d,"order.json",ord,strlen(ord));p=cfg(f);test_check("CFG-017",p.code==2&&test_contains(p.err,"E_CONFIG_SCHEMA"),"min >= avg rejected");test_proc_free(&p);free(f);
 const char *avg="{\"chunking\":{\"avg_bytes\":70000}}\n";f=putcase(d,"avg.json",avg,strlen(avg));p=cfg(f);test_check("CFG-018",p.code==2&&test_contains(p.err,"E_CONFIG_SCHEMA"),"non-power-of-two avg rejected");test_proc_free(&p);free(f);
 const char *mx="{\"chunking\":{\"max_bytes\":33554432}}\n";f=putcase(d,"max.json",mx,strlen(mx));p=cfg(f);test_check("CFG-019",p.code==2&&test_contains(p.err,"E_CONFIG_SCHEMA"),"max > 16MiB rejected");test_proc_free(&p);free(f);
 char before[65],after[65];test_head(repo,before);size_t no=test_count_objects(repo,0);const char *svg="{\"output\":{\"format\":\"svg\"}}\n";f=putcase(d,"svg.json",svg,strlen(svg));const char *ac[]={"./darc","--repo",repo,"--config",f,"snapshot","create",src,"--timestamp","1",NULL};p=test_exec(ac,NULL);test_head(repo,after);test_check("CFG-020",p.code==2&&test_contains(p.err,"E_CONFIG_FORMAT_FOR_COMMAND")&&!strcmp(before,after)&&no==test_count_objects(repo,0),"unsupported command format rejected before repository mutation");test_proc_free(&p);free(f);
 DarcBuf deep;darc_buf_init(&deep);for(int i=0;i<70;i++)darc_buf_put(&deep,"{\"a\":",5);darc_buf_put(&deep,"0",1);for(int i=0;i<70;i++)darc_buf_put(&deep,"}",1);darc_buf_put(&deep,"\n",1);f=putcase(d,"deep.json",deep.data,deep.len);darc_buf_free(&deep);p=cfg(f);test_check("CFG-021",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"document nesting depth limit enforced");test_proc_free(&p);free(f);
 const char *huge="{\"chunking\":{\"avg_bytes\":999999999999999999999999999999999}}\n";f=putcase(d,"huge.json",huge,strlen(huge));p=cfg(f);test_check("CFG-022",p.code==2&&test_contains(p.err,"E_CONFIG_PARSE"),"integer overflow rejected");test_proc_free(&p);free(f);
 free(cf);free(src);free(repo);free(d);
}
