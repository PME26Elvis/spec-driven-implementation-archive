#include "mdedit/document.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void doc_error(char *error, size_t cap, const char *message) {
    if (error != NULL && cap != 0U) (void)snprintf(error, cap, "%s", message);
}

static bool grow_array(void **items, size_t *cap, size_t needed, size_t item_size) {
    if (needed <= *cap) return true;
    size_t next = *cap == 0U ? 16U : *cap;
    while (next < needed) {
        if (next > SIZE_MAX / 2U) return false;
        next *= 2U;
    }
    size_t bytes = 0U;
    if (!md_size_mul(next, item_size, &bytes)) return false;
    void *p = realloc(*items, bytes);
    if (p == NULL) return false;
    *items = p;
    *cap = next;
    return true;
}

void md_render_model_init(MdRenderModel *model) {
    memset(model, 0, sizeof(*model));
}

void md_render_model_free(MdRenderModel *model) {
    for (size_t i = 0U; i < model->heading_count; ++i) free(model->headings[i].label);
    free(model->headings);
    free(model->blocks);
    uint64_t generation = model->generation;
    memset(model, 0, sizeof(*model));
    model->generation = generation;
}

static bool model_block(MdRenderModel *model, MdBlock block) {
    if (!grow_array((void **)&model->blocks, &model->block_cap,
                    model->block_count + 1U, sizeof(*model->blocks))) return false;
    model->blocks[model->block_count++] = block;
    return true;
}

static bool inline_plain(const char *source, size_t start, size_t end, MdBuf *out);

static bool model_heading(MdRenderModel *model, const char *source,
                          const MdBlock *block) {
    MdBuf label;
    md_buf_init(&label);
    if (!inline_plain(source, block->content_start, block->content_end, &label)) {
        md_buf_free(&label);
        return false;
    }
    if (label.len == 0U && !md_buf_append_cstr(&label, "(empty heading)")) {
        md_buf_free(&label);
        return false;
    }
    if (!grow_array((void **)&model->headings, &model->heading_cap,
                    model->heading_count + 1U, sizeof(*model->headings))) {
        md_buf_free(&label);
        return false;
    }
    MdHeading *h = &model->headings[model->heading_count++];
    h->label = label.data;
    h->source_offset = block->source_start;
    h->level = block->level;
    h->block_index = model->block_count - 1U;
    return true;
}

typedef struct {
    size_t start;
    size_t content_end;
    size_t end;
    int number;
} SourceLine;

static SourceLine source_line(const char *source, size_t len, size_t start, int number) {
    SourceLine line = {start, start, start, number};
    while (line.content_end < len && source[line.content_end] != '\n') ++line.content_end;
    line.end = line.content_end < len ? line.content_end + 1U : line.content_end;
    if (line.content_end > start && source[line.content_end - 1U] == '\r') --line.content_end;
    return line;
}

static size_t line_indent(const char *s, size_t start, size_t end) {
    size_t i = start;
    size_t spaces = 0U;
    while (i < end && spaces < 4U) {
        if (s[i] == ' ') { ++spaces; ++i; }
        else if (s[i] == '\t') { spaces = 4U; ++i; }
        else break;
    }
    return i;
}

static bool line_blank(const char *s, size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r') return false;
    }
    return true;
}

static bool line_thematic(const char *s, size_t start, size_t end) {
    size_t p = line_indent(s, start, end);
    if (p >= end || (s[p] != '*' && s[p] != '-' && s[p] != '_')) return false;
    char mark = s[p];
    size_t count = 0U;
    for (; p < end; ++p) {
        if (s[p] == mark) ++count;
        else if (s[p] != ' ' && s[p] != '\t') return false;
    }
    return count >= 3U;
}

static bool line_table_separator(const char *s, size_t start, size_t end) {
    while (start<end&&(s[start]==' '||s[start]=='\t')) ++start;
    while (end>start&&(s[end-1U]==' '||s[end-1U]=='\t'||s[end-1U]=='\r')) --end;
    if (start<end&&s[start]=='|') ++start;
    if (end>start&&s[end-1U]=='|') --end;
    bool saw_pipe=false,saw_cell=false;
    size_t cell=start;
    for (size_t p=start;p<=end;++p) {
        if (p<end&&s[p]!='|') continue;
        if (p<end) saw_pipe=true;
        size_t a=cell,b=p;
        while (a<b&&(s[a]==' '||s[a]=='\t')) ++a;
        while (b>a&&(s[b-1U]==' '||s[b-1U]=='\t')) --b;
        if (a<b&&s[a]==':') ++a;
        if (b>a&&s[b-1U]==':') --b;
        size_t dashes=0U;
        while (a<b&&s[a]=='-') { ++a; ++dashes; }
        if (a!=b||dashes<3U) return false;
        saw_cell=true; cell=p+1U;
    }
    return saw_pipe&&saw_cell;
}

static bool line_might_start_block(const char *s, size_t start, size_t end) {
    size_t p = line_indent(s, start, end);
    if (p >= end) return true;
    if (s[p] == '#' || s[p] == '>' || s[p] == '`' || s[p] == '~' || s[p] == '<') return true;
    if ((s[p] == '-' || s[p] == '*' || s[p] == '+') && p + 1U < end &&
        (s[p+1U] == ' ' || s[p+1U] == '\t')) return true;
    if (isdigit((unsigned char)s[p])) {
        while (p < end && isdigit((unsigned char)s[p])) ++p;
        if (p + 1U < end && (s[p] == '.' || s[p] == ')') &&
            (s[p+1U] == ' ' || s[p+1U] == '\t')) return true;
    }
    return line_thematic(s, start, end);
}

static bool parse_atx(const char *s, SourceLine line, MdBlock *block) {
    size_t p = line_indent(s, line.start, line.content_end);
    size_t hashes = 0U;
    while (p + hashes < line.content_end && s[p + hashes] == '#' && hashes < 7U) ++hashes;
    if (hashes == 0U || hashes > 6U ||
        (p + hashes < line.content_end && s[p + hashes] != ' ' && s[p + hashes] != '\t')) return false;
    size_t content = p + hashes;
    while (content < line.content_end && (s[content] == ' ' || s[content] == '\t')) ++content;
    size_t content_end = line.content_end;
    while (content_end > content && (s[content_end-1U] == ' ' || s[content_end-1U] == '\t')) --content_end;
    size_t closing = content_end;
    while (closing > content && s[closing-1U] == '#') --closing;
    if (closing < content_end && closing > content &&
        (s[closing-1U] == ' ' || s[closing-1U] == '\t')) {
        content_end = closing - 1U;
        while (content_end > content && (s[content_end-1U] == ' ' || s[content_end-1U] == '\t')) --content_end;
    }
    *block = (MdBlock){MD_BLOCK_HEADING, line.start, line.end, content,
                       content_end, (int)hashes, line.number, line.number, false};
    return true;
}

static bool parse_list(const char *s, SourceLine line, MdBlock *block) {
    size_t p = line_indent(s, line.start, line.content_end);
    size_t indent = p - line.start;
    MdBlockType type;
    size_t marker_end = p;
    if (p < line.content_end && (s[p] == '-' || s[p] == '*' || s[p] == '+') &&
        p + 1U < line.content_end && (s[p+1U] == ' ' || s[p+1U] == '\t')) {
        type = MD_BLOCK_UL_ITEM;
        marker_end = p + 2U;
    } else if (p < line.content_end && isdigit((unsigned char)s[p])) {
        while (marker_end < line.content_end && isdigit((unsigned char)s[marker_end])) ++marker_end;
        if (marker_end + 1U >= line.content_end ||
            (s[marker_end] != '.' && s[marker_end] != ')') ||
            (s[marker_end+1U] != ' ' && s[marker_end+1U] != '\t')) return false;
        type = MD_BLOCK_OL_ITEM;
        marker_end += 2U;
    } else return false;
    while (marker_end < line.content_end && (s[marker_end] == ' ' || s[marker_end] == '\t')) ++marker_end;
    bool checked = false;
    if (type == MD_BLOCK_UL_ITEM && marker_end + 3U <= line.content_end &&
        s[marker_end] == '[' && (s[marker_end+1U] == ' ' || s[marker_end+1U] == 'x' || s[marker_end+1U] == 'X') &&
        s[marker_end+2U] == ']') {
        checked = s[marker_end+1U] != ' ';
        type = MD_BLOCK_TASK_ITEM;
        marker_end += 3U;
        if (marker_end < line.content_end && s[marker_end] == ' ') ++marker_end;
    }
    *block = (MdBlock){type, line.start, line.end, marker_end, line.content_end,
                       (int)(indent / 2U), line.number, line.number, checked};
    return true;
}

static bool parse_quote(const char *s, SourceLine line, MdBlock *block) {
    size_t p = line_indent(s, line.start, line.content_end);
    if (p >= line.content_end || s[p] != '>') return false;
    int level = 0;
    while (p < line.content_end && s[p] == '>') {
        ++level; ++p;
        if (p < line.content_end && s[p] == ' ') ++p;
    }
    *block = (MdBlock){MD_BLOCK_QUOTE, line.start, line.end, p, line.content_end,
                       level, line.number, line.number, false};
    return true;
}

static bool parse_fence_start(const char *s, SourceLine line, char *mark,
                              size_t *run, size_t *content) {
    size_t p = line_indent(s, line.start, line.content_end);
    if (p >= line.content_end || (s[p] != '`' && s[p] != '~')) return false;
    *mark = s[p];
    size_t n = 0U;
    while (p + n < line.content_end && s[p+n] == *mark) ++n;
    if (n < 3U) return false;
    *run = n;
    *content = p + n;
    while (*content < line.content_end && s[*content] == ' ') ++*content;
    return true;
}

static bool fence_close(const char *s, SourceLine line, char mark, size_t run) {
    size_t p = line_indent(s, line.start, line.content_end);
    size_t n = 0U;
    while (p + n < line.content_end && s[p+n] == mark) ++n;
    if (n < run) return false;
    p += n;
    while (p < line.content_end && (s[p] == ' ' || s[p] == '\t')) ++p;
    return p == line.content_end;
}

static void trim_content(const char *s, size_t *start, size_t *end) {
    while (*start < *end && (s[*start] == ' ' || s[*start] == '\t')) ++*start;
    while (*end > *start && (s[*end-1U] == ' ' || s[*end-1U] == '\t' || s[*end-1U] == '\r')) --*end;
}

bool md_markdown_parse(const char *source, size_t len, MdRenderModel *model,
                       char *error, size_t error_cap) {
    size_t bad = 0U;
    if (!md_utf8_validate(source, len, &bad)) {
        if (error != NULL && error_cap != 0U)
            (void)snprintf(error, error_cap, "Invalid UTF-8 at byte %zu", bad);
        return false;
    }
    uint64_t generation = model->generation + 1U;
    md_render_model_free(model);
    model->generation = generation;
    size_t at = 0U;
    int line_no = 1;
    while (at < len) {
        SourceLine line = source_line(source, len, at, line_no);
        MdBlock block;
        if (line_blank(source, line.start, line.content_end)) {
            block = (MdBlock){MD_BLOCK_BLANK,line.start,line.end,line.start,line.content_end,0,line_no,line_no,false};
            if (!model_block(model, block)) goto oom;
            at = line.end; ++line_no; continue;
        }
        char fence_mark = '\0';
        size_t fence_run = 0U, info_start = 0U;
        if (parse_fence_start(source, line, &fence_mark, &fence_run, &info_start)) {
            size_t end = line.end;
            size_t content_start = line.end;
            size_t content_end = line.end;
            int end_line = line_no;
            size_t p = line.end;
            while (p < len) {
                SourceLine candidate = source_line(source, len, p, end_line + 1);
                if (fence_close(source, candidate, fence_mark, fence_run)) {
                    content_end = candidate.start;
                    end = candidate.end;
                    end_line = candidate.number;
                    break;
                }
                content_end = candidate.end;
                end = candidate.end;
                end_line = candidate.number;
                p = candidate.end;
            }
            block = (MdBlock){MD_BLOCK_FENCED_CODE,line.start,end,content_start,content_end,
                              (int)fence_run,line_no,end_line,false};
            if (!model_block(model, block)) goto oom;
            at = end; line_no = end_line + 1; continue;
        }
        if (parse_atx(source, line, &block)) {
            if (!model_block(model, block) || !model_heading(model, source, &block)) goto oom;
            at = line.end; ++line_no; continue;
        }
        if (line.end < len) {
            SourceLine next = source_line(source, len, line.end, line_no + 1);
            size_t p = line_indent(source, next.start, next.content_end);
            size_t n = 0U;
            char mark = p < next.content_end ? source[p] : '\0';
            if (mark == '=' || mark == '-') {
                while (p+n < next.content_end && source[p+n] == mark) ++n;
                size_t q = p+n;
                while (q < next.content_end && (source[q] == ' ' || source[q] == '\t')) ++q;
                if (n >= 1U && q == next.content_end && !line_blank(source,line.start,line.content_end)) {
                    size_t cs=line.start, ce=line.content_end; trim_content(source,&cs,&ce);
                    block=(MdBlock){MD_BLOCK_SETEXT_HEADING,line.start,next.end,cs,ce,
                                    mark=='='?1:2,line_no,line_no+1,false};
                    if (!model_block(model,block) || !model_heading(model,source,&block)) goto oom;
                    at=next.end; line_no+=2; continue;
                }
            }
            if (memchr(source + line.start, '|', line.content_end - line.start) != NULL &&
                line_table_separator(source, next.start, next.content_end)) {
                size_t end = next.end;
                int end_line = next.number;
                size_t p2 = next.end;
                while (p2 < len) {
                    SourceLine row = source_line(source,len,p2,end_line+1);
                    if (line_blank(source,row.start,row.content_end) ||
                        memchr(source+row.start,'|',row.content_end-row.start)==NULL) break;
                    end=row.end; end_line=row.number; p2=row.end;
                }
                block=(MdBlock){MD_BLOCK_TABLE,line.start,end,line.start,end,0,line_no,end_line,false};
                if (!model_block(model,block)) goto oom;
                at=end; line_no=end_line+1; continue;
            }
        }
        if (parse_quote(source,line,&block) || parse_list(source,line,&block)) {
            if (!model_block(model,block)) goto oom;
            at=line.end; ++line_no; continue;
        }
        if (line_thematic(source,line.start,line.content_end)) {
            block=(MdBlock){MD_BLOCK_THEMATIC,line.start,line.end,line.start,line.content_end,0,line_no,line_no,false};
            if (!model_block(model,block)) goto oom;
            at=line.end; ++line_no; continue;
        }
        size_t indent_end=line_indent(source,line.start,line.content_end);
        if (indent_end-line.start>=4U) {
            block=(MdBlock){MD_BLOCK_INDENTED_CODE,line.start,line.end,indent_end,line.content_end,0,line_no,line_no,false};
        } else if ((line.content_end-line.start>=2U && source[line.start]=='!' && source[line.start+1U]=='[') ||
                   (line.content_end-line.start>=4U && memcmp(source+line.start,"<img",4U)==0)) {
            block=(MdBlock){MD_BLOCK_IMAGE,line.start,line.end,line.start,line.content_end,0,line_no,line_no,false};
        } else if (source[indent_end]=='<') {
            block=(MdBlock){MD_BLOCK_HTML,line.start,line.end,line.start,line.content_end,0,line_no,line_no,false};
        } else {
            size_t paragraph_end=line.end;
            int end_line=line_no;
            size_t p=line.end;
            while (p<len) {
                SourceLine next=source_line(source,len,p,end_line+1);
                if (line_blank(source,next.start,next.content_end) ||
                    line_might_start_block(source,next.start,next.content_end)) break;
                if (next.end<len) {
                    SourceLine after=source_line(source,len,next.end,end_line+2);
                    if (line_table_separator(source,after.start,after.content_end)) break;
                }
                paragraph_end=next.end; end_line=next.number; p=next.end;
            }
            size_t cs=line.start, ce=paragraph_end;
            while (ce>cs && (source[ce-1U]=='\n'||source[ce-1U]=='\r')) --ce;
            trim_content(source,&cs,&ce);
            block=(MdBlock){MD_BLOCK_PARAGRAPH,line.start,paragraph_end,cs,ce,0,line_no,end_line,false};
        }
        if (!model_block(model,block)) goto oom;
        at=block.source_end; line_no=block.line_end+1;
    }
    return true;
oom:
    doc_error(error,error_cap,"Out of memory parsing Markdown");
    md_render_model_free(model);
    model->generation=generation;
    return false;
}

static bool is_ascii_punctuation(char c) {
    return strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", c) != NULL;
}

static bool inline_plain(const char *source, size_t start, size_t end, MdBuf *out) {
    size_t i = start;
    while (i < end) {
        if (source[i] == '\\' && i + 1U < end && is_ascii_punctuation(source[i+1U])) {
            if (!md_buf_append_char(out, source[i+1U])) return false;
            i += 2U;
            continue;
        }
        if (source[i] == '!' && i + 1U < end && source[i+1U] == '[') {
            size_t close = i + 2U;
            while (close < end && source[close] != ']') ++close;
            if (close < end && close + 1U < end && source[close+1U] == '(') {
                if (!md_buf_append(out, source + i + 2U, close - (i + 2U))) return false;
                size_t paren = close + 2U;
                unsigned depth = 1U;
                while (paren < end && depth != 0U) {
                    if (source[paren] == '(') ++depth;
                    else if (source[paren] == ')') --depth;
                    ++paren;
                }
                i = paren;
                continue;
            }
        }
        if (source[i] == '[') {
            size_t close = i + 1U;
            while (close < end && source[close] != ']') ++close;
            if (close < end && close + 1U < end && source[close+1U] == '(') {
                if (!md_buf_append(out, source + i + 1U, close - (i + 1U))) return false;
                size_t paren = close + 2U;
                unsigned depth = 1U;
                while (paren < end && depth != 0U) {
                    if (source[paren] == '(') ++depth;
                    else if (source[paren] == ')') --depth;
                    ++paren;
                }
                i = paren;
                continue;
            }
        }
        if (source[i] == '<') {
            size_t close = i + 1U;
            while (close < end && source[close] != '>') ++close;
            if (close < end) {
                bool autolink = false;
                for (size_t j = i + 1U; j < close; ++j) {
                    if (source[j] == ':' || source[j] == '@') { autolink = true; break; }
                }
                if (autolink && !md_buf_append(out, source+i+1U, close-i-1U)) return false;
                i = close + 1U;
                continue;
            }
        }
        if (source[i] == '*' || source[i] == '_' || source[i] == '~' || source[i] == '`') {
            ++i;
            continue;
        }
        if (source[i] == '\r') { ++i; continue; }
        if (!md_buf_append_char(out, source[i])) return false;
        ++i;
    }
    return true;
}

bool md_markdown_plain_text(const char *source, size_t len, MdBuf *out) {
    MdRenderModel model;
    md_render_model_init(&model);
    char error[128];
    if (!md_markdown_parse(source,len,&model,error,sizeof(error))) return false;
    out->len=0U;
    if (!md_buf_reserve(out,0U)) { md_render_model_free(&model); return false; }
    out->data[0]='\0';
    for (size_t i=0U;i<model.block_count;++i) {
        MdBlock *b=&model.blocks[i];
        if (b->type==MD_BLOCK_BLANK) {
            if (out->len!=0U && out->data[out->len-1U]!='\n' && !md_buf_append_char(out,'\n')) goto fail;
            continue;
        }
        if (b->type==MD_BLOCK_THEMATIC) {
            if (!md_buf_append_char(out,'\n')) goto fail;
            continue;
        }
        if (b->type==MD_BLOCK_TABLE) {
            size_t at=b->source_start;
            int row=0;
            while (at<b->source_end) {
                SourceLine line=source_line(source,b->source_end,at,row+1);
                if (row!=1) {
                    for (size_t p=line.start;p<line.content_end;++p) {
                        if (source[p]=='|') {
                            if (!md_buf_append_char(out,' ')) goto fail;
                        } else if (source[p]!=':' && source[p]!='-' &&
                                  !md_buf_append_char(out,source[p])) goto fail;
                    }
                    if (!md_buf_append_char(out,'\n')) goto fail;
                }
                at=line.end; ++row;
            }
            continue;
        }
        if (!inline_plain(source,b->content_start,b->content_end,out)) goto fail;
        if (out->len==0U || out->data[out->len-1U]!='\n') {
            if (!md_buf_append_char(out,'\n')) goto fail;
        }
    }
    while (out->len>0U && out->data[out->len-1U]=='\n') {
        --out->len; out->data[out->len]='\0';
    }
    md_render_model_free(&model);
    return true;
fail:
    md_render_model_free(&model);
    return false;
}

static void undo_entry_free(MdUndoEntry *entry) {
    free(entry->removed);
    free(entry->inserted);
    memset(entry,0,sizeof(*entry));
}

static void undo_stack_clear(MdUndoStack *stack) {
    for (size_t i=0U;i<stack->len;++i) undo_entry_free(&stack->items[i]);
    free(stack->items);
    memset(stack,0,sizeof(*stack));
}

static bool undo_stack_push(MdUndoStack *stack, MdUndoEntry entry) {
    if (!grow_array((void **)&stack->items,&stack->cap,stack->len+1U,sizeof(*stack->items))) return false;
    stack->items[stack->len++]=entry;
    return true;
}

static bool document_reparse(MdDocument *doc, char *error, size_t error_cap) {
    return md_markdown_parse(doc->source.data==NULL?"":doc->source.data,doc->source.len,
                             &doc->render,error,error_cap);
}

void md_document_init(MdDocument *doc, unsigned untitled_index) {
    memset(doc,0,sizeof(*doc));
    md_buf_init(&doc->source);
    md_render_model_init(&doc->render);
    doc->untitled=true;
    doc->mode=MD_MODE_SOURCE;
    doc->zoom=1.0;
    doc->split_ratio=0.5;
    (void)snprintf(doc->display_name,sizeof(doc->display_name),"Untitled %u",untitled_index);
    uint8_t digest[32];
    char identity[64];
    int n=snprintf(identity,sizeof(identity),"untitled:%u:%llu",untitled_index,
                   (unsigned long long)md_now_millis());
    md_sha256(identity,n<0?0U:(size_t)n,digest);
    md_hex_encode(digest,sizeof(digest),doc->id);
    char ignored[1];
    (void)md_document_set_source(doc,"",0U,false,ignored,sizeof(ignored));
}

void md_document_free(MdDocument *doc) {
    md_buf_free(&doc->source);
    md_render_model_free(&doc->render);
    undo_stack_clear(&doc->undo);
    undo_stack_clear(&doc->redo);
    memset(doc,0,sizeof(*doc));
}

bool md_document_set_source(MdDocument *doc, const char *source, size_t len,
                            bool dirty, char *error, size_t error_cap) {
    size_t bad=0U;
    if (!md_utf8_validate(source,len,&bad)) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"Invalid UTF-8 at byte %zu",bad);
        return false;
    }
    MdBuf normalized;
    md_buf_init(&normalized);
    for (size_t i=0U;i<len;++i) {
        if (source[i]=='\r') {
            if (i+1U<len&&source[i+1U]=='\n') ++i;
            if (!md_buf_append_char(&normalized,'\n')) goto oom;
        } else if (!md_buf_append_char(&normalized,source[i])) goto oom;
    }
    if (!md_buf_assign(&doc->source,normalized.data==NULL?"":normalized.data,normalized.len)) goto oom;
    md_buf_free(&normalized);
    doc->cursor=MD_MIN(doc->cursor,doc->source.len);
    doc->anchor=MD_MIN(doc->anchor,doc->source.len);
    doc->dirty=dirty;
    ++doc->edit_generation;
    if (!document_reparse(doc,error,error_cap)) return false;
    return true;
oom:
    md_buf_free(&normalized);
    doc_error(error,error_cap,"Out of memory setting document text");
    return false;
}

bool md_document_load(MdDocument *doc, const char *path,
                      char *error, size_t error_cap) {
    MdBytes bytes;
    md_bytes_init(&bytes);
    if (!md_read_file(path,&bytes,error,error_cap)) { md_bytes_free(&bytes); return false; }
    size_t bad=0U;
    if (!md_utf8_validate((const char *)bytes.data,bytes.len,&bad)) {
        if (error!=NULL&&error_cap!=0U) (void)snprintf(error,error_cap,"%s is not valid UTF-8 (byte %zu)",path,bad);
        md_bytes_free(&bytes); return false;
    }
    if (!md_document_set_source(doc,(const char *)bytes.data,bytes.len,false,error,error_cap)) {
        md_bytes_free(&bytes); return false;
    }
    md_sha256(bytes.data,bytes.len,doc->disk_sha256);
    doc->has_disk_sha256=true;
    md_bytes_free(&bytes);
    if (strlen(path)>=sizeof(doc->path)) { doc_error(error,error_cap,"Path is too long"); return false; }
    strcpy(doc->path,path);
    char basename[MD_PATH_MAX];
    if (!md_path_basename(basename,path) || strlen(basename)>=sizeof(doc->display_name)) {
        doc_error(error,error_cap,"Filename is too long"); return false;
    }
    strcpy(doc->display_name,basename);
    uint8_t digest[32]; md_sha256(path,strlen(path),digest); md_hex_encode(digest,32U,doc->id);
    doc->untitled=false; doc->orphaned=false; doc->conflict=false; doc->cursor=doc->anchor=0U;
    undo_stack_clear(&doc->undo); undo_stack_clear(&doc->redo);
    return true;
}

MdRange md_document_selection(const MdDocument *doc) {
    MdRange range;
    range.start=MD_MIN(doc->cursor,doc->anchor);
    range.end=MD_MAX(doc->cursor,doc->anchor);
    return range;
}

static void update_dirty_name(MdDocument *doc) {
    (void)doc;
}

bool md_document_replace(MdDocument *doc, size_t start, size_t end,
                         const char *text, size_t text_len, const char *label,
                         bool coalesce, char *error, size_t error_cap) {
    if (start>end||end>doc->source.len||!md_utf8_is_boundary(doc->source.data,doc->source.len,start)||
        !md_utf8_is_boundary(doc->source.data,doc->source.len,end)) {
        doc_error(error,error_cap,"Edit range is not a valid UTF-8 boundary"); return false;
    }
    size_t bad=0U;
    if (!md_utf8_validate(text,text_len,&bad)) { doc_error(error,error_cap,"Replacement is invalid UTF-8"); return false; }
    MdUndoEntry entry;
    memset(&entry,0,sizeof(entry));
    entry.start=start;
    entry.removed_len=end-start;
    entry.inserted_len=text_len;
    entry.removed=md_strndup(doc->source.data+start,entry.removed_len);
    entry.inserted=md_strndup(text,text_len);
    if (entry.removed==NULL||entry.inserted==NULL) { undo_entry_free(&entry); doc_error(error,error_cap,"Out of memory recording edit"); return false; }
    entry.cursor_before=doc->cursor; entry.anchor_before=doc->anchor;
    entry.cursor_after=start+text_len; entry.anchor_after=entry.cursor_after;
    entry.timestamp_ms=md_now_millis();
    (void)snprintf(entry.label,sizeof(entry.label),"%s",label==NULL?"Edit":label);
    if (!md_buf_replace(&doc->source,start,end,text,text_len)) { undo_entry_free(&entry); doc_error(error,error_cap,"Out of memory applying edit"); return false; }
    doc->cursor=entry.cursor_after; doc->anchor=entry.anchor_after;
    bool merged=false;
    if (coalesce&&entry.removed_len==0U&&doc->undo.len>0U) {
        MdUndoEntry *prev=&doc->undo.items[doc->undo.len-1U];
        if (prev->removed_len==0U&&strcmp(prev->label,entry.label)==0&&
            prev->start+prev->inserted_len==entry.start&&entry.timestamp_ms-prev->timestamp_ms<=1500U) {
            char *combined=realloc(prev->inserted,prev->inserted_len+entry.inserted_len+1U);
            if (combined!=NULL) {
                memcpy(combined+prev->inserted_len,entry.inserted,entry.inserted_len+1U);
                prev->inserted=combined; prev->inserted_len+=entry.inserted_len;
                prev->cursor_after=entry.cursor_after; prev->anchor_after=entry.anchor_after;
                prev->timestamp_ms=entry.timestamp_ms; merged=true;
            }
        }
    }
    if (!merged&&!undo_stack_push(&doc->undo,entry)) {
        (void)md_buf_replace(&doc->source,start,start+text_len,entry.removed,entry.removed_len);
        undo_entry_free(&entry); doc_error(error,error_cap,"Out of memory extending undo history"); return false;
    }
    if (merged) undo_entry_free(&entry);
    undo_stack_clear(&doc->redo);
    doc->dirty=true; ++doc->edit_generation; update_dirty_name(doc);
    if (!document_reparse(doc,error,error_cap)) return false;
    return true;
}

static bool apply_undo_entry(MdDocument *doc, MdUndoEntry *entry, bool reverse,
                             char *error, size_t error_cap) {
    size_t end=entry->start+(reverse?entry->inserted_len:entry->removed_len);
    const char *text=reverse?entry->removed:entry->inserted;
    size_t len=reverse?entry->removed_len:entry->inserted_len;
    if (!md_buf_replace(&doc->source,entry->start,end,text,len)) {
        doc_error(error,error_cap,"Out of memory applying undo/redo"); return false;
    }
    doc->cursor=reverse?entry->cursor_before:entry->cursor_after;
    doc->anchor=reverse?entry->anchor_before:entry->anchor_after;
    doc->dirty=true; ++doc->edit_generation;
    return document_reparse(doc,error,error_cap);
}

bool md_document_undo(MdDocument *doc, char *error, size_t error_cap) {
    if (doc->undo.len==0U) return false;
    MdUndoEntry entry=doc->undo.items[--doc->undo.len];
    if (!apply_undo_entry(doc,&entry,true,error,error_cap)) { doc->undo.items[doc->undo.len++]=entry; return false; }
    if (!undo_stack_push(&doc->redo,entry)) { doc_error(error,error_cap,"Out of memory recording redo"); return false; }
    return true;
}

bool md_document_redo(MdDocument *doc, char *error, size_t error_cap) {
    if (doc->redo.len==0U) return false;
    MdUndoEntry entry=doc->redo.items[--doc->redo.len];
    if (!apply_undo_entry(doc,&entry,false,error,error_cap)) { doc->redo.items[doc->redo.len++]=entry; return false; }
    if (!undo_stack_push(&doc->undo,entry)) { doc_error(error,error_cap,"Out of memory recording undo"); return false; }
    return true;
}

bool md_document_insert_utf8(MdDocument *doc, const char *text, size_t len,
                             char *error, size_t error_cap) {
    MdRange selection=md_document_selection(doc);
    return md_document_replace(doc,selection.start,selection.end,text,len,"Typing",true,error,error_cap);
}

bool md_document_backspace(MdDocument *doc, char *error, size_t error_cap) {
    MdRange selection=md_document_selection(doc);
    if (selection.start==selection.end) {
        if (selection.start==0U) return false;
        selection.start=md_grapheme_prev(doc->source.data,doc->source.len,selection.start);
    }
    return md_document_replace(doc,selection.start,selection.end,"",0U,"Backspace",false,error,error_cap);
}

bool md_document_delete(MdDocument *doc, char *error, size_t error_cap) {
    MdRange selection=md_document_selection(doc);
    if (selection.start==selection.end) {
        if (selection.end>=doc->source.len) return false;
        selection.end=md_grapheme_next(doc->source.data,doc->source.len,selection.end);
    }
    return md_document_replace(doc,selection.start,selection.end,"",0U,"Delete",false,error,error_cap);
}

bool md_document_move_selection(MdDocument *doc,size_t drop_offset,bool copy,
                                char *error,size_t error_cap) {
    MdRange selection=md_document_selection(doc);
    if (selection.start==selection.end) {
        doc_error(error,error_cap,"A non-empty selection is required");
        return false;
    }
    if (drop_offset>doc->source.len||
        !md_utf8_is_boundary(doc->source.data,doc->source.len,drop_offset)) {
        doc_error(error,error_cap,"Drop position is not a valid UTF-8 boundary");
        return false;
    }
    if (drop_offset>=selection.start&&drop_offset<=selection.end) return true;
    size_t selected_len=selection.end-selection.start;
    if (copy) {
        char *selected=md_strndup(doc->source.data+selection.start,selected_len);
        if (selected==NULL) {
            doc_error(error,error_cap,"Out of memory copying the selection");
            return false;
        }
        bool ok=md_document_replace(doc,drop_offset,drop_offset,selected,selected_len,
                                    "Drag copy",false,error,error_cap);
        free(selected);
        if (ok) {
            doc->anchor=drop_offset;
            doc->cursor=drop_offset+selected_len;
        }
        return ok;
    }
    MdBuf moved;
    md_buf_init(&moved);
    size_t selected_at=0U;
    bool ok=false;
    if (drop_offset<selection.start) {
        selected_at=drop_offset;
        ok=md_buf_append(&moved,doc->source.data,drop_offset)&&
           md_buf_append(&moved,doc->source.data+selection.start,selected_len)&&
           md_buf_append(&moved,doc->source.data+drop_offset,selection.start-drop_offset)&&
           md_buf_append(&moved,doc->source.data+selection.end,doc->source.len-selection.end);
    } else {
        selected_at=drop_offset-selected_len;
        ok=md_buf_append(&moved,doc->source.data,selection.start)&&
           md_buf_append(&moved,doc->source.data+selection.end,drop_offset-selection.end)&&
           md_buf_append(&moved,doc->source.data+selection.start,selected_len)&&
           md_buf_append(&moved,doc->source.data+drop_offset,doc->source.len-drop_offset);
    }
    if (ok) ok=md_document_replace(doc,0U,doc->source.len,moved.data,moved.len,
                                   "Drag move",false,error,error_cap);
    md_buf_free(&moved);
    if (!ok) {
        if (error!=NULL&&error_cap!=0U&&error[0]=='\0')
            doc_error(error,error_cap,"Out of memory moving the selection");
        return false;
    }
    doc->anchor=selected_at;
    doc->cursor=selected_at+selected_len;
    return true;
}

void md_document_move_left(MdDocument *doc, bool extend) {
    size_t next;
    if (!extend&&doc->cursor!=doc->anchor) next=MD_MIN(doc->cursor,doc->anchor);
    else next=md_grapheme_prev(doc->source.data,doc->source.len,doc->cursor);
    doc->cursor=next;
    if (!extend) doc->anchor=next;
}

void md_document_move_right(MdDocument *doc, bool extend) {
    size_t next;
    if (!extend&&doc->cursor!=doc->anchor) next=MD_MAX(doc->cursor,doc->anchor);
    else next=md_grapheme_next(doc->source.data,doc->source.len,doc->cursor);
    doc->cursor=next;
    if (!extend) doc->anchor=next;
}

static size_t line_start_at(const MdDocument *doc,size_t at) {
    while (at>0U&&doc->source.data[at-1U]!='\n') --at;
    return at;
}

static size_t line_end_at(const MdDocument *doc,size_t at) {
    while (at<doc->source.len&&doc->source.data[at]!='\n') ++at;
    return at;
}

void md_document_move_home(MdDocument *doc, bool document, bool extend) {
    size_t next=document?0U:line_start_at(doc,doc->cursor);
    doc->cursor=next; if (!extend) doc->anchor=next;
}

void md_document_move_end(MdDocument *doc, bool document, bool extend) {
    size_t next=document?doc->source.len:line_end_at(doc,doc->cursor);
    doc->cursor=next; if (!extend) doc->anchor=next;
}

bool md_document_format(MdDocument *doc, const char *open, const char *close,
                        const char *label, char *error, size_t error_cap) {
    MdRange r=md_document_selection(doc);
    size_t open_len=strlen(open), close_len=strlen(close);
    if (r.start==r.end) {
        MdBuf pair; md_buf_init(&pair);
        if (!md_buf_append_cstr(&pair,open)||!md_buf_append_cstr(&pair,close)) {
            md_buf_free(&pair); doc_error(error,error_cap,"Out of memory formatting text"); return false;
        }
        bool ok=md_document_replace(doc,r.start,r.end,pair.data,pair.len,label,false,error,error_cap);
        if (ok) doc->cursor=doc->anchor=r.start+open_len;
        md_buf_free(&pair); return ok;
    }
    bool enclosed=r.start>=open_len&&r.end+close_len<=doc->source.len&&
        memcmp(doc->source.data+r.start-open_len,open,open_len)==0&&
        memcmp(doc->source.data+r.end,close,close_len)==0;
    if (enclosed) {
        size_t old_start=r.start, old_end=r.end;
        MdBuf inner; md_buf_init(&inner);
        if (!md_buf_append(&inner,doc->source.data+r.start,r.end-r.start)) {
            md_buf_free(&inner); doc_error(error,error_cap,"Out of memory toggling format"); return false;
        }
        bool ok=md_document_replace(doc,r.start-open_len,r.end+close_len,inner.data,inner.len,label,false,error,error_cap);
        if (ok) { doc->anchor=old_start-open_len; doc->cursor=old_end-open_len; }
        md_buf_free(&inner); return ok;
    }
    MdBuf wrapped; md_buf_init(&wrapped);
    if (!md_buf_append_cstr(&wrapped,open)||
        !md_buf_append(&wrapped,doc->source.data+r.start,r.end-r.start)||
        !md_buf_append_cstr(&wrapped,close)) {
        md_buf_free(&wrapped); doc_error(error,error_cap,"Out of memory formatting text"); return false;
    }
    bool ok=md_document_replace(doc,r.start,r.end,wrapped.data,wrapped.len,label,false,error,error_cap);
    if (ok) { doc->anchor=r.start+open_len; doc->cursor=r.end+open_len; }
    md_buf_free(&wrapped); return ok;
}

bool md_document_heading_level(MdDocument *doc, int level,
                               char *error, size_t error_cap) {
    if (level<0||level>6) { doc_error(error,error_cap,"Heading level must be 0 through 6"); return false; }
    size_t start=line_start_at(doc,doc->cursor), end=line_end_at(doc,doc->cursor);
    size_t p=start;
    while (p<end&&p-start<3U&&doc->source.data[p]==' ') ++p;
    size_t prefix=p;
    while (p<end&&doc->source.data[p]=='#'&&p-prefix<6U) ++p;
    if (p>prefix&&(p==end||doc->source.data[p]==' '||doc->source.data[p]=='\t')) {
        while (p<end&&(doc->source.data[p]==' '||doc->source.data[p]=='\t')) ++p;
    } else p=prefix;
    MdBuf replacement; md_buf_init(&replacement);
    if (!md_buf_append(&replacement,doc->source.data+start,prefix-start)) goto oom;
    if (level>0) {
        for (int i=0;i<level;++i) if (!md_buf_append_char(&replacement,'#')) goto oom;
        if (!md_buf_append_char(&replacement,' ')) goto oom;
    }
    if (!md_buf_append(&replacement,doc->source.data+p,end-p)) goto oom;
    {
        bool ok=md_document_replace(doc,start,end,replacement.data,replacement.len,"Heading level",false,error,error_cap);
        md_buf_free(&replacement); return ok;
    }
oom:
    md_buf_free(&replacement); doc_error(error,error_cap,"Out of memory changing heading"); return false;
}

bool md_document_toggle_task(MdDocument *doc, size_t source_offset,
                             char *error, size_t error_cap) {
    for (size_t i=0U;i<doc->render.block_count;++i) {
        MdBlock *b=&doc->render.blocks[i];
        if (b->type!=MD_BLOCK_TASK_ITEM||source_offset<b->source_start||source_offset>b->source_end) continue;
        size_t p=b->source_start;
        while (p+2U<b->content_start&&doc->source.data[p]!='[') ++p;
        if (p+2U>=doc->source.len||doc->source.data[p]!='['||doc->source.data[p+2U]!=']') break;
        char value=doc->source.data[p+1U]==' '?'x':' ';
        return md_document_replace(doc,p+1U,p+2U,&value,1U,"Toggle task",false,error,error_cap);
    }
    doc_error(error,error_cap,"No task item at the requested position"); return false;
}

typedef struct {
    size_t whole_start;
    size_t whole_end;
    size_t label_start;
    size_t label_end;
    size_t destination_content_start;
    size_t destination_start;
    size_t destination_end;
} LinkSpan;

static bool link_span_at(const MdDocument *doc,size_t source_offset,LinkSpan *span) {
    const char *source=doc->source.data;
    for (size_t open=0U;open<doc->source.len;++open) {
        if (source[open]!='['||(open>0U&&source[open-1U]=='!')) continue;
        size_t close=open+1U;
        bool escaped=false;
        while (close<doc->source.len) {
            if (escaped) escaped=false;
            else if (source[close]=='\\') escaped=true;
            else if (source[close]==']') break;
            ++close;
        }
        if (close+1U>=doc->source.len||source[close+1U]!='(') continue;
        size_t content_start=close+2U,end=content_start;
        unsigned depth=1U;
        escaped=false;
        while (end<doc->source.len&&depth!=0U) {
            if (escaped) escaped=false;
            else if (source[end]=='\\') escaped=true;
            else if (source[end]=='(') ++depth;
            else if (source[end]==')') --depth;
            if (depth!=0U) ++end;
        }
        if (depth!=0U) continue;
        size_t destination=content_start; while (destination<end&&(source[destination]==' '||source[destination]=='\t'||source[destination]=='\n'||source[destination]=='\r')) ++destination;
        size_t destination_end=end;
        while (destination_end>destination&&(source[destination_end-1U]==' '||source[destination_end-1U]=='\t'||source[destination_end-1U]=='\n'||source[destination_end-1U]=='\r')) --destination_end;
        if (destination<destination_end&&source[destination]=='<') {
            size_t angle=destination+1U; bool angle_escaped=false;
            while (angle<destination_end) { if (angle_escaped) angle_escaped=false; else if (source[angle]=='\\') angle_escaped=true; else if (source[angle]=='>') break; ++angle; }
            if (angle>=destination_end) continue;
            ++destination; destination_end=angle;
        } else {
            unsigned nested=0U; bool token_escaped=false;
            for (size_t q=destination;q<destination_end;++q) {
                char c=source[q]; if (token_escaped) { token_escaped=false; continue; } if (c=='\\') { token_escaped=true; continue; }
                if (c=='(') { ++nested; continue; } if (c==')'&&nested>0U) { --nested; continue; }
                if (nested!=0U||(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')) continue;
                size_t title=q; while (title<destination_end&&(source[title]==' '||source[title]=='\t'||source[title]=='\n'||source[title]=='\r')) ++title;
                if (title>=destination_end||(source[title]!='\"'&&source[title]!='\''&&source[title]!='(')) continue;
                char title_close=source[title]=='('?')':source[title]; size_t finish=title+1U; bool title_escaped=false;
                while (finish<destination_end) { if (title_escaped) title_escaped=false; else if (source[finish]=='\\') title_escaped=true; else if (source[finish]==title_close) break; ++finish; }
                if (finish>=destination_end) continue;
                size_t tail=finish+1U; while (tail<destination_end&&(source[tail]==' '||source[tail]=='\t'||source[tail]=='\n'||source[tail]=='\r')) ++tail;
                if (tail==destination_end) { destination_end=q; while (destination_end>destination&&(source[destination_end-1U]==' '||source[destination_end-1U]=='\t'||source[destination_end-1U]=='\n'||source[destination_end-1U]=='\r')) --destination_end; break; }
            }
        }
        if (destination_end<=destination) continue;
        if (source_offset>=open&&source_offset<=end) {
            *span=(LinkSpan){open,end+1U,open+1U,close,content_start,destination,destination_end};
            return true;
        }
        open=end;
    }
    return false;
}

bool md_document_edit_link(MdDocument *doc,size_t source_offset,
                           const char *label,const char *destination,
                           char *error,size_t error_cap) {
    LinkSpan span;
    if (!link_span_at(doc,source_offset,&span)) {
        doc_error(error,error_cap,"No inline Markdown link at the requested position");
        return false;
    }
    const char *new_label=label==NULL?doc->source.data+span.label_start:label;
    size_t new_label_len=label==NULL?span.label_end-span.label_start:strlen(label);
    const char *new_destination=destination==NULL?doc->source.data+span.destination_start:destination;
    size_t new_destination_len=destination==NULL?span.destination_end-span.destination_start:strlen(destination);
    size_t bad=0U;
    if (!md_utf8_validate(new_label,new_label_len,&bad)||
        !md_utf8_validate(new_destination,new_destination_len,&bad)) {
        doc_error(error,error_cap,"Link text or destination is invalid UTF-8");
        return false;
    }
    MdBuf replacement;
    md_buf_init(&replacement);
    bool ok=md_buf_append_char(&replacement,'[')&&
            md_buf_append(&replacement,new_label,new_label_len)&&
            md_buf_append_cstr(&replacement,"](")&&
            md_buf_append(&replacement,doc->source.data+span.destination_content_start,
                          span.destination_start-span.destination_content_start)&&
            md_buf_append(&replacement,new_destination,new_destination_len)&&
            md_buf_append(&replacement,doc->source.data+span.destination_end,
                          span.whole_end-1U-span.destination_end)&&
            md_buf_append_char(&replacement,')');
    if (ok) ok=md_document_replace(doc,span.whole_start,span.whole_end,
                                   replacement.data,replacement.len,"Edit link",false,
                                   error,error_cap);
    md_buf_free(&replacement);
    if (!ok&&error!=NULL&&error_cap!=0U&&error[0]=='\0')
        doc_error(error,error_cap,"Out of memory editing link");
    return ok;
}

bool md_document_link_at(const MdDocument *doc,size_t source_offset,
                         MdBuf *label,MdBuf *destination) {
    LinkSpan span;
    if (!link_span_at(doc,source_offset,&span)) return false;
    return md_buf_assign(label,doc->source.data+span.label_start,span.label_end-span.label_start)&&
           md_buf_assign(destination,doc->source.data+span.destination_start,
                         span.destination_end-span.destination_start);
}

size_t md_document_line_for_offset(const MdDocument *doc, size_t offset) {
    offset=MD_MIN(offset,doc->source.len);
    size_t line=1U;
    for (size_t i=0U;i<offset;++i) if (doc->source.data[i]=='\n') ++line;
    return line;
}

size_t md_document_column_for_offset(const MdDocument *doc, size_t offset) {
    offset=MD_MIN(offset,doc->source.len);
    size_t start=line_start_at(doc,offset);
    return md_grapheme_count(doc->source.data+start,offset-start)+1U;
}

typedef struct {
    MdBuf *cells;
    size_t count;
} TableRow;

typedef struct {
    TableRow *rows;
    size_t row_count;
    size_t columns;
    int *align;
} ParsedTable;

static void table_free(ParsedTable *table) {
    for (size_t r=0U;r<table->row_count;++r) {
        for (size_t c=0U;c<table->rows[r].count;++c) md_buf_free(&table->rows[r].cells[c]);
        free(table->rows[r].cells);
    }
    free(table->rows); free(table->align); memset(table,0,sizeof(*table));
}

static bool table_row_add_cell(TableRow *row,const char *s,size_t len) {
    MdBuf *cells=realloc(row->cells,(row->count+1U)*sizeof(*cells));
    if (cells==NULL) return false;
    row->cells=cells; md_buf_init(&row->cells[row->count]);
    size_t start=0U,end=len;
    while (start<end&&(s[start]==' '||s[start]=='\t')) ++start;
    while (end>start&&(s[end-1U]==' '||s[end-1U]=='\t'||s[end-1U]=='\r')) --end;
    if (!md_buf_append(&row->cells[row->count],s+start,end-start)) return false;
    ++row->count; return true;
}

static bool table_parse_row(const char *s,size_t start,size_t end,TableRow *row) {
    memset(row,0,sizeof(*row));
    while (start<end&&(s[start]==' '||s[start]=='\t')) ++start;
    while (end>start&&(s[end-1U]==' '||s[end-1U]=='\t'||s[end-1U]=='\r')) --end;
    if (start<end&&s[start]=='|') ++start;
    if (end>start&&s[end-1U]=='|') --end;
    size_t cell_start=start;
    bool escaped=false;
    size_t ticks=0U;
    for (size_t i=start;i<=end;++i) {
        char c=i<end?s[i]:'|';
        if (escaped) { escaped=false; continue; }
        if (c=='\\') { escaped=true; continue; }
        if (c=='`') {
            size_t run=1U;
            while (i+run<end&&s[i+run]=='`') ++run;
            if (ticks==0U) ticks=run;
            else if (ticks==run) ticks=0U;
            i+=run-1U; continue;
        }
        if (c=='|'&&ticks==0U) {
            if (!table_row_add_cell(row,s+cell_start,i-cell_start)) return false;
            cell_start=i+1U;
        }
    }
    return row->count>0U;
}

static bool parsed_table_add_row(ParsedTable *table,TableRow row) {
    TableRow *rows=realloc(table->rows,(table->row_count+1U)*sizeof(*rows));
    if (rows==NULL) return false;
    table->rows=rows; table->rows[table->row_count++]=row;
    table->columns=MD_MAX(table->columns,row.count);
    return true;
}

static bool table_parse(const MdDocument *doc,const MdBlock *block,ParsedTable *table) {
    memset(table,0,sizeof(*table));
    size_t at=block->source_start;
    size_t physical=0U;
    TableRow separator; memset(&separator,0,sizeof(separator));
    while (at<block->source_end) {
        SourceLine line=source_line(doc->source.data,block->source_end,at,(int)physical+1);
        TableRow row;
        if (!table_parse_row(doc->source.data,line.start,line.content_end,&row)) { table_free(table); return false; }
        if (physical==1U) separator=row;
        else if (!parsed_table_add_row(table,row)) {
            for (size_t c=0U;c<row.count;++c) md_buf_free(&row.cells[c]);
            free(row.cells);
            table_free(table); return false;
        }
        ++physical; at=line.end;
    }
    if (physical<2U||table->row_count==0U) { table_free(table); return false; }
    table->align=calloc(table->columns,sizeof(*table->align));
    if (table->align==NULL) { table_free(table); return false; }
    for (size_t c=0U;c<separator.count&&c<table->columns;++c) {
        const MdBuf *cell=&separator.cells[c];
        size_t first=0U,last=cell->len;
        while (first<last&&cell->data[first]==' ') ++first;
        while (last>first&&cell->data[last-1U]==' ') --last;
        bool left=first<last&&cell->data[first]==':';
        bool right=last>first&&cell->data[last-1U]==':';
        table->align[c]=left&&right?2:(right?3:(left?1:0));
    }
    for (size_t c=0U;c<separator.count;++c) md_buf_free(&separator.cells[c]);
    free(separator.cells);
    for (size_t r=0U;r<table->row_count;++r) {
        if (table->rows[r].count<table->columns) {
            MdBuf *cells=realloc(table->rows[r].cells,table->columns*sizeof(*cells));
            if (cells==NULL) { table_free(table); return false; }
            table->rows[r].cells=cells;
            while (table->rows[r].count<table->columns) {
                md_buf_init(&table->rows[r].cells[table->rows[r].count++]);
            }
        }
    }
    return true;
}

static bool table_insert_row(ParsedTable *table,size_t index) {
    if (index>table->row_count) index=table->row_count;
    TableRow row; row.count=table->columns; row.cells=calloc(row.count,sizeof(*row.cells));
    if (row.cells==NULL&&row.count!=0U) return false;
    for (size_t c=0U;c<row.count;++c) md_buf_init(&row.cells[c]);
    TableRow *rows=realloc(table->rows,(table->row_count+1U)*sizeof(*rows));
    if (rows==NULL) { free(row.cells); return false; }
    table->rows=rows;
    memmove(table->rows+index+1U,table->rows+index,(table->row_count-index)*sizeof(*rows));
    table->rows[index]=row; ++table->row_count; return true;
}

static bool table_insert_col(ParsedTable *table,size_t index) {
    if (index>table->columns) index=table->columns;
    for (size_t r=0U;r<table->row_count;++r) {
        MdBuf *cells=realloc(table->rows[r].cells,(table->columns+1U)*sizeof(*cells));
        if (cells==NULL) return false;
        table->rows[r].cells=cells;
        memmove(cells+index+1U,cells+index,(table->columns-index)*sizeof(*cells));
        md_buf_init(&cells[index]); ++table->rows[r].count;
    }
    int *align=realloc(table->align,(table->columns+1U)*sizeof(*align));
    if (align==NULL) return false;
    table->align=align; memmove(align+index+1U,align+index,(table->columns-index)*sizeof(*align));
    align[index]=0; ++table->columns; return true;
}

static bool table_serialize(const ParsedTable *table,MdBuf *out) {
    for (size_t r=0U;r<table->row_count;++r) {
        if (!md_buf_append_cstr(out,"| ")) return false;
        for (size_t c=0U;c<table->columns;++c) {
            const MdBuf *cell=&table->rows[r].cells[c]; size_t slash_run=0U;
            for (size_t at=0U;at<cell->len;++at) {
                char ch=cell->data[at];
                if (ch=='|'&&(slash_run%2U)==0U&&!md_buf_append_char(out,'\\')) return false;
                if (!md_buf_append_char(out,ch)) return false;
                slash_run=ch=='\\'?slash_run+1U:0U;
            }
            if (!md_buf_append_cstr(out," |")) return false;
            if (c+1U<table->columns&&!md_buf_append_char(out,' ')) return false;
        }
        if (!md_buf_append_char(out,'\n')) return false;
        if (r==0U) {
            if (!md_buf_append_cstr(out,"| ")) return false;
            for (size_t c=0U;c<table->columns;++c) {
                const char *marker=table->align[c]==1?":---":table->align[c]==2?":---:":table->align[c]==3?"---:":"---";
                if (!md_buf_append_cstr(out,marker)||!md_buf_append_cstr(out," |")) return false;
                if (c+1U<table->columns&&!md_buf_append_char(out,' ')) return false;
            }
            if (!md_buf_append_char(out,'\n')) return false;
        }
    }
    return true;
}

bool md_document_table_action(MdDocument *doc, size_t source_offset,
                              size_t row, size_t col, MdTableAction action,
                              char *error, size_t error_cap) {
    MdBlock *block=NULL;
    for (size_t i=0U;i<doc->render.block_count;++i) {
        if (doc->render.blocks[i].type==MD_BLOCK_TABLE&&source_offset>=doc->render.blocks[i].source_start&&
            source_offset<=doc->render.blocks[i].source_end) { block=&doc->render.blocks[i]; break; }
    }
    if (block==NULL) { doc_error(error,error_cap,"No table at the requested position"); return false; }
    size_t source_start=block->source_start,source_end=block->source_end;
    ParsedTable table;
    if (!table_parse(doc,block,&table)) { doc_error(error,error_cap,"Cannot parse table structure"); return false; }
    if (row>=table.row_count) row=table.row_count-1U;
    if (col>=table.columns) col=table.columns-1U;
    bool ok=true;
    if (action==MD_TABLE_ROW_ABOVE) ok=table_insert_row(&table,MD_MAX(row,1U));
    else if (action==MD_TABLE_ROW_BELOW) ok=table_insert_row(&table,MD_MAX(row+1U,1U));
    else if (action==MD_TABLE_ROW_DELETE) {
        if (row==0U) { table_free(&table); doc_error(error,error_cap,"Header row cannot be deleted directly"); return false; }
        for (size_t c=0U;c<table.rows[row].count;++c) md_buf_free(&table.rows[row].cells[c]);
        free(table.rows[row].cells);
        memmove(table.rows+row,table.rows+row+1U,(table.row_count-row-1U)*sizeof(*table.rows)); --table.row_count;
    } else if (action==MD_TABLE_COL_BEFORE) ok=table_insert_col(&table,col);
    else if (action==MD_TABLE_COL_AFTER) ok=table_insert_col(&table,col+1U);
    else if (action==MD_TABLE_COL_DELETE) {
        if (table.columns<=1U) { table_free(&table); doc_error(error,error_cap,"Deleting the final table column requires removing the table"); return false; }
        for (size_t r=0U;r<table.row_count;++r) {
            md_buf_free(&table.rows[r].cells[col]);
            memmove(table.rows[r].cells+col,table.rows[r].cells+col+1U,(table.columns-col-1U)*sizeof(*table.rows[r].cells));
            --table.rows[r].count;
        }
        memmove(table.align+col,table.align+col+1U,(table.columns-col-1U)*sizeof(*table.align)); --table.columns;
    } else {
        table.align[col]=action==MD_TABLE_ALIGN_LEFT?1:action==MD_TABLE_ALIGN_CENTER?2:
                         action==MD_TABLE_ALIGN_RIGHT?3:0;
    }
    MdBuf serialized; md_buf_init(&serialized);
    if (!ok||!table_serialize(&table,&serialized)) {
        table_free(&table); md_buf_free(&serialized); doc_error(error,error_cap,"Out of memory editing table"); return false;
    }
    table_free(&table);
    ok=md_document_replace(doc,source_start,source_end,serialized.data,serialized.len,"Table structure",false,error,error_cap);
    md_buf_free(&serialized); return ok;
}

bool md_document_table_set_cell(MdDocument *doc,size_t source_offset,
                                size_t row,size_t col,const char *text,
                                size_t text_len,char *error,size_t error_cap) {
    size_t bad=0U;
    if (!md_utf8_validate(text,text_len,&bad)||memchr(text,'\n',text_len)!=NULL||
        memchr(text,'\r',text_len)!=NULL) {
        doc_error(error,error_cap,"Table cells require valid single-line UTF-8 text");
        return false;
    }
    MdBlock *block=NULL;
    for (size_t i=0U;i<doc->render.block_count;++i) {
        if (doc->render.blocks[i].type==MD_BLOCK_TABLE&&
            source_offset>=doc->render.blocks[i].source_start&&
            source_offset<=doc->render.blocks[i].source_end) {
            block=&doc->render.blocks[i];
            break;
        }
    }
    if (block==NULL) {
        doc_error(error,error_cap,"No table at the requested position");
        return false;
    }
    size_t source_start=block->source_start,source_end=block->source_end;
    ParsedTable table;
    if (!table_parse(doc,block,&table)) {
        doc_error(error,error_cap,"Cannot parse table structure");
        return false;
    }
    if (row>=table.row_count||col>=table.columns) {
        table_free(&table);
        doc_error(error,error_cap,"Table cell is out of range");
        return false;
    }
    if (!md_buf_assign(&table.rows[row].cells[col],text,text_len)) {
        table_free(&table);
        doc_error(error,error_cap,"Out of memory editing table cell");
        return false;
    }
    MdBuf serialized;
    md_buf_init(&serialized);
    bool ok=table_serialize(&table,&serialized);
    table_free(&table);
    if (ok) ok=md_document_replace(doc,source_start,source_end,serialized.data,
                                   serialized.len,"Edit table cell",false,error,error_cap);
    md_buf_free(&serialized);
    if (!ok&&error!=NULL&&error_cap!=0U&&error[0]=='\0')
        doc_error(error,error_cap,"Out of memory serializing table cell");
    return ok;
}

static size_t count_words(const char *s,size_t len) {
    size_t words=0U,at=0U;
    while (at<len) {
        size_t begin=at; uint32_t cp=0U;
        if (!md_utf8_decode(s,len,&at,&cp)) { ++begin; at=begin; continue; }
        if (md_unicode_is_space(cp)) continue;
        if (md_unicode_is_cjk(cp)) { ++words; continue; }
        if ((cp>='A'&&cp<='Z')||(cp>='a'&&cp<='z')||(cp>='0'&&cp<='9')||cp=='_') {
            ++words;
            while (at<len) {
                size_t look=at; uint32_t next=0U;
                if (!md_utf8_decode(s,len,&look,&next)) break;
                bool ascii_word=(next>='A'&&next<='Z')||(next>='a'&&next<='z')||
                                (next>='0'&&next<='9')||next=='_';
                if (ascii_word) { at=look; continue; }
                if ((next=='\''||next=='-')&&look<len) {
                    size_t after=look; uint32_t tail=0U;
                    if (md_utf8_decode(s,len,&after,&tail)&&
                        ((tail>='A'&&tail<='Z')||(tail>='a'&&tail<='z')||(tail>='0'&&tail<='9'))) {
                        at=look; continue;
                    }
                }
                break;
            }
            continue;
        }
        if (cp<0x80U&&is_ascii_punctuation((char)cp)) continue;
        ++words;
        while (at<len) {
            size_t look=at; uint32_t next=0U;
            if (!md_utf8_decode(s,len,&look,&next)||md_unicode_is_space(next)||md_unicode_is_cjk(next)||
                (next<0x80U&&is_ascii_punctuation((char)next))) break;
            at=look;
        }
    }
    return words;
}

static size_t count_substring(const char *s,size_t len,const char *needle) {
    size_t n=strlen(needle),count=0U;
    if (n==0U) return 0U;
    for (size_t i=0U;i+n<=len;++i) {
        if (memcmp(s+i,needle,n)==0) { ++count; i+=n-1U; }
    }
    return count;
}

void md_statistics_compute(const MdDocument *doc, MdStatistics *stats) {
    memset(stats,0,sizeof(*stats));
    stats->raw_characters=md_grapheme_count(doc->source.data==NULL?"":doc->source.data,doc->source.len);
    if (doc->source.len!=0U) {
        stats->total_lines=1U;
        bool nonempty=false;
        for (size_t i=0U;i<doc->source.len;++i) {
            if (doc->source.data[i]=='\n') {
                if (nonempty) ++stats->nonempty_lines;
                ++stats->total_lines; nonempty=false;
            } else if (doc->source.data[i]!=' '&&doc->source.data[i]!='\t'&&doc->source.data[i]!='\r') nonempty=true;
        }
        if (nonempty) ++stats->nonempty_lines;
        if (doc->source.data[doc->source.len-1U]=='\n') --stats->total_lines;
    }
    for (size_t i=0U;i<doc->render.block_count;++i) {
        MdBlockType type=doc->render.blocks[i].type;
        if (type==MD_BLOCK_PARAGRAPH) ++stats->paragraphs;
        if (type==MD_BLOCK_HEADING||type==MD_BLOCK_SETEXT_HEADING) ++stats->headings;
        if (type==MD_BLOCK_IMAGE) ++stats->images;
        if (type==MD_BLOCK_FENCED_CODE) ++stats->fenced_code_blocks;
    }
    size_t inline_images=count_substring(doc->source.data,doc->source.len,"![");
    if (inline_images>stats->images) stats->images=inline_images;
    stats->links=count_substring(doc->source.data,doc->source.len,"](");
    if (stats->links>=stats->images) stats->links-=stats->images;
    MdBuf plain; md_buf_init(&plain);
    if (md_markdown_plain_text(doc->source.data,doc->source.len,&plain)) {
        stats->rendered_characters=md_grapheme_count(plain.data==NULL?"":plain.data,plain.len);
        stats->words=count_words(plain.data==NULL?"":plain.data,plain.len);
    }
    md_buf_free(&plain);
}

void md_search_results_init(MdSearchResults *results) {
    memset(results,0,sizeof(*results));
}

void md_search_results_free(MdSearchResults *results) {
    free(results->matches); free(results->query); memset(results,0,sizeof(*results));
}

static bool byte_equal(char a,char b,bool case_sensitive) {
    if (case_sensitive) return a==b;
    unsigned char ua=(unsigned char)a,ub=(unsigned char)b;
    if (ua>='A'&&ua<='Z') ua=(unsigned char)(ua-'A'+'a');
    if (ub>='A'&&ub<='Z') ub=(unsigned char)(ub-'A'+'a');
    return ua==ub;
}

static bool search_equal(const char *a,const char *b,size_t len,bool case_sensitive) {
    for (size_t i=0U;i<len;++i) if (!byte_equal(a[i],b[i],case_sensitive)) return false;
    return true;
}

static bool whole_boundary(const char *s,size_t len,size_t start,size_t end) {
    if (start>0U) {
        size_t p=md_utf8_prev(s,start),q=p; uint32_t cp=0U;
        if (md_utf8_decode(s,len,&q,&cp)&&md_unicode_is_word_char(cp)) return false;
    }
    if (end<len) {
        size_t q=end; uint32_t cp=0U;
        if (md_utf8_decode(s,len,&q,&cp)&&md_unicode_is_word_char(cp)) return false;
    }
    return true;
}

bool md_document_find(const MdDocument *doc, const char *query,
                      bool case_sensitive, bool whole_word,
                      MdSearchResults *results) {
    size_t query_len=strlen(query),bad=0U;
    if (query_len==0U||!md_utf8_validate(query,query_len,&bad)) return false;
    md_search_results_free(results);
    results->query=md_strdup(query);
    if (results->query==NULL) return false;
    results->case_sensitive=case_sensitive; results->whole_word=whole_word;
    for (size_t i=0U;i+query_len<=doc->source.len;) {
        if (md_utf8_is_boundary(doc->source.data,doc->source.len,i)&&
            md_utf8_is_boundary(doc->source.data,doc->source.len,i+query_len)&&
            search_equal(doc->source.data+i,query,query_len,case_sensitive)&&
            (!whole_word||whole_boundary(doc->source.data,doc->source.len,i,i+query_len))) {
            if (!grow_array((void **)&results->matches,&results->cap,results->count+1U,sizeof(*results->matches))) {
                md_search_results_free(results); return false;
            }
            results->matches[results->count++]=(MdRange){i,i+query_len};
            i+=query_len;
        } else i=md_utf8_next(doc->source.data,doc->source.len,i);
    }
    results->active=0U;
    return true;
}

bool md_document_replace_active(MdDocument *doc, MdSearchResults *results,
                                const char *replacement,
                                char *error, size_t error_cap) {
    if (results->count==0U||results->active>=results->count||results->query==NULL) return false;
    MdRange r=results->matches[results->active];
    bool case_sensitive=results->case_sensitive,whole_word=results->whole_word;
    char *query=md_strdup(results->query);
    if (query==NULL) { doc_error(error,error_cap,"Out of memory replacing match"); return false; }
    bool ok=md_document_replace(doc,r.start,r.end,replacement,strlen(replacement),"Replace",false,error,error_cap);
    if (ok) ok=md_document_find(doc,query,case_sensitive,whole_word,results);
    free(query); return ok;
}

bool md_document_replace_all(MdDocument *doc, const char *query,
                             const char *replacement, bool case_sensitive,
                             bool whole_word, size_t *replaced,
                             char *error, size_t error_cap) {
    MdSearchResults results; md_search_results_init(&results);
    if (!md_document_find(doc,query,case_sensitive,whole_word,&results)) {
        md_search_results_free(&results); return false;
    }
    if (replaced!=NULL) *replaced=results.count;
    if (results.count==0U) { md_search_results_free(&results); return true; }
    MdBuf output; md_buf_init(&output);
    size_t at=0U,replacement_len=strlen(replacement);
    for (size_t i=0U;i<results.count;++i) {
        if (!md_buf_append(&output,doc->source.data+at,results.matches[i].start-at)||
            !md_buf_append(&output,replacement,replacement_len)) goto oom;
        at=results.matches[i].end;
    }
    if (!md_buf_append(&output,doc->source.data+at,doc->source.len-at)) goto oom;
    {
        bool ok=md_document_replace(doc,0U,doc->source.len,output.data,output.len,"Replace All",false,error,error_cap);
        md_buf_free(&output); md_search_results_free(&results); return ok;
    }
oom:
    md_buf_free(&output); md_search_results_free(&results); doc_error(error,error_cap,"Out of memory replacing matches"); return false;
}
