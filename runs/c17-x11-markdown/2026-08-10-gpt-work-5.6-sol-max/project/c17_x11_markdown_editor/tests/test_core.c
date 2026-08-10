#include "mdedit/core.h"
#include "mdedit/diff.h"
#include "mdedit/document.h"
#include "mdedit/image.h"
#include "mdedit/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failed=0;
static int passed=0;

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); return false; } } while (0)
#define RUN(fn) do { if (fn()) { ++passed; printf("PASS %s\n",#fn); } else { ++failed; } } while (0)

static bool test_sha256(void) {
    uint8_t digest[32]; char hex[65];
    md_sha256("",0U,digest); md_hex_encode(digest,32U,hex);
    CHECK(strcmp(hex,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0);
    md_sha256("abc",3U,digest); md_hex_encode(digest,32U,hex);
    CHECK(strcmp(hex,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0);
    uint8_t decoded[32]; CHECK(md_hex_decode(hex,64U,decoded,32U)); CHECK(memcmp(digest,decoded,32U)==0);
    CHECK(md_crc32("123456789",9U)==UINT32_C(0xcbf43926)); return true;
}

static bool test_base64_vectors(void) {
    static const char *plain[]={"","f","fo","foo","foob","fooba","foobar"};
    static const char *encoded[]={"","Zg==","Zm8=","Zm9v","Zm9vYg==","Zm9vYmE=","Zm9vYmFy"};
    for (size_t i=0U;i<MD_ARRAY_LEN(plain);++i) {
        MdBuf out; md_buf_init(&out); CHECK(md_base64_encode((const uint8_t *)plain[i],strlen(plain[i]),&out)); CHECK(strcmp(out.data,encoded[i])==0);
        MdBytes bytes; md_bytes_init(&bytes); char error[128]; CHECK(md_base64_decode(out.data,out.len,&bytes,error,sizeof(error)));
        CHECK(bytes.len==strlen(plain[i])&&(bytes.len==0U||memcmp(bytes.data,plain[i],bytes.len)==0)); md_buf_free(&out); md_bytes_free(&bytes);
    }
    uint8_t all[256]; for (size_t i=0U;i<256U;++i) all[i]=(uint8_t)i;
    MdBuf out; md_buf_init(&out); MdBytes back; md_bytes_init(&back); char error[128];
    CHECK(md_base64_encode(all,sizeof(all),&out)); CHECK(md_base64_decode(out.data,out.len,&back,error,sizeof(error)));
    CHECK(back.len==sizeof(all)&&memcmp(back.data,all,sizeof(all))==0);
    static const char *invalid[]={"A","AAA","====","Z===","Zm$=","AA=A","AAA==","AA==AAAA","Zh==","Zm9="};
    for (size_t i=0U;i<MD_ARRAY_LEN(invalid);++i)
        CHECK(!md_base64_decode(invalid[i],strlen(invalid[i]),&back,error,sizeof(error)));
    for (size_t length=1U;length<=9U;++length) {
        uint8_t sample[9]; for (size_t i=0U;i<length;++i) sample[i]=(uint8_t)(i*29U+7U);
        out.len=0U; out.data[0]='\0'; back.len=0U;
        CHECK(md_base64_encode(sample,length,&out));
        CHECK(out.len==((length+2U)/3U)*4U);
        if (length%3U==1U) CHECK(out.data[out.len-1U]=='='&&out.data[out.len-2U]=='=');
        if (length%3U==2U) CHECK(out.data[out.len-1U]=='='&&out.data[out.len-2U]!='=');
        if (length%3U==0U) CHECK(out.data[out.len-1U]!='=');
        CHECK(md_base64_decode(out.data,out.len,&back,error,sizeof(error))&&
              back.len==length&&memcmp(back.data,sample,length)==0);
    }
    md_buf_free(&out); md_bytes_free(&back); return true;
}

static bool test_utf8_and_graphemes(void) {
    const char *text="A繁e\xCC\x81\xE2\x9C\x88\xEF\xB8\x8F\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB";
    size_t len=strlen(text),bad=0U; CHECK(md_utf8_validate(text,len,&bad)); CHECK(md_grapheme_count(text,len)==5U);
    size_t at=0U; for (unsigned i=0U;i<5U;++i) { size_t next=md_grapheme_next(text,len,at); CHECK(next>at); at=next; }
    CHECK(at==len); CHECK(md_grapheme_prev(text,len,len)>0U);
    const char invalid[]={ (char)0xc0,(char)0x80 }; CHECK(!md_utf8_validate(invalid,sizeof(invalid),&bad)&&bad==0U);
    return true;
}

static bool test_json_parser(void) {
    static const char json[]="{\"a\":[1,true,null,\"繁體\\n中文\"],\"emoji\":\"\\ud83d\\udc69\"}";
    MdJsonError error; MdJson *root=md_json_parse(json,sizeof(json)-1U,&error); CHECK(root!=NULL&&root->type==MD_JSON_OBJECT);
    const MdJson *a=md_json_get(root,"a"); CHECK(a!=NULL&&a->type==MD_JSON_ARRAY&&a->as.array.len==4U);
    CHECK(strcmp(md_json_string(md_json_get(root,"emoji")),"\xF0\x9F\x91\xA9")==0); md_json_free(root);
    root=md_json_parse("{\"a\":1,}",8U,&error); CHECK(root==NULL&&error.line==1U);
    root=md_json_parse("{\"a\":1,\"a\":2}",13U,&error); CHECK(root==NULL); return true;
}

static bool test_markdown_parser(void) {
    const char *source="# H1\nSetext\n---\n\n> quote\n- [x] done\n1. ordered\n\n```c\n# literal\n```\n\n| a | b |\n| :--- | ---: |\n| 1 | 2 |\n\n![alt](a.png)\n";
    MdRenderModel model; md_render_model_init(&model); char error[128];
    CHECK(md_markdown_parse(source,strlen(source),&model,error,sizeof(error)));
    CHECK(model.heading_count==2U); bool table=false,task=false,fence=false,image=false;
    for (size_t i=0U;i<model.block_count;++i) { table|=model.blocks[i].type==MD_BLOCK_TABLE; task|=model.blocks[i].type==MD_BLOCK_TASK_ITEM; fence|=model.blocks[i].type==MD_BLOCK_FENCED_CODE; image|=model.blocks[i].type==MD_BLOCK_IMAGE; }
    CHECK(table&&task&&fence&&image); md_render_model_free(&model); return true;
}

static bool test_markdown_corpus_matrix(void) {
    static const char *cases[]={
        "","plain text","繁體中文 only\n","English only 123\n","混合 English 42\n",
        "# H1\n## H2\n### H3\n#### H4\n##### H5\n###### H6\n",
        "Nested ***strong emphasis*** and escaped \\* punctuation.\n",
        "- outer\n  - inner\n10. ten\n11. eleven\n- [ ] open\n- [x] done\n",
        "> quote\n>> nested\n\n`**literal**`\n\n```text\n# not heading\n```\n",
        "[label](https://example.com \"title\") <https://example.com> ![alt](asset.png)\n",
        "| A | B |\n| :--- | ---: |\n| `a|b` | escaped \\| |\n",
        "**unterminated\n[broken](\n```unterminated\n| incomplete |\n<stray\n"
    };
    for (size_t i=0U;i<MD_ARRAY_LEN(cases);++i) {
        MdRenderModel model; md_render_model_init(&model); char error[256]={0};
        CHECK(md_markdown_parse(cases[i],strlen(cases[i]),&model,error,sizeof(error)));
        for (size_t b=0U;b<model.block_count;++b) CHECK(model.blocks[b].source_start<=model.blocks[b].source_end&&model.blocks[b].source_end<=strlen(cases[i]));
        MdBuf plain; md_buf_init(&plain); CHECK(md_markdown_plain_text(cases[i],strlen(cases[i]),&plain)); CHECK(md_utf8_validate(plain.data,plain.len,NULL));
        md_buf_free(&plain); md_render_model_free(&model);
    }
    return true;
}

static bool test_plain_text_and_statistics(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128];
    const char *source="# 標題\n\n**Hello** world 123.\n\n繁體中文。\n\n[link](https://example.com) ![alt](a.png)\n\n```c\nint x;\n```\n";
    CHECK(md_document_set_source(&doc,source,strlen(source),false,error,sizeof(error)));
    MdStatistics stats; md_statistics_compute(&doc,&stats);
    CHECK(stats.headings==1U&&stats.images==1U&&stats.links==1U&&stats.fenced_code_blocks==1U);
    CHECK(stats.words>=10U&&stats.raw_characters>stats.rendered_characters);
    MdBuf plain; md_buf_init(&plain); CHECK(md_markdown_plain_text(source,strlen(source),&plain));
    CHECK(strstr(plain.data,"Hello")!=NULL&&strstr(plain.data,"https://")==NULL); md_buf_free(&plain); md_document_free(&doc); return true;
}

static bool test_statistics_exact(void) {
    static const char source[]="# 中\n\nHello 123 中文\n\n![alt](a.bmp)\n\n[link](url)\n\n```c\nx\n```\n";
    MdDocument doc; md_document_init(&doc,1U); char error[128]; CHECK(md_document_set_source(&doc,source,sizeof(source)-1U,false,error,sizeof(error)));
    MdStatistics stats; md_statistics_compute(&doc,&stats);
    CHECK(stats.raw_characters==58U); CHECK(stats.rendered_characters==25U); CHECK(stats.words==8U);
    CHECK(stats.total_lines==11U&&stats.nonempty_lines==7U&&stats.paragraphs==2U);
    CHECK(stats.headings==1U&&stats.images==1U&&stats.links==1U&&stats.fenced_code_blocks==1U);
    md_document_free(&doc); return true;
}

static bool test_document_edit_undo_redo(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128];
    CHECK(md_document_insert_utf8(&doc,"Hello",5U,error,sizeof(error)));
    CHECK(md_document_insert_utf8(&doc," 繁體",7U,error,sizeof(error)));
    CHECK(doc.dirty&&doc.undo.len==1U);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&doc.source.len==0U);
    CHECK(md_document_redo(&doc,error,sizeof(error))&&strcmp(doc.source.data,"Hello 繁體")==0);
    CHECK(md_document_undo(&doc,error,sizeof(error))); CHECK(md_document_insert_utf8(&doc,"new",3U,error,sizeof(error))); CHECK(doc.redo.len==0U);
    md_document_free(&doc); return true;
}

static bool test_grapheme_deletion(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128]; const char *text="Ae\xCC\x81" "B";
    CHECK(md_document_set_source(&doc,text,strlen(text),false,error,sizeof(error))); doc.cursor=doc.anchor=strlen(text)-1U;
    CHECK(md_document_backspace(&doc,error,sizeof(error))); CHECK(strcmp(doc.source.data,"AB")==0);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.source.data,text)==0); md_document_free(&doc); return true;
}

static bool test_unicode_editing_units(void) {
    static const char *units[]={
        "e\xCC\x81",
        "\xE2\x9C\x88\xEF\xB8\x8F",
        "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD",
        "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB",
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"
    };
    for (size_t i=0U;i<MD_ARRAY_LEN(units);++i) {
        MdDocument doc; md_document_init(&doc,1U); char error[128]; MdBuf text; md_buf_init(&text);
        CHECK(md_buf_append_cstr(&text,"A")&&md_buf_append_cstr(&text,units[i])&&md_buf_append_cstr(&text,"B"));
        CHECK(md_grapheme_count(text.data,text.len)==3U);
        CHECK(md_document_set_source(&doc,text.data,text.len,false,error,sizeof(error)));
        doc.cursor=doc.anchor=text.len-1U; CHECK(md_document_backspace(&doc,error,sizeof(error)));
        CHECK(strcmp(doc.source.data,"AB")==0&&md_utf8_validate(doc.source.data,doc.source.len,NULL));
        CHECK(md_document_undo(&doc,error,sizeof(error))&&doc.source.len==text.len&&memcmp(doc.source.data,text.data,text.len)==0);
        doc.cursor=doc.anchor=1U; CHECK(md_document_delete(&doc,error,sizeof(error))); CHECK(strcmp(doc.source.data,"AB")==0);
        md_buf_free(&text); md_document_free(&doc);
    }
    return true;
}

static bool test_selection_drag_move(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128]={0};
    CHECK(md_document_set_source(&doc,"abc DEF ghi\n繁體 中文\n**styled** tail",strlen("abc DEF ghi\n繁體 中文\n**styled** tail"),false,error,sizeof(error)));
    doc.anchor=4U; doc.cursor=7U;
    CHECK(md_document_move_selection(&doc,11U,false,error,sizeof(error))&&strncmp(doc.source.data,"abc  ghiDEF",11U)==0);
    CHECK(doc.undo.len==1U&&md_document_undo(&doc,error,sizeof(error))&&strncmp(doc.source.data,"abc DEF ghi",11U)==0);
    CHECK(md_document_redo(&doc,error,sizeof(error))&&strncmp(doc.source.data,"abc  ghiDEF",11U)==0);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    doc.anchor=8U; doc.cursor=11U; CHECK(md_document_move_selection(&doc,0U,false,error,sizeof(error))&&strncmp(doc.source.data,"ghiabc DEF ",11U)==0);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    const char *han=strstr(doc.source.data,"繁體"); CHECK(han!=NULL); size_t han_at=(size_t)(han-doc.source.data);
    doc.anchor=han_at; doc.cursor=han_at+strlen("繁體"); CHECK(md_document_move_selection(&doc,doc.source.len,false,error,sizeof(error)));
    CHECK(strcmp(doc.source.data+doc.source.len-strlen("繁體"),"繁體")==0&&md_utf8_validate(doc.source.data,doc.source.len,NULL));
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    const char *styled=strstr(doc.source.data,"**styled**"); CHECK(styled!=NULL); size_t styled_at=(size_t)(styled-doc.source.data);
    doc.anchor=styled_at; doc.cursor=styled_at+strlen("**styled**"); CHECK(md_document_move_selection(&doc,0U,true,error,sizeof(error)));
    CHECK(strncmp(doc.source.data,"**styled**",strlen("**styled**"))==0&&doc.undo.len==1U);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    doc.anchor=4U; doc.cursor=7U; MdBuf unchanged; md_buf_init(&unchanged); CHECK(md_buf_assign(&unchanged,doc.source.data,doc.source.len));
    CHECK(md_document_move_selection(&doc,5U,false,error,sizeof(error))&&doc.source.len==unchanged.len&&memcmp(doc.source.data,unchanged.data,unchanged.len)==0);
    md_buf_free(&unchanged); md_document_free(&doc); return true;
}

static bool test_format_heading_task(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128];
    CHECK(md_document_set_source(&doc,"hello\n- [ ] task\n",17U,false,error,sizeof(error)));
    doc.anchor=0U; doc.cursor=5U; CHECK(md_document_format(&doc,"**","**","Bold",error,sizeof(error))); CHECK(strncmp(doc.source.data,"**hello**",9U)==0);
    doc.cursor=2U; doc.anchor=2U; CHECK(md_document_heading_level(&doc,3,error,sizeof(error))); CHECK(strncmp(doc.source.data,"### **hello**",12U)==0);
    size_t task=(size_t)(strstr(doc.source.data,"task")-doc.source.data); CHECK(md_document_toggle_task(&doc,task,error,sizeof(error))); CHECK(strstr(doc.source.data,"[x]")!=NULL);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&strstr(doc.source.data,"[ ]")!=NULL); md_document_free(&doc); return true;
}

static bool test_rendered_source_transactions(void) {
    static const char original[]="# Heading\n\nParagraph [label](https://old.example/a(b)) text.\n\n- [ ] task\n";
    MdDocument doc; md_document_init(&doc,1U); char error[256]={0};
    CHECK(md_document_set_source(&doc,original,sizeof(original)-1U,false,error,sizeof(error)));
    doc.mode=MD_MODE_RENDERED;
    const char *heading=strstr(doc.source.data,"Heading"); CHECK(heading!=NULL); size_t h=(size_t)(heading-doc.source.data);
    CHECK(md_document_replace(&doc,h,h+7U,"Changed",7U,"Rendered paragraph",false,error,sizeof(error)));
    CHECK(strncmp(doc.source.data,"# Changed",9U)==0&&md_document_undo(&doc,error,sizeof(error))&&memcmp(doc.source.data,original,sizeof(original)-1U)==0);
    const char *label=strstr(doc.source.data,"label"); CHECK(label!=NULL); size_t link=(size_t)(label-doc.source.data);
    MdBuf link_label,link_destination; md_buf_init(&link_label); md_buf_init(&link_destination); CHECK(md_document_link_at(&doc,link,&link_label,&link_destination)); CHECK(strcmp(link_label.data,"label")==0&&strcmp(link_destination.data,"https://old.example/a(b)")==0); md_buf_free(&link_label); md_buf_free(&link_destination);
    CHECK(md_document_edit_link(&doc,link,"shown",NULL,error,sizeof(error))&&strstr(doc.source.data,"[shown](https://old.example/a(b))")!=NULL);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    CHECK(md_document_edit_link(&doc,link,NULL,"https://new.example/path",error,sizeof(error))&&strstr(doc.source.data,"[label](https://new.example/path)")!=NULL);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    const char *task=strstr(doc.source.data,"task"); CHECK(task!=NULL); CHECK(md_document_toggle_task(&doc,(size_t)(task-doc.source.data),error,sizeof(error))&&strstr(doc.source.data,"[x]")!=NULL);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&memcmp(doc.source.data,original,sizeof(original)-1U)==0);
    MdBuf stable; md_buf_init(&stable); CHECK(md_buf_assign(&stable,doc.source.data,doc.source.len));
    for (int mode=0;mode<4;++mode) doc.mode=(MdEditorMode)mode;
    CHECK(doc.source.len==stable.len&&memcmp(doc.source.data,stable.data,stable.len)==0);
    static const char titled[]="[shown](<https://example.com/a b> \"Preserved title\")";
    CHECK(md_document_set_source(&doc,titled,sizeof(titled)-1U,false,error,sizeof(error)));
    MdBuf titled_label,titled_destination; md_buf_init(&titled_label); md_buf_init(&titled_destination);
    CHECK(md_document_link_at(&doc,2U,&titled_label,&titled_destination)&&strcmp(titled_destination.data,"https://example.com/a b")==0);
    CHECK(md_document_edit_link(&doc,2U,"renamed",NULL,error,sizeof(error))&&strcmp(doc.source.data,"[renamed](<https://example.com/a b> \"Preserved title\")")==0);
    CHECK(md_document_edit_link(&doc,2U,NULL,"https://new.example/x",error,sizeof(error))&&strcmp(doc.source.data,"[renamed](<https://new.example/x> \"Preserved title\")")==0);
    md_buf_free(&titled_label); md_buf_free(&titled_destination);
    md_buf_free(&stable); md_document_free(&doc); return true;
}

static bool test_table_operations(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128]; const char *table="| A | B |\n| --- | :---: |\n| 1 | 2 |\n";
    CHECK(md_document_set_source(&doc,table,strlen(table),false,error,sizeof(error)));
    CHECK(md_document_table_action(&doc,1U,1U,0U,MD_TABLE_ROW_BELOW,error,sizeof(error)));
    CHECK(md_document_table_action(&doc,1U,1U,0U,MD_TABLE_COL_AFTER,error,sizeof(error)));
    CHECK(md_document_table_action(&doc,1U,1U,1U,MD_TABLE_ALIGN_RIGHT,error,sizeof(error)));
    CHECK(strstr(doc.source.data,"---:")!=NULL); CHECK(md_document_undo(&doc,error,sizeof(error)));
    CHECK(md_document_table_set_cell(&doc,1U,1U,0U,"繁體 cell",strlen("繁體 cell"),error,sizeof(error)));
    CHECK(strstr(doc.source.data,"繁體 cell")!=NULL&&md_document_undo(&doc,error,sizeof(error)));
    CHECK(md_document_table_set_cell(&doc,1U,1U,0U,"pipe | value",strlen("pipe | value"),error,sizeof(error)));
    CHECK(strstr(doc.source.data,"pipe \\| value")!=NULL&&doc.render.block_count==1U&&doc.render.blocks[0].type==MD_BLOCK_TABLE);
    CHECK(md_document_undo(&doc,error,sizeof(error)));
    static const MdTableAction alignments[]={MD_TABLE_ALIGN_DEFAULT,MD_TABLE_ALIGN_LEFT,MD_TABLE_ALIGN_CENTER,MD_TABLE_ALIGN_RIGHT};
    static const char *markers[]={"| --- |", "| :--- |", "| :---: |", "| ---: |"};
    for (size_t i=0U;i<MD_ARRAY_LEN(alignments);++i) {
        CHECK(md_document_table_action(&doc,1U,1U,0U,alignments[i],error,sizeof(error)));
        CHECK(strstr(doc.source.data,markers[i])!=NULL);
    }
    CHECK(md_document_table_action(&doc,1U,1U,1U,MD_TABLE_COL_DELETE,error,sizeof(error)));
    CHECK(md_document_table_action(&doc,1U,1U,0U,MD_TABLE_ROW_DELETE,error,sizeof(error)));
    md_document_free(&doc); return true;
}

static bool test_search_replace(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128];
    CHECK(md_document_set_source(&doc,"Cat cat category 搜尋 搜尋\n",strlen("Cat cat category 搜尋 搜尋\n"),false,error,sizeof(error)));
    MdSearchResults results; md_search_results_init(&results);
    CHECK(md_document_find(&doc,"cat",false,true,&results)&&results.count==2U);
    CHECK(md_document_find(&doc,"搜尋",true,true,&results)&&results.count==2U);
    size_t replaced=0U; CHECK(md_document_replace_all(&doc,"搜尋","搜尋搜尋",true,false,&replaced,error,sizeof(error))&&replaced==2U);
    CHECK(strstr(doc.source.data,"搜尋搜尋 搜尋搜尋")!=NULL); CHECK(md_document_undo(&doc,error,sizeof(error)));
    md_search_results_free(&results); md_document_free(&doc); return true;
}

static bool test_search_after_edit_and_nonrecursive_replace(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128]={0};
    CHECK(md_document_set_source(&doc,"One one stone 搜尋x 搜尋 a a a",strlen("One one stone 搜尋x 搜尋 a a a"),false,error,sizeof(error)));
    MdSearchResults results; md_search_results_init(&results);
    CHECK(md_document_find(&doc,"one",false,true,&results)&&results.count==2U);
    CHECK(md_document_find(&doc,"One",true,true,&results)&&results.count==1U);
    CHECK(md_document_find(&doc,"搜尋",true,true,&results)&&results.count==1U);
    doc.cursor=doc.anchor=doc.source.len; CHECK(md_document_insert_utf8(&doc," one",4U,error,sizeof(error)));
    CHECK(md_document_find(&doc,"one",false,true,&results)&&results.count==3U);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&md_document_find(&doc,"one",false,true,&results)&&results.count==2U);
    size_t replaced=0U; CHECK(md_document_replace_all(&doc,"a","aa",true,true,&replaced,error,sizeof(error))&&replaced==3U);
    CHECK(strstr(doc.source.data,"aa aa aa")!=NULL&&doc.undo.len==1U);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&strstr(doc.source.data,"a a a")!=NULL);
    md_search_results_free(&results); md_document_free(&doc); return true;
}

static bool test_myers_diff(void) {
    MdDiff diff; md_diff_init(&diff); char error[128]={0};
    CHECK(md_diff_lines("a\nb\nc\n",6U,"a\nB\nc\nd\n",8U,&diff,error,sizeof(error)));
    bool inserted=false,deleted=false; for (size_t i=0U;i<diff.count;++i) { inserted|=diff.hunks[i].kind==MD_DIFF_INSERT; deleted|=diff.hunks[i].kind==MD_DIFF_DELETE; }
    CHECK(inserted&&deleted); md_diff_free(&diff); return true;
}

static bool test_diff_matrix_and_determinism(void) {
    static const struct { const char *a; const char *b; } cases[]={
        {"",""},{"","one\n"},{"one\n",""},{"a\nb\n","a\nX\nb\n"},{"a\nX\nb\n","a\nb\n"},
        {"one long source line with word\n","one long target line with WORD\n"},
        {"a\nb\nc\nd\n","A\nb\nc\nD\n"},{"中文內容\n","中文修改\n"},
        {"same\nrepeat\nsame\nrepeat\n","same\nrepeat\nchanged\nrepeat\n"}
    };
    char error[128]={0};
    for (size_t c=0U;c<MD_ARRAY_LEN(cases);++c) {
        MdDiff first,second; md_diff_init(&first); md_diff_init(&second);
        CHECK(md_diff_lines(cases[c].a,strlen(cases[c].a),cases[c].b,strlen(cases[c].b),&first,error,sizeof(error)));
        CHECK(md_diff_lines(cases[c].a,strlen(cases[c].a),cases[c].b,strlen(cases[c].b),&second,error,sizeof(error)));
        CHECK(first.count==second.count);
        for (size_t i=0U;i<first.count;++i) {
            CHECK(first.hunks[i].kind==second.hunks[i].kind);
            CHECK(first.hunks[i].a_start==second.hunks[i].a_start&&first.hunks[i].a_count==second.hunks[i].a_count);
            CHECK(first.hunks[i].b_start==second.hunks[i].b_start&&first.hunks[i].b_count==second.hunks[i].b_count);
        }
        md_diff_free(&first); md_diff_free(&second);
    }
    MdBuf a,b; md_buf_init(&a); md_buf_init(&b);
    for (size_t i=0U;i<600U;++i) { CHECK(md_buf_appendf(&a,"repeated %zu %s\n",i%7U,i==310U?"old":"same")); CHECK(md_buf_appendf(&b,"repeated %zu %s\n",i%7U,i==310U?"new":"same")); }
    MdDiff large; md_diff_init(&large); CHECK(md_diff_lines(a.data,a.len,b.data,b.len,&large,error,sizeof(error)));
    bool changed=false; for (size_t i=0U;i<large.count;++i) changed|=large.hunks[i].kind!=MD_DIFF_EQUAL; CHECK(changed);
    md_diff_free(&large); md_buf_free(&a); md_buf_free(&b); return true;
}

static bool test_token_diff(void) {
    const char *a="hello 繁體",*b="hello world 繁體"; MdDiff diff; md_diff_init(&diff); char error[128]={0};
    CHECK(md_diff_tokens(a,strlen(a),b,strlen(b),&diff,error,sizeof(error))); bool inserted=false;
    for (size_t i=0U;i<diff.count;++i) inserted|=diff.hunks[i].kind==MD_DIFF_INSERT;
    CHECK(inserted); md_diff_free(&diff); return true;
}

static bool test_delta_roundtrip(void) {
    const char *a="one\ntwo\nthree\n",*b="zero\none\nTWO\nthree\n四\n"; MdBytes delta; md_bytes_init(&delta); MdBuf result; md_buf_init(&result); char error[128]={0};
    CHECK(md_delta_encode(a,strlen(a),b,strlen(b),&delta,error,sizeof(error)));
    CHECK(md_delta_apply(a,strlen(a),delta.data,delta.len,&result,error,sizeof(error)));
    CHECK(result.len==strlen(b)&&memcmp(result.data,b,result.len)==0);
    if (delta.len>5U) { delta.data[5U]^=0xffU; CHECK(!md_delta_apply(a,strlen(a),delta.data,delta.len,&result,error,sizeof(error))); }
    md_bytes_free(&delta); md_buf_free(&result); return true;
}

static bool test_lzss(void) {
    MdBuf input; md_buf_init(&input); for (unsigned i=0U;i<1000U;++i) CHECK(md_buf_append_cstr(&input,"abcabcabc-繁體-"));
    MdBytes compressed; md_bytes_init(&compressed); MdBytes output; md_bytes_init(&output); char error[128];
    CHECK(md_lzss_compress((const uint8_t *)input.data,input.len,&compressed)); CHECK(compressed.len<input.len);
    CHECK(md_lzss_decompress(compressed.data,compressed.len,input.len,&output,error,sizeof(error)));
    CHECK(output.len==input.len&&memcmp(output.data,input.data,input.len)==0);
    CHECK(!md_lzss_decompress(compressed.data,compressed.len-1U,input.len,&output,error,sizeof(error)));
    static const uint8_t invalid_distance[]={1U,0U,0U}; CHECK(!md_lzss_decompress(invalid_distance,sizeof(invalid_distance),3U,&output,error,sizeof(error)));
    static const uint8_t trailing[]={0U,'a','x'}; CHECK(!md_lzss_decompress(trailing,sizeof(trailing),1U,&output,error,sizeof(error)));
    static const uint8_t truncated_match[]={1U,0U}; CHECK(!md_lzss_decompress(truncated_match,sizeof(truncated_match),3U,&output,error,sizeof(error)));
    md_buf_free(&input); md_bytes_free(&compressed); md_bytes_free(&output); return true;
}

static bool test_outline_mapping_and_duplicate_identity(void) {
    static const char source[]="# Same\ntext\n## Child\n# Same\n"; MdDocument doc; md_document_init(&doc,1U); char error[128];
    CHECK(md_document_set_source(&doc,source,sizeof(source)-1U,false,error,sizeof(error))&&doc.render.heading_count==3U);
    CHECK(strcmp(doc.render.headings[0].label,"Same")==0&&strcmp(doc.render.headings[2].label,"Same")==0);
    CHECK(doc.render.headings[0].source_offset!=doc.render.headings[2].source_offset&&doc.render.headings[1].level==2);
    size_t second=doc.render.headings[2].source_offset; const char *label=doc.source.data+second+2U;
    CHECK(md_document_replace(&doc,(size_t)(label-doc.source.data),(size_t)(label-doc.source.data)+4U,"Other",5U,"Heading rendered edit",false,error,sizeof(error)));
    CHECK(doc.render.heading_count==3U&&strcmp(doc.render.headings[0].label,"Same")==0&&strcmp(doc.render.headings[2].label,"Other")==0);
    CHECK(md_document_undo(&doc,error,sizeof(error))&&strcmp(doc.render.headings[2].label,"Same")==0);
    md_document_free(&doc); return true;
}

static void put_u16(uint8_t *p,uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); }
static void put_u32(uint8_t *p,uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8U); p[2]=(uint8_t)(v>>16U); p[3]=(uint8_t)(v>>24U); }

static bool test_image_and_data_uri(void) {
    uint8_t bmp[70]; memset(bmp,0,sizeof(bmp)); bmp[0]='B'; bmp[1]='M'; put_u32(bmp+2U,70U); put_u32(bmp+10U,54U); put_u32(bmp+14U,40U); put_u32(bmp+18U,2U); put_u32(bmp+22U,2U); put_u16(bmp+26U,1U); put_u16(bmp+28U,24U); put_u32(bmp+34U,16U);
    for (size_t i=54U;i<70U;++i) bmp[i]=(uint8_t)(i*3U);
    MdImage image; md_image_init(&image); char error[128]; CHECK(md_image_decode(bmp,sizeof(bmp),&image,error,sizeof(error))); CHECK(image.width==2U&&image.height==2U); md_image_free(&image);
    MdBuf uri; md_buf_init(&uri); CHECK(md_image_make_data_uri(MD_IMAGE_BMP,bmp,sizeof(bmp),&uri));
    MdBytes bytes; md_bytes_init(&bytes); MdImageFormat format; CHECK(md_image_parse_data_uri(uri.data,uri.len,&format,&bytes,error,sizeof(error))); CHECK(format==MD_IMAGE_BMP&&bytes.len==sizeof(bmp));
    char template_path[]="/tmp/mdeditor-images-XXXXXX"; char *root=mkdtemp(template_path); CHECK(root!=NULL);
    char png_path[MD_PATH_MAX],jpeg_path[MD_PATH_MAX]; CHECK(md_path_join(png_path,root,"valid.png")&&md_path_join(jpeg_path,root,"valid.jpg"));
    uint8_t rgba[4U*3U*4U]; for (size_t i=0U;i<12U;++i) { rgba[i*4U]=(uint8_t)(i*17U); rgba[i*4U+1U]=(uint8_t)(255U-i*11U); rgba[i*4U+2U]=(uint8_t)(40U+i*5U); rgba[i*4U+3U]=255U; }
    CHECK(md_image_write_png(png_path,rgba,4U,3U,error,sizeof(error)));
    CHECK(md_image_write_jpeg(jpeg_path,rgba,4U,3U,90,error,sizeof(error)));
    MdBytes original; md_bytes_init(&original);
    CHECK(md_image_load(png_path,&image,&original,error,sizeof(error))&&image.format==MD_IMAGE_PNG&&image.width==4U&&image.height==3U&&memcmp(image.rgba,rgba,sizeof(rgba))==0);
    CHECK(md_image_make_data_uri(MD_IMAGE_PNG,original.data,original.len,&uri)); bytes.len=0U;
    CHECK(md_image_parse_data_uri(uri.data,uri.len,&format,&bytes,error,sizeof(error))&&format==MD_IMAGE_PNG);
    md_image_free(&image); md_bytes_free(&original); md_bytes_init(&original);
    CHECK(md_image_load(jpeg_path,&image,&original,error,sizeof(error))&&image.format==MD_IMAGE_JPEG&&image.width==4U&&image.height==3U);
    CHECK(md_image_make_data_uri(MD_IMAGE_JPEG,original.data,original.len,&uri)); bytes.len=0U;
    CHECK(md_image_parse_data_uri(uri.data,uri.len,&format,&bytes,error,sizeof(error))&&format==MD_IMAGE_JPEG);
    static const uint8_t corrupt_png[]={137U,80U,78U,71U,13U,10U,26U,10U,0U};
    static const uint8_t corrupt_jpeg[]={0xffU,0xd8U,0xffU,0xe0U,0U};
    static const uint8_t corrupt_bmp[]={'B','M',0U,0U};
    CHECK(!md_image_decode(corrupt_png,sizeof(corrupt_png),&image,error,sizeof(error)));
    CHECK(!md_image_decode(corrupt_jpeg,sizeof(corrupt_jpeg),&image,error,sizeof(error)));
    CHECK(!md_image_decode(corrupt_bmp,sizeof(corrupt_bmp),&image,error,sizeof(error)));
    md_image_free(&image); md_buf_free(&uri); md_bytes_free(&bytes); md_bytes_free(&original);
    CHECK(unlink(png_path)==0&&unlink(jpeg_path)==0&&rmdir(root)==0); return true;
}

static bool test_line_column(void) {
    MdDocument doc; md_document_init(&doc,1U); char error[128]; const char *s="ab\n繁體x\n";
    CHECK(md_document_set_source(&doc,s,strlen(s),false,error,sizeof(error))); size_t x=(size_t)(strchr(s,'x')-s);
    CHECK(md_document_line_for_offset(&doc,x)==2U&&md_document_column_for_offset(&doc,x)==3U); md_document_free(&doc); return true;
}

int main(void) {
    RUN(test_sha256); RUN(test_base64_vectors); RUN(test_utf8_and_graphemes); RUN(test_json_parser);
    RUN(test_markdown_parser); RUN(test_markdown_corpus_matrix); RUN(test_plain_text_and_statistics); RUN(test_statistics_exact); RUN(test_document_edit_undo_redo);
    RUN(test_grapheme_deletion); RUN(test_unicode_editing_units); RUN(test_selection_drag_move);
    RUN(test_format_heading_task); RUN(test_rendered_source_transactions); RUN(test_table_operations); RUN(test_search_replace); RUN(test_search_after_edit_and_nonrecursive_replace);
    RUN(test_myers_diff); RUN(test_diff_matrix_and_determinism); RUN(test_token_diff); RUN(test_delta_roundtrip); RUN(test_lzss); RUN(test_outline_mapping_and_duplicate_identity); RUN(test_image_and_data_uri); RUN(test_line_column);
    printf("TEST_SUMMARY total=%d passed=%d failed=%d skipped=0\n",passed+failed,passed,failed);
    return failed==0?0:1;
}
