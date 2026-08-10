#include "mdedit/core.h"
#include "mdedit/json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { EXIT_USAGE=2,EXIT_CONFIG=3,EXIT_INPUT=4,EXIT_OUTPUT=5,EXIT_VERIFY=6,EXIT_INTERNAL=7 };

static const char *required_screenshots[]={
    "UI-EMPTY-LIGHT","UI-EMPTY-DARK","UI-WORKSPACE-MULTITAB","UI-SOURCE","UI-SPLIT",
    "UI-PREVIEW","UI-RENDERED-EDIT","UI-MARKDOWN-ALL","UI-IMAGE-SELECTED","UI-IMAGE-RESIZE",
    "UI-TABLE-EDIT","UI-OUTLINE","UI-COMMAND-PALETTE","UI-STATISTICS","UI-VERSION-HISTORY",
    "UI-DIFF-SIDE-BY-SIDE","UI-DIFF-INLINE","UI-MODAL-BLUR","UI-FROSTED-SCROLLED",
    "UI-EXTERNAL-CONFLICT","UI-RECOVERY-CENTER","UI-ERROR-SAVE"
};

static const char *required_categories[]={"unit","integration","e2e","performance","failure","regression"};

static const char *required_performance[]={
    "PERF-MEDIUM-OPEN","PERF-LARGE-OPEN","PERF-LARGE-PREVIEW","PERF-MEDIUM-TYPING",
    "PERF-LARGE-TYPING","PERF-LARGE-FIND","PERF-LARGE-REPLACE-ALL","PERF-LARGE-OUTLINE",
    "PERF-LONG-LINE","PERF-MEMORY-20"
};

static const char *required_failures[]={
    "FAIL-READONLY-SAVE","FAIL-ENOSPC","FAIL-PARTIAL-WRITE","FAIL-CLOSE","FAIL-RENAME",
    "FAIL-INVALID-UTF8","FAIL-TRUNCATED-SESSION","FAIL-CORRUPT-HISTORY","FAIL-CORRUPT-RECOVERY",
    "FAIL-MISSING-IMAGE","FAIL-CORRUPT-PNG","FAIL-CORRUPT-JPEG","FAIL-CORRUPT-BMP",
    "FAIL-SYMLINK-CYCLE","FAIL-SPECIAL-PATHS"
};

static const char *required_artifacts[]={
    "APP","TOOL-LOCSCAN","TOOL-FIXTUREGEN","TOOL-EVIDENCECHECK","TEST-RUNNER","LOC-REPORT"
};

typedef struct { size_t checked_files; size_t screenshots; size_t tests; size_t fixtures; } Summary;

static bool is_object(const MdJson *v) { return v!=NULL&&v->type==MD_JSON_OBJECT; }
static bool is_array(const MdJson *v) { return v!=NULL&&v->type==MD_JSON_ARRAY; }

static bool result_pass(const MdJson *entry) {
    const char *result=md_json_string(md_json_get(entry,"result"));
    return result!=NULL&&(strcmp(result,"pass")==0||strcmp(result,"PASS")==0||strcmp(result,"passed")==0);
}

static bool valid_sha256(const char *hex,bool allow_empty) {
    uint8_t decoded[32];
    if (hex==NULL) return false;
    if (hex[0]=='\0') return allow_empty;
    return strlen(hex)==64U&&md_hex_decode(hex,64U,decoded,sizeof(decoded));
}

static bool nonempty_string(const MdJson *object,const char *key) {
    const char *value=md_json_string(md_json_get(object,key));
    return value!=NULL&&value[0]!='\0';
}

static bool nonnegative_number(const MdJson *object,const char *key,double *out) {
    const MdJson *value=md_json_get(object,key);
    if (value==NULL||value->type!=MD_JSON_NUMBER||value->as.number<0.0) return false;
    if (out!=NULL) *out=value->as.number;
    return true;
}

static bool safe_relative(const char *path) {
    if (path==NULL||path[0]=='/'||path[0]=='\0') return false;
    char normalized[MD_PATH_MAX]; return md_path_normalize_relative(path,normalized)&&strcmp(path,normalized)==0;
}

static bool path_under_root(const char *root,const char *relative,char full[MD_PATH_MAX]) {
    if (!safe_relative(relative)||!md_path_join(full,root,relative)) return false;
    char root_real[MD_PATH_MAX],file_real[MD_PATH_MAX];
    if (realpath(root,root_real)==NULL||realpath(full,file_real)==NULL) return false;
    size_t n=strlen(root_real);
    return strncmp(root_real,file_real,n)==0&&(file_real[n]=='\0'||file_real[n]=='/');
}

static bool digest_entry(const char *root,const MdJson *entry,bool size_required,Summary *summary) {
    const char *relative=md_json_string(md_json_get(entry,"path"));
    const char *expected=md_json_string(md_json_get(entry,"sha256"));
    uint64_t size=0U; bool has_size=md_json_u64(md_json_get(entry,"size"),&size);
    if (relative==NULL||!valid_sha256(expected,false)||(size_required&&!has_size)) {
        fprintf(stderr,"evidencecheck: malformed artifact reference\n"); return false;
    }
    char full[MD_PATH_MAX];
    if (!path_under_root(root,relative,full)) { fprintf(stderr,"evidencecheck: unsafe/missing path: %s\n",relative); return false; }
    MdFileDigest digest; char error[256];
    if (!md_file_digest(full,&digest,error,sizeof(error))) { fprintf(stderr,"evidencecheck: %s\n",error); return false; }
    char actual[65]; md_hex_encode(digest.sha256,32U,actual);
    if (strcmp(actual,expected)!=0||(has_size&&digest.size!=size)) {
        fprintf(stderr,"evidencecheck: size/digest mismatch: %s\n",relative); return false;
    }
    ++summary->checked_files; return true;
}

static bool image_dimensions(const uint8_t *data,size_t len,uint32_t *width,uint32_t *height) {
    static const uint8_t png[8]={137U,80U,78U,71U,13U,10U,26U,10U};
    if (len>=24U&&memcmp(data,png,8U)==0&&memcmp(data+12U,"IHDR",4U)==0) {
        *width=((uint32_t)data[16]<<24U)|((uint32_t)data[17]<<16U)|((uint32_t)data[18]<<8U)|data[19];
        *height=((uint32_t)data[20]<<24U)|((uint32_t)data[21]<<16U)|((uint32_t)data[22]<<8U)|data[23];
        return *width!=0U&&*height!=0U;
    }
    if (len>=26U&&data[0]=='B'&&data[1]=='M') {
        *width=(uint32_t)data[18]|((uint32_t)data[19]<<8U)|((uint32_t)data[20]<<16U)|((uint32_t)data[21]<<24U);
        int32_t h=(int32_t)((uint32_t)data[22]|((uint32_t)data[23]<<8U)|((uint32_t)data[24]<<16U)|((uint32_t)data[25]<<24U));
        *height=h<0?(uint32_t)(-(int64_t)h):(uint32_t)h; return *width!=0U&&*height!=0U;
    }
    if (len>=4U&&data[0]==0xffU&&data[1]==0xd8U) {
        size_t at=2U;
        while (at+4U<=len) {
            while (at<len&&data[at]!=0xffU) ++at;
            while (at<len&&data[at]==0xffU) ++at;
            if (at>=len) break;
            uint8_t marker=data[at++];
            if (marker==0xd8U||marker==0xd9U) continue;
            if (at+2U>len) break;
            uint16_t segment=(uint16_t)((uint16_t)data[at]<<8U|data[at+1U]);
            if (segment<2U||at+segment>len) break;
            if ((marker>=0xc0U&&marker<=0xc3U)||(marker>=0xc5U&&marker<=0xc7U)||
                (marker>=0xc9U&&marker<=0xcbU)||(marker>=0xcdU&&marker<=0xcfU)) {
                if (segment<7U) return false;
                *height=(uint32_t)((uint32_t)data[at+3U]<<8U|data[at+4U]);
                *width=(uint32_t)((uint32_t)data[at+5U]<<8U|data[at+6U]); return *width!=0U&&*height!=0U;
            }
            at+=segment;
        }
    }
    return false;
}

static bool id_present(const MdJson *array,const char *id) {
    if (!is_array(array)) return false;
    for (size_t i=0U;i<array->as.array.len;++i) {
        const char *candidate=md_json_string(md_json_get(array->as.array.items[i],"id"));
        if (candidate!=NULL&&strcmp(candidate,id)==0) return true;
    }
    return false;
}

static bool validate_screenshots(const char *root,const MdJson *screenshots,Summary *summary) {
    if (!is_array(screenshots)) return false;
    for (size_t r=0U;r<MD_ARRAY_LEN(required_screenshots);++r) {
        if (!id_present(screenshots,required_screenshots[r])) { fprintf(stderr,"evidencecheck: missing screenshot %s\n",required_screenshots[r]); return false; }
    }
    for (size_t i=0U;i<screenshots->as.array.len;++i) {
        const MdJson *entry=screenshots->as.array.items[i];
        const char *id=md_json_string(md_json_get(entry,"id")); const char *path=md_json_string(md_json_get(entry,"path"));
        const char *description=md_json_string(md_json_get(entry,"description"));
        uint64_t expected_w=0U,expected_h=0U;
        const MdJson *requirements=md_json_get(entry,"requirements");
        if (!is_object(entry)||id==NULL||description==NULL||description[0]=='\0'||!is_array(requirements)||requirements->as.array.len==0U||
            !md_json_u64(md_json_get(entry,"width"),&expected_w)||!md_json_u64(md_json_get(entry,"height"),&expected_h)||
            !digest_entry(root,entry,false,summary)) return false;
        for (size_t r=0U;r<requirements->as.array.len;++r) {
            const char *requirement=md_json_string(requirements->as.array.items[r]);
            if (requirement==NULL||requirement[0]=='\0') return false;
        }
        char full[MD_PATH_MAX]; if (!path_under_root(root,path,full)) return false;
        MdBytes bytes; md_bytes_init(&bytes); char error[256];
        if (!md_read_file(full,&bytes,error,sizeof(error))) { md_bytes_free(&bytes); return false; }
        uint32_t width=0U,height=0U; bool ok=image_dimensions(bytes.data,bytes.len,&width,&height); md_bytes_free(&bytes);
        if (!ok||width!=expected_w||height!=expected_h) { fprintf(stderr,"evidencecheck: image dimensions/header mismatch: %s\n",path); return false; }
        ++summary->screenshots;
    }
    return true;
}

static bool validate_tests(const char *root,const MdJson *runs,Summary *summary) {
    if (!is_array(runs)) return false;
    bool categories[MD_ARRAY_LEN(required_categories)]; memset(categories,0,sizeof(categories));
    for (size_t i=0U;i<runs->as.array.len;++i) {
        const MdJson *entry=runs->as.array.items[i]; const char *id=md_json_string(md_json_get(entry,"id"));
        const char *category=md_json_string(md_json_get(entry,"category"));
        const MdJson *duration=md_json_get(entry,"duration_ms");
        uint64_t test_count=0U,passed_count=0U,failed_count=0U,skipped_count=0U;
        if (!is_object(entry)||id==NULL||category==NULL||duration==NULL||duration->type!=MD_JSON_NUMBER||duration->as.number<0.0||
            !md_json_u64(md_json_get(entry,"test_count"),&test_count)||test_count==0U||
            !md_json_u64(md_json_get(entry,"passed_count"),&passed_count)||
            !md_json_u64(md_json_get(entry,"failed_count"),&failed_count)||
            !md_json_u64(md_json_get(entry,"skipped_count"),&skipped_count)||
            test_count!=passed_count+failed_count+skipped_count||failed_count!=0U||skipped_count!=0U||
            !result_pass(entry)||!digest_entry(root,entry,false,summary)) { fprintf(stderr,"evidencecheck: invalid/failing test run %zu\n",i); return false; }
        for (size_t c=0U;c<MD_ARRAY_LEN(required_categories);++c) if (strcmp(category,required_categories[c])==0) categories[c]=true;
        summary->tests+=(size_t)test_count;
    }
    for (size_t c=0U;c<MD_ARRAY_LEN(required_categories);++c) if (!categories[c]) { fprintf(stderr,"evidencecheck: missing test category %s\n",required_categories[c]); return false; }
    return true;
}

static bool verify_fixture_manifest(const char *root,const char *manifest_relative) {
    char manifest_path[MD_PATH_MAX]; if (!path_under_root(root,manifest_relative,manifest_path)) return false;
    MdBytes bytes; md_bytes_init(&bytes); char error[256];
    if (!md_read_file(manifest_path,&bytes,error,sizeof(error))) return false;
    MdJsonError parse; MdJson *manifest=md_json_parse((const char *)bytes.data,bytes.len,&parse); md_bytes_free(&bytes);
    const MdJson *files=manifest==NULL?NULL:md_json_get(manifest,"files"); uint64_t schema=0U,seed=0U;
    if (!is_object(manifest)||!md_json_u64(md_json_get(manifest,"schema_version"),&schema)||schema!=1U||
        !nonempty_string(manifest,"generator_version")||!nonempty_string(manifest,"profile")||
        !md_json_u64(md_json_get(manifest,"seed"),&seed)||!is_array(files)||files->as.array.len==0U) {
        md_json_free(manifest); return false;
    }
    char manifest_dir[MD_PATH_MAX]; if (!md_path_dirname(manifest_dir,manifest_path)) { md_json_free(manifest); return false; }
    for (size_t i=0U;i<files->as.array.len;++i) {
        const MdJson *entry=files->as.array.items[i]; const char *relative=md_json_string(md_json_get(entry,"path"));
        const char *sha=md_json_string(md_json_get(entry,"sha256")); uint64_t size=0U;
        if (!is_object(entry)||relative==NULL||!valid_sha256(sha,false)||
            !md_json_u64(md_json_get(entry,"size"),&size)||!nonempty_string(entry,"role")||!safe_relative(relative)) {
            md_json_free(manifest); return false;
        }
        char path[MD_PATH_MAX]; if (!md_path_join(path,manifest_dir,relative)||!md_path_is_within(root,path)) { md_json_free(manifest); return false; }
        MdFileDigest digest; if (!md_file_digest(path,&digest,error,sizeof(error))) { md_json_free(manifest); return false; }
        char actual[65]; md_hex_encode(digest.sha256,32U,actual);
        if (digest.size!=size||strcmp(actual,sha)!=0) { md_json_free(manifest); return false; }
    }
    md_json_free(manifest); return true;
}

static bool validate_fixtures(const char *root,const MdJson *fixtures,Summary *summary) {
    static const char *required_profiles[]={"small","unicode","markdown-all","workspace","medium","large","stress-long-line","failure"};
    if (!is_array(fixtures)||fixtures->as.array.len==0U) return false;
    for (size_t r=0U;r<MD_ARRAY_LEN(required_profiles);++r) {
        bool found=false;
        for (size_t i=0U;i<fixtures->as.array.len;++i) {
            const char *profile=md_json_string(md_json_get(fixtures->as.array.items[i],"profile"));
            if (profile!=NULL&&strcmp(profile,required_profiles[r])==0) { found=true; break; }
        }
        if (!found) { fprintf(stderr,"evidencecheck: missing fixture profile %s\n",required_profiles[r]); return false; }
    }
    for (size_t i=0U;i<fixtures->as.array.len;++i) {
        const MdJson *entry=fixtures->as.array.items[i]; const char *profile=md_json_string(md_json_get(entry,"profile"));
        const char *path=md_json_string(md_json_get(entry,"path"));
        if (profile==NULL||path==NULL||!digest_entry(root,entry,false,summary)||!verify_fixture_manifest(root,path)) {
            fprintf(stderr,"evidencecheck: fixture reference failed at %zu\n",i); return false;
        }
        ++summary->fixtures;
    }
    return true;
}

static bool fixture_digest_for_profile(const char *root,const char *profile,char digest_hex[65]) {
    if (profile==NULL||profile[0]=='\0'||strchr(profile,'/')!=NULL) return false;
    char relative[MD_PATH_MAX],full[MD_PATH_MAX],error[256];
    if (snprintf(relative,sizeof(relative),"evidence/fixtures/%s/fixture-manifest.json",profile)<0||
        !path_under_root(root,relative,full)) return false;
    MdFileDigest digest;
    if (!md_file_digest(full,&digest,error,sizeof(error))) return false;
    md_hex_encode(digest.sha256,32U,digest_hex); return true;
}

static bool validate_performance(const char *root,const MdJson *array) {
    if (!is_array(array)) return false;
    for (size_t i=0U;i<array->as.array.len;++i) {
        const MdJson *entry=array->as.array.items[i];
        const char *id=md_json_string(md_json_get(entry,"id"));
        const char *profile=md_json_string(md_json_get(entry,"profile"));
        const char *fixture_sha=md_json_string(md_json_get(entry,"fixture_manifest_sha256"));
        const MdJson *durations=md_json_get(entry,"durations_ms");
        const MdJson *environment=md_json_get(entry,"environment");
        uint64_t runs=0U,cpu=0U,memory=0U; char actual_fixture_sha[65];
        double duration=0.0,threshold=0.0;
        bool valid=is_object(entry)&&id!=NULL&&profile!=NULL&&valid_sha256(fixture_sha,false)&&
            nonempty_string(entry,"operation")&&nonempty_string(entry,"verification")&&result_pass(entry)&&
            md_json_u64(md_json_get(entry,"runs"),&runs)&&runs>0U&&is_array(durations)&&durations->as.array.len==runs&&
            nonnegative_number(entry,"duration_ms",&duration)&&nonnegative_number(entry,"p95_ms",NULL)&&
            nonnegative_number(entry,"threshold_ms",&threshold)&&is_object(environment)&&
            nonempty_string(environment,"os")&&nonempty_string(environment,"architecture")&&
            md_json_u64(md_json_get(environment,"logical_cpu_count"),&cpu)&&cpu>0U&&
            md_json_u64(md_json_get(environment,"memory_total_kib"),&memory)&&memory>0U&&
            fixture_digest_for_profile(root,profile,actual_fixture_sha)&&strcmp(fixture_sha,actual_fixture_sha)==0;
        for (size_t d=0U;valid&&d<durations->as.array.len;++d)
            valid=durations->as.array.items[d]->type==MD_JSON_NUMBER&&durations->as.array.items[d]->as.number>=0.0;
        if (valid&&threshold>0.0) valid=duration<=threshold;
        if (!valid) { fprintf(stderr,"evidencecheck: malformed/unbound performance entry %s\n",id==NULL?"(missing id)":id); return false; }
    }
    for (size_t r=0U;r<MD_ARRAY_LEN(required_performance);++r) {
        size_t count=0U;
        for (size_t i=0U;i<array->as.array.len;++i) {
            const char *id=md_json_string(md_json_get(array->as.array.items[i],"id"));
            if (id!=NULL&&strcmp(id,required_performance[r])==0) ++count;
        }
        if (count!=1U) { fprintf(stderr,"evidencecheck: missing/duplicate performance run %s\n",required_performance[r]); return false; }
    }
    return true;
}

static bool validate_failures(const MdJson *array) {
    if (!is_array(array)) return false;
    for (size_t i=0U;i<array->as.array.len;++i) {
        const MdJson *entry=array->as.array.items[i];
        const char *id=md_json_string(md_json_get(entry,"id"));
        const char *before=md_json_string(md_json_get(entry,"original_sha256_before"));
        const char *after=md_json_string(md_json_get(entry,"original_sha256_after"));
        if (!is_object(entry)||id==NULL||!nonempty_string(entry,"fault_or_fixture")||
            !nonempty_string(entry,"verification")||!valid_sha256(before,true)||!valid_sha256(after,true)||!result_pass(entry)) {
            fprintf(stderr,"evidencecheck: malformed/failing failure entry %s\n",id==NULL?"(missing id)":id); return false;
        }
    }
    for (size_t r=0U;r<MD_ARRAY_LEN(required_failures);++r) {
        size_t count=0U;
        for (size_t i=0U;i<array->as.array.len;++i) {
            const char *id=md_json_string(md_json_get(array->as.array.items[i],"id"));
            if (id!=NULL&&strcmp(id,required_failures[r])==0) ++count;
        }
        if (count!=1U) { fprintf(stderr,"evidencecheck: missing/duplicate failure run %s\n",required_failures[r]); return false; }
    }
    return true;
}

static bool validate_artifacts(const char *root,const MdJson *artifacts,Summary *summary) {
    if (!is_array(artifacts)) return false;
    for (size_t r=0U;r<MD_ARRAY_LEN(required_artifacts);++r) if (!id_present(artifacts,required_artifacts[r])) {
        fprintf(stderr,"evidencecheck: missing artifact %s\n",required_artifacts[r]); return false;
    }
    for (size_t i=0U;i<artifacts->as.array.len;++i) if (!digest_entry(root,artifacts->as.array.items[i],true,summary)) return false;
    return true;
}

static bool required_string(const MdJson *root,const char *key) {
    const char *s=md_json_string(md_json_get(root,key)); return s!=NULL&&s[0]!='\0';
}

static int validate(const char *root,const char *manifest_relative) {
    char path[MD_PATH_MAX];
    if (!path_under_root(root,manifest_relative,path)) { fprintf(stderr,"evidencecheck: manifest path is unsafe or missing\n"); return EXIT_INPUT; }
    MdBytes bytes; md_bytes_init(&bytes); char error[256];
    if (!md_read_file(path,&bytes,error,sizeof(error))) { fprintf(stderr,"evidencecheck: %s\n",error); return EXIT_INPUT; }
    MdJsonError parse; MdJson *manifest=md_json_parse((const char *)bytes.data,bytes.len,&parse); md_bytes_free(&bytes);
    if (!is_object(manifest)) { fprintf(stderr,"evidencecheck: malformed manifest at %zu:%zu: %s\n",parse.line,parse.column,parse.message); md_json_free(manifest); return EXIT_CONFIG; }
    uint64_t schema=0U,total=0U,passed=0U,failed=0U,skipped=0U;
    const MdJson *summary_json=md_json_get(manifest,"test_summary");
    bool top=md_json_u64(md_json_get(manifest,"schema_version"),&schema)&&schema==1U&&
        required_string(manifest,"product_version")&&required_string(manifest,"build_id")&&
        required_string(manifest,"source_revision")&&required_string(manifest,"generated_at")&&is_object(summary_json)&&
        md_json_u64(md_json_get(summary_json,"total"),&total)&&md_json_u64(md_json_get(summary_json,"passed"),&passed)&&
        md_json_u64(md_json_get(summary_json,"failed"),&failed)&&md_json_u64(md_json_get(summary_json,"skipped"),&skipped)&&
        total==passed+failed+skipped&&failed==0U&&skipped==0U;
    if (!top) { fprintf(stderr,"evidencecheck: top-level schema/test summary is invalid or has failures/skips\n"); md_json_free(manifest); return EXIT_CONFIG; }
    Summary summary={0};
    bool ok=validate_tests(root,md_json_get(manifest,"test_runs"),&summary)&&summary.tests==total&&
        validate_screenshots(root,md_json_get(manifest,"screenshots"),&summary)&&
        validate_fixtures(root,md_json_get(manifest,"fixtures"),&summary)&&
        validate_performance(root,md_json_get(manifest,"performance_runs"))&&
        validate_failures(md_json_get(manifest,"failure_runs"))&&
        validate_artifacts(root,md_json_get(manifest,"artifacts"),&summary);
    md_json_free(manifest);
    if (!ok) return EXIT_VERIFY;
    printf("evidencecheck PASS tests=%zu screenshots=%zu fixtures=%zu referenced_files=%zu\n",
           summary.tests,summary.screenshots,summary.fixtures,summary.checked_files);
    return 0;
}

static void usage(void) { fputs("usage: evidencecheck --root DIR --manifest RELATIVE_PATH\n",stderr); }

int main(int argc,char **argv) {
    const char *root=NULL,*manifest=NULL;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--root")==0&&i+1<argc) root=argv[++i];
        else if (strcmp(argv[i],"--manifest")==0&&i+1<argc) manifest=argv[++i];
        else { usage(); return EXIT_USAGE; }
    }
    if (root==NULL||manifest==NULL) { usage(); return EXIT_USAGE; }
    struct stat st; if (stat(root,&st)!=0||!S_ISDIR(st.st_mode)) { fprintf(stderr,"evidencecheck: root is not a directory\n"); return EXIT_INPUT; }
    return validate(root,manifest);
}
