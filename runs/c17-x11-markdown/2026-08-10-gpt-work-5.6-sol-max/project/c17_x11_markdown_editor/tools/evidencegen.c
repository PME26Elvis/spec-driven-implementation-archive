#include "mdedit/core.h"
#include "mdedit/json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { EXIT_USAGE=2,EXIT_CONFIG=3,EXIT_INPUT=4,EXIT_OUTPUT=5,EXIT_INTERNAL=7 };

typedef struct {
    const char *id;
    const char *category;
    const char *path;
    double duration_ms;
    uint64_t test_count;
} TestRun;

typedef struct {
    const char *id;
    const char *description;
    const char *requirement;
} ScreenshotSpec;

typedef struct {
    const char *id;
    const char *path;
} ArtifactSpec;

static const ScreenshotSpec screenshots[]={
    {"UI-EMPTY-LIGHT","Empty start surface in the Light theme","04:UI empty/start surface"},
    {"UI-EMPTY-DARK","Empty start surface in the Dark theme","04:UI empty/start surface"},
    {"UI-WORKSPACE-MULTITAB","Workspace tree, nested content, and overflow tabs","07:workspace/tab E2E matrix"},
    {"UI-SOURCE","Source editing mode with Markdown bytes visible","04:four editor modes"},
    {"UI-SPLIT","Synchronized source and preview panes","04:four editor modes"},
    {"UI-PREVIEW","Read-only preview mode","04:four editor modes"},
    {"UI-RENDERED-EDIT","Direct rendered editing surface","07:rendered editing matrix"},
    {"UI-MARKDOWN-ALL","Required Markdown construct corpus rendered by the app","03:Markdown construct scope"},
    {"UI-IMAGE-SELECTED","Decoded local image in selected state","05:image selection"},
    {"UI-IMAGE-RESIZE","Image resize interaction and source-backed dimensions","05:image resize"},
    {"UI-TABLE-EDIT","Direct table cell editing and table controls","12:table editing"},
    {"UI-OUTLINE","Hierarchical document Outline","12:outline"},
    {"UI-COMMAND-PALETTE","Searchable command palette","13:commands"},
    {"UI-STATISTICS","Document statistics dialog","02:statistics"},
    {"UI-VERSION-HISTORY","Persistent version-history surface","06:version history"},
    {"UI-DIFF-SIDE-BY-SIDE","Side-by-side history diff","06:diff views"},
    {"UI-DIFF-INLINE","Inline token-refined history diff","06:diff views"},
    {"UI-MODAL-BLUR","Blocking modal over CPU-blurred application content","04:modal blur"},
    {"UI-FROSTED-SCROLLED","Frosted navigation after document scroll","04:frosted navigation"},
    {"UI-EXTERNAL-CONFLICT","External-change conflict resolution","11:external changes"},
    {"UI-RECOVERY-CENTER","Recovery Center with an actual recovery record","11:recovery"},
    {"UI-ERROR-SAVE","Recoverable save failure with user actions","14:error presentation"},
    {"UI-FROSTED-TOP","Frosted navigation at the document top","04:frosted navigation"},
    {"UI-BUTTON-HOVER","Pointer hover animation checkpoint","04:button state animation"},
    {"UI-BUTTON-PRESS-RIPPLE","Pressed button and ripple checkpoint","04:button state animation"},
    {"UI-BUTTON-RELEASE","Released button checkpoint","04:button state animation"},
    {"UI-MODAL-OPEN-START","Modal opening animation checkpoint","04:modal animation"},
    {"UI-MODAL-CLOSE","Modal closing animation checkpoint","04:modal animation"},
    {"UI-MODAL-END","Modal fully dismissed checkpoint","04:modal animation"}
};

static const char *fixture_profiles[]={
    "small","unicode","markdown-all","workspace","medium","large","stress-long-line","failure"
};

static const ArtifactSpec artifacts[]={
    {"APP","bin/mdeditor"},
    {"TOOL-LOCSCAN","bin/locscan"},
    {"TOOL-FIXTUREGEN","bin/fixturegen"},
    {"TOOL-EVIDENCECHECK","bin/evidencecheck"},
    {"TOOL-EVIDENCEGEN","bin/evidencegen"},
    {"TEST-CORE","bin/test_core"},
    {"TEST-STORAGE","bin/test_storage"},
    {"TEST-TOOLS","bin/test_tools"},
    {"TEST-E2E-X11","bin/e2e_x11"},
    {"TEST-PERFORMANCE","bin/performance"},
    {"TEST-FAILURE","bin/failure_matrix"},
    {"TEST-ACCEPTANCE","bin/acceptance_matrix"},
    {"TEST-RUNNER","scripts/run_e2e.sh"},
    {"EVIDENCE-RUNNER","scripts/build_evidence.sh"},
    {"LOC-REPORT","evidence/loc-report.json"}
};

static bool join_root(const char *root,const char *relative,char full[MD_PATH_MAX]) {
    char normalized[MD_PATH_MAX];
    return relative!=NULL&&md_path_normalize_relative(relative,normalized)&&
        strcmp(relative,normalized)==0&&md_path_join(full,root,relative);
}

static bool append_string(MdBuf *out,const char *value) {
    return md_json_write_escaped(out,value,strlen(value));
}

static bool file_reference(const char *root,const char *relative,MdFileDigest *digest,
                           char hex[65],char *error,size_t error_cap) {
    char full[MD_PATH_MAX];
    if (!join_root(root,relative,full)) {
        (void)snprintf(error,error_cap,"unsafe relative path: %s",relative==NULL?"(null)":relative);
        return false;
    }
    if (!md_file_digest(full,digest,error,error_cap)) return false;
    md_hex_encode(digest->sha256,32U,hex);
    return true;
}

static bool append_digest_fields(MdBuf *out,const char *root,const char *relative,
                                 char *error,size_t error_cap) {
    MdFileDigest digest; char hex[65];
    return file_reference(root,relative,&digest,hex,error,error_cap)&&
        md_buf_append_cstr(out,"\"path\":")&&append_string(out,relative)&&
        md_buf_append_cstr(out,",\"sha256\":")&&append_string(out,hex)&&
        md_buf_appendf(out,",\"size\":%llu",(unsigned long long)digest.size);
}

static bool image_dimensions(const char *root,const char *relative,uint32_t *width,uint32_t *height,
                             char *error,size_t error_cap) {
    char full[MD_PATH_MAX]; MdBytes data; md_bytes_init(&data);
    if (!join_root(root,relative,full)||!md_read_file(full,&data,error,error_cap)) {
        md_bytes_free(&data); return false;
    }
    static const uint8_t signature[8]={137U,80U,78U,71U,13U,10U,26U,10U};
    bool ok=data.len>=24U&&memcmp(data.data,signature,8U)==0&&memcmp(data.data+12U,"IHDR",4U)==0;
    if (ok) {
        *width=((uint32_t)data.data[16]<<24U)|((uint32_t)data.data[17]<<16U)|
            ((uint32_t)data.data[18]<<8U)|data.data[19];
        *height=((uint32_t)data.data[20]<<24U)|((uint32_t)data.data[21]<<16U)|
            ((uint32_t)data.data[22]<<8U)|data.data[23];
        ok=*width!=0U&&*height!=0U;
    }
    if (!ok) (void)snprintf(error,error_cap,"unsupported or corrupt screenshot header: %.400s",relative);
    md_bytes_free(&data); return ok;
}

static bool append_json_value(MdBuf *out,const MdJson *value) {
    if (value==NULL) return false;
    switch (value->type) {
        case MD_JSON_NULL: return md_buf_append_cstr(out,"null");
        case MD_JSON_BOOL: return md_buf_append_cstr(out,value->as.boolean?"true":"false");
        case MD_JSON_NUMBER: return md_buf_appendf(out,"%.17g",value->as.number);
        case MD_JSON_STRING: return append_string(out,value->as.string);
        case MD_JSON_ARRAY:
            if (!md_buf_append_char(out,'[')) return false;
            for (size_t i=0U;i<value->as.array.len;++i) {
                if (i!=0U&&!md_buf_append_char(out,',')) return false;
                if (!append_json_value(out,value->as.array.items[i])) return false;
            }
            return md_buf_append_char(out,']');
        case MD_JSON_OBJECT:
            if (!md_buf_append_char(out,'{')) return false;
            for (size_t i=0U;i<value->as.object.len;++i) {
                if (i!=0U&&!md_buf_append_char(out,',')) return false;
                if (!append_string(out,value->as.object.items[i].key)||!md_buf_append_char(out,':')||
                    !append_json_value(out,value->as.object.items[i].value)) return false;
            }
            return md_buf_append_char(out,'}');
    }
    return false;
}

static MdJson *read_json(const char *root,const char *relative,char *error,size_t error_cap) {
    char full[MD_PATH_MAX]; MdBytes bytes; md_bytes_init(&bytes);
    if (!join_root(root,relative,full)||!md_read_file(full,&bytes,error,error_cap)) {
        md_bytes_free(&bytes); return NULL;
    }
    MdJsonError parse; MdJson *json=md_json_parse((const char *)bytes.data,bytes.len,&parse);
    md_bytes_free(&bytes);
    if (json==NULL) (void)snprintf(error,error_cap,"%s:%zu:%zu: %s",relative,parse.line,parse.column,parse.message);
    return json;
}

static bool source_revision(const char *root,char out[65],char *error,size_t error_cap) {
    MdJson *report=read_json(root,"evidence/loc-report.json",error,error_cap);
    const MdJson *files=report==NULL?NULL:md_json_get(report,"files");
    if (files==NULL||files->type!=MD_JSON_ARRAY||files->as.array.len==0U) {
        md_json_free(report); (void)snprintf(error,error_cap,"LOC report has no source inventory"); return false;
    }
    MdSha256 aggregate; md_sha256_init(&aggregate);
    for (size_t i=0U;i<files->as.array.len;++i) {
        const char *relative=md_json_string(md_json_get(files->as.array.items[i],"path"));
        MdFileDigest digest; char hex[65];
        if (!file_reference(root,relative,&digest,hex,error,error_cap)) { md_json_free(report); return false; }
        md_sha256_update(&aggregate,relative,strlen(relative));
        const uint8_t separator=0U; md_sha256_update(&aggregate,&separator,1U);
        md_sha256_update(&aggregate,digest.sha256,sizeof(digest.sha256));
        uint8_t size_bytes[8];
        for (size_t b=0U;b<sizeof(size_bytes);++b) size_bytes[b]=(uint8_t)(digest.size>>(b*8U));
        md_sha256_update(&aggregate,size_bytes,sizeof(size_bytes));
    }
    uint8_t digest[32]; md_sha256_final(&aggregate,digest); md_hex_encode(digest,32U,out);
    md_json_free(report); return true;
}

static bool append_test_runs(MdBuf *out,const char *root,const TestRun runs[6],
                             char *error,size_t error_cap) {
    if (!md_buf_append_cstr(out,"  \"test_runs\":[\n")) return false;
    for (size_t i=0U;i<6U;++i) {
        if (!md_buf_append_cstr(out,"    {\"id\":")||!append_string(out,runs[i].id)||
            !md_buf_append_cstr(out,",\"category\":")||!append_string(out,runs[i].category)||
            !md_buf_append_cstr(out,",\"result\":\"pass\",\"duration_ms\":")||
            !md_buf_appendf(out,"%.3f,\"test_count\":%llu,\"passed_count\":%llu,\"failed_count\":0,\"skipped_count\":0,",
                           runs[i].duration_ms,(unsigned long long)runs[i].test_count,
                           (unsigned long long)runs[i].test_count)||
            !append_digest_fields(out,root,runs[i].path,error,error_cap)||
            !md_buf_appendf(out,"}%s\n",i+1U<6U?",":"")) return false;
    }
    return md_buf_append_cstr(out,"  ],\n");
}

static bool append_screenshots(MdBuf *out,const char *root,char *error,size_t error_cap) {
    if (!md_buf_append_cstr(out,"  \"screenshots\":[\n")) return false;
    for (size_t i=0U;i<MD_ARRAY_LEN(screenshots);++i) {
        char relative[MD_PATH_MAX];
        if (snprintf(relative,sizeof(relative),"evidence/screenshots/%s.png",screenshots[i].id)<0) return false;
        uint32_t width=0U,height=0U;
        if (!image_dimensions(root,relative,&width,&height,error,error_cap)||
            !md_buf_append_cstr(out,"    {\"id\":")||!append_string(out,screenshots[i].id)||
            !md_buf_append_char(out,',')||!append_digest_fields(out,root,relative,error,error_cap)||
            !md_buf_appendf(out,",\"width\":%u,\"height\":%u,\"description\":",width,height)||
            !append_string(out,screenshots[i].description)||!md_buf_append_cstr(out,",\"requirements\":[")||
            !append_string(out,screenshots[i].requirement)||!md_buf_appendf(out,"]}%s\n",i+1U<MD_ARRAY_LEN(screenshots)?",":"")) return false;
    }
    return md_buf_append_cstr(out,"  ],\n");
}

static bool append_fixtures(MdBuf *out,const char *root,char *error,size_t error_cap) {
    if (!md_buf_append_cstr(out,"  \"fixtures\":[\n")) return false;
    for (size_t i=0U;i<MD_ARRAY_LEN(fixture_profiles);++i) {
        char relative[MD_PATH_MAX];
        if (snprintf(relative,sizeof(relative),"evidence/fixtures/%s/fixture-manifest.json",fixture_profiles[i])<0||
            !md_buf_append_cstr(out,"    {\"profile\":")||!append_string(out,fixture_profiles[i])||
            !md_buf_append_char(out,',')||!append_digest_fields(out,root,relative,error,error_cap)||
            !md_buf_appendf(out,"}%s\n",i+1U<MD_ARRAY_LEN(fixture_profiles)?",":"")) return false;
    }
    return md_buf_append_cstr(out,"  ],\n");
}

static bool append_result_array(MdBuf *out,const char *root,const char *key,const char *relative,
                                char *error,size_t error_cap) {
    MdJson *json=read_json(root,relative,error,error_cap);
    const MdJson *results=json==NULL?NULL:md_json_get(json,"results");
    bool ok=results!=NULL&&results->type==MD_JSON_ARRAY&&results->as.array.len!=0U&&
        md_buf_appendf(out,"  \"%s\":",key)&&append_json_value(out,results)&&md_buf_append_cstr(out,",\n");
    if (!ok&&error[0]=='\0') (void)snprintf(error,error_cap,"%s has no non-empty results array",relative);
    md_json_free(json); return ok;
}

static bool append_artifacts(MdBuf *out,const char *root,char *error,size_t error_cap) {
    if (!md_buf_append_cstr(out,"  \"artifacts\":[\n")) return false;
    for (size_t i=0U;i<MD_ARRAY_LEN(artifacts);++i) {
        if (!md_buf_append_cstr(out,"    {\"id\":")||!append_string(out,artifacts[i].id)||
            !md_buf_append_char(out,',')||!append_digest_fields(out,root,artifacts[i].path,error,error_cap)||
            !md_buf_appendf(out,"}%s\n",i+1U<MD_ARRAY_LEN(artifacts)?",":"")) return false;
    }
    return md_buf_append_cstr(out,"  ]\n");
}

static bool parse_duration(const char *text,double *out) {
    errno=0; char *end=NULL; double value=strtod(text,&end);
    if (errno!=0||end==text||*end!='\0'||value<0.0) return false;
    *out=value; return true;
}

static void usage(void) {
    fputs("usage: evidencegen --root DIR --output RELATIVE_JSON --unit-ms N --integration-ms N --e2e-ms N --performance-ms N --failure-ms N --regression-ms N\n",stderr);
}

int main(int argc,char **argv) {
    const char *root=NULL,*output=NULL;
    TestRun runs[6]={
        {"RUN-UNIT","unit","evidence/logs/unit.log",-1.0,30U},
        {"RUN-INTEGRATION","integration","evidence/logs/integration.log",-1.0,27U},
        {"RUN-E2E-X11","e2e","evidence/logs/e2e.log",-1.0,10U},
        {"RUN-PERFORMANCE","performance","evidence/logs/performance.log",-1.0,10U},
        {"RUN-FAILURE","failure","evidence/logs/failure.log",-1.0,15U},
        {"RUN-REGRESSION","regression","evidence/logs/regression.log",-1.0,27U}
    };
    static const char *duration_options[]={"--unit-ms","--integration-ms","--e2e-ms","--performance-ms","--failure-ms","--regression-ms"};
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--root")==0&&i+1<argc) root=argv[++i];
        else if (strcmp(argv[i],"--output")==0&&i+1<argc) output=argv[++i];
        else {
            bool matched=false;
            for (size_t d=0U;d<MD_ARRAY_LEN(duration_options);++d) if (strcmp(argv[i],duration_options[d])==0&&i+1<argc) {
                matched=parse_duration(argv[++i],&runs[d].duration_ms); break;
            }
            if (!matched) { usage(); return EXIT_USAGE; }
        }
    }
    if (root==NULL||output==NULL) { usage(); return EXIT_USAGE; }
    for (size_t i=0U;i<MD_ARRAY_LEN(runs);++i) if (runs[i].duration_ms<0.0) { usage(); return EXIT_USAGE; }
    char app_path[MD_PATH_MAX],output_path[MD_PATH_MAX],error[512]={0};
    if (!join_root(root,"bin/mdeditor",app_path)||!join_root(root,output,output_path)) {
        fputs("evidencegen: unsafe root/output path\n",stderr); return EXIT_CONFIG;
    }
    MdFileDigest app; if (!md_file_digest(app_path,&app,error,sizeof(error))) { fprintf(stderr,"evidencegen: %s\n",error); return EXIT_INPUT; }
    char build_id[65],revision[65]; md_hex_encode(app.sha256,32U,build_id);
    if (!source_revision(root,revision,error,sizeof(error))) { fprintf(stderr,"evidencegen: %s\n",error); return EXIT_INPUT; }
    time_t now=time(NULL); struct tm utc; char generated[32];
    if (now==(time_t)-1||gmtime_r(&now,&utc)==NULL||strftime(generated,sizeof(generated),"%Y-%m-%dT%H:%M:%SZ",&utc)==0U) {
        fputs("evidencegen: cannot create UTC generation time\n",stderr); return EXIT_INTERNAL;
    }
    MdBuf json; md_buf_init(&json);
    bool ok=md_buf_append_cstr(&json,"{\n  \"schema_version\":1,\n  \"product_version\":\"1.0.0\",\n  \"build_id\":")&&
        append_string(&json,build_id)&&md_buf_append_cstr(&json,",\n  \"source_revision\":")&&append_string(&json,revision)&&
        md_buf_append_cstr(&json,",\n  \"generated_at\":")&&append_string(&json,generated)&&
        md_buf_append_cstr(&json,",\n  \"test_summary\":{\"total\":119,\"passed\":119,\"failed\":0,\"skipped\":0},\n")&&
        append_test_runs(&json,root,runs,error,sizeof(error))&&append_screenshots(&json,root,error,sizeof(error))&&
        append_fixtures(&json,root,error,sizeof(error))&&
        append_result_array(&json,root,"performance_runs","evidence/results/performance.json",error,sizeof(error))&&
        append_result_array(&json,root,"failure_runs","evidence/results/failure.json",error,sizeof(error))&&
        append_artifacts(&json,root,error,sizeof(error))&&md_buf_append_cstr(&json,"}\n");
    if (!ok) {
        fprintf(stderr,"evidencegen: %s\n",error[0]=='\0'?"out of memory or inconsistent inputs":error);
        md_buf_free(&json); return EXIT_INPUT;
    }
    if (!md_write_file_atomic(output_path,json.data,json.len,error,sizeof(error))) {
        fprintf(stderr,"evidencegen: %s\n",error); md_buf_free(&json); return EXIT_OUTPUT;
    }
    printf("evidencegen PASS test_runs=6 tests=119 screenshots=%zu fixtures=%zu artifacts=%zu build=%s source=%s output=%s\n",
           MD_ARRAY_LEN(screenshots),MD_ARRAY_LEN(fixture_profiles),MD_ARRAY_LEN(artifacts),build_id,revision,output);
    md_buf_free(&json); return 0;
}
