#ifndef MDEDIT_DOCUMENT_H
#define MDEDIT_DOCUMENT_H

#include "mdedit/core.h"

typedef enum {
    MD_MODE_SOURCE,
    MD_MODE_SPLIT,
    MD_MODE_PREVIEW,
    MD_MODE_RENDERED
} MdEditorMode;

typedef enum {
    MD_BLOCK_PARAGRAPH,
    MD_BLOCK_HEADING,
    MD_BLOCK_SETEXT_HEADING,
    MD_BLOCK_THEMATIC,
    MD_BLOCK_QUOTE,
    MD_BLOCK_UL_ITEM,
    MD_BLOCK_OL_ITEM,
    MD_BLOCK_TASK_ITEM,
    MD_BLOCK_FENCED_CODE,
    MD_BLOCK_INDENTED_CODE,
    MD_BLOCK_TABLE,
    MD_BLOCK_IMAGE,
    MD_BLOCK_HTML,
    MD_BLOCK_BLANK
} MdBlockType;

typedef struct {
    MdBlockType type;
    size_t source_start;
    size_t source_end;
    size_t content_start;
    size_t content_end;
    int level;
    int line_start;
    int line_end;
    bool checked;
} MdBlock;

typedef struct {
    char *label;
    size_t source_offset;
    int level;
    size_t block_index;
} MdHeading;

typedef struct {
    MdBlock *blocks;
    size_t block_count;
    size_t block_cap;
    MdHeading *headings;
    size_t heading_count;
    size_t heading_cap;
    uint64_t generation;
} MdRenderModel;

typedef struct {
    size_t start;
    char *removed;
    size_t removed_len;
    char *inserted;
    size_t inserted_len;
    size_t cursor_before;
    size_t anchor_before;
    size_t cursor_after;
    size_t anchor_after;
    char label[48];
    uint64_t timestamp_ms;
} MdUndoEntry;

typedef struct {
    MdUndoEntry *items;
    size_t len;
    size_t cap;
} MdUndoStack;

typedef struct {
    char id[65];
    char path[MD_PATH_MAX];
    char history_root[MD_PATH_MAX];
    char recovery_root[MD_PATH_MAX];
    char display_name[256];
    MdBuf source;
    MdRenderModel render;
    MdUndoStack undo;
    MdUndoStack redo;
    size_t cursor;
    size_t anchor;
    bool dirty;
    bool conflict;
    bool orphaned;
    bool untitled;
    MdEditorMode mode;
    double zoom;
    double source_scroll;
    double preview_scroll;
    double split_ratio;
    uint8_t disk_sha256[32];
    bool has_disk_sha256;
    uint64_t edit_generation;
} MdDocument;

typedef struct {
    size_t raw_characters;
    size_t rendered_characters;
    size_t words;
    size_t total_lines;
    size_t nonempty_lines;
    size_t paragraphs;
    size_t headings;
    size_t images;
    size_t links;
    size_t fenced_code_blocks;
} MdStatistics;

typedef struct {
    MdRange *matches;
    size_t count;
    size_t cap;
    size_t active;
    bool case_sensitive;
    bool whole_word;
    char *query;
} MdSearchResults;

typedef enum {
    MD_TABLE_ROW_ABOVE,
    MD_TABLE_ROW_BELOW,
    MD_TABLE_ROW_DELETE,
    MD_TABLE_COL_BEFORE,
    MD_TABLE_COL_AFTER,
    MD_TABLE_COL_DELETE,
    MD_TABLE_ALIGN_DEFAULT,
    MD_TABLE_ALIGN_LEFT,
    MD_TABLE_ALIGN_CENTER,
    MD_TABLE_ALIGN_RIGHT
} MdTableAction;

void md_render_model_init(MdRenderModel *model);
void md_render_model_free(MdRenderModel *model);
bool md_markdown_parse(const char *source, size_t len, MdRenderModel *model,
                       char *error, size_t error_cap);
bool md_markdown_plain_text(const char *source, size_t len, MdBuf *out);

void md_document_init(MdDocument *doc, unsigned untitled_index);
void md_document_free(MdDocument *doc);
bool md_document_load(MdDocument *doc, const char *path,
                      char *error, size_t error_cap);
bool md_document_set_source(MdDocument *doc, const char *source, size_t len,
                            bool dirty, char *error, size_t error_cap);
bool md_document_replace(MdDocument *doc, size_t start, size_t end,
                         const char *text, size_t text_len, const char *label,
                         bool coalesce, char *error, size_t error_cap);
bool md_document_undo(MdDocument *doc, char *error, size_t error_cap);
bool md_document_redo(MdDocument *doc, char *error, size_t error_cap);
bool md_document_insert_utf8(MdDocument *doc, const char *text, size_t len,
                             char *error, size_t error_cap);
bool md_document_backspace(MdDocument *doc, char *error, size_t error_cap);
bool md_document_delete(MdDocument *doc, char *error, size_t error_cap);
bool md_document_move_selection(MdDocument *doc, size_t drop_offset, bool copy,
                                char *error, size_t error_cap);
void md_document_move_left(MdDocument *doc, bool extend);
void md_document_move_right(MdDocument *doc, bool extend);
void md_document_move_home(MdDocument *doc, bool document, bool extend);
void md_document_move_end(MdDocument *doc, bool document, bool extend);
bool md_document_format(MdDocument *doc, const char *open, const char *close,
                        const char *label, char *error, size_t error_cap);
bool md_document_heading_level(MdDocument *doc, int level,
                               char *error, size_t error_cap);
bool md_document_toggle_task(MdDocument *doc, size_t source_offset,
                             char *error, size_t error_cap);
bool md_document_edit_link(MdDocument *doc, size_t source_offset,
                           const char *label, const char *destination,
                           char *error, size_t error_cap);
bool md_document_link_at(const MdDocument *doc, size_t source_offset,
                         MdBuf *label, MdBuf *destination);
bool md_document_table_action(MdDocument *doc, size_t source_offset,
                              size_t row, size_t col, MdTableAction action,
                              char *error, size_t error_cap);
bool md_document_table_set_cell(MdDocument *doc, size_t source_offset,
                                size_t row, size_t col,
                                const char *text, size_t text_len,
                                char *error, size_t error_cap);

void md_statistics_compute(const MdDocument *doc, MdStatistics *stats);
void md_search_results_init(MdSearchResults *results);
void md_search_results_free(MdSearchResults *results);
bool md_document_find(const MdDocument *doc, const char *query,
                      bool case_sensitive, bool whole_word,
                      MdSearchResults *results);
bool md_document_replace_active(MdDocument *doc, MdSearchResults *results,
                                const char *replacement,
                                char *error, size_t error_cap);
bool md_document_replace_all(MdDocument *doc, const char *query,
                             const char *replacement, bool case_sensitive,
                             bool whole_word, size_t *replaced,
                             char *error, size_t error_cap);

size_t md_document_line_for_offset(const MdDocument *doc, size_t offset);
size_t md_document_column_for_offset(const MdDocument *doc, size_t offset);
MdRange md_document_selection(const MdDocument *doc);

#endif
