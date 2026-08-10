#include "mdedit/core.h"
#include "mdedit/document.h"
#include "mdedit/image.h"
#include "mdedit/storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int passed=0,failed=0;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); return false; } } while (0)
#define RUN(x) do { if (x()) { ++passed; printf("PASS %s\n",#x); } else ++failed; } while (0)

static char test_root[MD_PATH_MAX];

static bool path_join(char out[MD_PATH_MAX],const char *relative) { return md_path_join(out,test_root,relative); }

static bool write_text(const char *relative,const char *text) {
    char path[MD_PATH_MAX],dir[MD_PATH_MAX],error[256];
    return path_join(path,relative)&&md_path_dirname(dir,path)&&md_mkdirs(dir,0755,error,sizeof(error))&&
           md_write_file_atomic(path,text,strlen(text),error,sizeof(error));
}

static bool file_text(const char *relative,const char *expected) {
    char path[MD_PATH_MAX],error[256]; MdBytes bytes; md_bytes_init(&bytes);
    bool ok=path_join(path,relative)&&md_read_file(path,&bytes,error,sizeof(error))&&
            bytes.len==strlen(expected)&&memcmp(bytes.data,expected,bytes.len)==0;
    md_bytes_free(&bytes); return ok;
}

static bool test_safe_save_and_conflict(void) {
    char path[MD_PATH_MAX],error[512]; CHECK(path_join(path,"save/document.md"));
    char save_dir[MD_PATH_MAX]; CHECK(md_path_dirname(save_dir,path)); CHECK(md_mkdirs(save_dir,0755,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); CHECK(md_document_set_source(&doc,"initial\n",8U,true,error,sizeof(error)));
    CHECK(md_safe_save_document(&doc,path,false,error,sizeof(error))); CHECK(!doc.dirty&&file_text("save/document.md","initial\n"));
    doc.cursor=doc.anchor=doc.source.len; CHECK(md_document_insert_utf8(&doc,"edited\n",7U,error,sizeof(error)));
    CHECK(write_text("save/document.md","external\n"));
    CHECK(!md_safe_save_document(&doc,path,false,error,sizeof(error))&&doc.conflict&&doc.dirty);
    CHECK(file_text("save/document.md","external\n"));
    CHECK(md_safe_save_document(&doc,path,true,error,sizeof(error))); CHECK(file_text("save/document.md","initial\nedited\n"));
    char existing[MD_PATH_MAX]; CHECK(path_join(existing,"save/existing.md")&&write_text("save/existing.md","KEEP\n"));
    doc.cursor=doc.anchor=doc.source.len; CHECK(md_document_insert_utf8(&doc,"new save-as bytes\n",18U,error,sizeof(error)));
    CHECK(!md_safe_save_document(&doc,existing,false,error,sizeof(error))&&doc.dirty&&file_text("save/existing.md","KEEP\n"));
    CHECK(md_safe_save_document(&doc,existing,true,error,sizeof(error))&&!doc.dirty&&strcmp(doc.path,existing)==0);
    md_document_free(&doc); return true;
}

static bool fault_case(MdFaultKind kind,size_t after) {
    char path[MD_PATH_MAX],error[512]; CHECK(path_join(path,"fault/original.md")); CHECK(write_text("fault/original.md","ORIGINAL\n"));
    MdDocument doc; md_document_init(&doc,1U); CHECK(md_document_load(&doc,path,error,sizeof(error)));
    doc.cursor=doc.anchor=doc.source.len; CHECK(md_document_insert_utf8(&doc,"CHANGED\n",8U,error,sizeof(error)));
    md_storage_set_fault((MdFaultInjection){kind,after}); bool saved=md_safe_save_document(&doc,path,true,error,sizeof(error)); md_storage_clear_fault();
    CHECK(!saved&&doc.dirty&&file_text("fault/original.md","ORIGINAL\n")); md_document_free(&doc); return true;
}

static bool test_fault_injection(void) {
    CHECK(fault_case(MD_FAULT_ENOSPC,0U)); CHECK(fault_case(MD_FAULT_EACCES,0U));
    CHECK(fault_case(MD_FAULT_PARTIAL_WRITE,3U)); CHECK(fault_case(MD_FAULT_CLOSE,0U));
    CHECK(fault_case(MD_FAULT_RENAME,0U)); return true;
}

static bool test_invalid_utf8_open(void) {
    char path[MD_PATH_MAX],error[256]; CHECK(path_join(path,"invalid.md"));
    uint8_t bytes[]={0x61U,0x80U,0x62U}; CHECK(md_write_file_atomic(path,bytes,sizeof(bytes),error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); CHECK(!md_document_load(&doc,path,error,sizeof(error)));
    MdBytes after; md_bytes_init(&after); CHECK(md_read_file(path,&after,error,sizeof(error))&&after.len==sizeof(bytes)&&memcmp(after.data,bytes,sizeof(bytes))==0);
    md_bytes_free(&after); md_document_free(&doc); return true;
}

static bool test_preferences(void) {
    char config[MD_PATH_MAX]; CHECK(path_join(config,"xdg-config")); CHECK(setenv("XDG_CONFIG_HOME",config,1)==0);
    MdPreferences prefs; md_preferences_defaults(&prefs); prefs.dark_theme=true; prefs.font_size=21; prefs.line_spacing=1.6;
    prefs.default_embed_images=true; prefs.autosave_enabled=false; prefs.autosave_interval=45; prefs.default_mode=MD_MODE_RENDERED; prefs.sync_scroll=false; prefs.restore_session=false;
    char error[256]={0}; CHECK(md_preferences_save(&prefs,error,sizeof(error)));
    MdPreferences loaded; CHECK(md_preferences_load(&loaded,error,sizeof(error))); CHECK(loaded.dark_theme&&loaded.font_size==21&&loaded.line_spacing==1.6&&loaded.default_mode==MD_MODE_RENDERED);
    CHECK(loaded.default_embed_images&&!loaded.autosave_enabled&&loaded.autosave_interval==45&&!loaded.sync_scroll&&!loaded.restore_session);
    char path[MD_PATH_MAX]; CHECK(md_preferences_path(path)); CHECK(md_write_file_atomic(path,"{broken",7U,error,sizeof(error)));
    CHECK(!md_preferences_load(&loaded,error,sizeof(error))); CHECK(loaded.font_size==16&&!loaded.dark_theme);
    return true;
}

static bool test_recent_items(void) {
    MdWorkspace recent; md_workspace_init(&recent); char path[MD_PATH_MAX],error[256]={0};
    for (int i=0;i<25;++i) { CHECK(path_join(path,"missing recent/file")); size_t used=strlen(path); CHECK((size_t)snprintf(path+used,sizeof(path)-used,"-%02d.md",i)<sizeof(path)-used); CHECK(md_recent_add_file(&recent,path)); }
    CHECK(recent.recent_file_count==20U); char newest[MD_PATH_MAX]; strcpy(newest,recent.recent_files[7]); CHECK(md_recent_add_file(&recent,newest)); CHECK(recent.recent_file_count==20U&&strcmp(recent.recent_files[0],newest)==0);
    for (int i=0;i<12;++i) { CHECK(path_join(path,"missing workspace")); size_t used=strlen(path); CHECK((size_t)snprintf(path+used,sizeof(path)-used,"-%02d",i)<sizeof(path)-used); CHECK(md_recent_add_workspace(&recent,path)); }
    CHECK(recent.recent_workspace_count==10U); CHECK(md_recent_save(&recent,error,sizeof(error)));
    MdWorkspace loaded; md_workspace_init(&loaded); CHECK(md_recent_load(&loaded,error,sizeof(error))); CHECK(loaded.recent_file_count==20U&&loaded.recent_workspace_count==10U); CHECK(strcmp(loaded.recent_files[0],newest)==0);
    CHECK(md_recent_remove_file(&loaded,newest)&&loaded.recent_file_count==19U);
    char removed_workspace[MD_PATH_MAX]; strcpy(removed_workspace,loaded.recent_workspaces[3]); CHECK(md_recent_remove_workspace(&loaded,removed_workspace)&&loaded.recent_workspace_count==9U);
    md_recent_clear_files(&loaded); md_recent_clear_workspaces(&loaded); CHECK(loaded.recent_file_count==0U&&loaded.recent_workspace_count==0U);
    md_workspace_free(&loaded); md_workspace_free(&recent); return true;
}

static bool test_workspace_and_symlink(void) {
    CHECK(write_text("workspace/z.md","# z\n")); CHECK(write_text("workspace/A/a.md","# a\n"));
    CHECK(write_text("workspace/繁體 空格/b.md","# b\n")); CHECK(write_text("workspace/.mdeditor/hidden.md","hidden\n"));
    char cycle_dir[MD_PATH_MAX],cycle_link[MD_PATH_MAX],error[256]={0};
    CHECK(path_join(cycle_dir,"workspace/cycle")&&md_mkdirs(cycle_dir,0755,error,sizeof(error)));
    CHECK(md_path_join(cycle_link,cycle_dir,"back")); CHECK(symlink("..",cycle_link)==0||errno==EEXIST);
    MdWorkspace ws; md_workspace_init(&ws); char root[MD_PATH_MAX]; CHECK(path_join(root,"workspace"));
    CHECK(md_workspace_open(&ws,root,error,sizeof(error))); CHECK(ws.count>=6U);
    for (size_t i=0U;i<ws.count;++i) CHECK(strncmp(ws.entries[i].path,".mdeditor",9U)!=0);
    CHECK(md_workspace_create_file(&ws,"new folder/new.md",false,error,sizeof(error))==false);
    CHECK(md_workspace_create_file(&ws,"new folder",true,error,sizeof(error)));
    CHECK(md_workspace_create_file(&ws,"new folder/new.md",false,error,sizeof(error)));
    CHECK(md_workspace_rename(&ws,"new folder/new.md","new folder/renamed.md",error,sizeof(error)));
    CHECK(md_workspace_set_directory_collapsed(&ws,"A",true)&&md_workspace_directory_collapsed(&ws,"A"));
    CHECK(md_workspace_set_directory_collapsed(&ws,"A",false)&&!md_workspace_directory_collapsed(&ws,"A"));
    CHECK(md_workspace_delete(&ws,"new folder/renamed.md",false,error,sizeof(error)));
    CHECK(md_workspace_delete(&ws,"new folder",false,error,sizeof(error)));
    CHECK(md_workspace_create_file(&ws,"delete tree",true,error,sizeof(error)));
    CHECK(md_workspace_create_file(&ws,"delete tree/child.md",false,error,sizeof(error)));
    CHECK(!md_workspace_delete(&ws,"delete tree",false,error,sizeof(error)));
    CHECK(md_workspace_delete(&ws,"delete tree",true,error,sizeof(error)));
    md_workspace_free(&ws); return true;
}

static bool test_workspace_session_corruption(void) {
    char root[MD_PATH_MAX],error[256]={0}; CHECK(path_join(root,"session-workspace")); CHECK(md_mkdirs(root,0755,error,sizeof(error)));
    MdWorkspace ws; md_workspace_init(&ws); CHECK(md_workspace_open(&ws,root,error,sizeof(error)));
    MdDocument docs[2]; md_document_init(&docs[0],1U); md_document_init(&docs[1],2U);
    docs[0].mode=MD_MODE_SPLIT; docs[0].cursor=0U; docs[1].zoom=1.25;
    CHECK(md_workspace_set_directory_collapsed(&ws,"docs",true));
    CHECK(md_workspace_save_session(&ws,docs,2U,1U,error,sizeof(error)));
    MdBuf json; md_buf_init(&json); CHECK(md_workspace_load_session(&ws,&json,error,sizeof(error))&&strstr(json.data,"\"active_tab\": 1")!=NULL&&strstr(json.data,"collapsed_directories")!=NULL&&strstr(json.data,"docs")!=NULL);
    char session[MD_PATH_MAX]; CHECK(md_path_join(session,root,".mdeditor/session.json")); CHECK(md_write_file_atomic(session,"{truncated",10U,error,sizeof(error)));
    CHECK(!md_workspace_load_session(&ws,&json,error,sizeof(error))); CHECK(access(session,F_OK)!=0);
    md_buf_free(&json); md_document_free(&docs[0]); md_document_free(&docs[1]); md_workspace_free(&ws); return true;
}

static bool test_history_roundtrip(void) {
    char history[MD_PATH_MAX],error[512]={0}; CHECK(path_join(history,"history")); CHECK(md_mkdirs(history,0755,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); strcpy(doc.path,"/tmp/history-test.md");
    for (unsigned i=0U;i<25U;++i) {
        char text[128]; int n=snprintf(text,sizeof(text),"# Version %u\nline %u 繁體\n",i,i);
        CHECK(md_document_set_source(&doc,text,(size_t)n,true,error,sizeof(error)));
        CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error)));
    }
    MdVersionList list; md_version_list_init(&list); CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.count==25U);
    CHECK(list.items[0].full_snapshot&&list.items[20].full_snapshot);
    for (size_t i=0U;i<list.count;++i) { MdBuf source; md_buf_init(&source); CHECK(md_history_reconstruct(&list,i,&source,error,sizeof(error))); CHECK(strstr(source.data,"Version")!=NULL); md_buf_free(&source); }
    size_t before=list.count; CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error))); md_version_list_free(&list);
    CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.count==before);
    CHECK(md_history_create(history,&doc,true,NULL,error,sizeof(error))); md_version_list_free(&list);
    CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.count==before+1U);
    CHECK(md_history_pin(&list,0U,true,error,sizeof(error))); md_version_list_free(&list);
    CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.items[0].pinned);
    md_version_list_free(&list); md_document_free(&doc); return true;
}

static bool test_history_corruption(void) {
    char history[MD_PATH_MAX],error[512]={0}; CHECK(path_join(history,"history-corrupt")); CHECK(md_mkdirs(history,0755,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); strcpy(doc.path,"/tmp/corrupt.md"); CHECK(md_document_set_source(&doc,"one\n",4U,true,error,sizeof(error)));
    CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error))); CHECK(md_document_set_source(&doc,"two\n",4U,true,error,sizeof(error))); CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error)));
    MdVersionList list; md_version_list_init(&list); CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.count==2U);
    MdBytes bytes; md_bytes_init(&bytes); CHECK(md_read_file(list.items[1].record_path,&bytes,error,sizeof(error))); CHECK(bytes.len>80U); bytes.data[bytes.len-1U]^=0xffU;
    CHECK(md_write_file_atomic(list.items[1].record_path,bytes.data,bytes.len,error,sizeof(error))); md_bytes_free(&bytes);
    MdBuf source; md_buf_init(&source); CHECK(!md_history_reconstruct(&list,1U,&source,error,sizeof(error))); CHECK(strcmp(doc.source.data,"two\n")==0);
    md_buf_free(&source); md_version_list_free(&list); md_document_free(&doc); return true;
}

static bool test_history_retention_and_pins(void) {
    char history[MD_PATH_MAX],error[512]={0}; CHECK(path_join(history,"history-retention")); CHECK(md_mkdirs(history,0755,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); strcpy(doc.path,"/tmp/retention.md");
    md_history_set_retention_limits(30U,UINT64_MAX);
    for (unsigned i=0U;i<21U;++i) { char text[96]; int n=snprintf(text,sizeof(text),"version %u stable payload\n",i); CHECK(md_document_set_source(&doc,text,(size_t)n,true,error,sizeof(error))); CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error))); }
    MdVersionList list; md_version_list_init(&list); CHECK(md_history_list(history,&doc,&list,error,sizeof(error))&&list.count==21U); CHECK(md_history_pin(&list,0U,true,error,sizeof(error))); md_version_list_free(&list);
    for (unsigned i=21U;i<50U;++i) { char text[96]; int n=snprintf(text,sizeof(text),"version %u stable payload\n",i); CHECK(md_document_set_source(&doc,text,(size_t)n,true,error,sizeof(error))); CHECK(md_history_create(history,&doc,false,NULL,error,sizeof(error))); }
    CHECK(md_history_list(history,&doc,&list,error,sizeof(error))); CHECK(list.count<=30U); bool pinned=false;
    for (size_t i=0U;i<list.count;++i) { pinned|=list.items[i].sequence==1U&&list.items[i].pinned; MdBuf source; md_buf_init(&source); CHECK(md_history_reconstruct(&list,i,&source,error,sizeof(error))); md_buf_free(&source); }
    CHECK(pinned); md_version_list_free(&list); md_history_reset_retention_limits(); md_document_free(&doc);

    char default_history[MD_PATH_MAX]; CHECK(path_join(default_history,"history-retention-default")); CHECK(md_mkdirs(default_history,0755,error,sizeof(error)));
    MdDocument many; md_document_init(&many,2U); strcpy(many.path,"/tmp/default-retention.md");
    for (unsigned i=0U;i<225U;++i) { char text[80]; int n=snprintf(text,sizeof(text),"default version %u\n",i); CHECK(md_document_set_source(&many,text,(size_t)n,true,error,sizeof(error))); CHECK(md_history_create(default_history,&many,false,NULL,error,sizeof(error))); }
    CHECK(md_history_list(default_history,&many,&list,error,sizeof(error))&&list.count<=200U&&list.count>0U);
    MdBuf latest; md_buf_init(&latest); CHECK(md_history_reconstruct(&list,list.count-1U,&latest,error,sizeof(error))&&strstr(latest.data,"default version 224")!=NULL);
    md_buf_free(&latest); md_version_list_free(&list); md_document_free(&many); return true;
}

static bool test_recovery(void) {
    char root[MD_PATH_MAX],error[512]={0}; CHECK(path_join(root,"recovery"));
    MdDocument a,b; md_document_init(&a,1U); md_document_init(&b,2U);
    CHECK(md_document_set_source(&a,"recover 繁體\n",strlen("recover 繁體\n"),true,error,sizeof(error)));
    CHECK(md_document_set_source(&b,"second\n",7U,true,error,sizeof(error)));
    CHECK(md_recovery_write(root,&a,error,sizeof(error))&&md_recovery_write(root,&b,error,sizeof(error)));
    MdRecoveryList list; md_recovery_list_init(&list); CHECK(md_recovery_scan(root,&list,error,sizeof(error))&&list.count==2U);
    MdBuf source; md_buf_init(&source); CHECK(md_recovery_open(&list.items[0],&source,error,sizeof(error))); CHECK(source.len>0U); md_buf_free(&source);
    MdBytes bytes; md_bytes_init(&bytes); CHECK(md_read_file(list.items[0].record_path,&bytes,error,sizeof(error))); bytes.data[0]='X'; CHECK(md_write_file_atomic(list.items[0].record_path,bytes.data,bytes.len,error,sizeof(error))); md_bytes_free(&bytes);
    md_recovery_list_free(&list); CHECK(md_recovery_scan(root,&list,error,sizeof(error))&&list.count==2U);
    size_t valid=0U; for (size_t i=0U;i<list.count;++i) if (list.items[i].valid) ++valid; CHECK(valid==1U);
    md_recovery_list_free(&list); md_document_free(&a); md_document_free(&b); return true;
}

static void bmp_header(uint8_t bmp[70],uint8_t r) {
    memset(bmp,0,70U); bmp[0]='B'; bmp[1]='M'; bmp[2]=70U; bmp[10]=54U; bmp[14]=40U; bmp[18]=2U; bmp[22]=2U; bmp[26]=1U; bmp[28]=24U; bmp[34]=16U;
    for (size_t i=54U;i<70U;++i) bmp[i]=(uint8_t)(r+i);
}

static bool test_assets_and_exports(void) {
    char workspace[MD_PATH_MAX],doc_path[MD_PATH_MAX],image_path[MD_PATH_MAX],error[512]={0};
    CHECK(path_join(workspace,"asset-workspace")); CHECK(md_mkdirs(workspace,0755,error,sizeof(error)));
    CHECK(md_path_join(doc_path,workspace,"docs/report.md")); char docs[MD_PATH_MAX]; CHECK(md_path_dirname(docs,doc_path)&&md_mkdirs(docs,0755,error,sizeof(error)));
    CHECK(path_join(image_path,"source image.bmp")); uint8_t bmp[70]; bmp_header(bmp,10U); CHECK(md_write_file_atomic(image_path,bmp,sizeof(bmp),error,sizeof(error)));
    char relative[MD_PATH_MAX]; CHECK(md_asset_import_relative(workspace,doc_path,image_path,relative,error,sizeof(error))); CHECK(strstr(relative,"assets/")!=NULL||strstr(relative,"../assets/")!=NULL);
    MdBuf uri; md_buf_init(&uri); CHECK(md_image_make_data_uri(MD_IMAGE_BMP,bmp,sizeof(bmp),&uri)); char external[MD_PATH_MAX];
    CHECK(md_asset_externalize(workspace,doc_path,uri.data,uri.len,external,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); strcpy(doc.path,doc_path);
    MdBuf source; md_buf_init(&source); CHECK(md_buf_appendf(&source,"# Report\n\n![local](%s)\n![embedded](%s)\n",relative,uri.data));
    CHECK(md_document_set_source(&doc,source.data,source.len,true,error,sizeof(error)));
    char single[MD_PATH_MAX],assets[MD_PATH_MAX]; CHECK(md_path_join(single,workspace,"portable.md")); CHECK(md_path_join(assets,workspace,"package.md"));
    bool single_ok=md_export_portable_single(&doc,single,error,sizeof(error));
    if (!single_ok) fprintf(stderr,"portable-single error: %s\n",error);
    CHECK(single_ok); MdBytes exported; md_bytes_init(&exported); CHECK(md_read_file(single,&exported,error,sizeof(error))); CHECK(strstr((const char *)exported.data,"data:image/bmp;base64,")!=NULL); md_bytes_free(&exported);
    CHECK(md_export_portable_assets(&doc,assets,error,sizeof(error))); CHECK(access(assets,F_OK)==0);

    char special_image[MD_PATH_MAX]; CHECK(md_path_join(special_image,docs,"a & b.bmp"));
    CHECK(md_write_file_atomic(special_image,bmp,sizeof(bmp),error,sizeof(error)));
    MdDocument special; md_document_init(&special,9U); strcpy(special.path,doc_path);
    static const char special_source[]=
        "![with title](<a & b.bmp> \"Caption kept\")\n"
        "<img alt='html' src=\"a &amp; b.bmp\" width=\"20\">\n";
    CHECK(md_document_set_source(&special,special_source,sizeof(special_source)-1U,true,error,sizeof(error)));
    char special_single[MD_PATH_MAX],special_package[MD_PATH_MAX];
    CHECK(md_path_join(special_single,workspace,"special-single.md"));
    CHECK(md_path_join(special_package,workspace,"special-package.md"));
    CHECK(md_export_portable_single(&special,special_single,error,sizeof(error)));
    CHECK(md_read_file(special_single,&exported,error,sizeof(error)));
    CHECK(strstr((const char *)exported.data,"\"Caption kept\"")!=NULL);
    CHECK(strstr((const char *)exported.data,"<img alt='html' src=\"data:image/bmp;base64,")!=NULL);
    md_bytes_free(&exported); md_bytes_init(&exported);
    CHECK(md_export_portable_assets(&special,special_package,error,sizeof(error)));
    CHECK(md_read_file(special_package,&exported,error,sizeof(error)));
    CHECK(strstr((const char *)exported.data,"special-package.assets/")!=NULL);
    CHECK(strstr((const char *)exported.data,"\"Caption kept\"")!=NULL);
    md_bytes_free(&exported); md_document_free(&special);

    MdDocument broken; md_document_init(&broken,10U); strcpy(broken.path,doc_path);
    static const char broken_source[]="![missing](not-there.png)\n";
    CHECK(md_document_set_source(&broken,broken_source,sizeof(broken_source)-1U,true,error,sizeof(error)));
    char failed_export[MD_PATH_MAX],failed_assets[MD_PATH_MAX];
    CHECK(md_path_join(failed_export,workspace,"transaction-failed.md"));
    CHECK(md_path_join(failed_assets,workspace,"transaction-failed.assets"));
    CHECK(!md_export_portable_assets(&broken,failed_export,error,sizeof(error)));
    CHECK(access(failed_export,F_OK)!=0&&access(failed_assets,F_OK)!=0);
    CHECK(strcmp(broken.source.data,broken_source)==0&&broken.dirty);
    md_document_free(&broken);
    md_buf_free(&source); md_buf_free(&uri); md_document_free(&doc); return true;
}

static bool test_save_as_relocation(void) {
    char old_dir[MD_PATH_MAX],new_dir[MD_PATH_MAX],doc_path[MD_PATH_MAX],image_path[MD_PATH_MAX],destination[MD_PATH_MAX],error[1024]={0};
    CHECK(path_join(old_dir,"relocation/original")); CHECK(path_join(new_dir,"relocation/moved"));
    CHECK(md_mkdirs(old_dir,0755,error,sizeof(error))&&md_mkdirs(new_dir,0755,error,sizeof(error)));
    CHECK(md_path_join(doc_path,old_dir,"report.md")&&md_path_join(image_path,old_dir,"picture.bmp")&&md_path_join(destination,new_dir,"report.md"));
    uint8_t bmp[70]; bmp_header(bmp,91U); CHECK(md_write_file_atomic(image_path,bmp,sizeof(bmp),error,sizeof(error)));
    static const char source[]="# Relocation\n\n![asset](picture.bmp \"Caption\")\n<img alt='second' src=\"picture.bmp\">\n";
    CHECK(md_write_file_atomic(doc_path,source,sizeof(source)-1U,error,sizeof(error)));
    MdDocument doc; md_document_init(&doc,1U); CHECK(md_document_load(&doc,doc_path,error,sizeof(error))); CHECK(md_document_has_relative_images(&doc));
    doc.cursor=doc.anchor=doc.source.len; CHECK(md_document_insert_utf8(&doc,"\nchanged",8U,error,sizeof(error)));
    CHECK(md_save_as_with_relocation(&doc,destination,MD_RELOCATE_COPY_REBASE,false,error,sizeof(error)));
    CHECK(strcmp(doc.path,destination)==0&&!doc.dirty&&strstr(doc.source.data,"report.assets/")!=NULL&&strstr(doc.source.data,"\"Caption\"")!=NULL&&doc.undo.len>=1U);
    CHECK(strstr(doc.source.data,"<img alt='second' src=\"report.assets/")!=NULL);
    const char *relative=strstr(doc.source.data,"report.assets/"); CHECK(relative!=NULL); const char *close=relative+strcspn(relative," \t\r\n)\""); CHECK(close>relative);
    char relative_path[MD_PATH_MAX],copied[MD_PATH_MAX]; size_t relative_len=(size_t)(close-relative); CHECK(relative_len<sizeof(relative_path)); memcpy(relative_path,relative,relative_len); relative_path[relative_len]='\0';
    CHECK(md_path_join(copied,new_dir,relative_path)); MdBytes bytes; md_bytes_init(&bytes); CHECK(md_read_file(copied,&bytes,error,sizeof(error))&&bytes.len==sizeof(bmp)&&memcmp(bytes.data,bmp,sizeof(bmp))==0); md_bytes_free(&bytes);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&strstr(doc.source.data,"picture.bmp")!=NULL&&doc.dirty);
    CHECK(md_document_redo(&doc,error,sizeof(error))&&strstr(doc.source.data,"report.assets/")!=NULL);
    md_document_free(&doc);

    char keep_path[MD_PATH_MAX]; CHECK(md_path_join(keep_path,new_dir,"keep.md")); MdDocument keep; md_document_init(&keep,2U); CHECK(md_document_load(&keep,doc_path,error,sizeof(error)));
    CHECK(md_save_as_with_relocation(&keep,keep_path,MD_RELOCATE_KEEP_REFERENCES,false,error,sizeof(error))&&strcmp(keep.source.data,source)==0); md_document_free(&keep);

    char failed_path[MD_PATH_MAX]; CHECK(md_path_join(failed_path,new_dir,"failed.md")); MdDocument failed_doc; md_document_init(&failed_doc,3U); CHECK(md_document_load(&failed_doc,doc_path,error,sizeof(error)));
    MdBuf before; md_buf_init(&before); CHECK(md_buf_assign(&before,failed_doc.source.data,failed_doc.source.len));
    md_storage_set_fault((MdFaultInjection){MD_FAULT_ENOSPC,0U}); bool saved=md_save_as_with_relocation(&failed_doc,failed_path,MD_RELOCATE_COPY_REBASE,false,error,sizeof(error)); md_storage_clear_fault();
    CHECK(!saved&&strcmp(failed_doc.path,doc_path)==0&&failed_doc.source.len==before.len&&memcmp(failed_doc.source.data,before.data,before.len)==0&&access(failed_path,F_OK)!=0);
    md_buf_free(&before); md_document_free(&failed_doc); return true;
}

int main(void) {
    char template_path[]="/tmp/mdeditor-storage-XXXXXX"; char *root=mkdtemp(template_path);
    if (root==NULL||strlen(root)>=sizeof(test_root)) { perror("mkdtemp"); return 1; }
    strcpy(test_root,root);
    RUN(test_safe_save_and_conflict); RUN(test_fault_injection); RUN(test_invalid_utf8_open); RUN(test_preferences); RUN(test_recent_items);
    RUN(test_workspace_and_symlink); RUN(test_workspace_session_corruption); RUN(test_history_roundtrip);
    RUN(test_history_corruption); RUN(test_history_retention_and_pins); RUN(test_recovery); RUN(test_assets_and_exports); RUN(test_save_as_relocation);
    printf("TEST_SUMMARY total=%d passed=%d failed=%d skipped=0\n",passed+failed,passed,failed);
    printf("TEST_ROOT %s\n",test_root); return failed==0?0:1;
}
