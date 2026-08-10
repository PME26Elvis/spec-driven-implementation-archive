#include "mdedit/core.h"
#include "mdedit/document.h"
#include "mdedit/image.h"
#include "mdedit/json.h"
#include "mdedit/storage.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

enum { TYPING_SAMPLES=40, MEMORY_SAMPLES=20, PERFORMANCE_RESULTS=10 };

typedef struct {
    const char *id;
    const char *profile;
    const char *operation;
    char manifest_sha256[65];
    double samples[64];
    size_t sample_count;
    double duration_ms;
    double p95_ms;
    double threshold_ms;
    size_t affected;
    long rss_kib;
    long rss_samples[MEMORY_SAMPLES];
    size_t rss_count;
    bool passed;
    char verification[256];
} PerfResult;

typedef struct {
    char os[192];
    char architecture[80];
    long logical_cpus;
    unsigned long long memory_kib;
} Environment;

static double monotonic_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC,&value)!=0) return 0.0;
    return (double)value.tv_sec*1000.0+(double)value.tv_nsec/1000000.0;
}

static long resident_kib(void) {
    FILE *stream=fopen("/proc/self/statm","r");
    if (stream==NULL) return -1L;
    unsigned long total=0UL,resident=0UL;
    int fields=fscanf(stream,"%lu %lu",&total,&resident); (void)fclose(stream); (void)total;
    long page=sysconf(_SC_PAGESIZE);
    if (fields!=2||page<=0L||resident>(unsigned long)LONG_MAX/(unsigned long)page) return -1L;
    return (long)(resident*(unsigned long)page/1024UL);
}

static Environment environment_read(void) {
    Environment env; memset(&env,0,sizeof(env));
    struct utsname info;
    if (uname(&info)==0) {
        (void)snprintf(env.os,sizeof(env.os),"%s %s",info.sysname,info.release);
        (void)snprintf(env.architecture,sizeof(env.architecture),"%s",info.machine);
    } else {
        (void)snprintf(env.os,sizeof(env.os),"unknown");
        (void)snprintf(env.architecture,sizeof(env.architecture),"unknown");
    }
    env.logical_cpus=sysconf(_SC_NPROCESSORS_ONLN); if (env.logical_cpus<1L) env.logical_cpus=1L;
    FILE *stream=fopen("/proc/meminfo","r");
    if (stream!=NULL) {
        char line[256];
        while (fgets(line,sizeof(line),stream)!=NULL) {
            if (sscanf(line,"MemTotal: %llu kB",&env.memory_kib)==1) break;
        }
        (void)fclose(stream);
    }
    return env;
}

static bool profile_path(char out[MD_PATH_MAX],const char *fixtures,const char *profile,const char *leaf) {
    char root[MD_PATH_MAX]; return md_path_join(root,fixtures,profile)&&md_path_join(out,root,leaf);
}

static bool manifest_digest(const char *fixtures,const char *profile,char hex[65],char *error,size_t error_cap) {
    char path[MD_PATH_MAX]; MdFileDigest digest;
    if (!profile_path(path,fixtures,profile,"fixture-manifest.json")||!md_file_digest(path,&digest,error,error_cap)) return false;
    md_hex_encode(digest.sha256,32U,hex); return true;
}

static bool load_timed(MdDocument *doc,const char *path,double *duration,char *error,size_t error_cap) {
    double start=monotonic_ms(); bool ok=md_document_load(doc,path,error,error_cap); *duration=monotonic_ms()-start; return ok;
}

static int compare_double(const void *left,const void *right) {
    double a=*(const double *)left,b=*(const double *)right; return a<b?-1:a>b?1:0;
}

static double percentile95(const double *samples,size_t count) {
    if (count==0U) return 0.0;
    double copy[64]; memcpy(copy,samples,count*sizeof(*samples)); qsort(copy,count,sizeof(*copy),compare_double);
    size_t index=(count*95U+99U)/100U; if (index==0U) index=1U; return copy[index-1U];
}

static void result_base(PerfResult *result,const char *id,const char *profile,const char *operation,
                        double threshold,const char *fixtures,char *error,size_t error_cap) {
    memset(result,0,sizeof(*result)); result->id=id; result->profile=profile; result->operation=operation;
    result->threshold_ms=threshold; result->rss_kib=-1L;
    if (!manifest_digest(fixtures,profile,result->manifest_sha256,error,error_cap)) result->passed=false;
}

static bool measure_open(PerfResult *result,const char *fixtures,const char *profile,const char *leaf,
                         double threshold,char *error,size_t error_cap) {
    result_base(result,strcmp(profile,"medium")==0?"PERF-MEDIUM-OPEN":"PERF-LARGE-OPEN",profile,
                "open-to-interactive source parse",threshold,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,profile,leaf)) return false;
    MdDocument doc; md_document_init(&doc,1U); bool ok=load_timed(&doc,path,&result->duration_ms,error,error_cap);
    result->samples[0]=result->duration_ms; result->sample_count=1U; result->rss_kib=resident_kib();
    result->passed=ok&&result->duration_ms<threshold&&doc.source.len>0U&&doc.render.block_count>0U;
    (void)snprintf(result->verification,sizeof(result->verification),"source_bytes=%zu render_blocks=%zu",doc.source.len,doc.render.block_count);
    md_document_free(&doc); return ok;
}

static bool measure_preview(PerfResult *result,const char *fixtures,char *error,size_t error_cap) {
    result_base(result,"PERF-LARGE-PREVIEW","large","full render-model and local image decode",10000.0,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,"large","large.md")) return false;
    MdDocument doc; md_document_init(&doc,1U); double start=monotonic_ms(); bool ok=md_document_load(&doc,path,error,error_cap); size_t images=0U;
    for (size_t i=0U;ok&&i<20U;++i) {
        char relative[128],asset[MD_PATH_MAX]; (void)snprintf(relative,sizeof(relative),"assets/generated-%02zu.bmp",i);
        if (!profile_path(asset,fixtures,"large",relative)) { ok=false; break; }
        MdImage image; MdBytes original; md_image_init(&image); md_bytes_init(&original);
        ok=md_image_load(asset,&image,&original,error,error_cap); if (ok) ++images;
        md_image_free(&image); md_bytes_free(&original);
    }
    result->duration_ms=monotonic_ms()-start; result->samples[0]=result->duration_ms; result->sample_count=1U;
    result->passed=ok&&images==20U&&result->duration_ms<result->threshold_ms;
    (void)snprintf(result->verification,sizeof(result->verification),"render_blocks=%zu decoded_unique_images=%zu",doc.render.block_count,images);
    md_document_free(&doc); return ok;
}

static bool measure_typing(PerfResult *result,const char *fixtures,const char *profile,const char *leaf,
                           double threshold,char *error,size_t error_cap) {
    result_base(result,strcmp(profile,"medium")==0?"PERF-MEDIUM-TYPING":"PERF-LARGE-TYPING",profile,
                "ordinary visible source insertion latency p95",threshold,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,profile,leaf)) return false;
    MdDocument doc; md_document_init(&doc,1U); double ignored=0.0; bool ok=load_timed(&doc,path,&ignored,error,error_cap);
    for (size_t i=0U;ok&&i<TYPING_SAMPLES;++i) {
        doc.cursor=doc.anchor=doc.source.len; double start=monotonic_ms();
        ok=md_document_insert_utf8(&doc,"x",1U,error,error_cap); result->samples[i]=monotonic_ms()-start; result->sample_count=i+1U;
    }
    result->p95_ms=percentile95(result->samples,result->sample_count); result->duration_ms=result->p95_ms;
    result->passed=ok&&result->sample_count==TYPING_SAMPLES&&result->p95_ms<threshold;
    (void)snprintf(result->verification,sizeof(result->verification),"samples=%zu p95_source_update_ms=%.3f",result->sample_count,result->p95_ms);
    md_document_free(&doc); return ok;
}

static bool measure_find_replace(PerfResult *find_result,PerfResult *replace_result,const char *fixtures,
                                 char *error,size_t error_cap) {
    result_base(find_result,"PERF-LARGE-FIND","large","literal Find and full match count",2000.0,fixtures,error,error_cap);
    result_base(replace_result,"PERF-LARGE-REPLACE-ALL","large","Replace All one transaction",5000.0,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,"large","large.md")) return false;
    MdDocument doc; md_document_init(&doc,1U); double ignored=0.0; bool ok=load_timed(&doc,path,&ignored,error,error_cap);
    MdSearchResults matches; md_search_results_init(&matches); double start=monotonic_ms();
    if (ok) ok=md_document_find(&doc,"搜尋短語",true,false,&matches);
    find_result->duration_ms=monotonic_ms()-start; find_result->samples[0]=find_result->duration_ms; find_result->sample_count=1U; find_result->affected=matches.count;
    find_result->passed=ok&&matches.count>=1000U&&find_result->duration_ms<find_result->threshold_ms;
    (void)snprintf(find_result->verification,sizeof(find_result->verification),"matches=%zu first_and_count_complete=true",matches.count);
    size_t replaced=0U; start=monotonic_ms();
    if (ok) ok=md_document_replace_all(&doc,"搜尋短語","效能替換",true,false,&replaced,error,error_cap);
    replace_result->duration_ms=monotonic_ms()-start; replace_result->samples[0]=replace_result->duration_ms; replace_result->sample_count=1U; replace_result->affected=replaced;
    replace_result->passed=ok&&replaced>=1000U&&replace_result->duration_ms<replace_result->threshold_ms&&doc.undo.len==1U;
    (void)snprintf(replace_result->verification,sizeof(replace_result->verification),"replaced=%zu undo_entries=%zu",replaced,doc.undo.len);
    md_search_results_free(&matches); md_document_free(&doc); return ok;
}

static bool measure_outline(PerfResult *result,const char *fixtures,char *error,size_t error_cap) {
    result_base(result,"PERF-LARGE-OUTLINE","large","Outline construction and heading update",2000.0,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,"large","large.md")) return false;
    MdDocument doc; md_document_init(&doc,1U); double construction=0.0; bool ok=load_timed(&doc,path,&construction,error,error_cap);
    size_t headings=doc.render.heading_count; double start=monotonic_ms();
    if (ok&&headings>0U) {
        size_t block_index=doc.render.headings[0].block_index;
        size_t at=block_index<doc.render.block_count?doc.render.blocks[block_index].content_start:doc.render.headings[0].source_offset;
        ok=md_document_replace(&doc,at,at,"Live ",5U,"Outline heading update",false,error,error_cap);
    }
    double update=monotonic_ms()-start; result->samples[0]=construction; result->samples[1]=update; result->sample_count=2U;
    result->duration_ms=construction; result->p95_ms=update; result->affected=headings;
    result->passed=ok&&headings>=1000U&&construction<2000.0&&update<250.0&&strncmp(doc.render.headings[0].label,"Live ",5U)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"headings=%zu construction_ms=%.3f update_ms=%.3f",headings,construction,update);
    md_document_free(&doc); return ok;
}

static bool measure_long_line(PerfResult *result,const char *fixtures,char *error,size_t error_cap) {
    result_base(result,"PERF-LONG-LINE","stress-long-line","1 MiB single-line open, navigate, edit, save, reopen",5000.0,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,"stress-long-line","stress-long-line.md")) return false;
    MdDocument doc; md_document_init(&doc,1U); double start=monotonic_ms(); bool ok=md_document_load(&doc,path,error,error_cap);
    if (ok) { doc.cursor=doc.anchor=doc.source.len; md_document_move_left(&doc,false); md_document_move_right(&doc,false); ok=md_document_insert_utf8(&doc,"Z",1U,error,error_cap); }
    char temporary[]="/tmp/mdeditor-perf-long-XXXXXX"; int fd=mkstemp(temporary); if (fd>=0) (void)close(fd); else ok=false;
    if (ok) ok=md_safe_save_document(&doc,temporary,true,error,error_cap);
    MdDocument reopened; md_document_init(&reopened,2U); if (ok) ok=md_document_load(&reopened,temporary,error,error_cap);
    result->duration_ms=monotonic_ms()-start; result->samples[0]=result->duration_ms; result->sample_count=1U;
    result->passed=ok&&reopened.source.len==1024U*1024U+1U&&result->duration_ms<result->threshold_ms;
    (void)snprintf(result->verification,sizeof(result->verification),"saved_reopened_bytes=%zu valid_utf8=%s",reopened.source.len,ok&&md_utf8_validate(reopened.source.data,reopened.source.len,NULL)?"true":"false");
    if (fd>=0) (void)unlink(temporary);
    md_document_free(&reopened); md_document_free(&doc); return ok;
}

static bool measure_memory(PerfResult *result,const char *fixtures,char *error,size_t error_cap) {
    result_base(result,"PERF-MEMORY-20","medium","twenty open/close retained-memory observation",0.0,fixtures,error,error_cap);
    char path[MD_PATH_MAX]; if (!profile_path(path,fixtures,"medium","medium.md")) return false;
    long baseline=resident_kib(); double start=monotonic_ms(); bool ok=true;
    for (size_t i=0U;i<MEMORY_SAMPLES;++i) {
        MdDocument doc; md_document_init(&doc,(unsigned)i+1U); ok=md_document_load(&doc,path,error,error_cap); md_document_free(&doc);
        result->rss_samples[i]=resident_kib(); result->rss_count=i+1U; if (!ok) break;
    }
    result->duration_ms=monotonic_ms()-start; result->samples[0]=result->duration_ms; result->sample_count=1U;
    long last=result->rss_count==0U?-1L:result->rss_samples[result->rss_count-1U]; long half_min=LONG_MAX,half_max=0L;
    for (size_t i=MEMORY_SAMPLES/2U;i<result->rss_count;++i) { half_min=MD_MIN(half_min,result->rss_samples[i]); half_max=MD_MAX(half_max,result->rss_samples[i]); }
    bool bounded=baseline>=0L&&last>=0L&&last<=baseline+131072L&&half_min!=LONG_MAX&&half_max-half_min<=65536L;
    result->passed=ok&&result->rss_count==MEMORY_SAMPLES&&bounded; result->rss_kib=last;
    (void)snprintf(result->verification,sizeof(result->verification),"baseline_kib=%ld final_kib=%ld second_half_range_kib=%ld",baseline,last,half_min==LONG_MAX?-1L:half_max-half_min);
    return ok;
}

static bool append_environment(MdBuf *json,const Environment *env) {
    return md_buf_append_cstr(json,"\"environment\":{")&&
           md_buf_append_cstr(json,"\"os\":")&&md_json_write_escaped(json,env->os,strlen(env->os))&&
           md_buf_append_cstr(json,",\"architecture\":")&&md_json_write_escaped(json,env->architecture,strlen(env->architecture))&&
           md_buf_appendf(json,",\"logical_cpu_count\":%ld,\"memory_total_kib\":%llu}",env->logical_cpus,env->memory_kib);
}

static bool append_result(MdBuf *json,const PerfResult *result,const Environment *env,bool comma) {
    if (!md_buf_append_cstr(json,"    {\"id\":")||!md_json_write_escaped(json,result->id,strlen(result->id))||
        !md_buf_append_cstr(json,",\"profile\":")||!md_json_write_escaped(json,result->profile,strlen(result->profile))||
        !md_buf_append_cstr(json,",\"fixture_manifest_sha256\":")||!md_json_write_escaped(json,result->manifest_sha256,strlen(result->manifest_sha256))||
        !md_buf_append_cstr(json,",\"operation\":")||!md_json_write_escaped(json,result->operation,strlen(result->operation))||
        !md_buf_appendf(json,",\"runs\":%zu,\"durations_ms\":[",result->sample_count)) return false;
    for (size_t i=0U;i<result->sample_count;++i) if (!md_buf_appendf(json,"%.3f%s",result->samples[i],i+1U<result->sample_count?",":"")) return false;
    if (!md_buf_appendf(json,"],\"duration_ms\":%.3f,\"p95_ms\":%.3f,\"threshold_ms\":%.3f,\"affected\":%zu,\"peak_or_current_rss_kib\":%ld,",
        result->duration_ms,result->p95_ms,result->threshold_ms,result->affected,result->rss_kib)||!append_environment(json,env)) return false;
    if (result->rss_count>0U) {
        if (!md_buf_append_cstr(json,",\"rss_samples_kib\":[")) return false;
        for (size_t i=0U;i<result->rss_count;++i) if (!md_buf_appendf(json,"%ld%s",result->rss_samples[i],i+1U<result->rss_count?",":"")) return false;
        if (!md_buf_append_char(json,']')) return false;
    }
    return md_buf_append_cstr(json,",\"verification\":")&&md_json_write_escaped(json,result->verification,strlen(result->verification))&&
           md_buf_appendf(json,",\"result\":\"%s\"}%s\n",result->passed?"pass":"fail",comma?",":"");
}

static bool write_results(const char *path,const PerfResult results[PERFORMANCE_RESULTS],const Environment *env,
                          char *error,size_t error_cap) {
    MdBuf json; md_buf_init(&json); bool ok=md_buf_append_cstr(&json,"{\n  \"schema_version\":1,\n  \"results\":[\n");
    for (size_t i=0U;ok&&i<PERFORMANCE_RESULTS;++i) ok=append_result(&json,&results[i],env,i+1U<PERFORMANCE_RESULTS);
    if (ok) ok=md_buf_append_cstr(&json,"  ]\n}\n")&&md_write_file_atomic(path,json.data,json.len,error,error_cap);
    md_buf_free(&json); return ok;
}

static void usage(void) { fputs("usage: performance --fixtures DIR --output JSON\n",stderr); }

int main(int argc,char **argv) {
    const char *fixtures=NULL,*output=NULL;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--fixtures")==0&&i+1<argc) fixtures=argv[++i];
        else if (strcmp(argv[i],"--output")==0&&i+1<argc) output=argv[++i];
        else { usage(); return 2; }
    }
    if (fixtures==NULL||output==NULL) { usage(); return 2; }
    PerfResult results[PERFORMANCE_RESULTS]; char error[512]={0}; bool operational=true;
    operational=measure_open(&results[0],fixtures,"medium","medium.md",2000.0,error,sizeof(error))&&operational;
    operational=measure_open(&results[1],fixtures,"large","large.md",5000.0,error,sizeof(error))&&operational;
    operational=measure_preview(&results[2],fixtures,error,sizeof(error))&&operational;
    operational=measure_typing(&results[3],fixtures,"medium","medium.md",100.0,error,sizeof(error))&&operational;
    operational=measure_typing(&results[4],fixtures,"large","large.md",200.0,error,sizeof(error))&&operational;
    operational=measure_find_replace(&results[5],&results[6],fixtures,error,sizeof(error))&&operational;
    operational=measure_outline(&results[7],fixtures,error,sizeof(error))&&operational;
    operational=measure_long_line(&results[8],fixtures,error,sizeof(error))&&operational;
    operational=measure_memory(&results[9],fixtures,error,sizeof(error))&&operational;
    Environment env=environment_read();
    if (!write_results(output,results,&env,error,sizeof(error))) { fprintf(stderr,"performance: %s\n",error); return 1; }
    size_t passed=0U; for (size_t i=0U;i<PERFORMANCE_RESULTS;++i) { passed+=results[i].passed?1U:0U; printf("%s %s duration_ms=%.3f %s\n",results[i].passed?"PASS":"FAIL",results[i].id,results[i].duration_ms,results[i].verification); }
    printf("PERFORMANCE_SUMMARY total=%d passed=%zu failed=%zu skipped=0 output=%s\n",PERFORMANCE_RESULTS,passed,(size_t)PERFORMANCE_RESULTS-passed,output);
    if (!operational&&error[0]!='\0') fprintf(stderr,"performance: %s\n",error);
    return operational&&passed==PERFORMANCE_RESULTS?0:1;
}
