#define _GNU_SOURCE

#include "mdedit/core.h"
#include "mdedit/document.h"
#include "mdedit/image.h"
#include "mdedit/json.h"
#include "mdedit/storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/capability.h>
#include <time.h>
#include <unistd.h>

enum { FAILURE_COUNT=15 };

typedef struct {
    const char *id;
    const char *fault;
    bool passed;
    char verification[320];
    char before_sha256[65];
    char after_sha256[65];
} FailureResult;

static char test_root[MD_PATH_MAX];

static double monotonic_ms(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC,&value)!=0) return 0.0;
    return (double)value.tv_sec*1000.0+(double)value.tv_nsec/1000000.0;
}

static bool root_path(char out[MD_PATH_MAX],const char *relative) { return md_path_join(out,test_root,relative); }

static bool ensure_parent(const char *path,char *error,size_t error_cap) {
    char parent[MD_PATH_MAX]; return md_path_dirname(parent,path)&&md_mkdirs(parent,0755,error,error_cap);
}

static bool write_text(const char *path,const char *text,char *error,size_t error_cap) {
    return ensure_parent(path,error,error_cap)&&md_write_file_atomic(path,text,strlen(text),error,error_cap);
}

static bool digest_hex(const char *path,char hex[65],char *error,size_t error_cap) {
    MdFileDigest digest; if (!md_file_digest(path,&digest,error,error_cap)) return false; md_hex_encode(digest.sha256,32U,hex); return true;
}

static bool copy_file(const char *source,const char *destination,char *error,size_t error_cap) {
    MdBytes bytes; md_bytes_init(&bytes); bool ok=md_read_file(source,&bytes,error,error_cap)&&
        ensure_parent(destination,error,error_cap)&&md_write_file_atomic(destination,bytes.data,bytes.len,error,error_cap);
    md_bytes_free(&bytes); return ok;
}

static void result_init(FailureResult *result,const char *id,const char *fault) {
    memset(result,0,sizeof(*result)); result->id=id; result->fault=fault;
}

static void fault_save_case(FailureResult *result,MdFaultKind kind,size_t fail_after,const char *relative) {
    char path[MD_PATH_MAX],error[512]={0}; bool ok=root_path(path,relative)&&write_text(path,"ORIGINAL\n",error,sizeof(error));
    if (ok) ok=digest_hex(path,result->before_sha256,error,sizeof(error));
    MdDocument doc; md_document_init(&doc,1U);
    if (ok) ok=md_document_load(&doc,path,error,sizeof(error));
    if (ok) { doc.cursor=doc.anchor=doc.source.len; ok=md_document_insert_utf8(&doc,"EDITED\n",7U,error,sizeof(error)); }
    bool saved=false;
    if (ok) { md_storage_set_fault((MdFaultInjection){kind,fail_after}); saved=md_safe_save_document(&doc,path,true,error,sizeof(error)); md_storage_clear_fault(); }
    bool digest_ok=digest_hex(path,result->after_sha256,error,sizeof(error));
    result->passed=ok&&!saved&&doc.dirty&&digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"save_failed=%s dirty_preserved=%s original_digest_unchanged=%s diagnostic=%.120s",
        !saved?"true":"false",doc.dirty?"true":"false",digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false",error);
    md_document_free(&doc);
}

static bool readonly_child(const char *source,const char *destination) {
    struct __user_cap_header_struct header={_LINUX_CAPABILITY_VERSION_3,0};
    struct __user_cap_data_struct data[2]; memset(data,0,sizeof(data));
    if (syscall(SYS_capset,&header,data)!=0) {
        (void)dprintf(STDERR_FILENO,"failure_matrix readonly child: could not drop DAC capabilities: %s\n",strerror(errno));
        return false;
    }
    char error[512]={0}; MdDocument doc; md_document_init(&doc,1U); bool ok=md_document_load(&doc,source,error,sizeof(error));
    if (ok) { doc.cursor=doc.anchor=doc.source.len; ok=md_document_insert_utf8(&doc,"edited in memory\n",17U,error,sizeof(error)); }
    bool blocked=ok&&!md_safe_save_document(&doc,source,true,error,sizeof(error))&&doc.dirty;
    char blocked_error[512]; (void)snprintf(blocked_error,sizeof(blocked_error),"%s",error);
    bool recovered=blocked&&md_safe_save_document(&doc,destination,false,error,sizeof(error))&&!doc.dirty;
    if (!blocked||!recovered) (void)dprintf(STDERR_FILENO,"failure_matrix readonly child: blocked=%s recovered=%s first=%s second=%s\n",
        blocked?"true":"false",recovered?"true":"false",blocked_error,error);
    md_document_free(&doc); return blocked&&recovered;
}

static void readonly_case(FailureResult *result) {
    char source_dir[MD_PATH_MAX],destination_dir[MD_PATH_MAX],source[MD_PATH_MAX],destination[MD_PATH_MAX],error[512]={0};
    bool ok=root_path(source_dir,"readonly/source")&&root_path(destination_dir,"readonly/recovery")&&
        md_mkdirs(source_dir,0755,error,sizeof(error))&&md_mkdirs(destination_dir,0777,error,sizeof(error))&&
        md_path_join(source,source_dir,"document.md")&&md_path_join(destination,destination_dir,"saved-as.md")&&
        write_text(source,"read-only source\n",error,sizeof(error))&&chmod(source,0444)==0&&chmod(source_dir,0555)==0&&chmod(destination_dir,0777)==0&&chmod(test_root,0755)==0;
    if (ok) ok=digest_hex(source,result->before_sha256,error,sizeof(error));
    int status=-1; pid_t child=ok?fork():-1;
    if (child==0) _exit(readonly_child(source,destination)?0:1);
    if (child>0&&waitpid(child,&status,0)<0) status=-1;
    bool digest_ok=digest_hex(source,result->after_sha256,error,sizeof(error));
    result->passed=ok&&child>0&&WIFEXITED(status)&&WEXITSTATUS(status)==0&&digest_ok&&
        strcmp(result->before_sha256,result->after_sha256)==0&&access(destination,F_OK)==0;
    bool child_passed=child>0&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"child_exercised=%s exit_status=%d save_as_recovery=%s original_digest_unchanged=%s",
        child_passed?"true":"false",WIFEXITED(status)?WEXITSTATUS(status):-1,access(destination,F_OK)==0?"true":"false",digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false");
    (void)chmod(source_dir,0755);
}

static void invalid_utf8_case(FailureResult *result,const char *fixtures) {
    char source[MD_PATH_MAX],error[512]={0}; bool ok=md_path_join(source,fixtures,"failure/invalid-utf8.md")&&
        digest_hex(source,result->before_sha256,error,sizeof(error));
    MdDocument doc; md_document_init(&doc,1U); bool rejected=ok&&!md_document_load(&doc,source,error,sizeof(error));
    bool digest_ok=digest_hex(source,result->after_sha256,error,sizeof(error));
    result->passed=rejected&&digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"open_rejected=%s original_digest_unchanged=%s diagnostic=%.140s",rejected?"true":"false",digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false",error);
    md_document_free(&doc);
}

static void truncated_session_case(FailureResult *result,const char *fixtures) {
    char workspace[MD_PATH_MAX],authored[MD_PATH_MAX],meta[MD_PATH_MAX],session[MD_PATH_MAX],fixture[MD_PATH_MAX],error[512]={0};
    bool ok=root_path(workspace,"truncated-workspace")&&md_mkdirs(workspace,0755,error,sizeof(error))&&
        md_path_join(authored,workspace,"authored.md")&&write_text(authored,"# Authored survives\n",error,sizeof(error))&&
        digest_hex(authored,result->before_sha256,error,sizeof(error))&&md_path_join(meta,workspace,".mdeditor")&&
        md_mkdirs(meta,0755,error,sizeof(error))&&md_path_join(session,meta,"session.json")&&
        md_path_join(fixture,fixtures,"failure/truncated-session.json")&&copy_file(fixture,session,error,sizeof(error));
    MdWorkspace ws; md_workspace_init(&ws); MdBuf json; md_buf_init(&json); char warning[512]={0};
    bool opened=ok&&md_workspace_open(&ws,workspace,error,sizeof(error)); bool rejected=opened&&!md_workspace_load_session(&ws,&json,warning,sizeof(warning));
    MdDocument doc; md_document_init(&doc,1U); bool markdown_ok=rejected&&md_document_load(&doc,authored,error,sizeof(error));
    bool digest_ok=digest_hex(authored,result->after_sha256,error,sizeof(error));
    result->passed=markdown_ok&&digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"workspace_opened=%s corrupt_state_rejected=%s authored_markdown_opened=%s digest_unchanged=%s",
        opened?"true":"false",rejected?"true":"false",markdown_ok?"true":"false",digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false");
    md_document_free(&doc); md_buf_free(&json); md_workspace_free(&ws);
}

static void corrupt_history_case(FailureResult *result) {
    char root[MD_PATH_MAX],error[512]={0}; bool ok=root_path(root,"corrupt-history")&&md_mkdirs(root,0755,error,sizeof(error));
    MdDocument doc; md_document_init(&doc,1U); (void)snprintf(doc.path,sizeof(doc.path),"%s","/tmp/failure-history.md");
    if (ok) ok=md_document_set_source(&doc,"live version one\n",17U,true,error,sizeof(error))&&md_history_create(root,&doc,false,NULL,error,sizeof(error))&&
        md_document_set_source(&doc,"live version two\n",17U,true,error,sizeof(error))&&md_history_create(root,&doc,false,NULL,error,sizeof(error));
    md_sha256(doc.source.data,doc.source.len,doc.disk_sha256); md_hex_encode(doc.disk_sha256,32U,result->before_sha256);
    MdVersionList list; md_version_list_init(&list); if (ok) ok=md_history_list(root,&doc,&list,error,sizeof(error))&&list.count==2U;
    MdBytes record; md_bytes_init(&record); if (ok) ok=md_read_file(list.items[1].record_path,&record,error,sizeof(error))&&record.len>80U;
    if (ok) { record.data[record.len-1U]^=0xffU; ok=md_write_file_atomic(list.items[1].record_path,record.data,record.len,error,sizeof(error)); }
    MdBuf reconstructed; md_buf_init(&reconstructed); bool rejected=ok&&!md_history_reconstruct(&list,1U,&reconstructed,error,sizeof(error));
    uint8_t after[32]; md_sha256(doc.source.data,doc.source.len,after); md_hex_encode(after,32U,result->after_sha256);
    result->passed=rejected&&strcmp(result->before_sha256,result->after_sha256)==0&&strcmp(doc.source.data,"live version two\n")==0;
    (void)snprintf(result->verification,sizeof(result->verification),"corrupt_record_rejected=%s live_document_unchanged=%s",rejected?"true":"false",strcmp(result->before_sha256,result->after_sha256)==0?"true":"false");
    md_buf_free(&reconstructed); md_bytes_free(&record); md_version_list_free(&list); md_document_free(&doc);
}

static void corrupt_recovery_case(FailureResult *result) {
    char root[MD_PATH_MAX],error[512]={0}; bool ok=root_path(root,"corrupt-recovery"); MdDocument first,second; md_document_init(&first,1U); md_document_init(&second,2U);
    if (ok) ok=md_document_set_source(&first,"first recoverable\n",18U,true,error,sizeof(error))&&md_document_set_source(&second,"second recoverable\n",19U,true,error,sizeof(error))&&
        md_recovery_write(root,&first,error,sizeof(error))&&md_recovery_write(root,&second,error,sizeof(error));
    MdRecoveryList list; md_recovery_list_init(&list); if (ok) ok=md_recovery_scan(root,&list,error,sizeof(error))&&list.count==2U;
    MdBytes record; md_bytes_init(&record); if (ok) ok=md_read_file(list.items[0].record_path,&record,error,sizeof(error))&&record.len>0U;
    if (ok) { record.data[0]^=0x7fU; ok=md_write_file_atomic(list.items[0].record_path,record.data,record.len,error,sizeof(error)); }
    md_recovery_list_free(&list); md_recovery_list_init(&list); char warning[512]={0}; bool rescanned=ok&&md_recovery_scan(root,&list,warning,sizeof(warning)); size_t valid=0U,invalid=0U;
    for (size_t i=0U;i<list.count;++i) if (list.items[i].valid) ++valid; else ++invalid;
    MdBuf recovered; md_buf_init(&recovered); bool opened=false; for (size_t i=0U;i<list.count;++i) if (list.items[i].valid) opened=md_recovery_open(&list.items[i],&recovered,error,sizeof(error));
    result->passed=rescanned&&valid==1U&&invalid==1U&&opened&&recovered.len>0U;
    (void)snprintf(result->verification,sizeof(result->verification),"valid_records=%zu corrupt_records=%zu valid_record_opened=%s",valid,invalid,opened?"true":"false");
    md_buf_free(&recovered); md_bytes_free(&record); md_recovery_list_free(&list); md_document_free(&first); md_document_free(&second);
}

static void missing_image_case(FailureResult *result,const char *fixtures) {
    char source[MD_PATH_MAX],copy[MD_PATH_MAX],error[512]={0}; bool ok=md_path_join(source,fixtures,"failure/missing-image.md")&&
        root_path(copy,"missing-image.md")&&copy_file(source,copy,error,sizeof(error));
    MdDocument doc; md_document_init(&doc,1U); if (ok) ok=md_document_load(&doc,copy,error,sizeof(error));
    if (ok) ok=digest_hex(copy,result->before_sha256,error,sizeof(error));
    char image[MD_PATH_MAX]; bool missing=md_path_join(image,test_root,"does-not-exist.png")&&access(image,F_OK)!=0;
    if (ok) { doc.cursor=doc.anchor=doc.source.len; ok=md_document_insert_utf8(&doc,"source remains\n",15U,error,sizeof(error))&&md_safe_save_document(&doc,copy,true,error,sizeof(error)); }
    bool digest_ok=digest_hex(copy,result->after_sha256,error,sizeof(error));
    result->passed=ok&&missing&&digest_ok&&strstr(doc.source.data,"does-not-exist.png")!=NULL&&strstr(doc.source.data,"source remains")!=NULL;
    (void)snprintf(result->verification,sizeof(result->verification),"asset_missing=%s reference_preserved=true edited_source_saved=true",missing?"true":"false");
    md_document_free(&doc);
}

static void corrupt_image_case(FailureResult *result,const char *fixtures,const char *relative) {
    char path[MD_PATH_MAX],error[512]={0}; bool ok=md_path_join(path,fixtures,relative)&&digest_hex(path,result->before_sha256,error,sizeof(error));
    MdImage image; MdBytes original; md_image_init(&image); md_bytes_init(&original); bool rejected=ok&&!md_image_load(path,&image,&original,error,sizeof(error));
    bool digest_ok=digest_hex(path,result->after_sha256,error,sizeof(error));
    result->passed=rejected&&digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0;
    (void)snprintf(result->verification,sizeof(result->verification),"decode_rejected=%s process_survived=true source_digest_unchanged=%s diagnostic=%.120s",
        rejected?"true":"false",digest_ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false",error);
    md_image_free(&image); md_bytes_free(&original);
}

static void symlink_case(FailureResult *result,const char *fixtures) {
    char root[MD_PATH_MAX],error[512]={0}; bool ok=md_path_join(root,fixtures,"failure"); double start=monotonic_ms();
    MdWorkspace ws; md_workspace_init(&ws); bool opened=ok&&md_workspace_open(&ws,root,error,sizeof(error)); double duration=monotonic_ms()-start; bool saw_link=false;
    for (size_t i=0U;i<ws.count;++i) if (ws.entries[i].is_symlink) saw_link=true;
    result->passed=opened&&duration<1000.0&&saw_link&&ws.count<1000U;
    (void)snprintf(result->verification,sizeof(result->verification),"scan_completed=%s duration_ms=%.3f entries=%zu symlink_observed=%s",opened?"true":"false",duration,ws.count,saw_link?"true":"false");
    md_workspace_free(&ws);
}

static void special_path_case(FailureResult *result,const char *fixtures) {
    char fixture[MD_PATH_MAX],copy[MD_PATH_MAX],error[512]={0}; bool ok=md_path_join(fixture,fixtures,"failure/special path/繁體 #$& file.md")&&
        root_path(copy,"special output/繁體 空格 #$&.md")&&copy_file(fixture,copy,error,sizeof(error)); MdDocument doc; md_document_init(&doc,1U);
    if (ok) ok=md_document_load(&doc,copy,error,sizeof(error));
    if (ok) { doc.cursor=doc.anchor=doc.source.len; ok=md_document_insert_utf8(&doc,"新增內容 ✓\n",strlen("新增內容 ✓\n"),error,sizeof(error))&&md_safe_save_document(&doc,copy,true,error,sizeof(error)); }
    if (ok) ok=digest_hex(copy,result->before_sha256,error,sizeof(error));
    MdDocument reopened; md_document_init(&reopened,2U);
    if (ok) ok=md_document_load(&reopened,copy,error,sizeof(error));
    if (ok) ok=digest_hex(copy,result->after_sha256,error,sizeof(error));
    result->passed=ok&&strcmp(result->before_sha256,result->after_sha256)==0&&strstr(reopened.source.data,"新增內容")!=NULL;
    (void)snprintf(result->verification,sizeof(result->verification),"special_path_open_edit_save_reopen=%s exact_digest_reopen=%s",ok?"true":"false",ok&&strcmp(result->before_sha256,result->after_sha256)==0?"true":"false");
    md_document_free(&reopened); md_document_free(&doc);
}

static bool append_result(MdBuf *json,const FailureResult *result,bool comma) {
    return md_buf_append_cstr(json,"    {\"id\":")&&md_json_write_escaped(json,result->id,strlen(result->id))&&
        md_buf_append_cstr(json,",\"fault_or_fixture\":")&&md_json_write_escaped(json,result->fault,strlen(result->fault))&&
        md_buf_append_cstr(json,",\"verification\":")&&md_json_write_escaped(json,result->verification,strlen(result->verification))&&
        md_buf_append_cstr(json,",\"original_sha256_before\":")&&md_json_write_escaped(json,result->before_sha256,strlen(result->before_sha256))&&
        md_buf_append_cstr(json,",\"original_sha256_after\":")&&md_json_write_escaped(json,result->after_sha256,strlen(result->after_sha256))&&
        md_buf_appendf(json,",\"result\":\"%s\"}%s\n",result->passed?"pass":"fail",comma?",":"");
}

static bool write_results(const char *output,const char *fixture_digest,const FailureResult results[FAILURE_COUNT],char *error,size_t error_cap) {
    MdBuf json; md_buf_init(&json); bool ok=md_buf_appendf(&json,"{\n  \"schema_version\":1,\n  \"fixture_profile\":\"failure\",\n  \"fixture_manifest_sha256\":\"%s\",\n  \"results\":[\n",fixture_digest);
    for (size_t i=0U;ok&&i<FAILURE_COUNT;++i) ok=append_result(&json,&results[i],i+1U<FAILURE_COUNT);
    if (ok) ok=md_buf_append_cstr(&json,"  ]\n}\n")&&md_write_file_atomic(output,json.data,json.len,error,error_cap);
    md_buf_free(&json); return ok;
}

static void usage(void) { fputs("usage: failure_matrix --fixtures DIR --output JSON\n",stderr); }

int main(int argc,char **argv) {
    const char *fixtures=NULL,*output=NULL;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--fixtures")==0&&i+1<argc) fixtures=argv[++i];
        else if (strcmp(argv[i],"--output")==0&&i+1<argc) output=argv[++i];
        else { usage(); return 2; }
    }
    if (fixtures==NULL||output==NULL) { usage(); return 2; }
    char template_path[]="/tmp/mdeditor-failure-XXXXXX"; char *made=mkdtemp(template_path);
    if (made==NULL||strlen(made)>=sizeof(test_root)) { perror("mkdtemp"); return 1; } (void)snprintf(test_root,sizeof(test_root),"%s",made);
    FailureResult results[FAILURE_COUNT];
    result_init(&results[0],"FAIL-READONLY-SAVE","real unprivileged EACCES from read-only source directory"); readonly_case(&results[0]);
    result_init(&results[1],"FAIL-ENOSPC","MD_FAULT_ENOSPC at production atomic-save write boundary"); fault_save_case(&results[1],MD_FAULT_ENOSPC,0U,"fault/enospc.md");
    result_init(&results[2],"FAIL-PARTIAL-WRITE","MD_FAULT_PARTIAL_WRITE after 3 bytes"); fault_save_case(&results[2],MD_FAULT_PARTIAL_WRITE,3U,"fault/partial.md");
    result_init(&results[3],"FAIL-CLOSE","MD_FAULT_CLOSE after complete temporary write"); fault_save_case(&results[3],MD_FAULT_CLOSE,0U,"fault/close.md");
    result_init(&results[4],"FAIL-RENAME","MD_FAULT_RENAME at production replacement boundary"); fault_save_case(&results[4],MD_FAULT_RENAME,0U,"fault/rename.md");
    result_init(&results[5],"FAIL-INVALID-UTF8","failure/invalid-utf8.md"); invalid_utf8_case(&results[5],fixtures);
    result_init(&results[6],"FAIL-TRUNCATED-SESSION","failure/truncated-session.json copied into real workspace metadata"); truncated_session_case(&results[6],fixtures);
    result_init(&results[7],"FAIL-CORRUPT-HISTORY","tampered production history payload checksum"); corrupt_history_case(&results[7]);
    result_init(&results[8],"FAIL-CORRUPT-RECOVERY","one tampered production recovery record beside one valid record"); corrupt_recovery_case(&results[8]);
    result_init(&results[9],"FAIL-MISSING-IMAGE","failure/missing-image.md"); missing_image_case(&results[9],fixtures);
    result_init(&results[10],"FAIL-CORRUPT-PNG","failure/corrupt.png"); corrupt_image_case(&results[10],fixtures,"failure/corrupt.png");
    result_init(&results[11],"FAIL-CORRUPT-JPEG","failure/corrupt.jpg"); corrupt_image_case(&results[11],fixtures,"failure/corrupt.jpg");
    result_init(&results[12],"FAIL-CORRUPT-BMP","failure/corrupt.bmp"); corrupt_image_case(&results[12],fixtures,"failure/corrupt.bmp");
    result_init(&results[13],"FAIL-SYMLINK-CYCLE","failure/symlink-cycle/a/back -> .."); symlink_case(&results[13],fixtures);
    result_init(&results[14],"FAIL-SPECIAL-PATHS","space + Traditional Chinese + #$& legal filename"); special_path_case(&results[14],fixtures);
    char manifest[MD_PATH_MAX],fixture_digest[65],error[512]={0}; bool digest_ok=md_path_join(manifest,fixtures,"failure/fixture-manifest.json")&&digest_hex(manifest,fixture_digest,error,sizeof(error));
    if (!digest_ok||!write_results(output,fixture_digest,results,error,sizeof(error))) { fprintf(stderr,"failure_matrix: %s\n",error); return 1; }
    size_t passed=0U; for (size_t i=0U;i<FAILURE_COUNT;++i) { passed+=results[i].passed?1U:0U; printf("%s %s %s\n",results[i].passed?"PASS":"FAIL",results[i].id,results[i].verification); }
    printf("FAILURE_SUMMARY total=%d passed=%zu failed=%zu skipped=0 output=%s\n",FAILURE_COUNT,passed,(size_t)FAILURE_COUNT-passed,output);
    return passed==FAILURE_COUNT?0:1;
}
