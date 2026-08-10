#ifndef MDEDIT_UI_H
#define MDEDIT_UI_H

#include "mdedit/storage.h"

typedef enum {
    MD_MODAL_NONE,
    MD_MODAL_OPEN_PATH,
    MD_MODAL_SAVE_PATH,
    MD_MODAL_WORKSPACE_PATH,
    MD_MODAL_UNSAVED,
    MD_MODAL_STATISTICS,
    MD_MODAL_HISTORY,
    MD_MODAL_DIFF,
    MD_MODAL_COMMAND_PALETTE,
    MD_MODAL_PREFERENCES,
    MD_MODAL_SHORTCUTS,
    MD_MODAL_EXTERNAL_CONFLICT,
    MD_MODAL_RECOVERY,
    MD_MODAL_ERROR,
    MD_MODAL_IMAGE_STORAGE,
    MD_MODAL_IMAGE_PROPERTIES,
    MD_MODAL_TREE_ACTION,
    MD_MODAL_OVERWRITE,
    MD_MODAL_RELOCATION,
    MD_MODAL_LINK_PROPERTIES,
    MD_MODAL_HISTORY_RESTORE
} MdModalKind;

typedef enum {
    MD_CMD_NEW,
    MD_CMD_OPEN,
    MD_CMD_OPEN_WORKSPACE,
    MD_CMD_SAVE,
    MD_CMD_SAVE_AS,
    MD_CMD_SAVE_ALL,
    MD_CMD_CLOSE_TAB,
    MD_CMD_REOPEN_CLOSED,
    MD_CMD_UNDO,
    MD_CMD_REDO,
    MD_CMD_CUT,
    MD_CMD_COPY,
    MD_CMD_PASTE,
    MD_CMD_FIND,
    MD_CMD_REPLACE,
    MD_CMD_BOLD,
    MD_CMD_ITALIC,
    MD_CMD_STRIKE,
    MD_CMD_INLINE_CODE,
    MD_CMD_LINK,
    MD_CMD_INSERT_IMAGE,
    MD_CMD_INSERT_TABLE,
    MD_CMD_HEADING_1,
    MD_CMD_HEADING_2,
    MD_CMD_HEADING_3,
    MD_CMD_HEADING_4,
    MD_CMD_HEADING_5,
    MD_CMD_HEADING_6,
    MD_CMD_TOGGLE_TASK,
    MD_CMD_MODE_SOURCE,
    MD_CMD_MODE_SPLIT,
    MD_CMD_MODE_PREVIEW,
    MD_CMD_MODE_RENDERED,
    MD_CMD_STATISTICS,
    MD_CMD_HISTORY,
    MD_CMD_CREATE_VERSION,
    MD_CMD_TOGGLE_FILES,
    MD_CMD_TOGGLE_OUTLINE,
    MD_CMD_TOGGLE_SYNC,
    MD_CMD_TOGGLE_THEME,
    MD_CMD_PREFERENCES,
    MD_CMD_SHORTCUTS,
    MD_CMD_PALETTE,
    MD_CMD_EXPORT_SINGLE,
    MD_CMD_EXPORT_ASSETS,
    MD_CMD_CLEAR_RECENT_FILES,
    MD_CMD_CLEAR_RECENT_WORKSPACES,
    MD_CMD_COUNT
} MdCommandId;

typedef struct {
    MdCommandId id;
    const char *label;
    const char *shortcut;
    bool enabled;
} MdCommand;

typedef struct MdApp MdApp;

MdApp *md_app_create(void);
void md_app_destroy(MdApp *app);
int md_app_run(MdApp *app, int argc, char **argv);
bool md_app_execute(MdApp *app, MdCommandId command);
const MdCommand *md_app_commands(const MdApp *app, size_t *count);

#endif
