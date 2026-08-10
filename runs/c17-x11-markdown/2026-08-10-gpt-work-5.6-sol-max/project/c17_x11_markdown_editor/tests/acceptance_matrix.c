#include "mdedit/core.h"
#include "mdedit/document.h"

#include <stdio.h>
#include <string.h>

static int passed=0;
static int failed=0;

#define REQUIRE(expr) do { if (!(expr)) { fprintf(stderr,"  assertion failed at %s:%d: %s\n",__FILE__,__LINE__,#expr); return false; } } while (0)
#define RUN(id,fn) do { bool ok=(fn)(); if (ok) ++passed; else ++failed; printf("%s %s\n",ok?"PASS":"FAIL",id); } while (0)

static bool set_source(MdDocument *doc,const char *source,char error[256]) {
    md_document_init(doc,1U);
    return md_document_set_source(doc,source,strlen(source),false,error,256U);
}

static bool replace_exact_case(const char *source,const char *needle,const char *replacement,
                               const char *expected) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,source,error));
    const char *found=strstr(doc.source.data,needle); REQUIRE(found!=NULL);
    size_t at=(size_t)(found-doc.source.data); size_t original_len=doc.source.len;
    REQUIRE(md_document_replace(&doc,at,at+strlen(needle),replacement,strlen(replacement),
                                "Rendered text edit",false,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,expected)==0&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&doc.source.len==original_len&&strcmp(doc.source.data,source)==0);
    md_document_free(&doc); return true;
}

static bool rendered_paragraph(void) {
    return replace_exact_case("Paragraph old text.\n","old","new","Paragraph new text.\n");
}

static bool rendered_heading(void) {
    return replace_exact_case("## Old heading\n","Old","New","## New heading\n");
}

static bool heading_level_one_undo(void) {
    static const char original[]="## Structural heading\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    doc.cursor=doc.anchor=4U; REQUIRE(md_document_heading_level(&doc,4,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"#### Structural heading\n")==0&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_document_free(&doc); return true;
}

static bool formatting_case(const char *open,const char *close,const char *expected,const char *label) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,"format me",error));
    doc.anchor=0U; doc.cursor=6U;
    REQUIRE(md_document_format(&doc,open,close,label,error,sizeof(error))&&strcmp(doc.source.data,expected)==0);
    REQUIRE(doc.undo.len==1U&&md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,"format me")==0);
    md_document_free(&doc); return true;
}

static bool bold_selection(void) { return formatting_case("**","**","**format** me","Bold"); }
static bool italic_selection(void) { return formatting_case("*","*","*format* me","Italic"); }
static bool strike_selection(void) { return formatting_case("~~","~~","~~format~~ me","Strikethrough"); }

static bool inline_code_backtick(void) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,"a`b tail",error));
    doc.anchor=0U; doc.cursor=3U;
    REQUIRE(md_document_format(&doc,"``","``","Inline code",error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"``a`b`` tail")==0&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,"a`b tail")==0);
    md_document_free(&doc); return true;
}

static bool link_label(void) {
    static const char original[]="A [label](<https://example.com/a b> \"title\") link.";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    REQUIRE(md_document_edit_link(&doc,4U,"shown",NULL,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"A [shown](<https://example.com/a b> \"title\") link.")==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_document_free(&doc); return true;
}

static bool link_destination(void) {
    static const char original[]="[shown](https://old.example/path)";
    MdDocument doc; char error[256]={0}; MdBuf label,destination; md_buf_init(&label); md_buf_init(&destination);
    REQUIRE(set_source(&doc,original,error));
    REQUIRE(md_document_edit_link(&doc,2U,NULL,"https://new.example/path",error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"[shown](https://new.example/path)")==0);
    REQUIRE(md_document_link_at(&doc,2U,&label,&destination)&&strcmp(label.data,"shown")==0&&
            strcmp(destination.data,"https://new.example/path")==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_buf_free(&label); md_buf_free(&destination); md_document_free(&doc); return true;
}

static bool external_link_source_path(void) {
    MdDocument doc; char error[256]={0}; MdBuf label,destination; md_buf_init(&label); md_buf_init(&destination);
    REQUIRE(set_source(&doc,"[Open](https://example.com/safe)",error));
    REQUIRE(md_document_link_at(&doc,2U,&label,&destination)&&strncmp(destination.data,"https://",8U)==0);
    REQUIRE(strcmp(doc.source.data,"[Open](https://example.com/safe)")==0);
    md_buf_free(&label); md_buf_free(&destination); md_document_free(&doc); return true;
}

static bool list_enter_serialization(void) {
    static const char original[]="1. first\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    doc.cursor=doc.anchor=strlen("1. first");
    REQUIRE(md_document_insert_utf8(&doc,"\n1. ",4U,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"1. first\n1. \n")==0&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_document_free(&doc); return true;
}

static bool list_indent_outdent(void) {
    static const char original[]="- outer\n- inner\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    size_t inner=(size_t)(strstr(doc.source.data,"- inner")-doc.source.data);
    REQUIRE(md_document_replace(&doc,inner,inner,"  ",2U,"Indent list",false,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,"- outer\n  - inner\n")==0);
    REQUIRE(md_document_replace(&doc,inner,inner+2U,"",0U,"Outdent list",false,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,original)==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,"- outer\n  - inner\n")==0);
    md_document_free(&doc); return true;
}

static bool task_toggle(void) {
    static const char original[]="- [ ] task\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    REQUIRE(md_document_toggle_task(&doc,6U,error,sizeof(error))&&strcmp(doc.source.data,"- [x] task\n")==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_document_free(&doc); return true;
}

static bool nested_quote_edit(void) {
    return replace_exact_case("> outer\n>> nested old\n","old","new","> outer\n>> nested new\n");
}

static bool fenced_code_edit(void) {
    return replace_exact_case("```c\n**literal** # text\n```\n","literal","changed",
                              "```c\n**changed** # text\n```\n");
}

static bool fence_info_edit(void) {
    return replace_exact_case("```c\nint x;\n```\n","c\n","cpp\n","```cpp\nint x;\n```\n");
}

static bool image_delete_undo(void) {
    static const char original[]="Before\n\n![alt](asset.bmp)\n\nAfter\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    const char *image=strstr(doc.source.data,"![alt]"); REQUIRE(image!=NULL); size_t start=(size_t)(image-doc.source.data);
    const char *line_end=strchr(image,'\n'); REQUIRE(line_end!=NULL);
    REQUIRE(md_document_replace(&doc,start,(size_t)(line_end-doc.source.data)+1U,"",0U,"Delete image",false,error,sizeof(error)));
    REQUIRE(strstr(doc.source.data,"![alt]")==NULL&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,original)==0);
    md_document_free(&doc); return true;
}

static const char table_source[]="| A | B |\n| --- | --- |\n| 1 | 2 |\n";

static bool table_cell_edit(void) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,table_source,error));
    REQUIRE(md_document_table_set_cell(&doc,1U,1U,0U,"繁體 | cell",strlen("繁體 | cell"),error,sizeof(error)));
    REQUIRE(strstr(doc.source.data,"繁體 \\| cell")!=NULL&&doc.undo.len==1U);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,table_source)==0);
    md_document_free(&doc); return true;
}

static bool table_row_add_delete(void) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,table_source,error));
    REQUIRE(md_document_table_action(&doc,1U,1U,0U,MD_TABLE_ROW_BELOW,error,sizeof(error)));
    REQUIRE(strstr(doc.source.data,"|  |  |")!=NULL);
    REQUIRE(md_document_table_action(&doc,1U,2U,0U,MD_TABLE_ROW_DELETE,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,table_source)==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strstr(doc.source.data,"|  |  |")!=NULL);
    md_document_free(&doc); return true;
}

static bool table_column_add_delete(void) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,table_source,error));
    REQUIRE(md_document_table_action(&doc,1U,1U,1U,MD_TABLE_COL_AFTER,error,sizeof(error)));
    REQUIRE(strstr(doc.source.data,"| A | B |  |")!=NULL);
    REQUIRE(md_document_table_action(&doc,1U,1U,2U,MD_TABLE_COL_DELETE,error,sizeof(error)));
    REQUIRE(strcmp(doc.source.data,table_source)==0);
    REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strstr(doc.source.data,"| A | B |  |")!=NULL);
    md_document_free(&doc); return true;
}

static bool table_alignment_states(void) {
    static const MdTableAction actions[]={MD_TABLE_ALIGN_DEFAULT,MD_TABLE_ALIGN_LEFT,MD_TABLE_ALIGN_CENTER,MD_TABLE_ALIGN_RIGHT};
    static const char *markers[]={"| --- | --- |","| :--- | --- |","| :---: | --- |","| ---: | --- |"};
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,table_source,error));
    for (size_t i=0U;i<MD_ARRAY_LEN(actions);++i) {
        REQUIRE(md_document_table_action(&doc,1U,1U,0U,actions[i],error,sizeof(error))&&strstr(doc.source.data,markers[i])!=NULL);
        REQUIRE(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,table_source)==0);
    }
    md_document_free(&doc); return true;
}

static bool table_tab_navigation_boundaries(void) {
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,table_source,error));
    const char *cells[]={"A","B","1","2"}; size_t previous=0U;
    for (size_t i=0U;i<MD_ARRAY_LEN(cells);++i) {
        const char *at=strstr(doc.source.data+previous,cells[i]); REQUIRE(at!=NULL);
        size_t offset=(size_t)(at-doc.source.data); REQUIRE(md_utf8_is_boundary(doc.source.data,doc.source.len,offset));
        REQUIRE(i==0U||offset>previous); previous=offset;
    }
    REQUIRE(strcmp(doc.source.data,table_source)==0); md_document_free(&doc); return true;
}

static bool exact_mode_cycle(void) {
    static const char original[]="# 繁體 heading\n\nParagraph e\xCC\x81 ✈️ 👩‍💻\n";
    MdDocument doc; char error[256]={0}; REQUIRE(set_source(&doc,original,error));
    uint8_t before[32],after[32]; md_sha256(doc.source.data,doc.source.len,before);
    static const MdEditorMode modes[]={MD_MODE_SOURCE,MD_MODE_SPLIT,MD_MODE_PREVIEW,MD_MODE_RENDERED,MD_MODE_SOURCE};
    for (size_t i=0U;i<MD_ARRAY_LEN(modes);++i) doc.mode=modes[i];
    md_sha256(doc.source.data,doc.source.len,after);
    REQUIRE(memcmp(before,after,sizeof(before))==0&&strcmp(doc.source.data,original)==0&&md_utf8_validate(doc.source.data,doc.source.len,NULL));
    md_document_free(&doc); return true;
}

int main(void) {
    RUN("RENDER-01-PARAGRAPH-SOURCE",rendered_paragraph);
    RUN("RENDER-02-HEADING-SOURCE",rendered_heading);
    RUN("RENDER-03-H2-H4-UNDO",heading_level_one_undo);
    RUN("RENDER-04-BOLD",bold_selection);
    RUN("RENDER-05-ITALIC",italic_selection);
    RUN("RENDER-06-STRIKE",strike_selection);
    RUN("RENDER-07-INLINE-CODE-BACKTICK",inline_code_backtick);
    RUN("RENDER-08-LINK-LABEL",link_label);
    RUN("RENDER-09-LINK-DESTINATION",link_destination);
    RUN("RENDER-10-EXTERNAL-HTTPS-SOURCE",external_link_source_path);
    RUN("RENDER-11-LIST-ENTER",list_enter_serialization);
    RUN("RENDER-12-LIST-INDENT-OUTDENT",list_indent_outdent);
    RUN("RENDER-13-TASK-TOGGLE",task_toggle);
    RUN("RENDER-14-NESTED-QUOTE",nested_quote_edit);
    RUN("RENDER-15-FENCE-PUNCTUATION",fenced_code_edit);
    RUN("RENDER-16-FENCE-INFO",fence_info_edit);
    RUN("RENDER-17-IMAGE-DELETE-UNDO",image_delete_undo);
    RUN("RENDER-18-TABLE-CELL",table_cell_edit);
    RUN("RENDER-19-TABLE-ROW",table_row_add_delete);
    RUN("RENDER-20-TABLE-COLUMN",table_column_add_delete);
    RUN("RENDER-21-TABLE-ALIGNMENT",table_alignment_states);
    RUN("RENDER-22-TABLE-TAB-BOUNDARIES",table_tab_navigation_boundaries);
    RUN("RENDER-23-MODE-CYCLE",exact_mode_cycle);
    printf("ACCEPTANCE_MATRIX_SUMMARY total=%d passed=%d failed=%d skipped=0\n",passed+failed,passed,failed);
    return failed==0?0:1;
}
