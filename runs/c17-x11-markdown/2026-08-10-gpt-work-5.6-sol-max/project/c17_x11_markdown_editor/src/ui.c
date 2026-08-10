#include "mdedit/ui.h"
#include "mdedit/image.h"
#include "mdedit/json.h"

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum {
    UI_DEFAULT_WIDTH=1440,
    UI_DEFAULT_HEIGHT=900,
    UI_MIN_WIDTH=760,
    UI_MIN_HEIGHT=560,
    UI_NAV_HEIGHT=72,
    UI_TAB_HEIGHT=43,
    UI_TOOL_HEIGHT=48,
    UI_STATUS_HEIGHT=30,
    UI_SIDEBAR_DEFAULT=270,
    UI_MAX_DOCS=128,
    UI_MAX_BUTTONS=64,
    UI_MAX_RIPPLES=16,
    UI_TEXT_CAP=8192,
    UI_MODAL_OPEN_MS=220,
    UI_MODAL_CLOSE_MS=180
};

typedef struct { int x,y,w,h; } UiRect;
typedef struct { unsigned long pixel; XRenderColor color; } UiColor;

typedef struct {
    UiColor window;
    UiColor surface;
    UiColor raised;
    UiColor editor;
    UiColor text;
    UiColor muted;
    UiColor accent;
    UiColor accent_soft;
    UiColor border;
    UiColor selection;
    UiColor code;
    UiColor link;
    UiColor success;
    UiColor warning;
    UiColor error;
    UiColor diff_add;
    UiColor diff_delete;
    UiColor shadow;
} UiPalette;

typedef enum {
    UI_FOCUS_NAV,
    UI_FOCUS_TABS,
    UI_FOCUS_SIDEBAR,
    UI_FOCUS_EDITOR,
    UI_FOCUS_FIND,
    UI_FOCUS_MODAL,
    UI_FOCUS_MENU
} UiFocus;

typedef struct {
    UiRect rect;
    MdCommandId command;
    char label[40];
    bool enabled;
    bool hovered;
    bool pressed;
    bool focused;
    double hover_progress;
} UiButton;

typedef struct {
    bool active;
    UiRect clip;
    double x;
    double y;
    double radius;
    uint64_t started;
} UiRipple;

typedef enum { UI_MENU_NONE,UI_MENU_FILE,UI_MENU_OVERFLOW,UI_MENU_CONTEXT,UI_MENU_IMAGE,UI_MENU_TABLE,UI_MENU_TREE } UiMenuKind;

typedef struct {
    UiMenuKind kind;
    UiRect rect;
    int selected;
    int item_count;
    int x;
    int y;
    size_t source_offset;
    size_t row;
    size_t col;
    char target[MD_PATH_MAX];
} UiMenu;

typedef enum {
    UI_PENDING_NONE,
    UI_PENDING_IMAGE_ALT,
    UI_PENDING_IMAGE_RELINK,
    UI_PENDING_IMAGE_SAVE_AS,
    UI_PENDING_TREE_NEW_FILE,
    UI_PENDING_TREE_NEW_FOLDER,
    UI_PENDING_TREE_RENAME,
    UI_PENDING_TREE_DELETE,
    UI_PENDING_SAVE_RELOCATION,
    UI_PENDING_LINK_EDIT
} UiPendingAction;

typedef struct {
    bool visible;
    bool replace;
    bool case_sensitive;
    bool whole_word;
    bool editing_replacement;
    MdBuf query;
    MdBuf replacement;
    MdSearchResults results;
} UiFind;

typedef struct {
    UiRect source;
    UiRect preview;
    UiRect content;
    UiRect sidebar;
    UiRect tabstrip;
    UiRect toolbar;
    UiRect status;
    UiRect divider;
} UiLayout;

typedef struct {
    bool selected;
    bool resizing;
    UiRect rect;
    size_t source_start;
    size_t source_end;
    int start_w;
    int start_h;
    int current_w;
    int current_h;
    int pointer_x;
    int pointer_y;
} UiImageSelection;

struct MdApp {
    Display *display;
    int screen;
    Window window;
    Pixmap back;
    GC gc;
    Visual *visual;
    Colormap colormap;
    int depth;
    XftDraw *xft;
    XftFont *font;
    XftFont *font_bold;
    XftFont *font_mono;
    XftFont *font_heading;
    XftFont *font_symbols;
    XftFont *font_emoji;
    XIM xim;
    XIC xic;

    Atom wm_delete;
    Atom clipboard;
    Atom utf8_string;
    Atom targets;
    Atom clipboard_property;
    Atom xdnd_aware;
    Atom xdnd_enter;
    Atom xdnd_position;
    Atom xdnd_status;
    Atom xdnd_drop;
    Atom xdnd_finished;
    Atom xdnd_selection;
    Atom text_uri_list;
    Atom test_command;
    Atom test_insert;
    Atom state_atom;

    int width;
    int height;
    bool mapped;
    bool running;
    bool dirty_frame;
    bool compact_nav;
    bool dark;
    bool test_mode;
    uint64_t quit_at;
    char test_state[64];

    MdPreferences prefs;
    MdDocument **docs;
    size_t doc_count;
    size_t doc_cap;
    size_t active_doc;
    unsigned untitled_counter;
    MdWorkspace workspace;

    MdCommand commands[MD_CMD_COUNT];
    UiButton buttons[UI_MAX_BUTTONS];
    size_t button_count;
    UiButton prior_buttons[UI_MAX_BUTTONS];
    size_t prior_button_count;
    UiRipple ripples[UI_MAX_RIPPLES];
    UiMenu menu;
    UiFind find;
    UiFocus focus;
    int focus_index;
    UiLayout layout;
    UiPalette palette;

    MdModalKind modal;
    MdModalKind closing_modal;
    uint64_t modal_started;
    bool modal_closing;
    double modal_progress;
    MdBuf modal_input;
    MdBuf modal_secondary;
    int modal_selection;
    MdCommandId pending_command;
    UiPendingAction pending_action;
    char pending_path[MD_PATH_MAX];
    MdRelocationPolicy relocation_policy;
    size_t pending_history_index;
    size_t pending_close_index;
    bool pending_close_after_save;
    bool pending_exit;
    char recently_closed_path[MD_PATH_MAX];
    UiRect modal_rect;
    char modal_message[1024];
    UiFocus focus_before_modal;

    double capsule_x;
    double capsule_target;
    double nav_frost;
    double sidebar_visual_width;
    bool sidebar_target_visible;
    bool outline_visible;
    bool dragging_sidebar;
    bool dragging_divider;
    bool dragging_scrollbar;
    bool dragging_selection;
    bool dragging_tab;
    int drag_tab_from;
    int drag_tab_to;
    int pointer_x;
    int pointer_y;
    size_t selection_drag_start;
    size_t selection_drop;
    MdBuf selection_drag_text;
    UiImageSelection image_selection;

    MdBuf clipboard_text;
    bool awaiting_paste;
    bool awaiting_xdnd;
    Window xdnd_source;
    int xdnd_version;

    MdVersionList versions;
    MdRecoveryList recoveries;
    size_t recovery_index;
    MdDiff diff;
    bool diff_side_by_side;
    size_t diff_change;
    MdBuf diff_base;
    MdBuf diff_target;

    uint64_t last_frame_ms;
    uint64_t last_autosave_ms;
    uint64_t last_external_check_ms;
    uint64_t toast_until;
    char toast[512];
    char last_activated_uri[512];
    bool recovery_scan_pending;
    const MdDocument *stats_document;
    uint64_t stats_generation;
    MdStatistics cached_statistics;
};

static void scan_startup_recovery(MdApp *app);

static const char *mode_name(MdEditorMode mode) {
    static const char *names[]={"Source","Split","Preview","Rendered Edit"};
    return names[(size_t)mode];
}

static MdDocument *active_document(MdApp *app) {
    return app->doc_count==0U||app->active_doc>=app->doc_count?NULL:app->docs[app->active_doc];
}

static const MdDocument *active_document_const(const MdApp *app) {
    return app->doc_count==0U||app->active_doc>=app->doc_count?NULL:app->docs[app->active_doc];
}

static size_t recovery_valid_count(const MdApp *app) {
    size_t count=0U;
    for (size_t i=0U;i<app->recoveries.count;++i) if (app->recoveries.items[i].valid) ++count;
    return count;
}

static MdRecoveryInfo *selected_recovery(MdApp *app) {
    if (app->recoveries.count==0U) return NULL;
    if (app->recovery_index>=app->recoveries.count||!app->recoveries.items[app->recovery_index].valid) {
        for (size_t i=0U;i<app->recoveries.count;++i) if (app->recoveries.items[i].valid) {
            app->recovery_index=i; break;
        }
    }
    return app->recovery_index<app->recoveries.count&&app->recoveries.items[app->recovery_index].valid?
           &app->recoveries.items[app->recovery_index]:NULL;
}

static void select_adjacent_recovery(MdApp *app,int direction) {
    if (recovery_valid_count(app)<2U||app->recoveries.count==0U) return;
    size_t at=app->recovery_index;
    for (size_t attempts=0U;attempts<app->recoveries.count;++attempts) {
        at=direction>0?(at+1U)%app->recoveries.count:
           (at+app->recoveries.count-1U)%app->recoveries.count;
        if (app->recoveries.items[at].valid) { app->recovery_index=at; return; }
    }
}

static bool point_in(UiRect r,int x,int y) {
    return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;
}

static bool tree_entry_visible(const MdWorkspace *workspace,size_t index) {
    if (index>=workspace->count) return false;
    const char *path=workspace->entries[index].path;
    for (size_t i=0U;i<workspace->collapsed_directory_count;++i) {
        const char *collapsed=workspace->collapsed_directories[i]; size_t len=strlen(collapsed);
        if (strncmp(path,collapsed,len)==0&&path[len]=='/') return false;
    }
    return true;
}

static ssize_t tree_index_for_visible_row(const MdWorkspace *workspace,int row) {
    if (row<0) return -1;
    int visible=0;
    for (size_t i=0U;i<workspace->count;++i) {
        if (!tree_entry_visible(workspace,i)) continue;
        if (visible==row) return (ssize_t)i;
        ++visible;
    }
    return -1;
}

static int tree_visible_count(const MdWorkspace *workspace) {
    int count=0; for (size_t i=0U;i<workspace->count;++i) if (tree_entry_visible(workspace,i)) ++count; return count;
}

static int tree_visible_row_for_path(const MdWorkspace *workspace,const char *path) {
    int row=0; for (size_t i=0U;i<workspace->count;++i) { if (!tree_entry_visible(workspace,i)) continue; if (strcmp(workspace->entries[i].path,path)==0) return row; ++row; } return -1;
}

static double ease_out_cubic(double t) {
    double u=1.0-MD_CLAMP(t,0.0,1.0); return 1.0-u*u*u;
}

static double ease_modal(double t) {
    t=MD_CLAMP(t,0.0,1.0);
    double u=1.0-t;
    return 1.0-u*u*(1.0+1.4*t);
}

static UiColor ui_color(MdApp *app,unsigned r,unsigned g,unsigned b) {
    XColor value;
    memset(&value,0,sizeof(value)); value.pixel=((unsigned long)r<<16U)|((unsigned long)g<<8U)|(unsigned long)b;
    value.red=(unsigned short)(r*257U); value.green=(unsigned short)(g*257U);
    value.blue=(unsigned short)(b*257U); value.flags=DoRed|DoGreen|DoBlue;
    if (app->display!=NULL) (void)XAllocColor(app->display,app->colormap,&value);
    UiColor result;
    result.pixel=value.pixel;
    result.color.red=value.red; result.color.green=value.green; result.color.blue=value.blue;
    result.color.alpha=65535U;
    return result;
}

static void palette_init(MdApp *app) {
    if (app->dark) {
        app->palette=(UiPalette){
            ui_color(app,18U,22U,31U),ui_color(app,25U,31U,43U),ui_color(app,35U,43U,58U),
            ui_color(app,20U,25U,35U),ui_color(app,234U,239U,249U),ui_color(app,151U,163U,184U),
            ui_color(app,111U,128U,255U),ui_color(app,51U,60U,103U),ui_color(app,66U,77U,99U),
            ui_color(app,64U,77U,135U),ui_color(app,31U,39U,53U),ui_color(app,108U,178U,255U),
            ui_color(app,59U,195U,139U),ui_color(app,247U,184U,75U),ui_color(app,247U,103U,111U),
            ui_color(app,34U,76U,61U),ui_color(app,87U,44U,53U),ui_color(app,5U,8U,14U)
        };
    } else {
        app->palette=(UiPalette){
            ui_color(app,241U,244U,250U),ui_color(app,252U,253U,255U),ui_color(app,255U,255U,255U),
            ui_color(app,255U,255U,255U),ui_color(app,30U,37U,51U),ui_color(app,100U,112U,134U),
            ui_color(app,76U,94U,240U),ui_color(app,225U,229U,255U),ui_color(app,211U,218U,231U),
            ui_color(app,207U,216U,255U),ui_color(app,244U,246U,250U),ui_color(app,41U,102U,196U),
            ui_color(app,28U,143U,98U),ui_color(app,190U,124U,16U),ui_color(app,202U,56U,67U),
            ui_color(app,220U,247U,236U),ui_color(app,255U,226U,230U),ui_color(app,87U,97U,117U)
        };
    }
}

static XftColor xft_color(UiColor color) {
    XftColor result; result.pixel=color.pixel; result.color=color.color; return result;
}

static void fill_rect(MdApp *app,UiRect r,UiColor color) {
    if (r.w<=0||r.h<=0) return;
    XSetForeground(app->display,app->gc,color.pixel);
    XFillRectangle(app->display,app->back,app->gc,r.x,r.y,(unsigned)r.w,(unsigned)r.h);
}

static void stroke_rect(MdApp *app,UiRect r,UiColor color,int width) {
    if (r.w<=0||r.h<=0) return;
    XSetForeground(app->display,app->gc,color.pixel); XSetLineAttributes(app->display,app->gc,(unsigned)width,LineSolid,CapRound,JoinRound);
    XDrawRectangle(app->display,app->back,app->gc,r.x,r.y,(unsigned)MD_MAX(0,r.w-1),(unsigned)MD_MAX(0,r.h-1));
}

static void fill_round(MdApp *app,UiRect r,int radius,UiColor color) {
    if (r.w<=0||r.h<=0) return;
    radius=MD_CLAMP(radius,0,MD_MIN(r.w,r.h)/2);
    if (radius==0) { fill_rect(app,r,color); return; }
    XSetForeground(app->display,app->gc,color.pixel);
    XFillRectangle(app->display,app->back,app->gc,r.x+radius,r.y,(unsigned)(r.w-2*radius),(unsigned)r.h);
    XFillRectangle(app->display,app->back,app->gc,r.x,r.y+radius,(unsigned)r.w,(unsigned)(r.h-2*radius));
    XFillArc(app->display,app->back,app->gc,r.x,r.y,(unsigned)(2*radius),(unsigned)(2*radius),90*64,90*64);
    XFillArc(app->display,app->back,app->gc,r.x+r.w-2*radius,r.y,(unsigned)(2*radius),(unsigned)(2*radius),0,90*64);
    XFillArc(app->display,app->back,app->gc,r.x,r.y+r.h-2*radius,(unsigned)(2*radius),(unsigned)(2*radius),180*64,90*64);
    XFillArc(app->display,app->back,app->gc,r.x+r.w-2*radius,r.y+r.h-2*radius,(unsigned)(2*radius),(unsigned)(2*radius),270*64,90*64);
}

static int text_width(MdApp *app,XftFont *font,const char *text,size_t len) {
    int width=0; size_t at=0U;
    while (at<len) {
        size_t next=md_grapheme_next(text,len,at),decode=at; uint32_t cp=0U; (void)md_utf8_decode(text,len,&decode,&cp);
        XftFont *chosen=font;
        if (!XftCharExists(app->display,chosen,(FcChar32)cp)&&app->font_symbols!=NULL&&XftCharExists(app->display,app->font_symbols,(FcChar32)cp)) chosen=app->font_symbols;
        else if (!XftCharExists(app->display,chosen,(FcChar32)cp)&&app->font_emoji!=NULL&&XftCharExists(app->display,app->font_emoji,(FcChar32)cp)) chosen=app->font_emoji;
        size_t count=next-at; if (count>(size_t)INT_MAX) count=(size_t)INT_MAX; XGlyphInfo extents; XftTextExtentsUtf8(app->display,chosen,(const FcChar8 *)text+at,(int)count,&extents); width+=(int)extents.xOff; at=next;
    }
    return width;
}

static void draw_text(MdApp *app,XftFont *font,UiColor color,int x,int baseline,const char *text,size_t len) {
    if (len==0U) return;
    XftColor xcolor=xft_color(color); size_t at=0U; int pen=x;
    while (at<len) {
        size_t next=md_grapheme_next(text,len,at),decode=at; uint32_t cp=0U; (void)md_utf8_decode(text,len,&decode,&cp); XftFont *chosen=font;
        if (!XftCharExists(app->display,chosen,(FcChar32)cp)&&app->font_symbols!=NULL&&XftCharExists(app->display,app->font_symbols,(FcChar32)cp)) chosen=app->font_symbols;
        else if (!XftCharExists(app->display,chosen,(FcChar32)cp)&&app->font_emoji!=NULL&&XftCharExists(app->display,app->font_emoji,(FcChar32)cp)) chosen=app->font_emoji;
        size_t count=next-at; if (count>(size_t)INT_MAX) count=(size_t)INT_MAX; XftDrawStringUtf8(app->xft,&xcolor,chosen,pen,baseline,(const FcChar8 *)text+at,(int)count); XGlyphInfo extents; XftTextExtentsUtf8(app->display,chosen,(const FcChar8 *)text+at,(int)count,&extents); pen+=(int)extents.xOff; at=next;
    }
}

static void draw_text_cstr(MdApp *app,XftFont *font,UiColor color,int x,int baseline,const char *text) {
    draw_text(app,font,color,x,baseline,text,strlen(text));
}

static void draw_text_elided_n(MdApp *app,XftFont *font,UiColor color,int x,int baseline,int max_width,const char *text,size_t len) {
    if (text_width(app,font,text,len)<=max_width) { draw_text(app,font,color,x,baseline,text,len); return; }
    static const char ellipsis[]="…"; int ellipsis_width=text_width(app,font,ellipsis,sizeof(ellipsis)-1U);
    size_t end=0U,next=0U;
    while (next<len) {
        next=md_grapheme_next(text,len,end);
        if (text_width(app,font,text,next)+ellipsis_width>max_width) break;
        end=next;
    }
    draw_text(app,font,color,x,baseline,text,end);
    draw_text(app,font,color,x+text_width(app,font,text,end),baseline,ellipsis,sizeof(ellipsis)-1U);
}

static void draw_text_elided(MdApp *app,XftFont *font,UiColor color,int x,int baseline,int max_width,const char *text) {
    draw_text_elided_n(app,font,color,x,baseline,max_width,text,strlen(text));
}

static void app_toast(MdApp *app,const char *fmt,...) {
    va_list ap; va_start(ap,fmt); (void)vsnprintf(app->toast,sizeof(app->toast),fmt,ap); va_end(ap);
    app->toast_until=md_now_millis()+3500U; app->dirty_frame=true;
}

static void modal_open(MdApp *app,MdModalKind kind,const char *message) {
    app->focus_before_modal=app->focus; app->focus=UI_FOCUS_MODAL; app->modal=kind;
    app->closing_modal=MD_MODAL_NONE; app->modal_started=md_now_millis(); app->modal_closing=false;
    app->modal_progress=0.0; app->modal_selection=0; app->modal_input.len=0U; app->modal_input.data[0]='\0';
    app->modal_secondary.len=0U; app->modal_secondary.data[0]='\0';
    (void)snprintf(app->modal_message,sizeof(app->modal_message),"%s",message==NULL?"":message);
    if (app->xic!=NULL) (void)XmbResetIC(app->xic);
    app->dirty_frame=true;
}

static void modal_close(MdApp *app) {
    if (app->modal==MD_MODAL_NONE||app->modal_closing) return;
    app->closing_modal=app->modal; app->modal_closing=true; app->modal_started=md_now_millis();
    app->dirty_frame=true;
}

static void modal_cancel(MdApp *app) {
    app->pending_action=UI_PENDING_NONE;
    app->pending_command=MD_CMD_COUNT;
    app->pending_path[0]='\0';
    app->pending_close_after_save=false;
    app->pending_exit=false;
    modal_close(app);
}

static void menu_close(MdApp *app) {
    app->menu.kind=UI_MENU_NONE;
    if (app->focus==UI_FOCUS_MENU) app->focus=UI_FOCUS_EDITOR;
    app->dirty_frame=true;
}

static const char *menu_label(const MdApp *app,int index) {
    static const char *file_items[]={"New Document","Open File…","Save","Save As…","Save All","Export Portable…","Preferences","Quit"};
    static const char *context_items[]={"Undo","Redo","Cut","Copy","Paste","Bold","Insert / Edit Link","Set Heading Level 4"};
    static const char *image_items[]={"Edit Alt Text…","Reset Display Size","Save Image As…","Locate / Relink…","Convert Relative ↔ Embedded","Remove Image","Image Properties","Dismiss"};
    static const char *table_items[]={"Add Row Above","Add Row Below","Delete Row","Add Column Before","Add Column After","Delete Column","Cycle Alignment","Dismiss"};
    static const char *tree_items[]={"New Markdown File…","New Folder…","Rename…","Delete…","Open","Collapse / Expand","Refresh Tree","Dismiss"};
    if (app->menu.kind==UI_MENU_OVERFLOW) {
        size_t document=app->menu.source_offset+(size_t)MD_MAX(index,0);
        return document<app->doc_count?app->docs[document]->display_name:"";
    }
    const char *const *items=file_items;
    if (app->menu.kind==UI_MENU_CONTEXT) items=context_items;
    else if (app->menu.kind==UI_MENU_IMAGE) items=image_items;
    else if (app->menu.kind==UI_MENU_TABLE) items=table_items;
    else if (app->menu.kind==UI_MENU_TREE) items=tree_items;
    return index>=0&&index<8?items[index]:"";
}

static void menu_activate(MdApp *app,int selected);
static void tree_menu_action(MdApp *app,int selected);
static void select_first_block_type(MdApp *app,MdBlockType type);
static const MdBlock *selected_image_block(MdApp *app);
static bool image_save_selected(MdApp *app,const char *path,bool overwrite,char *error,size_t error_cap);
static bool image_apply_pending(MdApp *app,UiPendingAction action,const char *value,char *error,size_t error_cap);
static void append_input(MdBuf *input,const char *text,size_t len);

static void ripple_add(MdApp *app,UiRect clip,int x,int y) {
    size_t slot=0U;
    for (size_t i=0U;i<UI_MAX_RIPPLES;++i) if (!app->ripples[i].active) { slot=i; break; }
    app->ripples[slot]=(UiRipple){true,clip,(double)x,(double)y,0.0,md_now_millis()};
}

static void app_commands_init(MdApp *app) {
    static const struct { const char *label; const char *shortcut; } definitions[MD_CMD_COUNT]={
        [MD_CMD_NEW]={"New Document","Ctrl+N"},[MD_CMD_OPEN]={"Open File…","Ctrl+O"},
        [MD_CMD_OPEN_WORKSPACE]={"Open Workspace…","Ctrl+Alt+O"},[MD_CMD_SAVE]={"Save","Ctrl+S"},
        [MD_CMD_SAVE_AS]={"Save As…","Ctrl+Shift+S"},[MD_CMD_SAVE_ALL]={"Save All","Ctrl+Alt+S"},
        [MD_CMD_CLOSE_TAB]={"Close Tab","Ctrl+W"},[MD_CMD_REOPEN_CLOSED]={"Reopen Closed File","Ctrl+Shift+T"},[MD_CMD_UNDO]={"Undo","Ctrl+Z"},
        [MD_CMD_REDO]={"Redo","Ctrl+Shift+Z"},[MD_CMD_CUT]={"Cut","Ctrl+X"},
        [MD_CMD_COPY]={"Copy","Ctrl+C"},[MD_CMD_PASTE]={"Paste","Ctrl+V"},
        [MD_CMD_FIND]={"Find","Ctrl+F"},[MD_CMD_REPLACE]={"Replace","Ctrl+H"},
        [MD_CMD_BOLD]={"Bold","Ctrl+B"},[MD_CMD_ITALIC]={"Italic","Ctrl+I"},
        [MD_CMD_STRIKE]={"Strikethrough","Ctrl+Shift+X"},[MD_CMD_INLINE_CODE]={"Inline Code","Ctrl+`"},
        [MD_CMD_LINK]={"Insert Link","Ctrl+K"},[MD_CMD_INSERT_IMAGE]={"Insert Image…","Ctrl+Shift+I"},
        [MD_CMD_INSERT_TABLE]={"Insert Table",""},
        [MD_CMD_HEADING_1]={"Set Heading Level 1",""},[MD_CMD_HEADING_2]={"Set Heading Level 2",""},
        [MD_CMD_HEADING_3]={"Set Heading Level 3",""},[MD_CMD_HEADING_4]={"Set Heading Level 4",""},
        [MD_CMD_HEADING_5]={"Set Heading Level 5",""},[MD_CMD_HEADING_6]={"Set Heading Level 6",""},
        [MD_CMD_TOGGLE_TASK]={"Toggle Task Checkbox",""},
        [MD_CMD_MODE_SOURCE]={"Toggle Source Mode","Ctrl+1"},
        [MD_CMD_MODE_SPLIT]={"Toggle Split Mode","Ctrl+2"},[MD_CMD_MODE_PREVIEW]={"Toggle Preview Mode","Ctrl+3"},
        [MD_CMD_MODE_RENDERED]={"Toggle Rendered Editing Mode","Ctrl+4"},[MD_CMD_STATISTICS]={"Document Statistics","Ctrl+Shift+D"},
        [MD_CMD_HISTORY]={"Version History","Ctrl+Alt+H"},[MD_CMD_CREATE_VERSION]={"Create Version",""},
        [MD_CMD_TOGGLE_FILES]={"Toggle Files Sidebar","Ctrl+Alt+1"},[MD_CMD_TOGGLE_OUTLINE]={"Toggle Outline Sidebar","Ctrl+Alt+2"},
        [MD_CMD_TOGGLE_SYNC]={"Toggle Synchronized Scrolling",""},[MD_CMD_TOGGLE_THEME]={"Toggle Light/Dark Theme",""},
        [MD_CMD_PREFERENCES]={"Preferences","Ctrl+,"},[MD_CMD_SHORTCUTS]={"Shortcut Reference","F1"},
        [MD_CMD_PALETTE]={"Command Palette","Ctrl+Shift+P"},[MD_CMD_EXPORT_SINGLE]={"Export Portable Single Markdown…",""},
        [MD_CMD_EXPORT_ASSETS]={"Export Portable Markdown + Assets…",""},
        [MD_CMD_CLEAR_RECENT_FILES]={"Clear Recent Files",""},
        [MD_CMD_CLEAR_RECENT_WORKSPACES]={"Clear Recent Workspaces",""}
    };
    for (size_t i=0U;i<MD_CMD_COUNT;++i) app->commands[i]=(MdCommand){(MdCommandId)i,definitions[i].label,definitions[i].shortcut,true};
}

static void commands_update(MdApp *app) {
    const MdDocument *doc=active_document_const(app); bool has=doc!=NULL;
    for (size_t i=0U;i<MD_CMD_COUNT;++i) app->commands[i].enabled=true;
    MdCommandId need_doc[]={MD_CMD_SAVE,MD_CMD_SAVE_AS,MD_CMD_CLOSE_TAB,MD_CMD_UNDO,MD_CMD_REDO,MD_CMD_CUT,
        MD_CMD_COPY,MD_CMD_FIND,MD_CMD_REPLACE,MD_CMD_BOLD,MD_CMD_ITALIC,MD_CMD_STRIKE,MD_CMD_INLINE_CODE,
        MD_CMD_LINK,MD_CMD_INSERT_IMAGE,MD_CMD_INSERT_TABLE,MD_CMD_HEADING_1,MD_CMD_HEADING_2,MD_CMD_HEADING_3,
        MD_CMD_HEADING_4,MD_CMD_HEADING_5,MD_CMD_HEADING_6,MD_CMD_TOGGLE_TASK,MD_CMD_MODE_SOURCE,MD_CMD_MODE_SPLIT,MD_CMD_MODE_PREVIEW,
        MD_CMD_MODE_RENDERED,MD_CMD_STATISTICS,MD_CMD_HISTORY,MD_CMD_CREATE_VERSION,MD_CMD_EXPORT_SINGLE,MD_CMD_EXPORT_ASSETS};
    for (size_t i=0U;i<MD_ARRAY_LEN(need_doc);++i) app->commands[need_doc[i]].enabled=has;
    app->commands[MD_CMD_UNDO].enabled=has&&doc->undo.len>0U;
    app->commands[MD_CMD_REDO].enabled=has&&doc->redo.len>0U;
    bool selection=has&&doc->cursor!=doc->anchor;
    app->commands[MD_CMD_COPY].enabled=selection; app->commands[MD_CMD_CUT].enabled=selection&&doc->mode!=MD_MODE_PREVIEW;
    app->commands[MD_CMD_PASTE].enabled=has&&doc->mode!=MD_MODE_PREVIEW;
    app->commands[MD_CMD_SAVE_ALL].enabled=false;
    for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty) app->commands[MD_CMD_SAVE_ALL].enabled=true;
    app->commands[MD_CMD_TOGGLE_FILES].enabled=app->workspace.root[0]!='\0';
    app->commands[MD_CMD_TOGGLE_OUTLINE].enabled=has;
    app->commands[MD_CMD_REOPEN_CLOSED].enabled=app->recently_closed_path[0]!='\0'&&access(app->recently_closed_path,R_OK)==0;
    app->commands[MD_CMD_CLEAR_RECENT_FILES].enabled=app->workspace.recent_file_count>0U;
    app->commands[MD_CMD_CLEAR_RECENT_WORKSPACES].enabled=app->workspace.recent_workspace_count>0U;
}

const MdCommand *md_app_commands(const MdApp *app,size_t *count) {
    if (count!=NULL) *count=MD_CMD_COUNT;
    return app->commands;
}

static bool grow_documents(MdApp *app) {
    if (app->doc_count<app->doc_cap) return true;
    size_t next=app->doc_cap==0U?8U:app->doc_cap*2U;
    if (next>UI_MAX_DOCS) next=UI_MAX_DOCS;
    if (next<=app->doc_cap) return false;
    MdDocument **docs=realloc(app->docs,next*sizeof(*docs)); if (docs==NULL) return false;
    app->docs=docs; app->doc_cap=next; return true;
}

static void assign_document_roots(MdApp *app,MdDocument *doc) {
    char base[MD_PATH_MAX],error[256];
    if (app->workspace.root[0]!='\0') {
        if (md_path_join(base,app->workspace.root,".mdeditor")) {
            (void)md_path_join(doc->history_root,base,"history");
            (void)md_path_join(doc->recovery_root,base,"recovery");
            (void)md_mkdirs(doc->history_root,0700,error,sizeof(error));
            (void)md_mkdirs(doc->recovery_root,0700,error,sizeof(error));
        }
        return;
    }
    const char *state=getenv("XDG_STATE_HOME"); char fallback[MD_PATH_MAX];
    if (state==NULL||state[0]=='\0') {
        const char *user_home=getenv("HOME");
        if (user_home==NULL||!md_path_join(fallback,user_home,".local/state")) return;
        state=fallback;
    }
    if (!md_path_join(base,state,"mdeditor")) return;
    if (md_path_join(doc->history_root,base,"history")) (void)md_mkdirs(doc->history_root,0700,error,sizeof(error));
    if (md_path_join(doc->recovery_root,base,"recovery")) (void)md_mkdirs(doc->recovery_root,0700,error,sizeof(error));
}

static bool add_new_document(MdApp *app) {
    if (!grow_documents(app)) { app_toast(app,"Cannot open another tab: document limit or memory exhausted"); return false; }
    MdDocument *doc=malloc(sizeof(*doc)); if (doc==NULL) return false;
    md_document_init(doc,++app->untitled_counter); doc->mode=app->prefs.default_mode;
    assign_document_roots(app,doc); app->docs[app->doc_count++]=doc; app->active_doc=app->doc_count-1U;
    app->capsule_target=(double)doc->mode; app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true; return true;
}

static ssize_t document_index_for_path(const MdApp *app,const char *path) {
    char wanted[MD_PATH_MAX]; const char *key=realpath(path,wanted); if (key==NULL) key=path;
    for (size_t i=0U;i<app->doc_count;++i) {
        char existing[MD_PATH_MAX]; const char *candidate=realpath(app->docs[i]->path,existing);
        if (candidate==NULL) candidate=app->docs[i]->path;
        if (candidate[0]!='\0'&&strcmp(key,candidate)==0) return (ssize_t)i;
    }
    return -1;
}

static bool open_document(MdApp *app,const char *path) {
    ssize_t existing=document_index_for_path(app,path);
    if (existing>=0) { app->active_doc=(size_t)existing; app->capsule_target=(double)app->docs[app->active_doc]->mode; app->dirty_frame=true; return true; }
    if (!grow_documents(app)) { app_toast(app,"Cannot open another document"); return false; }
    MdDocument *doc=malloc(sizeof(*doc)); if (doc==NULL) return false;
    md_document_init(doc,++app->untitled_counter); char error[512];
    if (!md_document_load(doc,path,error,sizeof(error))) { md_document_free(doc); free(doc); modal_open(app,MD_MODAL_ERROR,error); return false; }
    doc->mode=app->prefs.default_mode; assign_document_roots(app,doc);
    app->docs[app->doc_count++]=doc; app->active_doc=app->doc_count-1U; app->capsule_target=(double)doc->mode;
    app->focus=UI_FOCUS_EDITOR; (void)md_recent_add_file(&app->workspace,doc->path); (void)md_recent_save(&app->workspace,error,sizeof(error)); app_toast(app,"Opened %s",doc->display_name); return true;
}

static void remove_document(MdApp *app,size_t index) {
    if (index>=app->doc_count) return;
    md_document_free(app->docs[index]); free(app->docs[index]);
    memmove(app->docs+index,app->docs+index+1U,(app->doc_count-index-1U)*sizeof(*app->docs)); --app->doc_count;
    if (app->doc_count==0U) app->active_doc=0U;
    else if (app->active_doc>=app->doc_count) app->active_doc=app->doc_count-1U;
    MdDocument *doc=active_document(app); app->capsule_target=doc==NULL?0.0:(double)doc->mode;
    app->dirty_frame=true;
}

static bool save_workspace_session_snapshot(MdApp *app,char *error,size_t error_cap) {
    if (app->workspace.root[0]=='\0') return true;
    MdDocument *snapshot=app->doc_count==0U?NULL:malloc(app->doc_count*sizeof(*snapshot));
    if (app->doc_count>0U&&snapshot==NULL) { (void)snprintf(error,error_cap,"Out of memory preparing workspace session"); return false; }
    for (size_t i=0U;i<app->doc_count;++i) snapshot[i]=*app->docs[i];
    app->workspace.sidebar_width=app->sidebar_visual_width; app->workspace.sidebar_collapsed=!app->sidebar_target_visible;
    bool ok=md_workspace_save_session(&app->workspace,snapshot,app->doc_count,app->active_doc,error,error_cap); free(snapshot); return ok;
}

static void restore_workspace_session(MdApp *app) {
    if (!app->prefs.restore_session) return;
    MdBuf json; md_buf_init(&json); char warning[512]={0};
    if (!md_workspace_load_session(&app->workspace,&json,warning,sizeof(warning))) { if (warning[0]!='\0') app_toast(app,"%s",warning); md_buf_free(&json); return; }
    if (json.len==0U) { md_buf_free(&json); return; }
    MdJsonError parse; MdJson *root=md_json_parse(json.data,json.len,&parse); md_buf_free(&json); if (root==NULL) return;
    const MdJson *tabs=md_json_get(root,"tabs"); uint64_t active=0U; (void)md_json_u64(md_json_get(root,"active_tab"),&active);
    const MdJson *width=md_json_get(root,"sidebar_width"),*collapsed=md_json_get(root,"sidebar_collapsed"); bool sidebar_collapsed=false;
    if (width!=NULL&&width->type==MD_JSON_NUMBER) app->workspace.sidebar_width=MD_CLAMP(width->as.number,180.0,460.0);
    if (md_json_bool(collapsed,&sidebar_collapsed)) app->workspace.sidebar_collapsed=sidebar_collapsed;
    app->sidebar_target_visible=!app->workspace.sidebar_collapsed; app->sidebar_visual_width=app->sidebar_target_visible?app->workspace.sidebar_width:0.0;
    const MdJson *collapsed_dirs=md_json_get(root,"collapsed_directories");
    if (collapsed_dirs!=NULL&&collapsed_dirs->type==MD_JSON_ARRAY) {
        for (size_t i=0U;i<collapsed_dirs->as.array.len;++i) {
            const char *relative=md_json_string(collapsed_dirs->as.array.items[i]);
            if (relative!=NULL) (void)md_workspace_set_directory_collapsed(&app->workspace,relative,true);
        }
    }
    if (tabs!=NULL&&tabs->type==MD_JSON_ARRAY) for (size_t i=0U;i<tabs->as.array.len;++i) {
        const MdJson *entry=tabs->as.array.items[i]; const char *path=md_json_string(md_json_get(entry,"path")); if (path==NULL||path[0]=='\0'||!open_document(app,path)) continue;
        MdDocument *doc=active_document(app); uint64_t value=0U; const MdJson *number=NULL;
        if (md_json_u64(md_json_get(entry,"mode"),&value)&&value<=3U) doc->mode=(MdEditorMode)value;
        if (md_json_u64(md_json_get(entry,"cursor"),&value)) doc->cursor=MD_MIN((size_t)value,doc->source.len);
        if (md_json_u64(md_json_get(entry,"anchor"),&value)) doc->anchor=MD_MIN((size_t)value,doc->source.len);
        number=md_json_get(entry,"source_scroll"); if (number!=NULL&&number->type==MD_JSON_NUMBER) doc->source_scroll=MD_MAX(0.0,number->as.number);
        number=md_json_get(entry,"preview_scroll"); if (number!=NULL&&number->type==MD_JSON_NUMBER) doc->preview_scroll=MD_MAX(0.0,number->as.number);
        number=md_json_get(entry,"zoom"); if (number!=NULL&&number->type==MD_JSON_NUMBER) doc->zoom=MD_CLAMP(number->as.number,0.5,2.5);
        number=md_json_get(entry,"split_ratio"); if (number!=NULL&&number->type==MD_JSON_NUMBER) doc->split_ratio=MD_CLAMP(number->as.number,0.1,0.9);
    }
    if (app->doc_count>0U) app->active_doc=MD_MIN((size_t)active,app->doc_count-1U);
    MdDocument *doc=active_document(app); app->capsule_target=doc==NULL?0.0:(double)doc->mode; md_json_free(root);
}

static bool open_workspace(MdApp *app,const char *path) {
    char error[512]; MdWorkspace candidate; md_workspace_init(&candidate);
    if (!md_workspace_open(&candidate,path,error,sizeof(error))) { md_workspace_free(&candidate); modal_open(app,MD_MODAL_ERROR,error); return false; }
    if (app->workspace.root[0]!='\0'&&!save_workspace_session_snapshot(app,error,sizeof(error))) { md_workspace_free(&candidate); modal_open(app,MD_MODAL_ERROR,error); return false; }
    while (app->doc_count>0U) remove_document(app,app->doc_count-1U);
    candidate.recent_files=app->workspace.recent_files; candidate.recent_file_count=app->workspace.recent_file_count;
    candidate.recent_workspaces=app->workspace.recent_workspaces; candidate.recent_workspace_count=app->workspace.recent_workspace_count;
    app->workspace.recent_files=NULL; app->workspace.recent_file_count=0U; app->workspace.recent_workspaces=NULL; app->workspace.recent_workspace_count=0U;
    md_workspace_free(&app->workspace); app->workspace=candidate; (void)md_recent_add_workspace(&app->workspace,app->workspace.root); (void)md_recent_save(&app->workspace,error,sizeof(error));
    app->sidebar_target_visible=true; app->sidebar_visual_width=UI_SIDEBAR_DEFAULT;
    restore_workspace_session(app); for (size_t i=0U;i<app->doc_count;++i) assign_document_roots(app,app->docs[i]);
    app->recovery_scan_pending=true;
    app_toast(app,"Workspace: %s",app->workspace.root); app->dirty_frame=true; return true;
}

MdApp *md_app_create(void) {
    MdApp *app=calloc(1U,sizeof(*app)); if (app==NULL) return NULL;
    app->width=UI_DEFAULT_WIDTH; app->height=UI_DEFAULT_HEIGHT; app->running=true;
    app->focus=UI_FOCUS_NAV; app->sidebar_target_visible=true; app->sidebar_visual_width=UI_SIDEBAR_DEFAULT;
    app->drag_tab_from=-1; app->drag_tab_to=-1; app->diff_side_by_side=true;
    md_workspace_init(&app->workspace); md_buf_init(&app->modal_input); md_buf_init(&app->modal_secondary);
    md_buf_init(&app->clipboard_text); md_buf_init(&app->selection_drag_text); md_buf_init(&app->diff_base); md_buf_init(&app->diff_target);
    md_buf_init(&app->find.query); md_buf_init(&app->find.replacement); md_search_results_init(&app->find.results);
    md_version_list_init(&app->versions); md_recovery_list_init(&app->recoveries); md_diff_init(&app->diff);
    if (!md_buf_reserve(&app->modal_input,0U)||!md_buf_reserve(&app->modal_secondary,0U)||
        !md_buf_reserve(&app->clipboard_text,0U)||!md_buf_reserve(&app->selection_drag_text,0U)||
        !md_buf_reserve(&app->diff_base,0U)||!md_buf_reserve(&app->diff_target,0U)||
        !md_buf_reserve(&app->find.query,0U)||!md_buf_reserve(&app->find.replacement,0U)) {
        md_app_destroy(app); return NULL;
    }
    char warning[512]={0}; (void)md_preferences_load(&app->prefs,warning,sizeof(warning)); app->dark=app->prefs.dark_theme;
    char recent_warning[512]={0}; if (!md_recent_load(&app->workspace,recent_warning,sizeof(recent_warning))&&recent_warning[0]!='\0'&&warning[0]=='\0') (void)snprintf(warning,sizeof(warning),"%s",recent_warning);
    app_commands_init(app); if (warning[0]!='\0') app_toast(app,"%s",warning);
    return app;
}

static void close_fonts(MdApp *app) {
    if (app->font!=NULL) XftFontClose(app->display,app->font);
    if (app->font_bold!=NULL) XftFontClose(app->display,app->font_bold);
    if (app->font_mono!=NULL) XftFontClose(app->display,app->font_mono);
    if (app->font_heading!=NULL) XftFontClose(app->display,app->font_heading);
    if (app->font_symbols!=NULL) XftFontClose(app->display,app->font_symbols);
    if (app->font_emoji!=NULL) XftFontClose(app->display,app->font_emoji);
    app->font=NULL; app->font_bold=NULL; app->font_mono=NULL; app->font_heading=NULL; app->font_symbols=NULL; app->font_emoji=NULL;
}

void md_app_destroy(MdApp *app) {
    if (app==NULL) return;
    for (size_t i=0U;i<app->doc_count;++i) { md_document_free(app->docs[i]); free(app->docs[i]); }
    free(app->docs); md_workspace_free(&app->workspace); md_search_results_free(&app->find.results);
    md_buf_free(&app->find.query); md_buf_free(&app->find.replacement); md_buf_free(&app->modal_input); md_buf_free(&app->modal_secondary);
    md_buf_free(&app->clipboard_text); md_buf_free(&app->selection_drag_text); md_buf_free(&app->diff_base); md_buf_free(&app->diff_target);
    md_version_list_free(&app->versions); md_recovery_list_free(&app->recoveries); md_diff_free(&app->diff);
    if (app->display!=NULL) {
        if (app->xic!=NULL) XDestroyIC(app->xic);
        if (app->xim!=NULL) XCloseIM(app->xim);
        if (app->xft!=NULL) XftDrawDestroy(app->xft);
        close_fonts(app);
        if (app->back!=None) XFreePixmap(app->display,app->back);
        if (app->gc!=NULL) XFreeGC(app->display,app->gc);
        if (app->window!=None) XDestroyWindow(app->display,app->window);
        XCloseDisplay(app->display);
    }
    free(app);
}

static void layout_compute(MdApp *app) {
    int sidebar=app->workspace.root[0]=='\0'&&!app->outline_visible?0:(int)llround(app->sidebar_visual_width);
    int top=UI_NAV_HEIGHT+UI_TAB_HEIGHT+UI_TOOL_HEIGHT;
    int bottom=app->height-UI_STATUS_HEIGHT;
    app->layout.tabstrip=(UiRect){sidebar,UI_NAV_HEIGHT,app->width-sidebar,UI_TAB_HEIGHT};
    app->layout.toolbar=(UiRect){sidebar,UI_NAV_HEIGHT+UI_TAB_HEIGHT,app->width-sidebar,UI_TOOL_HEIGHT};
    app->layout.sidebar=(UiRect){0,UI_NAV_HEIGHT,sidebar,bottom-UI_NAV_HEIGHT};
    app->layout.status=(UiRect){sidebar,bottom,app->width-sidebar,UI_STATUS_HEIGHT};
    app->layout.content=(UiRect){sidebar,top,app->width-sidebar,bottom-top};
    MdDocument *doc=active_document(app);
    if (doc!=NULL&&doc->mode==MD_MODE_SPLIT) {
        int available=app->layout.content.w-7;
        int left=(int)llround((double)available*doc->split_ratio);
        if (available>=480) left=MD_CLAMP(left,240,available-240);
        else left=available/2;
        app->layout.source=(UiRect){sidebar,top,left,bottom-top};
        app->layout.divider=(UiRect){sidebar+left,top,7,bottom-top};
        app->layout.preview=(UiRect){sidebar+left+7,top,available-left,bottom-top};
    } else {
        app->layout.source=app->layout.content; app->layout.preview=app->layout.content;
        app->layout.divider=(UiRect){0,0,0,0};
    }
}

static UiButton *button_add(MdApp *app,UiRect rect,MdCommandId command,const char *label) {
    if (app->button_count>=UI_MAX_BUTTONS) return NULL;
    UiButton old={0};
    for (size_t i=0U;i<app->prior_button_count;++i) if (app->prior_buttons[i].command==command&&strcmp(app->prior_buttons[i].label,label)==0) old=app->prior_buttons[i];
    UiButton *button=&app->buttons[app->button_count++];
    *button=(UiButton){rect,command,"",app->commands[command].enabled,point_in(rect,app->pointer_x,app->pointer_y),old.pressed,old.focused,old.hover_progress};
    (void)snprintf(button->label,sizeof(button->label),"%s",label); return button;
}

static void draw_ripples(MdApp *app,UiRect rect,uint64_t now) {
    XRectangle clip={(short)rect.x,(short)rect.y,(unsigned short)MD_MAX(0,rect.w),(unsigned short)MD_MAX(0,rect.h)};
    XSetClipRectangles(app->display,app->gc,0,0,&clip,1,Unsorted);
    for (size_t i=0U;i<UI_MAX_RIPPLES;++i) {
        UiRipple *r=&app->ripples[i]; if (!r->active||memcmp(&r->clip,&rect,sizeof(rect))!=0) continue;
        double t=(double)(now-r->started)/480.0;
        if (t>=1.0) { r->active=false; continue; }
        double max_radius=hypot((double)rect.w,(double)rect.h); r->radius=max_radius*ease_out_cubic(t);
        UiColor ripple=app->dark?ui_color(app,95U+(unsigned)(50.0*(1.0-t)),106U,190U):ui_color(app,190U,198U,255U);
        XSetForeground(app->display,app->gc,ripple.pixel);
        int diameter=(int)llround(r->radius*2.0);
        XFillArc(app->display,app->back,app->gc,(int)llround(r->x-r->radius),(int)llround(r->y-r->radius),
                 (unsigned)MD_MAX(0,diameter),(unsigned)MD_MAX(0,diameter),0,360*64);
        app->dirty_frame=true;
    }
    XSetClipMask(app->display,app->gc,None);
}

static void draw_button(MdApp *app,UiButton *button,uint64_t now) {
    int elevation=(int)llround(button->hover_progress*2.0); UiRect rect=button->rect; rect.y-=elevation;
    if (button->hover_progress>0.02&&button->enabled) {
        UiRect shadow={rect.x+2,rect.y+4,rect.w,rect.h}; fill_round(app,shadow,10,app->palette.shadow);
    }
    UiColor fill=button->pressed?app->palette.accent_soft:(button->hovered?app->palette.raised:app->palette.surface);
    if (!button->enabled) fill=app->palette.window;
    fill_round(app,rect,10,fill);
    stroke_rect(app,rect,app->palette.border,1);
    if ((button->hovered&&button->enabled)||button->focused) {
        stroke_rect(app,(UiRect){rect.x-1,rect.y-1,rect.w+2,rect.h+2},button->focused?app->palette.accent:app->palette.border,2);
    }
    draw_ripples(app,button->rect,now);
    UiColor color=button->enabled?app->palette.text:app->palette.muted;
    int width=text_width(app,app->font_bold,button->label,strlen(button->label));
    draw_text_cstr(app,app->font_bold,color,rect.x+(rect.w-width)/2,rect.y+rect.h/2+5,button->label);
}

static unsigned mask_shift(unsigned long mask) {
    unsigned shift=0U; if (mask==0UL) return 0U;
    while ((mask&1UL)==0UL) { ++shift; mask>>=1U; }
    return shift;
}

static unsigned mask_bits(unsigned long mask) {
    unsigned bits=0U; while (mask!=0UL) { bits+=(unsigned)(mask&1UL); mask>>=1U; } return bits;
}

static uint8_t pixel_channel(unsigned long pixel,unsigned long mask) {
    unsigned shift=mask_shift(mask),bits=mask_bits(mask); if (bits==0U) return 0U;
    unsigned long value=(pixel&mask)>>shift; unsigned long maximum=(1UL<<bits)-1UL;
    return (uint8_t)((value*255UL+maximum/2UL)/maximum);
}

static unsigned long channel_pixel(uint8_t value,unsigned long mask) {
    unsigned shift=mask_shift(mask),bits=mask_bits(mask); if (bits==0U) return 0UL;
    unsigned long maximum=(1UL<<bits)-1UL; return (((unsigned long)value*maximum+127UL)/255UL<<shift)&mask;
}

static unsigned long ximage_pixel_fast(const XImage *image,int x,int y) {
    if (image->bits_per_pixel!=32) return XGetPixel((XImage *)image,x,y);
    const uint8_t *p=(const uint8_t *)image->data+(size_t)y*(size_t)image->bytes_per_line+(size_t)x*4U;
    if (image->byte_order==LSBFirst) return (unsigned long)p[0]|((unsigned long)p[1]<<8U)|((unsigned long)p[2]<<16U)|((unsigned long)p[3]<<24U);
    return ((unsigned long)p[0]<<24U)|((unsigned long)p[1]<<16U)|((unsigned long)p[2]<<8U)|(unsigned long)p[3];
}

static void ximage_put_pixel_fast(XImage *image,int x,int y,unsigned long pixel) {
    if (image->bits_per_pixel!=32) { XPutPixel(image,x,y,pixel); return; }
    uint8_t *p=(uint8_t *)image->data+(size_t)y*(size_t)image->bytes_per_line+(size_t)x*4U;
    if (image->byte_order==LSBFirst) { p[0]=(uint8_t)pixel; p[1]=(uint8_t)(pixel>>8U); p[2]=(uint8_t)(pixel>>16U); p[3]=(uint8_t)(pixel>>24U); }
    else { p[0]=(uint8_t)(pixel>>24U); p[1]=(uint8_t)(pixel>>16U); p[2]=(uint8_t)(pixel>>8U); p[3]=(uint8_t)pixel; }
}

static void blur_region(MdApp *app,UiRect region,int radius) {
    if (radius<=0||region.w<=1||region.h<=1) return;
    region.x=MD_CLAMP(region.x,0,app->width); region.y=MD_CLAMP(region.y,0,app->height);
    region.w=MD_MIN(region.w,app->width-region.x); region.h=MD_MIN(region.h,app->height-region.y);
    if (region.w<=1||region.h<=1) return;
    XImage *image=XGetImage(app->display,app->back,region.x,region.y,(unsigned)region.w,(unsigned)region.h,AllPlanes,ZPixmap);
    if (image==NULL) return;
    size_t pixels=0U,bytes=0U;
    if (!md_size_mul((size_t)region.w,(size_t)region.h,&pixels)||!md_size_mul(pixels,4U,&bytes)) { XDestroyImage(image); return; }
    uint8_t *source=malloc(bytes),*temp=malloc(bytes); if (source==NULL||temp==NULL) { free(source); free(temp); XDestroyImage(image); return; }
    for (int y=0;y<region.h;++y) for (int x=0;x<region.w;++x) {
        unsigned long p=ximage_pixel_fast(image,x,y); size_t at=((size_t)y*(size_t)region.w+(size_t)x)*4U;
        source[at]=pixel_channel(p,app->visual->red_mask); source[at+1U]=pixel_channel(p,app->visual->green_mask);
        source[at+2U]=pixel_channel(p,app->visual->blue_mask); source[at+3U]=255U;
    }
    for (int y=0;y<region.h;++y) for (int x=0;x<region.w;++x) {
        unsigned sums[3]={0U,0U,0U}; int count=0;
        for (int sx=MD_MAX(0,x-radius);sx<=MD_MIN(region.w-1,x+radius);++sx) {
            size_t at=((size_t)y*(size_t)region.w+(size_t)sx)*4U;
            sums[0]+=source[at]; sums[1]+=source[at+1U]; sums[2]+=source[at+2U]; ++count;
        }
        size_t out=((size_t)y*(size_t)region.w+(size_t)x)*4U;
        temp[out]=(uint8_t)(sums[0]/(unsigned)count); temp[out+1U]=(uint8_t)(sums[1]/(unsigned)count);
        temp[out+2U]=(uint8_t)(sums[2]/(unsigned)count); temp[out+3U]=255U;
    }
    for (int y=0;y<region.h;++y) for (int x=0;x<region.w;++x) {
        unsigned sums[3]={0U,0U,0U}; int count=0;
        for (int sy=MD_MAX(0,y-radius);sy<=MD_MIN(region.h-1,y+radius);++sy) {
            size_t at=((size_t)sy*(size_t)region.w+(size_t)x)*4U;
            sums[0]+=temp[at]; sums[1]+=temp[at+1U]; sums[2]+=temp[at+2U]; ++count;
        }
        uint8_t red=(uint8_t)(sums[0]/(unsigned)count),green=(uint8_t)(sums[1]/(unsigned)count),blue=(uint8_t)(sums[2]/(unsigned)count);
        unsigned long p=channel_pixel(red,app->visual->red_mask)|channel_pixel(green,app->visual->green_mask)|channel_pixel(blue,app->visual->blue_mask);
        ximage_put_pixel_fast(image,x,y,p);
    }
    XPutImage(app->display,app->back,app->gc,image,0,0,region.x,region.y,(unsigned)region.w,(unsigned)region.h);
    free(source); free(temp); XDestroyImage(image);
}

static void xft_clip(MdApp *app,UiRect rect) {
    XRectangle clip={(short)rect.x,(short)rect.y,(unsigned short)MD_MAX(0,rect.w),(unsigned short)MD_MAX(0,rect.h)};
    XftDrawSetClipRectangles(app->xft,0,0,&clip,1);
}

static void xft_unclip(MdApp *app) { XftDrawSetClip(app->xft,NULL); }

static size_t line_start_for_number(const MdDocument *doc,size_t number) {
    size_t at=0U,line=0U;
    while (at<doc->source.len&&line<number) { if (doc->source.data[at++]=='\n') ++line; }
    return at;
}

static void draw_selection_line(MdApp *app,const MdDocument *doc,UiRect area,size_t start,size_t end,int y,int line_height,int text_x) {
    MdRange selection=md_document_selection(doc); if (selection.start==selection.end||selection.end<=start||selection.start>=end) return;
    size_t a=MD_MAX(selection.start,start),b=MD_MIN(selection.end,end);
    int x1=text_x+text_width(app,app->font,doc->source.data+start,a-start);
    int x2=text_x+text_width(app,app->font,doc->source.data+start,b-start);
    UiRect highlight={x1,y+2,MD_MAX(2,x2-x1),line_height-3};
    if (highlight.x<area.x+area.w&&highlight.x+highlight.w>area.x) fill_round(app,highlight,3,app->palette.selection);
}

static void draw_source_editor(MdApp *app,UiRect area,bool editable) {
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    fill_rect(app,area,app->palette.editor); int gutter=58;
    fill_rect(app,(UiRect){area.x,area.y,gutter,area.h},app->palette.surface);
    int line_height=(int)llround((double)app->prefs.font_size*app->prefs.line_spacing*doc->zoom);
    line_height=MD_CLAMP(line_height,16,64); int first=(int)(doc->source_scroll/(double)line_height);
    size_t at=line_start_for_number(doc,(size_t)MD_MAX(0,first)); int y=area.y-(int)fmod(doc->source_scroll,(double)line_height);
    size_t line=(size_t)MD_MAX(0,first); int text_x=area.x+gutter+14;
    xft_clip(app,area);
    while (at<=doc->source.len&&y<area.y+area.h) {
        size_t end=at; while (end<doc->source.len&&doc->source.data[end]!='\n') ++end;
        size_t display_end=at; int visible_width=0,max_width=area.w-gutter-32;
        while (display_end<end&&display_end-at<4096U) {
            size_t next=md_grapheme_next(doc->source.data,doc->source.len,display_end);
            int glyph=text_width(app,app->font_mono,doc->source.data+display_end,next-display_end);
            if (visible_width+glyph>max_width) break;
            visible_width+=glyph; display_end=next;
        }
        draw_selection_line(app,doc,area,at,display_end,y,line_height,text_x);
        char number[32]; int n=snprintf(number,sizeof(number),"%zu",line+1U);
        draw_text(app,app->font_mono,app->palette.muted,area.x+gutter-10-text_width(app,app->font_mono,number,(size_t)n),y+line_height-5,number,(size_t)n);
        UiColor syntax=app->palette.text;
        size_t content=at; while (content<end&&(doc->source.data[content]==' '||doc->source.data[content]=='\t')) ++content;
        if (content<end&&doc->source.data[content]=='#') syntax=app->palette.accent;
        else if (content<end&&doc->source.data[content]=='>') syntax=app->palette.muted;
        else if (content+2U<end&&memcmp(doc->source.data+content,"```",3U)==0) syntax=app->palette.warning;
        else if (content<end&&(doc->source.data[content]=='-'||doc->source.data[content]=='*')) syntax=app->palette.link;
        draw_text(app,app->font_mono,syntax,text_x,y+line_height-5,doc->source.data+at,display_end-at);
        if (display_end<end) draw_text_cstr(app,app->font_mono,app->palette.muted,text_x+visible_width,y+line_height-5,"…");
        if (editable&&app->focus==UI_FOCUS_EDITOR&&doc->cursor>=at&&doc->cursor<=display_end&&doc->cursor==doc->anchor) {
            int cx=text_x+text_width(app,app->font_mono,doc->source.data+at,doc->cursor-at);
            XSetForeground(app->display,app->gc,app->palette.accent.pixel);
            XFillRectangle(app->display,app->back,app->gc,cx,y+3,2U,(unsigned)MD_MAX(2,line_height-5));
        }
        if (end==doc->source.len) break;
        at=end+1U; ++line; y+=line_height;
    }
    xft_unclip(app);
    size_t total_lines=doc->render.block_count==0U?1U:(size_t)doc->render.blocks[doc->render.block_count-1U].line_end+1U;
    double content_h=(double)total_lines*(double)line_height;
    if (content_h>(double)area.h) {
        int track_h=area.h-16; int thumb_h=MD_MAX(28,(int)((double)track_h*(double)area.h/content_h));
        double max_scroll=content_h-(double)area.h; int thumb_y=area.y+8+(int)((double)(track_h-thumb_h)*doc->source_scroll/MD_MAX(1.0,max_scroll));
        fill_round(app,(UiRect){area.x+area.w-8,area.y+8,4,track_h},2,app->palette.border);
        fill_round(app,(UiRect){area.x+area.w-10,thumb_y,8,thumb_h},4,app->palette.muted);
    }
}

static bool html_attribute(const char *source,size_t len,const char *name,MdBuf *out) {
    size_t name_len=strlen(name);
    for (size_t at=0U;at+name_len+2U<len;++at) {
        if (strncasecmp(source+at,name,name_len)!=0) continue;
        size_t p=at+name_len; while (p<len&&(source[p]==' '||source[p]=='\t')) ++p; if (p>=len||source[p]!='=') continue; ++p;
        while (p<len&&(source[p]==' '||source[p]=='\t')) ++p;
        if (p>=len||(source[p]!='\"'&&source[p]!='\'')) continue;
        char quote=source[p++]; size_t end=p; while (end<len&&source[end]!=quote) ++end; if (end>=len) return false;
        out->len=0U; out->data[0]='\0';
        for (size_t i=p;i<end;) {
            if (i+5U<=end&&memcmp(source+i,"&amp;",5U)==0) { if (!md_buf_append_char(out,'&')) return false; i+=5U; }
            else if (i+6U<=end&&memcmp(source+i,"&quot;",6U)==0) { if (!md_buf_append_char(out,'\"')) return false; i+=6U; }
            else if (i+4U<=end&&memcmp(source+i,"&lt;",4U)==0) { if (!md_buf_append_char(out,'<')) return false; i+=4U; }
            else { if (!md_buf_append_char(out,source[i])) return false; ++i; }
        }
        return true;
    }
    return false;
}

static bool markdown_escape_target(unsigned char c) {
    return (c>=0x21U&&c<=0x2fU)||(c>=0x3aU&&c<=0x40U)||
           (c>=0x5bU&&c<=0x60U)||(c>=0x7bU&&c<=0x7eU);
}

static bool append_markdown_unescaped(MdBuf *out,const char *source,size_t len) {
    for (size_t i=0U;i<len;++i) {
        if (source[i]=='\\'&&i+1U<len&&markdown_escape_target((unsigned char)source[i+1U])) ++i;
        if (!md_buf_append_char(out,source[i])) return false;
    }
    return true;
}

static bool image_source(const MdDocument *doc,const MdBlock *block,char *alt,size_t alt_cap,MdBuf *destination) {
    const char *s=doc->source.data+block->source_start; size_t len=block->source_end-block->source_start;
    size_t first=0U; while (first<len&&(s[first]==' '||s[first]=='\t')) ++first;
    if (first+4U<=len&&memcmp(s+first,"<img",4U)==0) {
        MdBuf alt_value; md_buf_init(&alt_value); if (!md_buf_reserve(&alt_value,0U)) return false;
        bool has_source=html_attribute(s+first,len-first,"src",destination); bool has_alt=html_attribute(s+first,len-first,"alt",&alt_value);
        if (has_alt) { size_t count=MD_MIN(alt_value.len,alt_cap-1U); memcpy(alt,alt_value.data,count); alt[count]='\0'; } else alt[0]='\0';
        md_buf_free(&alt_value); return has_source;
    }
    if (first+3U>len||s[first]!='!'||s[first+1U]!='['||alt_cap==0U) return false;
    size_t open=first+1U,middle=open+1U; bool escaped=false;
    while (middle<len) {
        if (escaped) escaped=false;
        else if (s[middle]=='\\') escaped=true;
        else if (s[middle]==']') break;
        ++middle;
    }
    if (middle+1U>=len||s[middle+1U]!='(') return false;
    size_t content=middle+2U,close=content; unsigned depth=1U; escaped=false;
    while (close<len&&depth!=0U) {
        if (escaped) escaped=false;
        else if (s[close]=='\\') escaped=true;
        else if (s[close]=='(') ++depth;
        else if (s[close]==')') --depth;
        if (depth!=0U) ++close;
    }
    if (depth!=0U) return false;
    size_t destination_start=content,destination_end=close;
    while (destination_start<destination_end&&
           (s[destination_start]==' '||s[destination_start]=='\t'||s[destination_start]=='\n'||s[destination_start]=='\r')) ++destination_start;
    while (destination_end>destination_start&&
           (s[destination_end-1U]==' '||s[destination_end-1U]=='\t'||s[destination_end-1U]=='\n'||s[destination_end-1U]=='\r')) --destination_end;
    if (destination_start<destination_end&&s[destination_start]=='<') {
        size_t angle=destination_start+1U; escaped=false;
        while (angle<destination_end) {
            if (escaped) escaped=false;
            else if (s[angle]=='\\') escaped=true;
            else if (s[angle]=='>') break;
            ++angle;
        }
        if (angle>=destination_end) return false;
        size_t tail=angle+1U;
        while (tail<destination_end&&(s[tail]==' '||s[tail]=='\t'||s[tail]=='\n'||s[tail]=='\r')) ++tail;
        if (tail<destination_end) {
            char title_open=s[tail],title_close=title_open=='('?')':title_open;
            if (title_open!='\''&&title_open!='"'&&title_open!='(') return false;
            size_t title_end=tail+1U; escaped=false;
            while (title_end<destination_end) {
                if (escaped) escaped=false;
                else if (s[title_end]=='\\') escaped=true;
                else if (s[title_end]==title_close) break;
                ++title_end;
            }
            if (title_end>=destination_end) return false;
            ++title_end;
            while (title_end<destination_end&&(s[title_end]==' '||s[title_end]=='\t'||s[title_end]=='\n'||s[title_end]=='\r')) ++title_end;
            if (title_end!=destination_end) return false;
        }
        destination_start+=1U; destination_end=angle;
    } else {
        unsigned nested=0U; escaped=false;
        for (size_t q=destination_start;q<destination_end;++q) {
            if (escaped) { escaped=false; continue; }
            if (s[q]=='\\') { escaped=true; continue; }
            if (s[q]=='(') { ++nested; continue; }
            if (s[q]==')'&&nested>0U) { --nested; continue; }
            if (nested!=0U||(s[q]!=' '&&s[q]!='\t'&&s[q]!='\n'&&s[q]!='\r')) continue;
            size_t title=q;
            while (title<destination_end&&(s[title]==' '||s[title]=='\t'||s[title]=='\n'||s[title]=='\r')) ++title;
            if (title>=destination_end||(s[title]!='\''&&s[title]!='"'&&s[title]!='(')) continue;
            char title_close=s[title]=='('?')':s[title]; size_t finish=title+1U; escaped=false;
            while (finish<destination_end) {
                if (escaped) escaped=false;
                else if (s[finish]=='\\') escaped=true;
                else if (s[finish]==title_close) break;
                ++finish;
            }
            if (finish>=destination_end) continue;
            size_t tail=finish+1U;
            while (tail<destination_end&&(s[tail]==' '||s[tail]=='\t'||s[tail]=='\n'||s[tail]=='\r')) ++tail;
            if (tail==destination_end) {
                destination_end=q;
                while (destination_end>destination_start&&
                       (s[destination_end-1U]==' '||s[destination_end-1U]=='\t'||s[destination_end-1U]=='\n'||s[destination_end-1U]=='\r')) --destination_end;
                break;
            }
        }
    }
    if (destination_end<=destination_start) return false;
    MdBuf alt_value; md_buf_init(&alt_value);
    bool ok=md_buf_reserve(&alt_value,0U)&&append_markdown_unescaped(&alt_value,s+open+1U,middle-open-1U)&&
            alt_value.len<alt_cap;
    if (ok) { memcpy(alt,alt_value.data,alt_value.len+1U); destination->len=0U; destination->data[0]='\0'; ok=append_markdown_unescaped(destination,s+destination_start,destination_end-destination_start); }
    md_buf_free(&alt_value); return ok;
}

static int image_persisted_width(const MdDocument *doc,const MdBlock *block) {
    const char *s=doc->source.data+block->source_start; size_t len=block->source_end-block->source_start; MdBuf width; md_buf_init(&width); if (!md_buf_reserve(&width,0U)) return 0;
    int result=0; if (html_attribute(s,len,"width",&width)&&width.len>0U) { char *end=NULL; long value=strtol(width.data,&end,10); if (end!=NULL&&*end=='\0'&&value>=48L&&value<=10000L) result=(int)value; }
    md_buf_free(&width); return result;
}

static bool load_render_image(const MdDocument *doc,const char *destination,MdImage *image) {
    char error[256]; MdBytes bytes; md_bytes_init(&bytes); bool ok=false;
    if (strncmp(destination,"data:",5U)==0) {
        MdImageFormat format=MD_IMAGE_UNKNOWN;
        ok=md_image_parse_data_uri(destination,strlen(destination),&format,&bytes,error,sizeof(error))&&
           md_image_decode(bytes.data,bytes.len,image,error,sizeof(error));
    } else if (strncmp(destination,"http://",7U)!=0&&strncmp(destination,"https://",8U)!=0) {
        char path[MD_PATH_MAX];
        if (destination[0]=='/') (void)snprintf(path,sizeof(path),"%s",destination);
        else { char dir[MD_PATH_MAX]; if (md_path_dirname(dir,doc->path)&&md_path_join(path,dir,destination)) ok=md_image_load(path,image,&bytes,error,sizeof(error)); }
    }
    md_bytes_free(&bytes); return ok;
}

static void put_scaled_image(MdApp *app,const MdImage *image,UiRect target) {
    if (target.w<=0||target.h<=0||image->rgba==NULL) return;
    size_t bytes=0U; if (!md_size_mul((size_t)target.w*(size_t)target.h,4U,&bytes)) return;
    char *data=calloc(1U,bytes); if (data==NULL) return;
    XImage *ximage=XCreateImage(app->display,app->visual,(unsigned)app->depth,ZPixmap,0,data,(unsigned)target.w,(unsigned)target.h,32,0);
    if (ximage==NULL) { free(data); return; }
    for (int y=0;y<target.h;++y) for (int x=0;x<target.w;++x) {
        uint32_t sx=(uint32_t)((uint64_t)(unsigned)x*image->width/(uint32_t)target.w);
        uint32_t sy=(uint32_t)((uint64_t)(unsigned)y*image->height/(uint32_t)target.h);
        const uint8_t *p=image->rgba+((size_t)sy*image->width+sx)*4U;
        unsigned long pixel=channel_pixel(p[0],app->visual->red_mask)|channel_pixel(p[1],app->visual->green_mask)|channel_pixel(p[2],app->visual->blue_mask);
        XPutPixel(ximage,x,y,pixel);
    }
    XPutImage(app->display,app->back,app->gc,ximage,0,0,target.x,target.y,(unsigned)target.w,(unsigned)target.h);
    XDestroyImage(ximage);
}

static int block_height(MdApp *app,const MdBlock *block) {
    int base=(int)llround((double)app->prefs.font_size*app->prefs.line_spacing);
    if (block->type==MD_BLOCK_HEADING||block->type==MD_BLOCK_SETEXT_HEADING) return base+16+(6-block->level)*2;
    if (block->type==MD_BLOCK_FENCED_CODE) return MD_MAX(base*3,(block->line_end-block->line_start+1)*base+24);
    if (block->type==MD_BLOCK_TABLE) return MD_MAX(base*3,MD_MAX(2,block->line_end-block->line_start)*(base+8)+18);
    if (block->type==MD_BLOCK_IMAGE) return 400;
    if (block->type==MD_BLOCK_BLANK) return base/2;
    return MD_MAX(base+8,(block->line_end-block->line_start+1)*base+8);
}

static bool table_physical_line(const MdDocument *doc,const MdBlock *block,size_t physical,
                                size_t *start,size_t *end) {
    size_t at=block->source_start,index=0U;
    while (at<block->source_end) {
        size_t line_end=at; while (line_end<block->source_end&&doc->source.data[line_end]!='\n') ++line_end;
        if (index==physical) { *start=at; *end=line_end; return true; }
        at=line_end<block->source_end?line_end+1U:block->source_end; ++index;
    }
    return false;
}

static bool table_physical_cell_range(const MdDocument *doc,const MdBlock *block,size_t physical,
                                      size_t wanted_col,MdRange *range,size_t *column_count) {
    size_t start=0U,end=0U;
    if (!table_physical_line(doc,block,physical,&start,&end)) return false;
    while (start<end&&(doc->source.data[start]==' '||doc->source.data[start]=='\t')) ++start;
    while (end>start&&(doc->source.data[end-1U]==' '||doc->source.data[end-1U]=='\t'||doc->source.data[end-1U]=='\r')) --end;
    if (start<end&&doc->source.data[start]=='|') ++start;
    if (end>start&&doc->source.data[end-1U]=='|') --end;
    size_t cell_start=start,col=0U; bool escaped=false; size_t ticks=0U; bool found=false;
    for (size_t p=start;p<=end;++p) {
        char c=p<end?doc->source.data[p]:'|';
        if (escaped) { escaped=false; continue; }
        if (c=='\\') { escaped=true; continue; }
        if (c=='`') { size_t run=1U; while (p+run<end&&doc->source.data[p+run]=='`') ++run; if (ticks==0U) ticks=run; else if (ticks==run) ticks=0U; p+=run-1U; continue; }
        if (c=='|'&&ticks==0U) {
            size_t a=cell_start,b=p; while (a<b&&(doc->source.data[a]==' '||doc->source.data[a]=='\t')) ++a; while (b>a&&(doc->source.data[b-1U]==' '||doc->source.data[b-1U]=='\t')) --b;
            if (col==wanted_col) { *range=(MdRange){a,b}; found=true; }
            ++col; cell_start=p+1U;
        }
    }
    if (column_count!=NULL) *column_count=col;
    return found;
}

static bool table_cell_range(const MdDocument *doc,const MdBlock *block,size_t logical_row,
                             size_t wanted_col,MdRange *range,size_t *column_count) {
    return table_physical_cell_range(doc,block,logical_row==0U?0U:logical_row+1U,
                                     wanted_col,range,column_count);
}

static size_t table_column_count(const MdDocument *doc,const MdBlock *block) {
    MdRange ignored; size_t columns=0U; (void)table_cell_range(doc,block,0U,SIZE_MAX,&ignored,&columns); return MD_MAX(columns,1U);
}

static int draw_inline_piece(MdApp *app,const char *text,size_t len,int x,int baseline,
                             int right,XftFont *font,UiColor color,bool strike,bool code) {
    size_t at=0U; int pen=x;
    while (at<len&&text[at]!='\n'&&text[at]!='\r') {
        size_t next=md_grapheme_next(text,len,at); int width=text_width(app,font,text+at,next-at);
        if (pen+width>right) { if (pen+18<=right) draw_text_cstr(app,font,app->palette.muted,pen,baseline,"…"); return right; }
        if (code) fill_round(app,(UiRect){pen-2,baseline-app->prefs.font_size-4,width+4,app->prefs.font_size+9},4,app->palette.code);
        draw_text(app,font,color,pen,baseline,text+at,next-at);
        if (strike) fill_rect(app,(UiRect){pen,baseline-app->prefs.font_size/3,width,1},color);
        pen+=width; at=next;
    }
    return pen;
}

static int draw_inline_markdown(MdApp *app,const char *source,size_t start,size_t end,
                                int x,int baseline,int right,XftFont *base_font,UiColor base_color) {
    bool bold=false,italic=false,strike=false; size_t at=start; int pen=x;
    while (at<end&&source[at]!='\n'&&source[at]!='\r'&&pen<right) {
        if (source[at]=='\\'&&at+1U<end) { size_t next=md_grapheme_next(source,end,at+1U); pen=draw_inline_piece(app,source+at+1U,next-at-1U,pen,baseline,right,bold?app->font_bold:base_font,base_color,strike,false); at=next; continue; }
        if (at+2U<=end&&((source[at]=='*'&&source[at+1U]=='*')||(source[at]=='_'&&source[at+1U]=='_'))) { bold=!bold; at+=2U; continue; }
        if (at+2U<=end&&source[at]=='~'&&source[at+1U]=='~') { strike=!strike; at+=2U; continue; }
        if (source[at]=='*'||source[at]=='_') { italic=!italic; ++at; continue; }
        if (source[at]=='`') {
            size_t run=1U; while (at+run<end&&source[at+run]=='`') ++run; size_t close=at+run;
            while (close+run<=end&&memcmp(source+close,source+at,run)!=0) ++close;
            if (close+run<=end) { pen=draw_inline_piece(app,source+at+run,close-at-run,pen,baseline,right,app->font_mono,app->palette.text,false,true); at=close+run; continue; }
        }
        bool image=at+2U<=end&&source[at]=='!'&&source[at+1U]=='['; size_t bracket=image?at+1U:at;
        if (source[bracket]=='[') {
            size_t close=bracket+1U; while (close<end&&source[close]!=']') { if (source[close]=='\\'&&close+1U<end) ++close; ++close; }
            if (close+1U<end&&source[close+1U]=='(') { size_t destination=close+2U,finish=destination; unsigned depth=1U; while (finish<end&&depth!=0U) { if (source[finish]=='(') ++depth; else if (source[finish]==')') --depth; if (depth!=0U) ++finish; } if (depth==0U) { if (image) pen=draw_inline_piece(app,"Image: ",7U,pen,baseline,right,app->font,app->palette.muted,false,false); pen=draw_inline_piece(app,source+bracket+1U,close-bracket-1U,pen,baseline,right,bold?app->font_bold:base_font,image?app->palette.muted:app->palette.link,strike,false); at=finish+1U; continue; } }
        }
        if (source[at]=='<'&&at+1U<end) { size_t close=at+1U; while (close<end&&source[close]!='>') ++close; if (close<end&&((close-at>7U&&memcmp(source+at+1U,"http://",7U)==0)||(close-at>8U&&memcmp(source+at+1U,"https://",8U)==0))) { pen=draw_inline_piece(app,source+at+1U,close-at-1U,pen,baseline,right,app->font,app->palette.link,false,false); at=close+1U; continue; } }
        size_t next=md_grapheme_next(source,end,at); UiColor color=italic?app->palette.muted:base_color;
        pen=draw_inline_piece(app,source+at,next-at,pen,baseline,right,bold?app->font_bold:base_font,color,strike,false); at=next;
    }
    return pen;
}

static int inline_prefix_width(MdApp *app,const char *source,size_t start,size_t end,XftFont *font) {
    MdBuf plain; md_buf_init(&plain); (void)md_buf_reserve(&plain,0U); size_t at=start;
    while (at<end&&source[at]!='\n'&&source[at]!='\r') {
        if (source[at]=='\\'&&at+1U<end) { size_t next=md_grapheme_next(source,end,at+1U); (void)md_buf_append(&plain,source+at+1U,next-at-1U); at=next; continue; }
        if (at+2U<=end&&((source[at]=='*'&&source[at+1U]=='*')||(source[at]=='_'&&source[at+1U]=='_')||(source[at]=='~'&&source[at+1U]=='~'))) { at+=2U; continue; }
        if (source[at]=='*'||source[at]=='_'||source[at]=='`') { ++at; continue; }
        bool image=at+2U<=end&&source[at]=='!'&&source[at+1U]=='['; size_t bracket=image?at+1U:at;
        if (source[bracket]=='[') { size_t close=bracket+1U; while (close<end&&source[close]!=']') ++close; if (close+1U<end&&source[close+1U]=='(') { size_t finish=close+2U; unsigned depth=1U; while (finish<end&&depth!=0U) { if (source[finish]=='(') ++depth; else if (source[finish]==')') --depth; ++finish; } if (depth==0U) { (void)md_buf_append(&plain,source+bracket+1U,close-bracket-1U); at=finish; continue; } } }
        size_t next=md_grapheme_next(source,end,at); (void)md_buf_append(&plain,source+at,next-at); at=next;
    }
    int width=text_width(app,font,plain.data,plain.len); md_buf_free(&plain); return width;
}

static void draw_table_block(MdApp *app,const MdDocument *doc,const MdBlock *block,UiRect area,int y) {
    int physical=block->line_end-block->line_start+1,rows=MD_MAX(1,physical-1),row_h=30;
    int width=MD_MIN(area.w-64,720),cols=(int)table_column_count(doc,block),col_w=MD_MAX(1,width/cols),x=area.x+32;
    fill_round(app,(UiRect){x,y,width,rows*row_h},8,app->palette.surface);
    for (int r=0;r<=rows;++r) { XSetForeground(app->display,app->gc,app->palette.border.pixel); XDrawLine(app->display,app->back,app->gc,x,y+r*row_h,x+width,y+r*row_h); }
    for (int c=0;c<=cols;++c) { XSetForeground(app->display,app->gc,app->palette.border.pixel); XDrawLine(app->display,app->back,app->gc,x+c*col_w,y,x+c*col_w,y+rows*row_h); }
    for (int row=0;row<rows;++row) {
        for (int col=0;col<cols;++col) {
            MdRange cell; if (!table_cell_range(doc,block,(size_t)row,(size_t)col,&cell,NULL)) continue;
            bool active=doc->cursor>=cell.start&&doc->cursor<=cell.end;
            if (active) fill_round(app,(UiRect){x+col*col_w+2,y+row*row_h+2,col_w-4,row_h-4},5,app->palette.accent_soft);
            draw_text_elided_n(app,app->font,row==0?app->palette.text:(active?app->palette.accent:app->palette.text),x+col*col_w+9,y+row*row_h+21,col_w-18,doc->source.data+cell.start,cell.end-cell.start);
        }
    }
}

static void draw_rendered(MdApp *app,UiRect area,bool editable) {
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    fill_rect(app,area,app->palette.editor); int margin=MD_CLAMP(area.w/12,24,96);
    double scroll=doc->preview_scroll; int y=area.y+28-(int)scroll; app->image_selection.rect=(UiRect){0,0,0,0};
    xft_clip(app,area);
    for (size_t i=0U;i<doc->render.block_count;++i) {
        const MdBlock *block=&doc->render.blocks[i]; int height=block_height(app,block);
        if (y+height>=area.y&&y<area.y+area.h) {
            int x=area.x+margin; int usable=area.w-2*margin;
            if (block->type==MD_BLOCK_BLANK) { }
            else if (block->type==MD_BLOCK_THEMATIC) {
                fill_rect(app,(UiRect){x,y+height/2,usable,2},app->palette.border);
            } else if (block->type==MD_BLOCK_FENCED_CODE||block->type==MD_BLOCK_INDENTED_CODE) {
                fill_round(app,(UiRect){x,y,usable,height-8},10,app->palette.code);
                size_t line_start=block->content_start; int baseline=y+28,line_height=MD_MAX(18,(int)llround((double)app->prefs.font_size*app->prefs.line_spacing));
                while (line_start<block->content_end&&baseline<y+height-12) { size_t line_end=line_start; while (line_end<block->content_end&&doc->source.data[line_end]!='\n'&&doc->source.data[line_end]!='\r') ++line_end; draw_text_elided_n(app,app->font_mono,app->palette.text,x+16,baseline,usable-32,doc->source.data+line_start,line_end-line_start); while (line_end<block->content_end&&(doc->source.data[line_end]=='\n'||doc->source.data[line_end]=='\r')) ++line_end; line_start=line_end; baseline+=line_height; }
            } else if (block->type==MD_BLOCK_TABLE) {
                draw_table_block(app,doc,block,area,y);
            } else if (block->type==MD_BLOCK_IMAGE) {
                char alt[256]; MdBuf destination; md_buf_init(&destination); (void)md_buf_reserve(&destination,0U); UiRect image_rect={x,y,MD_MIN(360,usable),height-18};
                if (image_source(doc,block,alt,sizeof(alt),&destination)) {
                    MdImage image; md_image_init(&image);
                    if (load_render_image(doc,destination.data,&image)) {
                        double ratio=(double)image.width/(double)image.height;
                        int persisted=image_persisted_width(doc,block); if (persisted>0) image_rect.w=MD_MIN(persisted,MD_MAX(48,usable));
                        if ((double)image_rect.w/ratio>(double)(height-18)) image_rect.w=MD_MAX(48,(int)((double)(height-18)*ratio));
                        image_rect.h=MD_MAX(36,(int)((double)image_rect.w/ratio));
                        if (app->image_selection.resizing&&app->image_selection.source_start==block->source_start) {
                            image_rect.w=app->image_selection.current_w; image_rect.h=app->image_selection.current_h;
                        }
                        put_scaled_image(app,&image,image_rect); md_image_free(&image);
                    } else {
                        fill_round(app,image_rect,10,app->palette.code); draw_text_cstr(app,app->font_bold,app->palette.error,x+18,y+42,"Missing image");
                        draw_text_elided(app,app->font,app->palette.muted,x+18,y+70,image_rect.w-36,alt);
                        draw_text_elided(app,app->font_mono,app->palette.muted,x+18,y+96,image_rect.w-36,destination.data);
                    }
                    if (app->image_selection.selected&&app->image_selection.source_start==block->source_start) {
                        stroke_rect(app,(UiRect){image_rect.x-3,image_rect.y-3,image_rect.w+6,image_rect.h+6},app->palette.accent,3);
                        UiRect handle={image_rect.x+image_rect.w-7,image_rect.y+image_rect.h-7,14,14}; fill_round(app,handle,4,app->palette.accent);
                    }
                    if ((!app->image_selection.selected&&app->image_selection.rect.w==0)||
                        (app->image_selection.selected&&app->image_selection.source_start==block->source_start)) {
                        app->image_selection.rect=image_rect;
                        if (!app->image_selection.selected) { app->image_selection.source_start=block->source_start; app->image_selection.source_end=block->source_end; }
                    }
                }
                md_buf_free(&destination);
            } else {
                UiColor color=block->type==MD_BLOCK_QUOTE?app->palette.muted:app->palette.text;
                XftFont *font=app->font;
                if (block->type==MD_BLOCK_HEADING||block->type==MD_BLOCK_SETEXT_HEADING) { color=app->palette.text; font=app->font_heading; }
                if (block->type==MD_BLOCK_QUOTE) fill_round(app,(UiRect){x-12,y+3,4,height-10},2,app->palette.accent);
                if (block->type==MD_BLOCK_TASK_ITEM) {
                    UiRect check={x,y+5,18,18}; fill_round(app,check,5,block->checked?app->palette.accent:app->palette.surface); stroke_rect(app,check,app->palette.border,1);
                    if (block->checked) draw_text_cstr(app,app->font_bold,app->palette.raised,x+3,y+20,"✓");
                    x+=28; usable-=28;
                } else if (block->type==MD_BLOCK_UL_ITEM||block->type==MD_BLOCK_OL_ITEM) { draw_text_cstr(app,app->font_bold,app->palette.accent,x,y+24,block->type==MD_BLOCK_UL_ITEM?"•":"1."); x+=26; usable-=26; }
                size_t len=block->content_end>block->content_start?block->content_end-block->content_start:0U;
                MdRange selection=md_document_selection(doc); if (selection.start!=selection.end&&selection.end>block->content_start&&selection.start<block->content_end) fill_round(app,(UiRect){x-5,y+2,usable+10,MD_MIN(height-5,34)},7,app->palette.selection);
                (void)draw_inline_markdown(app,doc->source.data,block->content_start,block->content_start+len,x,y+MD_MIN(height-8,30),x+usable,font,color);
                if (editable&&app->focus==UI_FOCUS_EDITOR&&doc->cursor>=block->content_start&&doc->cursor<=block->content_end&&doc->cursor==doc->anchor) {
                    int cx=x+inline_prefix_width(app,doc->source.data,block->content_start,doc->cursor,font);
                    fill_rect(app,(UiRect){cx,y+6,2,MD_MIN(height-12,28)},app->palette.accent);
                }
            }
        }
        y+=height;
    }
    xft_unclip(app);
    if (y-(area.y-(int)scroll)>area.h) {
        double content_h=(double)(y-area.y)+(double)scroll; int thumb_h=MD_MAX(28,(int)((double)(area.h-16)*(double)area.h/content_h));
        double max_scroll=content_h-(double)area.h; int thumb_y=area.y+8+(int)((double)(area.h-16-thumb_h)*scroll/MD_MAX(1.0,max_scroll));
        fill_round(app,(UiRect){area.x+area.w-8,area.y+8,4,area.h-16},2,app->palette.border);
        fill_round(app,(UiRect){area.x+area.w-10,thumb_y,8,thumb_h},4,app->palette.muted);
    }
}

static void draw_start_surface(MdApp *app) {
    UiRect area=app->layout.content; fill_rect(app,area,app->palette.window);
    int card_w=MD_MIN(720,area.w-80),card_h=MD_MIN(430,area.h-70); UiRect card={area.x+(area.w-card_w)/2,area.y+(area.h-card_h)/2,card_w,card_h};
    fill_round(app,(UiRect){card.x+5,card.y+8,card.w,card.h},20,app->palette.shadow);
    fill_round(app,card,20,app->palette.raised); stroke_rect(app,card,app->palette.border,1);
    draw_text_cstr(app,app->font_heading,app->palette.text,card.x+42,card.y+64,"Markdown, without a wall between writing and reading.");
    draw_text_cstr(app,app->font,app->palette.muted,card.x+42,card.y+96,"Native C17 · Custom X11 · Source-backed rendered editing");
    UiButton *button=button_add(app,(UiRect){card.x+42,card.y+132,150,42},MD_CMD_NEW,"New Document"); if (button!=NULL) draw_button(app,button,md_now_millis());
    button=button_add(app,(UiRect){card.x+204,card.y+132,140,42},MD_CMD_OPEN,"Open File"); if (button!=NULL) draw_button(app,button,md_now_millis());
    button=button_add(app,(UiRect){card.x+356,card.y+132,180,42},MD_CMD_OPEN_WORKSPACE,"Open Workspace"); if (button!=NULL) draw_button(app,button,md_now_millis());
    draw_text_cstr(app,app->font_bold,app->palette.text,card.x+42,card.y+222,"Recent Workspaces");
    if (app->workspace.recent_workspace_count==0U) draw_text_cstr(app,app->font,app->palette.muted,card.x+42,card.y+252,"No unavailable path can block startup; choose a workspace above.");
    else for (size_t i=0U;i<MD_MIN(app->workspace.recent_workspace_count,2U);++i) {
        struct stat st; bool available=stat(app->workspace.recent_workspaces[i],&st)==0&&S_ISDIR(st.st_mode); char recent[MD_PATH_MAX+32U];
        (void)snprintf(recent,sizeof(recent),"%s%s",app->workspace.recent_workspaces[i],available?"":"  (missing — activate to remove)");
        draw_text_elided(app,app->font,available?app->palette.link:app->palette.warning,card.x+42,card.y+252+(int)i*25,card.w-84,recent);
    }
    draw_text_cstr(app,app->font_bold,app->palette.text,card.x+42,card.y+306,"Recent Files");
    if (app->workspace.recent_file_count==0U) draw_text_cstr(app,app->font,app->palette.muted,card.x+42,card.y+336,"Files you open are de-duplicated and kept in most-recent order.");
    else for (size_t i=0U;i<MD_MIN(app->workspace.recent_file_count,3U);++i) {
        struct stat st; bool available=stat(app->workspace.recent_files[i],&st)==0&&S_ISREG(st.st_mode); char recent[MD_PATH_MAX+32U];
        (void)snprintf(recent,sizeof(recent),"%s%s",app->workspace.recent_files[i],available?"":"  (missing — activate to remove)");
        draw_text_elided(app,app->font,available?app->palette.link:app->palette.warning,card.x+42,card.y+336+(int)i*24,card.w-84,recent);
    }
}

static void draw_sidebar(MdApp *app,uint64_t now) {
    UiRect area=app->layout.sidebar; if (area.w<=0) return;
    fill_rect(app,area,app->palette.surface); fill_rect(app,(UiRect){area.x+area.w-1,area.y,1,area.h},app->palette.border);
    int padding=12,segment_w=MD_MAX(50,(area.w-2*padding)/2); UiRect selector={padding,area.y+12,area.w-2*padding,38};
    fill_round(app,selector,12,app->palette.window);
    UiRect active_pill={selector.x+(app->outline_visible?segment_w:0),selector.y,segment_w,selector.h}; fill_round(app,active_pill,12,app->palette.accent_soft);
    draw_text_cstr(app,app->font_bold,app->outline_visible?app->palette.muted:app->palette.accent,selector.x+22,selector.y+25,"Files");
    draw_text_cstr(app,app->font_bold,app->outline_visible?app->palette.accent:app->palette.muted,selector.x+segment_w+18,selector.y+25,"Outline");
    UiButton *collapse=button_add(app,(UiRect){area.w-42,area.y+56,30,30},MD_CMD_TOGGLE_FILES,"‹"); if (collapse!=NULL) draw_button(app,collapse,now);
    int y=area.y+94; MdDocument *doc=active_document(app);
    xft_clip(app,(UiRect){area.x,area.y+88,area.w,area.h-88});
    if (app->outline_visible) {
        if (doc==NULL||doc->render.heading_count==0U) draw_text_cstr(app,app->font,app->palette.muted,20,y+20,"No headings yet");
        else for (size_t i=0U;i<doc->render.heading_count&&y<area.y+area.h-24;++i) {
            const MdHeading *heading=&doc->render.headings[i]; int indent=16+(heading->level-1)*13;
            bool current=doc->cursor>=heading->source_offset&&(i+1U==doc->render.heading_count||doc->cursor<doc->render.headings[i+1U].source_offset);
            bool focused=app->focus==UI_FOCUS_SIDEBAR&&app->focus_index==(int)i;
            if (current||focused) fill_round(app,(UiRect){8,y-4,area.w-16,29},8,focused?app->palette.selection:app->palette.accent_soft);
            char label[300]; (void)snprintf(label,sizeof(label),"H%d  %s",heading->level,heading->label[0]=='\0'?"(empty heading)":heading->label);
            draw_text_elided(app,app->font,current?app->palette.accent:app->palette.text,indent,y+16,area.w-indent-14,label); y+=31;
        }
    } else {
        if (app->workspace.root[0]=='\0') draw_text_cstr(app,app->font,app->palette.muted,20,y+20,"Open a workspace to browse files");
        else {
            char root_name[MD_PATH_MAX]; (void)md_path_basename(root_name,app->workspace.root);
            draw_text_elided(app,app->font_bold,app->palette.text,16,y+18,area.w-32,root_name); y+=34;
            int visible_index=0;
            for (size_t i=0U;i<app->workspace.count&&y<area.y+area.h-20;++i) {
                if (!tree_entry_visible(&app->workspace,i)) continue;
                const MdTreeEntry *entry=&app->workspace.entries[i]; const char *base=strrchr(entry->path,'/'); base=base==NULL?entry->path:base+1;
                bool is_active=false;
                if (doc!=NULL&&doc->path[0]!='\0') { char path[MD_PATH_MAX]; is_active=md_path_join(path,app->workspace.root,entry->path)&&strcmp(path,doc->path)==0; }
                bool focused=app->focus==UI_FOCUS_SIDEBAR&&app->focus_index==visible_index;
                if (is_active||focused) fill_round(app,(UiRect){8,y-4,area.w-16,28},7,focused?app->palette.selection:app->palette.accent_soft);
                int indent=14+entry->depth*15; const char *glyph=entry->is_directory?(md_workspace_directory_collapsed(&app->workspace,entry->path)?"▸":"▾"):"•";
                draw_text_cstr(app,app->font,entry->is_directory?app->palette.accent:app->palette.muted,indent,y+15,glyph);
                draw_text_elided(app,app->font,is_active?app->palette.accent:app->palette.text,indent+18,y+15,area.w-indent-30,base); y+=29; ++visible_index;
            }
        }
    }
    xft_unclip(app);
    if (app->dragging_sidebar) fill_rect(app,(UiRect){area.x+area.w-3,area.y,3,area.h},app->palette.accent);
}

static UiRect tab_rect_at(const MdApp *app,size_t index) {
    int start=app->layout.tabstrip.x+12,width=180,gap=6;
    int usable=app->layout.tabstrip.w-70; int max_tabs=MD_MAX(1,usable/(width+gap));
    if ((int)index>=max_tabs) return (UiRect){0,0,0,0};
    return (UiRect){start+(int)index*(width+gap),app->layout.tabstrip.y+6,width,app->layout.tabstrip.h-9};
}

static void draw_tabs(MdApp *app) {
    UiRect area=app->layout.tabstrip; fill_rect(app,area,app->palette.window);
    if (app->doc_count==0U) { draw_text_cstr(app,app->font,app->palette.muted,area.x+18,area.y+27,"Start"); return; }
    for (size_t i=0U;i<app->doc_count;++i) {
        UiRect rect=tab_rect_at(app,i); if (rect.w==0) break;
        bool active=i==app->active_doc; bool hover=point_in(rect,app->pointer_x,app->pointer_y);
        fill_round(app,rect,10,active?app->palette.raised:(hover?app->palette.surface:app->palette.window));
        if (active) fill_round(app,(UiRect){rect.x+12,rect.y+rect.h-3,rect.w-24,3},2,app->palette.accent);
        if (app->docs[i]->dirty) fill_round(app,(UiRect){rect.x+12,rect.y+rect.h/2-3,7,7},4,app->palette.warning);
        draw_text_elided(app,app->font,active?app->palette.text:app->palette.muted,rect.x+26,rect.y+23,rect.w-58,app->docs[i]->display_name);
        draw_text_cstr(app,app->font,hover?app->palette.error:app->palette.muted,rect.x+rect.w-25,rect.y+23,"×");
        if (app->dragging_tab&&app->drag_tab_to==(int)i) fill_rect(app,(UiRect){rect.x-4,rect.y+3,3,rect.h-6},app->palette.accent);
    }
    int visible=MD_MAX(1,(area.w-70)/186);
    if ((int)app->doc_count>visible) {
        fill_round(app,(UiRect){area.x+area.w-52,area.y+7,40,30},9,app->palette.surface);
        draw_text_cstr(app,app->font_bold,app->palette.text,area.x+area.w-43,area.y+28,"⋯");
    }
}

static void draw_toolbar(MdApp *app,uint64_t now) {
    UiRect area=app->layout.toolbar; fill_rect(app,area,app->palette.surface); fill_rect(app,(UiRect){area.x,area.y+area.h-1,area.w,1},app->palette.border);
    if (active_document(app)==NULL) return;
    static const struct { MdCommandId id; const char *label; int width; } items[]={
        {MD_CMD_BOLD,"B",38},{MD_CMD_ITALIC,"I",38},{MD_CMD_STRIKE,"Strike",58},{MD_CMD_INLINE_CODE,"</>",48},
        {MD_CMD_LINK,"Link",58},{MD_CMD_INSERT_IMAGE,"Image",64},{MD_CMD_INSERT_TABLE,"Table",62},
        {MD_CMD_FIND,"Find",56},{MD_CMD_STATISTICS,"Stats",60},{MD_CMD_HISTORY,"History",72}
    };
    int x=area.x+12;
    for (size_t i=0U;i<MD_ARRAY_LEN(items);++i) {
        if (x+items[i].width>area.x+area.w-15) break;
        UiButton *button=button_add(app,(UiRect){x,area.y+7,items[i].width,34},items[i].id,items[i].label);
        if (button!=NULL) draw_button(app,button,now);
        x+=items[i].width+7;
    }
    MdDocument *doc=active_document(app);
    if (doc!=NULL) { char zoom[32]; (void)snprintf(zoom,sizeof(zoom),"%d%%",(int)llround(doc->zoom*100.0)); draw_text_cstr(app,app->font,app->palette.muted,area.x+area.w-62,area.y+29,zoom); }
}

static void draw_navigation(MdApp *app,uint64_t now) {
    int radius=(int)llround(app->nav_frost*4.0); if (radius>0) blur_region(app,(UiRect){0,0,app->width,UI_NAV_HEIGHT},radius);
    fill_rect(app,(UiRect){0,0,app->width,UI_NAV_HEIGHT},app->dark?(app->nav_frost>0.3?app->palette.surface:app->palette.window):app->palette.raised);
    if (app->nav_frost>0.02) fill_rect(app,(UiRect){0,UI_NAV_HEIGHT-2,app->width,2},app->palette.shadow);
    fill_round(app,(UiRect){18,15,42,42},13,app->palette.accent); draw_text_cstr(app,app->font_bold,app->palette.raised,29,43,"M");
    if (!app->compact_nav) draw_text_cstr(app,app->font_bold,app->palette.text,72,34,"Lattice Markdown");
    if (!app->compact_nav) draw_text_elided(app,app->font,app->palette.muted,72,53,215,"Native source-backed editor");
    int x=app->compact_nav?74:310;
    static const struct { MdCommandId id; const char *label; int width; } left[]={
        {MD_CMD_NEW,"New",58},{MD_CMD_OPEN,"Open",64},{MD_CMD_OPEN_WORKSPACE,"Workspace",112},{MD_CMD_SAVE,"Save",62}
    };
    for (size_t i=0U;i<MD_ARRAY_LEN(left);++i) {
        if (app->compact_nav&&i>1U) break;
        UiButton *button=button_add(app,(UiRect){x,18,left[i].width,36},left[i].id,left[i].label); if (button!=NULL) draw_button(app,button,now); x+=left[i].width+8;
    }
    MdDocument *doc=active_document(app); int mode_width=app->compact_nav?276:396; int right_margin=app->compact_nav?58:134;
    UiRect modes={app->width-right_margin-mode_width,16,mode_width,40};
    if (doc!=NULL&&modes.x>x+8) {
        fill_round(app,modes,13,app->palette.surface); int segment=modes.w/4;
        int capsule_x=modes.x+(int)llround(app->capsule_x*(double)segment);
        fill_round(app,(UiRect){capsule_x+2,modes.y+2,segment-4,modes.h-4},11,app->palette.accent_soft);
        static const char *short_names[]={"Source","Split","Preview","Rendered"};
        for (int i=0;i<4;++i) {
            UiRect rect={modes.x+i*segment,modes.y,segment,modes.h}; MdCommandId id=(MdCommandId)(MD_CMD_MODE_SOURCE+i);
            UiButton *button=button_add(app,rect,id,short_names[i]);
            if (button!=NULL) {
                button->enabled=true;
                draw_text_cstr(app,app->font,i==(int)doc->mode?app->palette.accent:app->palette.muted,
                               rect.x+(rect.w-text_width(app,app->font,short_names[i],strlen(short_names[i])))/2,rect.y+26,short_names[i]);
            }
        }
    }
    UiButton *more=button_add(app,(UiRect){app->width-50,18,36,36},MD_CMD_PALETTE,"⋯"); if (more!=NULL) draw_button(app,more,now);
    if (!app->compact_nav) { UiButton *theme=button_add(app,(UiRect){app->width-94,18,36,36},MD_CMD_TOGGLE_THEME,app->dark?"L":"D"); if (theme!=NULL) draw_button(app,theme,now); }
}

static void draw_status(MdApp *app) {
    UiRect area=app->layout.status; fill_rect(app,area,app->palette.surface); fill_rect(app,(UiRect){area.x,area.y,area.w,1},app->palette.border);
    MdDocument *doc=active_document(app); if (doc==NULL) { draw_text_cstr(app,app->font,app->palette.muted,area.x+14,area.y+21,"Ready"); return; }
    size_t line=md_document_line_for_offset(doc,doc->cursor),column=md_document_column_for_offset(doc,doc->cursor);
    if (app->stats_document!=doc||app->stats_generation!=doc->edit_generation) { md_statistics_compute(doc,&app->cached_statistics); app->stats_document=doc; app->stats_generation=doc->edit_generation; }
    MdStatistics stats=app->cached_statistics; MdRange selection=md_document_selection(doc);
    char left[256],right[256];
    if (selection.start!=selection.end) (void)snprintf(left,sizeof(left),"Ln %zu, Col %zu  ·  %zu selected",line,column,md_grapheme_count(doc->source.data+selection.start,selection.end-selection.start));
    else (void)snprintf(left,sizeof(left),"Ln %zu, Col %zu  ·  %zu words  ·  %zu lines",line,column,stats.words,stats.total_lines);
    (void)snprintf(right,sizeof(right),"%s%s%s  ·  UTF-8",doc->dirty?"● Unsaved":"Saved",doc->conflict?"  ⚠ Conflict":"",doc->orphaned?"  Missing":"");
    draw_text_cstr(app,app->font,doc->conflict||doc->orphaned?app->palette.warning:app->palette.muted,area.x+14,area.y+21,left);
    int width=text_width(app,app->font,right,strlen(right)); draw_text_cstr(app,app->font,app->palette.muted,area.x+area.w-width-14,area.y+21,right);
}

static void find_refresh(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    if (!md_document_find(doc,app->find.query.data,app->find.case_sensitive,app->find.whole_word,&app->find.results)) app_toast(app,"Search allocation failed");
}

static void draw_find(MdApp *app) {
    if (!app->find.visible) return;
    UiRect root=app->layout.content; int width=app->find.replace?610:470,height=app->find.replace?102:58;
    UiRect panel={root.x+root.w-width-22,root.y+14,width,height}; fill_round(app,(UiRect){panel.x+4,panel.y+5,panel.w,panel.h},14,app->palette.shadow);
    fill_round(app,panel,14,app->palette.raised); stroke_rect(app,panel,app->palette.border,1);
    UiRect input={panel.x+14,panel.y+12,238,34}; fill_round(app,input,9,app->palette.editor); stroke_rect(app,input,app->focus==UI_FOCUS_FIND&&!app->find.editing_replacement?app->palette.accent:app->palette.border,1);
    draw_text_elided(app,app->font,app->find.query.len==0U?app->palette.muted:app->palette.text,input.x+10,input.y+23,input.w-20,app->find.query.len==0U?"Find in Markdown source":app->find.query.data);
    char count[64]; (void)snprintf(count,sizeof(count),"%zu / %zu",app->find.results.count==0U?0U:app->find.results.active+1U,app->find.results.count);
    draw_text_cstr(app,app->font,app->palette.muted,panel.x+263,panel.y+34,count);
    draw_text_cstr(app,app->font_bold,app->palette.text,panel.x+326,panel.y+34,"↑");
    draw_text_cstr(app,app->font_bold,app->palette.text,panel.x+352,panel.y+34,"↓");
    draw_text_cstr(app,app->font_bold,app->find.case_sensitive?app->palette.accent:app->palette.text,panel.x+380,panel.y+34,"Aa");
    draw_text_cstr(app,app->font_bold,app->find.whole_word?app->palette.accent:app->palette.text,panel.x+410,panel.y+34,"Word");
    draw_text_cstr(app,app->font_bold,app->palette.text,panel.x+panel.w-24,panel.y+34,"×");
    if (app->find.replace) {
        UiRect replacement={panel.x+14,panel.y+58,238,32}; fill_round(app,replacement,9,app->palette.editor); stroke_rect(app,replacement,app->focus==UI_FOCUS_FIND&&app->find.editing_replacement?app->palette.accent:app->palette.border,1);
        draw_text_elided(app,app->font,app->find.replacement.len==0U?app->palette.muted:app->palette.text,replacement.x+10,replacement.y+22,replacement.w-20,app->find.replacement.len==0U?"Replace with":app->find.replacement.data);
        draw_text_cstr(app,app->font_bold,app->palette.accent,panel.x+274,panel.y+81,"Replace");
        draw_text_cstr(app,app->font_bold,app->palette.accent,panel.x+365,panel.y+81,"Replace All");
    }
}

static const char *modal_title(MdModalKind modal) {
    static const char *titles[]={"","Open File","Save As","Open Workspace","Unsaved Changes","Document Statistics","Version History","Compare Changes","Command Palette","Preferences","Keyboard Shortcuts","External Change Detected","Recovery Center","Something Went Wrong","Image Storage","Image Properties","Workspace File Action","Confirm Overwrite","Relative Image Relocation","Link Properties","Restore Historical Version"};
    return titles[(size_t)modal];
}

static void draw_command_palette(MdApp *app,UiRect rect) {
    UiRect input={rect.x+24,rect.y+64,rect.w-48,42}; fill_round(app,input,11,app->palette.editor); stroke_rect(app,input,app->palette.accent,2);
    draw_text_elided(app,app->font,app->modal_input.len==0U?app->palette.muted:app->palette.text,input.x+14,input.y+28,input.w-28,app->modal_input.len==0U?"Type a command…":app->modal_input.data);
    int y=input.y+58,visible=0; size_t query_len=app->modal_input.len;
    for (size_t i=0U;i<MD_CMD_COUNT&&visible<9;++i) {
        const MdCommand *command=&app->commands[i]; bool match=query_len==0U;
        if (!match) {
            for (size_t at=0U;command->label[at]!='\0';++at) if (strncasecmp(command->label+at,app->modal_input.data,query_len)==0) { match=true; break; }
        }
        if (!match) continue;
        if (visible==app->modal_selection) fill_round(app,(UiRect){rect.x+20,y-20,rect.w-40,35},8,app->palette.accent_soft);
        draw_text_cstr(app,app->font,command->enabled?app->palette.text:app->palette.muted,rect.x+34,y,command->label);
        if (command->shortcut[0]!='\0') { int sw=text_width(app,app->font,command->shortcut,strlen(command->shortcut)); draw_text_cstr(app,app->font,app->palette.muted,rect.x+rect.w-sw-34,y,command->shortcut); }
        y+=38; ++visible;
    }
}

static void draw_statistics(MdApp *app,UiRect rect) {
    MdDocument *doc=active_document(app); if (doc==NULL) return; MdStatistics s; md_statistics_compute(doc,&s);
    static const char *labels[]={"Raw characters","Rendered characters","Words","Total lines","Non-empty lines","Paragraphs","Headings","Images","Links","Code blocks"};
    size_t values[]={s.raw_characters,s.rendered_characters,s.words,s.total_lines,s.nonempty_lines,s.paragraphs,s.headings,s.images,s.links,s.fenced_code_blocks};
    for (size_t i=0U;i<MD_ARRAY_LEN(values);++i) {
        int col=(int)(i%2U),row=(int)(i/2U),x=rect.x+28+col*(rect.w/2-20),y=rect.y+78+row*58;
        draw_text_cstr(app,app->font,app->palette.muted,x,y,labels[i]); char value[64]; (void)snprintf(value,sizeof(value),"%zu",values[i]);
        draw_text_cstr(app,app->font_heading,app->palette.text,x,y+27,value);
    }
}

static bool ui_line_range(const MdBuf *source,size_t wanted,MdRange *range) {
    size_t line=0U,start=0U; while (line<wanted&&start<source->len) { while (start<source->len&&source->data[start]!='\n') ++start; if (start<source->len) ++start; ++line; }
    if (line!=wanted||start>=source->len) return false;
    size_t end=start; while (end<source->len&&source->data[end]!='\n') ++end; *range=(MdRange){start,end}; return true;
}

static size_t diff_changed_count(const MdDiff *diff) {
    size_t count=0U; for (size_t i=0U;i<diff->count;++i) if (diff->hunks[i].kind!=MD_DIFF_EQUAL) ++count; return count;
}

static size_t diff_changed_hunk(const MdDiff *diff,size_t selected) {
    size_t at=0U; for (size_t i=0U;i<diff->count;++i) if (diff->hunks[i].kind!=MD_DIFF_EQUAL) { if (at==selected) return i; ++at; } return diff->count;
}

static void draw_diff_source_line(MdApp *app,const MdBuf *source,size_t line,UiRect row,char prefix,UiColor background) {
    fill_rect(app,row,background); char number[48]; (void)snprintf(number,sizeof(number),"%c %5zu",prefix,line+1U);
    draw_text_cstr(app,app->font_mono,app->palette.muted,row.x+8,row.y+21,number); MdRange range;
    if (ui_line_range(source,line,&range)) draw_text_elided_n(app,app->font_mono,app->palette.text,row.x+82,row.y+21,row.w-92,source->data+range.start,range.end-range.start);
}

static void draw_diff(MdApp *app,UiRect rect) {
    size_t changed=diff_changed_count(&app->diff); if (changed>0U) app->diff_change=MD_MIN(app->diff_change,changed-1U); else app->diff_change=0U;
    char summary[192]; (void)snprintf(summary,sizeof(summary),"%s · Change %zu of %zu · ←/→ previous/next · Tab switches view",app->diff_side_by_side?"Side by side":"Inline",changed==0U?0U:app->diff_change+1U,changed);
    draw_text_cstr(app,app->font,app->palette.muted,rect.x+26,rect.y+62,summary);
    UiRect body={rect.x+22,rect.y+80,rect.w-44,rect.h-132}; fill_round(app,body,10,app->palette.editor);
    if (changed==0U) { draw_text_cstr(app,app->font_heading,app->palette.success,body.x+26,body.y+54,"No textual changes"); return; }
    size_t hunk_index=diff_changed_hunk(&app->diff,app->diff_change); if (hunk_index>=app->diff.count) return;
    MdDiffHunk primary=app->diff.hunks[hunk_index]; size_t a_start=primary.a_start,a_count=primary.a_count,b_start=primary.b_start,b_count=primary.b_count;
    if (primary.kind==MD_DIFF_DELETE&&hunk_index+1U<app->diff.count&&app->diff.hunks[hunk_index+1U].kind==MD_DIFF_INSERT) b_count=app->diff.hunks[hunk_index+1U].b_count;
    else if (primary.kind==MD_DIFF_INSERT&&hunk_index>0U&&app->diff.hunks[hunk_index-1U].kind==MD_DIFF_DELETE) { MdDiffHunk prior=app->diff.hunks[hunk_index-1U]; a_start=prior.a_start; a_count=prior.a_count; }
    if (app->diff_side_by_side) {
        int half=(body.w-8)/2; UiRect left={body.x,body.y,half,body.h},right={body.x+half+8,body.y,half,body.h};
        fill_round(app,left,8,app->palette.surface); fill_round(app,right,8,app->palette.surface);
        draw_text_cstr(app,app->font_bold,app->palette.text,left.x+16,left.y+28,"− Previous"); draw_text_cstr(app,app->font_bold,app->palette.text,right.x+16,right.y+28,"+ Current");
        int y=body.y+40,row_h=29; size_t rows=MD_MIN((size_t)12U,MD_MAX(MD_MAX(a_count,b_count),1U));
        for (size_t row=0U;row<rows;++row) {
            if (row<a_count) draw_diff_source_line(app,&app->diff_base,a_start+row,(UiRect){left.x+6,y,left.w-12,row_h},'-',app->palette.diff_delete);
            else fill_rect(app,(UiRect){left.x+6,y,left.w-12,row_h},app->palette.editor);
            if (row<b_count) draw_diff_source_line(app,&app->diff_target,b_start+row,(UiRect){right.x+6,y,right.w-12,row_h},'+',app->palette.diff_add);
            else fill_rect(app,(UiRect){right.x+6,y,right.w-12,row_h},app->palette.editor);
            y+=row_h+2;
        }
    } else {
        int y=body.y+14,row_h=29; size_t before=a_start>0U?1U:0U;
        if (before!=0U) { draw_diff_source_line(app,&app->diff_base,a_start-1U,(UiRect){body.x+10,y,body.w-20,row_h},' ',app->palette.surface); y+=row_h+2; }
        for (size_t row=0U;row<MD_MIN(a_count,(size_t)7U);++row) { draw_diff_source_line(app,&app->diff_base,a_start+row,(UiRect){body.x+10,y,body.w-20,row_h},'-',app->palette.diff_delete); y+=row_h+2; }
        for (size_t row=0U;row<MD_MIN(b_count,(size_t)7U);++row) { draw_diff_source_line(app,&app->diff_target,b_start+row,(UiRect){body.x+10,y,body.w-20,row_h},'+',app->palette.diff_add); y+=row_h+2; }
        MdRange next; if (ui_line_range(&app->diff_target,b_start+b_count,&next)&&y+row_h<body.y+body.h) draw_diff_source_line(app,&app->diff_target,b_start+b_count,(UiRect){body.x+10,y,body.w-20,row_h},' ',app->palette.surface);
    }
    if (a_count==1U&&b_count==1U) {
        MdRange a,b; if (ui_line_range(&app->diff_base,a_start,&a)&&ui_line_range(&app->diff_target,b_start,&b)) { MdDiff tokens; md_diff_init(&tokens); char error[128]; if (md_diff_tokens(app->diff_base.data+a.start,a.end-a.start,app->diff_target.data+b.start,b.end-b.start,&tokens,error,sizeof(error))) { size_t token_changes=diff_changed_count(&tokens); char detail[96]; (void)snprintf(detail,sizeof(detail),"Token refinement: %zu changed run(s)",token_changes); draw_text_cstr(app,app->font,app->palette.muted,body.x+16,body.y+body.h-14,detail); } md_diff_free(&tokens); }
    }
}

static void dim_blurred_background(MdApp *app,double progress) {
    static const char quarter[8]={0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44};
    static const char half[8]={0x55,(char)0xaa,0x55,(char)0xaa,0x55,(char)0xaa,0x55,(char)0xaa};
    static const char three_quarters[8]={0x77,(char)0xdd,0x77,(char)0xdd,0x77,(char)0xdd,0x77,(char)0xdd};
    const char *pattern=progress<0.34?quarter:progress<0.72?half:three_quarters;
    Pixmap stipple=XCreateBitmapFromData(app->display,app->back,pattern,8U,8U); if (stipple==None) return;
    UiColor dim=app->dark?ui_color(app,8U,11U,18U):ui_color(app,67U,75U,94U);
    XSetForeground(app->display,app->gc,dim.pixel); XSetStipple(app->display,app->gc,stipple); XSetFillStyle(app->display,app->gc,FillStippled);
    XFillRectangle(app->display,app->back,app->gc,0,0,(unsigned)app->width,(unsigned)app->height);
    XSetFillStyle(app->display,app->gc,FillSolid); XFreePixmap(app->display,stipple);
}

static void draw_modal(MdApp *app,uint64_t now) {
    MdModalKind kind=app->modal_closing?app->closing_modal:app->modal; if (kind==MD_MODAL_NONE) return;
    uint64_t duration=app->modal_closing?UI_MODAL_CLOSE_MS:UI_MODAL_OPEN_MS; double raw=(double)(now-app->modal_started)/(double)duration;
    if (raw>=1.0) {
        if (app->modal_closing) { app->modal=MD_MODAL_NONE; app->closing_modal=MD_MODAL_NONE; app->modal_closing=false; app->focus=app->focus_before_modal; app->dirty_frame=true; return; }
        raw=1.0;
    } else app->dirty_frame=true;
    double eased=ease_modal(raw); app->modal_progress=app->modal_closing?1.0-eased:eased;
    int blur=(int)llround(5.0*app->modal_progress); blur_region(app,(UiRect){0,0,app->width,app->height},blur);
    dim_blurred_background(app,app->modal_progress);
    int width=kind==MD_MODAL_DIFF?900:(kind==MD_MODAL_COMMAND_PALETTE?680:kind==MD_MODAL_SHORTCUTS?760:600);
    int height=kind==MD_MODAL_DIFF?620:(kind==MD_MODAL_COMMAND_PALETTE?500:kind==MD_MODAL_SHORTCUTS?620:kind==MD_MODAL_HISTORY?560:kind==MD_MODAL_RECOVERY?470:kind==MD_MODAL_STATISTICS?440:kind==MD_MODAL_PREFERENCES?430:360);
    width=MD_MIN(width,app->width-60); height=MD_MIN(height,app->height-50);
    double scale=0.95+0.05*app->modal_progress; int scaled_w=(int)llround((double)width*scale),scaled_h=(int)llround((double)height*scale);
    UiRect rect={(app->width-scaled_w)/2,(app->height-scaled_h)/2,scaled_w,scaled_h}; app->modal_rect=rect;
    fill_round(app,(UiRect){rect.x+7,rect.y+10,rect.w,rect.h},20,app->palette.shadow); fill_round(app,rect,20,app->palette.raised); stroke_rect(app,rect,app->palette.border,1);
    draw_text_cstr(app,app->font_heading,app->palette.text,rect.x+26,rect.y+40,modal_title(kind));
    draw_text_cstr(app,app->font_bold,app->palette.muted,rect.x+rect.w-38,rect.y+35,"×");
    if (kind==MD_MODAL_COMMAND_PALETTE) draw_command_palette(app,rect);
    else if (kind==MD_MODAL_STATISTICS) draw_statistics(app,rect);
    else if (kind==MD_MODAL_DIFF) draw_diff(app,rect);
    else if (kind==MD_MODAL_HISTORY) {
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+26,rect.y+66,"Snapshots every ≤20 versions · SHA-256 verified · Myers deltas + LZSS");
        int y=rect.y+98;
        if (app->versions.count==0U) draw_text_cstr(app,app->font,app->palette.muted,rect.x+34,y,"No saved versions yet. Press C to create an explicit version.");
        size_t start=app->modal_selection>=9?(size_t)app->modal_selection-9U:0U;
        for (size_t i=start;i<app->versions.count&&i<start+10U;++i) { char row[256]; (void)snprintf(row,sizeof(row),"%s Version %llu  ·  %s  ·  %llu bytes",app->versions.items[i].pinned?"◆":"○",(unsigned long long)app->versions.items[i].sequence,app->versions.items[i].full_snapshot?"Full snapshot":"Delta",(unsigned long long)app->versions.items[i].encoded_size); if ((int)i==app->modal_selection) fill_round(app,(UiRect){rect.x+20,y-22,rect.w-40,34},8,app->palette.accent_soft); draw_text_cstr(app,app->font,app->palette.text,rect.x+34,y,row); y+=39; }
        draw_text_cstr(app,app->font_bold,app->palette.accent,rect.x+26,rect.y+rect.h-27,"Enter Compare   R Restore   P Pin/Unpin   Del Delete   C Create");
    } else if (kind==MD_MODAL_PREFERENCES) {
        char rows[9][256];
        (void)snprintf(rows[0],sizeof(rows[0]),"Theme                         %s",app->dark?"Dark":"Light");
        (void)snprintf(rows[1],sizeof(rows[1]),"Font size                     %d px",app->prefs.font_size);
        (void)snprintf(rows[2],sizeof(rows[2]),"Line spacing                  %.2f",app->prefs.line_spacing);
        (void)snprintf(rows[3],sizeof(rows[3]),"Image insertion               %s",app->prefs.default_embed_images?"Embed in Markdown":"Relative asset");
        (void)snprintf(rows[4],sizeof(rows[4]),"Periodic autosave             %s",app->prefs.autosave_enabled?"Enabled":"Disabled");
        (void)snprintf(rows[5],sizeof(rows[5]),"Autosave interval             %d seconds",app->prefs.autosave_interval);
        (void)snprintf(rows[6],sizeof(rows[6]),"Default mode                  %s",mode_name(app->prefs.default_mode));
        (void)snprintf(rows[7],sizeof(rows[7]),"Synchronized split scrolling  %s",app->prefs.sync_scroll?"Enabled":"Disabled");
        (void)snprintf(rows[8],sizeof(rows[8]),"Restore workspace session     %s",app->prefs.restore_session?"Enabled":"Disabled");
        int y=rect.y+72; for (int i=0;i<9;++i) { UiRect row={rect.x+20,y-22,rect.w-40,29}; if (app->modal_selection==i) fill_round(app,row,8,app->palette.accent_soft); draw_text_cstr(app,app->font,app->modal_selection==i?app->palette.accent:app->palette.text,rect.x+30,y,rows[i]); y+=31; }
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+30,rect.y+rect.h-34,"←/→ changes the selected value · Tab moves focus · Enter saves");
    } else if (kind==MD_MODAL_SHORTCUTS) {
        int y=rect.y+68,col=0; for (size_t i=0U;i<MD_CMD_COUNT;++i) if (app->commands[i].shortcut[0]!='\0') { int x=rect.x+28+col*(rect.w/2); draw_text_elided(app,app->font,app->palette.text,x,y,210,app->commands[i].label); draw_text_cstr(app,app->font,app->palette.muted,x+215,y,app->commands[i].shortcut); y+=28; if (y>rect.y+rect.h-38) { y=rect.y+68; ++col; if (col>1) break; } }
    } else if (kind==MD_MODAL_RECOVERY) {
        draw_text_cstr(app,app->font,app->palette.warning,rect.x+28,rect.y+78,"Recovery records are integrity-checked before opening.");
        MdRecoveryInfo *info=selected_recovery(app); size_t valid=recovery_valid_count(app),ordinal=0U;
        if (info!=NULL) {
            for (size_t i=0U;i<=app->recovery_index&&i<app->recoveries.count;++i) if (app->recoveries.items[i].valid) ++ordinal;
            UiRect record={rect.x+24,rect.y+92,rect.w-48,104}; fill_round(app,record,10,app->palette.editor); stroke_rect(app,record,app->palette.accent,2);
            char heading[128],identity[MD_PATH_MAX+32],stamp[64]="timestamp unavailable"; time_t when=(time_t)info->timestamp; struct tm utc;
            (void)snprintf(heading,sizeof(heading),"Record %zu of %zu  ·  SHA-256 verified  ·  Up/Down selects",ordinal,valid);
            if (info->untitled||info->document_path[0]=='\0') (void)snprintf(identity,sizeof(identity),"Untitled · %.*s",16,info->document_id);
            else (void)snprintf(identity,sizeof(identity),"%s",info->document_path);
            if (gmtime_r(&when,&utc)!=NULL) (void)strftime(stamp,sizeof(stamp),"Recovered %Y-%m-%d %H:%M:%S UTC",&utc);
            draw_text_elided(app,app->font_bold,app->palette.accent,record.x+12,record.y+27,record.w-24,heading);
            draw_text_elided(app,app->font,app->palette.text,record.x+12,record.y+56,record.w-24,identity);
            draw_text_cstr(app,app->font_mono,app->palette.muted,record.x+12,record.y+84,stamp);
        }
        static const char *choices[]={"Open Recovered Content","Compare with Disk","Discard Record","Defer Decision"};
        for (int i=0;i<4;++i) { int col=i%2,row=i/2; UiRect choice={rect.x+24+col*(rect.w/2-20),rect.y+212+row*42,rect.w/2-36,34}; if (app->modal_selection==i) fill_round(app,choice,8,app->palette.accent_soft); draw_text_cstr(app,app->font,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+8,choice.y+23,choices[i]); }
        draw_text_elided(app,app->font,app->palette.muted,rect.x+28,rect.y+318,rect.w-56,app->modal_message[0]=='\0'?"A completed recovery write is available; authored files have not been overwritten.":app->modal_message);
    } else if (kind==MD_MODAL_IMAGE_STORAGE) {
        if (app->modal_selection==0) fill_round(app,(UiRect){rect.x+20,rect.y+58,rect.w-40,66},10,app->palette.accent_soft);
        if (app->modal_selection==1) fill_round(app,(UiRect){rect.x+20,rect.y+134,rect.w-40,66},10,app->palette.accent_soft);
        draw_text_cstr(app,app->font,app->modal_selection==0?app->palette.accent:app->palette.text,rect.x+28,rect.y+82,"Relative asset"); draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+106,"Copies the image into an assets folder and keeps Markdown readable.");
        draw_text_cstr(app,app->font,app->modal_selection==1?app->palette.accent:app->palette.text,rect.x+28,rect.y+158,"Embed in Markdown"); draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+182,"Stores the exact image bytes as a Base64 data URI in this file.");
        UiRect input={rect.x+28,rect.y+224,rect.w-56,38}; fill_round(app,input,9,app->palette.editor); stroke_rect(app,input,app->palette.accent,1); draw_text_elided(app,app->font,app->modal_input.len==0U?app->palette.muted:app->palette.text,input.x+12,input.y+25,input.w-24,app->modal_input.len==0U?"Image path…":app->modal_input.data);
    } else if (kind==MD_MODAL_EXTERNAL_CONFLICT) {
        draw_text_cstr(app,app->font,app->palette.warning,rect.x+28,rect.y+82,"The backing file changed after it was loaded or saved.");
        static const char *choices[]={"Reload from Disk","Keep Current","Compare","Explicit Overwrite","Save As","Cancel"};
        for (int i=0;i<6;++i) { int col=i%3,row=i/3; UiRect choice={rect.x+22+col*((rect.w-44)/3),rect.y+104+row*43,(rect.w-56)/3,35}; if (app->modal_selection==i) fill_round(app,choice,8,app->palette.accent_soft); draw_text_elided(app,app->font,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+7,choice.y+24,choice.w-14,choices[i]); }
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+226,"Ordinary Save is blocked until this conflict is resolved.");
    } else if (kind==MD_MODAL_IMAGE_PROPERTIES||kind==MD_MODAL_TREE_ACTION) {
        draw_text_elided(app,app->font,app->palette.text,rect.x+28,rect.y+82,rect.w-56,app->modal_message);
        UiRect input={rect.x+28,rect.y+112,rect.w-56,42}; fill_round(app,input,9,app->palette.editor); stroke_rect(app,input,app->palette.accent,2);
        draw_text_elided(app,app->font,app->modal_input.len==0U?app->palette.muted:app->palette.text,input.x+12,input.y+28,input.w-24,app->modal_input.len==0U?"Enter a value":app->modal_input.data);
        draw_text_cstr(app,app->font_bold,app->palette.accent,rect.x+28,rect.y+rect.h-32,"Enter: Apply     Esc: Cancel");
    } else if (kind==MD_MODAL_OVERWRITE) {
        draw_text_elided(app,app->font,app->palette.warning,rect.x+28,rect.y+84,rect.w-56,app->modal_message);
        static const char *choices[]={"Overwrite Existing File","Choose Another Path","Cancel"};
        for (int i=0;i<3;++i) { UiRect choice={rect.x+24,rect.y+112+i*43,rect.w-48,35}; if (app->modal_selection==i) fill_round(app,choice,8,app->palette.accent_soft); draw_text_cstr(app,app->font,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+10,choice.y+24,choices[i]); }
    } else if (kind==MD_MODAL_RELOCATION) {
        draw_text_cstr(app,app->font,app->palette.warning,rect.x+28,rect.y+78,"Relative image meaning changes when this Markdown file moves to another folder.");
        static const char *choices[]={"Copy assets and rebase references (recommended)","Keep references exactly after warning","Cancel Save As"};
        static const char *details[]={"Copies exact image bytes beside the new document and rewrites working relative paths.","Preserves source references verbatim; they may resolve to different or missing files.","Leaves the document, path, and assets unchanged."};
        for (int i=0;i<3;++i) { UiRect choice={rect.x+22,rect.y+98+i*68,rect.w-44,58}; if (app->modal_selection==i) fill_round(app,choice,10,app->palette.accent_soft); draw_text_cstr(app,app->font_bold,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+10,choice.y+23,choices[i]); draw_text_elided(app,app->font,app->palette.muted,choice.x+10,choice.y+45,choice.w-20,details[i]); }
    } else if (kind==MD_MODAL_LINK_PROPERTIES) {
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+76,"Display text");
        UiRect label={rect.x+28,rect.y+88,rect.w-56,42}; fill_round(app,label,9,app->palette.editor); stroke_rect(app,label,app->modal_selection==0?app->palette.accent:app->palette.border,app->modal_selection==0?2:1); draw_text_elided(app,app->font,app->palette.text,label.x+12,label.y+28,label.w-24,app->modal_input.data);
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+160,"Destination");
        UiRect destination={rect.x+28,rect.y+172,rect.w-56,42}; fill_round(app,destination,9,app->palette.editor); stroke_rect(app,destination,app->modal_selection==1?app->palette.accent:app->palette.border,app->modal_selection==1?2:1); draw_text_elided(app,app->font,app->palette.text,destination.x+12,destination.y+28,destination.w-24,app->modal_secondary.data);
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+246,"Tab changes field · Enter applies one source-backed transaction · Esc cancels");
    } else if (kind==MD_MODAL_HISTORY_RESTORE) {
        draw_text_elided(app,app->font,app->palette.warning,rect.x+28,rect.y+82,rect.w-56,app->modal_message);
        static const char *choices[]={"Restore as one undoable source transaction","Compare this version with current content","Cancel"};
        for (int i=0;i<3;++i) { UiRect choice={rect.x+24,rect.y+110+i*48,rect.w-48,39}; if (app->modal_selection==i) fill_round(app,choice,8,app->palette.accent_soft); draw_text_cstr(app,app->font,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+10,choice.y+26,choices[i]); }
        draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+278,"Restore never writes disk automatically; the restored document remains dirty until Save.");
    } else if (kind==MD_MODAL_ERROR) {
        draw_text_elided(app,app->font_bold,app->palette.error,rect.x+28,rect.y+84,rect.w-56,app->modal_message);
        if (strncmp(app->modal_message,"Save failed for ",16U)==0) {
            const char *detail=strstr(app->modal_message,": ");
            if (detail!=NULL) draw_text_elided(app,app->font_mono,app->palette.muted,rect.x+28,rect.y+111,rect.w-56,detail+2);
            fill_round(app,(UiRect){rect.x+24,rect.y+128,rect.w-48,92},10,app->palette.editor);
            draw_text_cstr(app,app->font,app->palette.text,rect.x+38,rect.y+158,"Safe-save did not replace the original file.");
            draw_text_cstr(app,app->font,app->palette.text,rect.x+38,rect.y+188,"Edited bytes remain dirty and available in memory.");
            draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+246,"After dismiss: Ctrl+S retries Save.");
            draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+272,"Ctrl+Shift+S opens Save As; Esc cancels.");
        } else {
            fill_round(app,(UiRect){rect.x+24,rect.y+108,rect.w-48,70},10,app->palette.editor);
            draw_text_cstr(app,app->font,app->palette.text,rect.x+38,rect.y+139,"The failed operation was stopped without an automatic retry.");
            draw_text_cstr(app,app->font,app->palette.muted,rect.x+28,rect.y+206,"Dismiss to return to the editor and choose another appropriate action.");
        }
        draw_text_cstr(app,app->font_bold,app->palette.accent,rect.x+28,rect.y+rect.h-32,"Enter: Dismiss     Esc: Cancel");
    } else {
        draw_text_elided(app,app->font,kind==MD_MODAL_ERROR?app->palette.error:app->palette.text,rect.x+28,rect.y+84,rect.w-56,app->modal_message);
        if (kind==MD_MODAL_OPEN_PATH||kind==MD_MODAL_SAVE_PATH||kind==MD_MODAL_WORKSPACE_PATH) {
            UiRect input={rect.x+28,rect.y+112,rect.w-56,42}; fill_round(app,input,9,app->palette.editor); stroke_rect(app,input,app->palette.accent,2);
            draw_text_elided(app,app->font,app->modal_input.len==0U?app->palette.muted:app->palette.text,input.x+12,input.y+28,input.w-24,app->modal_input.len==0U?"Enter an absolute or relative path":app->modal_input.data);
        }
        if (kind==MD_MODAL_UNSAVED) {
            static const char *choices[]={"Save","Discard","Cancel"};
            for (int i=0;i<3;++i) { UiRect choice={rect.x+24+i*((rect.w-48)/3),rect.y+116,(rect.w-64)/3,38}; if (app->modal_selection==i) fill_round(app,choice,9,app->palette.accent_soft); draw_text_cstr(app,app->font_bold,app->modal_selection==i?app->palette.accent:app->palette.text,choice.x+12,choice.y+26,choices[i]); }
        }
        else draw_text_cstr(app,app->font_bold,app->palette.accent,rect.x+28,rect.y+rect.h-32,"Enter: Continue     Esc: Cancel");
    }
}

static void update_window_state_property(MdApp *app) {
    MdDocument *doc=active_document(app); char state[2688],tab_bytes[UI_MAX_DOCS*65U]; uint8_t sha[32],tab_sha[32]; char hex[65],tab_hex[65]; size_t active_ripples=0U,tab_len=0U;
    const MdBlock *selected_image=selected_image_block(app);
    int persisted_image_width=doc!=NULL&&selected_image!=NULL?image_persisted_width(doc,selected_image):0;
    for (size_t i=0U;i<UI_MAX_RIPPLES;++i) if (app->ripples[i].active) ++active_ripples;
    if (doc!=NULL) md_sha256(doc->source.data,doc->source.len,sha); else md_sha256("",0U,sha); md_hex_encode(sha,32U,hex);
    for (size_t i=0U;i<app->doc_count&&tab_len+65U<=sizeof(tab_bytes);++i) { memcpy(tab_bytes+tab_len,app->docs[i]->id,64U); tab_len+=64U; tab_bytes[tab_len++]='\n'; }
    md_sha256(tab_bytes,tab_len,tab_sha); md_hex_encode(tab_sha,32U,tab_hex);
    int n=snprintf(state,sizeof(state),
        "{\"documents\":%zu,\"active\":%zu,\"mode\":\"%s\",\"dirty\":%s,\"conflict\":%s,\"orphaned\":%s,"
        "\"source_len\":%zu,\"cursor\":%zu,\"anchor\":%zu,\"undo\":%zu,\"redo\":%zu,"
        "\"modal\":%d,\"modal_selection\":%d,\"modal_progress\":%.3f,\"modal_closing\":%s,\"menu\":%d,\"focus\":%d,\"focus_index\":%d,"
        "\"workspace_entries\":%zu,\"outline\":%s,\"capsule_x\":%.3f,\"nav_frost\":%.3f,\"sidebar_width\":%.1f,"
        "\"zoom\":%.3f,\"split_ratio\":%.3f,\"source_scroll\":%.3f,\"preview_scroll\":%.3f,\"active_ripples\":%zu,\"xim_available\":%s,\"xic_available\":%s,"
        "\"find_visible\":%s,\"find_replace\":%s,\"find_replacement_focus\":%s,\"find_matches\":%zu,\"find_active\":%zu,"
        "\"image_selected\":%s,\"image_resizing\":%s,\"image_x\":%d,\"image_y\":%d,\"image_w\":%d,\"image_h\":%d,\"image_persisted_width\":%d,\"recoveries\":%zu,\"recovery_index\":%zu,"
        "\"activated_external\":%s,\"active_id\":\"%s\",\"tab_order_sha256\":\"%s\",\"source_sha256\":\"%s\"}",
        app->doc_count,app->active_doc,doc==NULL?"none":mode_name(doc->mode),doc!=NULL&&doc->dirty?"true":"false",
        doc!=NULL&&doc->conflict?"true":"false",doc!=NULL&&doc->orphaned?"true":"false",doc==NULL?0U:doc->source.len,
        doc==NULL?0U:doc->cursor,doc==NULL?0U:doc->anchor,doc==NULL?0U:doc->undo.len,doc==NULL?0U:doc->redo.len,
        (int)app->modal,app->modal_selection,app->modal_progress,app->modal_closing?"true":"false",(int)app->menu.kind,(int)app->focus,app->focus_index,
        app->workspace.count,app->outline_visible?"true":"false",app->capsule_x,app->nav_frost,app->sidebar_visual_width,
        doc==NULL?1.0:doc->zoom,doc==NULL?0.5:doc->split_ratio,doc==NULL?0.0:doc->source_scroll,doc==NULL?0.0:doc->preview_scroll,
        active_ripples,app->xim!=NULL?"true":"false",app->xic!=NULL?"true":"false",
        app->find.visible?"true":"false",app->find.replace?"true":"false",app->find.editing_replacement?"true":"false",app->find.results.count,app->find.results.active,
        app->image_selection.selected?"true":"false",app->image_selection.resizing?"true":"false",app->image_selection.rect.x,app->image_selection.rect.y,app->image_selection.rect.w,app->image_selection.rect.h,persisted_image_width,recovery_valid_count(app),app->recovery_index,
        app->last_activated_uri[0]!='\0'?"true":"false",doc==NULL?"":doc->id,tab_hex,hex);
    if (n>0&&(size_t)n<sizeof(state)) XChangeProperty(app->display,app->window,app->state_atom,app->utf8_string,8,PropModeReplace,(const unsigned char *)state,n);
}

static void draw_frame(MdApp *app) {
    uint64_t now=md_now_millis(); app->dirty_frame=false; commands_update(app); layout_compute(app);
    memcpy(app->prior_buttons,app->buttons,app->button_count*sizeof(*app->buttons)); app->prior_button_count=app->button_count; app->button_count=0U;
    fill_rect(app,(UiRect){0,0,app->width,app->height},app->palette.window);
    draw_tabs(app); draw_toolbar(app,now);
    MdDocument *doc=active_document(app);
    if (doc==NULL) draw_start_surface(app);
    else if (doc->mode==MD_MODE_SOURCE) draw_source_editor(app,app->layout.source,true);
    else if (doc->mode==MD_MODE_SPLIT) {
        draw_source_editor(app,app->layout.source,true); draw_rendered(app,app->layout.preview,false);
        fill_rect(app,app->layout.divider,app->dragging_divider?app->palette.accent:app->palette.border);
    } else if (doc->mode==MD_MODE_PREVIEW) draw_rendered(app,app->layout.preview,false);
    else draw_rendered(app,app->layout.preview,true);
    draw_sidebar(app,now); draw_status(app); draw_navigation(app,now); draw_find(app);
    if (app->menu.kind!=UI_MENU_NONE) {
        UiRect menu=app->menu.rect; fill_round(app,(UiRect){menu.x+4,menu.y+5,menu.w,menu.h},12,app->palette.shadow); fill_round(app,menu,12,app->palette.raised); stroke_rect(app,menu,app->palette.border,1);
        int y=menu.y+29; for (int i=0;i<app->menu.item_count&&i<8;++i) { if (i==app->menu.selected) fill_round(app,(UiRect){menu.x+7,y-21,menu.w-14,30},7,app->palette.accent_soft); draw_text_cstr(app,app->font,i==app->menu.selected?app->palette.accent:app->palette.text,menu.x+17,y,menu_label(app,i)); y+=34; }
    }
    if (app->modal!=MD_MODAL_NONE||app->modal_closing) draw_modal(app,now);
    if (app->toast_until>now&&app->toast[0]!='\0') {
        int width=MD_MIN(app->width-40,text_width(app,app->font,app->toast,strlen(app->toast))+36); UiRect toast={app->width-width-20,app->height-UI_STATUS_HEIGHT-58,width,42};
        fill_round(app,(UiRect){toast.x+3,toast.y+4,toast.w,toast.h},12,app->palette.shadow); fill_round(app,toast,12,app->palette.raised); draw_text_elided(app,app->font,app->palette.text,toast.x+16,toast.y+27,toast.w-32,app->toast); app->dirty_frame=true;
    }
    XCopyArea(app->display,app->back,app->window,app->gc,0,0,(unsigned)app->width,(unsigned)app->height,0,0);
    update_window_state_property(app); XFlush(app->display); app->last_frame_ms=now;
}

static void remove_document_recovery(MdDocument *doc) {
    if (doc->recovery_root[0]=='\0') return;
    MdRecoveryList list; md_recovery_list_init(&list); char warning[256];
    if (md_recovery_scan(doc->recovery_root,&list,warning,sizeof(warning))) {
        for (size_t i=0U;i<list.count;++i) {
            bool same_id=doc->id[0]!='\0'&&strcmp(list.items[i].document_id,doc->id)==0;
            bool same_path=doc->path[0]!='\0'&&strcmp(list.items[i].document_path,doc->path)==0;
            if (same_id||same_path) { char ignored[128]; (void)md_recovery_remove(&list.items[i],ignored,sizeof(ignored)); }
        }
    }
    md_recovery_list_free(&list);
}

static bool save_document(MdApp *app,MdDocument *doc,const char *path,bool overwrite) {
    bool changed=doc->dirty; char error[1024];
    if (!md_safe_save_document(doc,path,overwrite,error,sizeof(error))) {
        if (doc->conflict) modal_open(app,MD_MODAL_EXTERNAL_CONFLICT,error);
        else {
            const char *target=path!=NULL&&path[0]!='\0'?path:(doc->path[0]!='\0'?doc->path:doc->display_name);
            char message[1024];
            (void)snprintf(message,sizeof(message),"Save failed for %.280s: %.420s. The original file remains intact and edited data remains in memory. Dismiss this message, then Retry Save or use Save As.",target,error);
            modal_open(app,MD_MODAL_ERROR,message);
        }
        return false;
    }
    if (changed&&doc->history_root[0]!='\0'&&!md_history_create(doc->history_root,doc,false,NULL,error,sizeof(error))) app_toast(app,"Saved, but history failed: %s",error);
    else app_toast(app,"Saved %s",doc->display_name);
    remove_document_recovery(doc); app->dirty_frame=true; return true;
}

static bool save_document_relocated(MdApp *app,MdDocument *doc,const char *path,
                                    MdRelocationPolicy policy,bool overwrite) {
    bool changed=doc->dirty; char error[1024];
    if (!md_save_as_with_relocation(doc,path,policy,overwrite,error,sizeof(error))) { modal_open(app,MD_MODAL_ERROR,error); return false; }
    if (changed&&doc->history_root[0]!='\0'&&!md_history_create(doc->history_root,doc,false,NULL,error,sizeof(error))) app_toast(app,"Saved with relocated images, but history failed: %s",error);
    else app_toast(app,policy==MD_RELOCATE_COPY_REBASE?"Saved with copied/rebased image assets":"Saved while deliberately preserving relative image references");
    remove_document_recovery(doc); (void)md_recent_add_file(&app->workspace,doc->path); (void)md_recent_save(&app->workspace,error,sizeof(error)); app->dirty_frame=true; return true;
}

static void save_all(MdApp *app) {
    size_t saved=0U,failed=0U,untitled=0U;
    for (size_t i=0U;i<app->doc_count;++i) {
        MdDocument *doc=app->docs[i]; if (!doc->dirty) continue;
        if (doc->untitled||doc->path[0]=='\0') { ++untitled; continue; }
        if (save_document(app,doc,NULL,false)) ++saved; else ++failed;
    }
    if (untitled>0U) { app->pending_command=MD_CMD_SAVE_ALL; modal_open(app,MD_MODAL_SAVE_PATH,"Save All paused: choose a path for the active untitled document."); }
    else if (failed>0U) app_toast(app,"Save All: %zu saved, %zu failed; failed tabs remain dirty",saved,failed);
    else app_toast(app,"Save All complete: %zu document(s)",saved);
}

static void remember_closed_document(MdApp *app,const MdDocument *doc) {
    if (doc!=NULL&&!doc->untitled&&doc->path[0]!='\0')
        (void)snprintf(app->recently_closed_path,sizeof(app->recently_closed_path),"%s",doc->path);
}

static void request_close_active(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    if (!doc->dirty) { remember_closed_document(app,doc); remove_document(app,app->active_doc); return; }
    app->pending_close_index=app->active_doc; app->pending_exit=false;
    char message[512]; (void)snprintf(message,sizeof(message),"%s has unsaved changes. Saving failure will keep the tab open and preserve the buffer.",doc->display_name);
    modal_open(app,MD_MODAL_UNSAVED,message);
}

static void request_exit(MdApp *app) {
    size_t dirty=0U; for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty) ++dirty;
    if (dirty==0U) { app->running=false; return; }
    app->pending_exit=true; app->pending_close_index=app->active_doc;
    char message[512]; (void)snprintf(message,sizeof(message),"%zu document(s) have unsaved changes. Save attempts every file-backed tab; failed and untitled tabs remain available.",dirty);
    modal_open(app,MD_MODAL_UNSAVED,message);
}

static bool format_inline_code(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL) return false; MdRange r=md_document_selection(doc); size_t longest=0U,run=0U;
    for (size_t i=r.start;i<r.end;++i) { if (doc->source.data[i]=='`') { ++run; longest=MD_MAX(longest,run); } else run=0U; }
    size_t ticks=longest+1U; MdBuf delimiter; md_buf_init(&delimiter);
    for (size_t i=0U;i<ticks;++i) if (!md_buf_append_char(&delimiter,'`')) { md_buf_free(&delimiter); return false; }
    char error[512]; bool ok=md_document_format(doc,delimiter.data,delimiter.data,"Inline code",error,sizeof(error)); md_buf_free(&delimiter);
    if (!ok) modal_open(app,MD_MODAL_ERROR,error);
    return ok;
}

static void history_open(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL) return; md_version_list_free(&app->versions); md_version_list_init(&app->versions); char error[512];
    if (doc->history_root[0]=='\0'||!md_history_list(doc->history_root,doc,&app->versions,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
    else modal_open(app,MD_MODAL_HISTORY,"");
}

static bool history_compare_index(MdApp *app,size_t index) {
    MdDocument *doc=active_document(app); if (doc==NULL||index>=app->versions.count) return false;
    app->diff_base.len=0U; app->diff_base.data[0]='\0'; app->diff_target.len=0U; app->diff_target.data[0]='\0'; char error[512]={0};
    if (!md_history_reconstruct(&app->versions,index,&app->diff_base,error,sizeof(error))||
        !md_buf_assign(&app->diff_target,doc->source.data,doc->source.len)) { modal_open(app,MD_MODAL_ERROR,error[0]=='\0'?"Out of memory preparing historical comparison":error); return false; }
    md_diff_free(&app->diff); md_diff_init(&app->diff); app->diff_change=0U;
    if (!md_diff_lines(app->diff_base.data,app->diff_base.len,app->diff_target.data,app->diff_target.len,&app->diff,error,sizeof(error))) { modal_open(app,MD_MODAL_ERROR,error); return false; }
    modal_open(app,MD_MODAL_DIFF,""); return true;
}

static bool history_refresh(MdApp *app,size_t selection) {
    MdDocument *doc=active_document(app); char error[512]; md_version_list_free(&app->versions); md_version_list_init(&app->versions);
    if (doc==NULL||doc->history_root[0]=='\0'||!md_history_list(doc->history_root,doc,&app->versions,error,sizeof(error))) { modal_open(app,MD_MODAL_ERROR,error); return false; }
    app->modal_selection=app->versions.count==0U?0:(int)MD_MIN(selection,app->versions.count-1U); app->dirty_frame=true; return true;
}

static void diff_open_current(MdApp *app,bool side_by_side) {
    MdDocument *doc=active_document(app); if (doc==NULL) return; app->diff_side_by_side=side_by_side;
    app->diff_base.len=0U; app->diff_base.data[0]='\0'; app->diff_target.len=0U; app->diff_target.data[0]='\0';
    char error[512]; bool base=false;
    if (app->versions.count>0U) base=md_history_reconstruct(&app->versions,app->versions.count-1U,&app->diff_base,error,sizeof(error));
    static const char no_history[]="# Previous saved content\n\nNo earlier history snapshot is available.\n";
    if (!base) (void)md_buf_assign(&app->diff_base,no_history,sizeof(no_history)-1U);
    (void)md_buf_assign(&app->diff_target,doc->source.data,doc->source.len); md_diff_free(&app->diff); md_diff_init(&app->diff);
    if (!md_diff_lines(app->diff_base.data,app->diff_base.len,app->diff_target.data,app->diff_target.len,&app->diff,error,sizeof(error))) { modal_open(app,MD_MODAL_ERROR,error); return; }
    modal_open(app,MD_MODAL_DIFF,"");
}

static bool append_markdown_alt(MdBuf *out,const char *alt);
static bool append_markdown_destination(MdBuf *out,const char *destination);

static bool insert_image_from_modal(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL||app->modal_input.len==0U) return false;
    char image_path[MD_PATH_MAX]; if (app->modal_input.len>=sizeof(image_path)) { modal_open(app,MD_MODAL_ERROR,"Image path is too long"); return false; }
    memcpy(image_path,app->modal_input.data,app->modal_input.len+1U); char destination[MD_PATH_MAX]; MdBuf uri; md_buf_init(&uri); char error[1024]; bool ok=false;
    if (app->modal_selection==1) {
        MdImage image; md_image_init(&image); MdBytes bytes; md_bytes_init(&bytes);
        ok=md_image_load(image_path,&image,&bytes,error,sizeof(error))&&md_image_make_data_uri(image.format,bytes.data,bytes.len,&uri);
        md_image_free(&image); md_bytes_free(&bytes);
    } else {
        if (doc->path[0]=='\0') { md_buf_free(&uri); modal_open(app,MD_MODAL_ERROR,"Save the document first so a stable relative asset folder can be created, or choose Embed in Markdown."); return false; }
        ok=md_asset_import_relative(app->workspace.root,doc->path,image_path,destination,error,sizeof(error));
    }
    if (!ok) { md_buf_free(&uri); modal_open(app,MD_MODAL_ERROR,error); return false; }
    char basename[MD_PATH_MAX]; (void)md_path_basename(basename,image_path); MdBuf markdown; md_buf_init(&markdown);
    const char *target=app->modal_selection==1?uri.data:destination;
    MdRange selected=md_document_selection(doc);
    ok=md_buf_append_cstr(&markdown,"\n![")&&append_markdown_alt(&markdown,basename)&&
       md_buf_append_cstr(&markdown,"](")&&append_markdown_destination(&markdown,target)&&
       md_buf_append_cstr(&markdown,")\n")&&
       md_document_replace(doc,selected.start,selected.end,markdown.data,markdown.len,"Insert image",false,error,sizeof(error));
    md_buf_free(&markdown); md_buf_free(&uri); if (!ok) { modal_open(app,MD_MODAL_ERROR,error); return false; }
    modal_close(app); app_toast(app,"Inserted %s as %s",basename,app->modal_selection==1?"embedded Base64":"relative asset"); return true;
}

bool md_app_execute(MdApp *app,MdCommandId command) {
    if (app==NULL||command<0||command>=MD_CMD_COUNT) return false;
    commands_update(app);
    if (!app->commands[command].enabled) { app_toast(app,"%s is unavailable in the current context",app->commands[command].label); return false; }
    MdDocument *doc=active_document(app); char error[1024]={0}; bool ok=true;
    switch (command) {
        case MD_CMD_NEW: ok=add_new_document(app); break;
        case MD_CMD_OPEN: app->pending_command=command; modal_open(app,MD_MODAL_OPEN_PATH,"Choose a UTF-8 Markdown or text file."); break;
        case MD_CMD_OPEN_WORKSPACE: app->pending_command=command; modal_open(app,MD_MODAL_WORKSPACE_PATH,"Choose an existing folder. Dirty documents must be resolved before switching."); break;
        case MD_CMD_SAVE:
            if (doc->untitled||doc->path[0]=='\0') { app->pending_command=command; modal_open(app,MD_MODAL_SAVE_PATH,"Choose where to save this document."); }
            else ok=save_document(app,doc,NULL,false);
            break;
        case MD_CMD_SAVE_AS: app->pending_command=command; modal_open(app,MD_MODAL_SAVE_PATH,"Choose a destination. Relative-image relocation requires a deliberate policy."); break;
        case MD_CMD_SAVE_ALL: save_all(app); break;
        case MD_CMD_CLOSE_TAB: request_close_active(app); break;
        case MD_CMD_REOPEN_CLOSED: {
            char reopen[MD_PATH_MAX]; (void)snprintf(reopen,sizeof(reopen),"%s",app->recently_closed_path);
            if (open_document(app,reopen)) { app->recently_closed_path[0]='\0'; app_toast(app,"Reopened %s",reopen); }
            else ok=false;
            break;
        }
        case MD_CMD_UNDO: ok=md_document_undo(doc,error,sizeof(error)); if (!ok) app_toast(app,"%s",error); break;
        case MD_CMD_REDO: ok=md_document_redo(doc,error,sizeof(error)); if (!ok) app_toast(app,"%s",error); break;
        case MD_CMD_CUT:
        case MD_CMD_COPY: {
            MdRange r=md_document_selection(doc); ok=md_buf_assign(&app->clipboard_text,doc->source.data+r.start,r.end-r.start);
            if (ok) { XSetSelectionOwner(app->display,app->clipboard,app->window,CurrentTime); ok=XGetSelectionOwner(app->display,app->clipboard)==app->window; }
            if (ok&&command==MD_CMD_CUT) ok=md_document_replace(doc,r.start,r.end,"",0U,"Cut",false,error,sizeof(error));
            if (!ok) app_toast(app,command==MD_CMD_CUT?"Clipboard ownership failed; selection was not deleted":"Copy failed");
            break;
        }
        case MD_CMD_PASTE:
            if (XGetSelectionOwner(app->display,app->clipboard)==app->window) {
                if (app->modal!=MD_MODAL_NONE&&
                    (app->modal==MD_MODAL_OPEN_PATH||app->modal==MD_MODAL_SAVE_PATH||app->modal==MD_MODAL_WORKSPACE_PATH||
                     app->modal==MD_MODAL_COMMAND_PALETTE||app->modal==MD_MODAL_IMAGE_STORAGE||app->modal==MD_MODAL_IMAGE_PROPERTIES||
                     app->modal==MD_MODAL_TREE_ACTION||app->modal==MD_MODAL_LINK_PROPERTIES)) {
                    MdBuf *target=app->modal==MD_MODAL_LINK_PROPERTIES&&app->modal_selection==1?&app->modal_secondary:&app->modal_input;
                    append_input(target,app->clipboard_text.data,app->clipboard_text.len);
                } else if (app->focus==UI_FOCUS_FIND) {
                    MdBuf *target=app->find.editing_replacement?&app->find.replacement:&app->find.query;
                    append_input(target,app->clipboard_text.data,app->clipboard_text.len); if (!app->find.editing_replacement) find_refresh(app);
                } else ok=md_document_insert_utf8(doc,app->clipboard_text.data,app->clipboard_text.len,error,sizeof(error));
            }
            else { app->awaiting_paste=true; XConvertSelection(app->display,app->clipboard,app->utf8_string,app->clipboard_property,app->window,CurrentTime); }
            break;
        case MD_CMD_FIND: app->find.visible=true; app->find.replace=false; app->find.editing_replacement=false; app->focus=UI_FOCUS_FIND; find_refresh(app); break;
        case MD_CMD_REPLACE: app->find.visible=true; app->find.replace=true; app->find.editing_replacement=false; app->focus=UI_FOCUS_FIND; find_refresh(app); break;
        case MD_CMD_BOLD: ok=md_document_format(doc,"**","**","Bold",error,sizeof(error)); break;
        case MD_CMD_ITALIC: ok=md_document_format(doc,"*","*","Italic",error,sizeof(error)); break;
        case MD_CMD_STRIKE: ok=md_document_format(doc,"~~","~~","Strikethrough",error,sizeof(error)); break;
        case MD_CMD_INLINE_CODE: ok=format_inline_code(app); break;
        case MD_CMD_LINK: {
            MdBuf label,destination; md_buf_init(&label); md_buf_init(&destination); (void)md_buf_reserve(&label,0U); (void)md_buf_reserve(&destination,0U);
            if (md_document_link_at(doc,doc->cursor,&label,&destination)) {
                app->pending_action=UI_PENDING_LINK_EDIT; modal_open(app,MD_MODAL_LINK_PROPERTIES,"Edit link display text and destination.");
                (void)md_buf_append(&app->modal_input,label.data,label.len); (void)md_buf_append(&app->modal_secondary,destination.data,destination.len); app->modal_selection=0;
            } else ok=md_document_format(doc,"[","](<https://example.com>)","Link",error,sizeof(error));
            md_buf_free(&label); md_buf_free(&destination); break;
        }
        case MD_CMD_INSERT_IMAGE: app->pending_command=command; modal_open(app,MD_MODAL_IMAGE_STORAGE,"Choose the storage model and image path."); app->modal_selection=app->prefs.default_embed_images?1:0; break;
        case MD_CMD_INSERT_TABLE: {
            static const char table[]="\n| Column 1 | Column 2 |\n| :--- | ---: |\n| Value | Value |\n";
            ok=md_document_insert_utf8(doc,table,sizeof(table)-1U,error,sizeof(error)); break;
        }
        case MD_CMD_HEADING_1: case MD_CMD_HEADING_2: case MD_CMD_HEADING_3:
        case MD_CMD_HEADING_4: case MD_CMD_HEADING_5: case MD_CMD_HEADING_6:
            ok=md_document_heading_level(doc,(int)(command-MD_CMD_HEADING_1)+1,error,sizeof(error)); break;
        case MD_CMD_TOGGLE_TASK:
            ok=md_document_toggle_task(doc,doc->cursor,error,sizeof(error)); break;
        case MD_CMD_MODE_SOURCE: case MD_CMD_MODE_SPLIT: case MD_CMD_MODE_PREVIEW: case MD_CMD_MODE_RENDERED:
            doc->mode=(MdEditorMode)(command-MD_CMD_MODE_SOURCE); app->capsule_target=(double)doc->mode; if (app->xic!=NULL) (void)XmbResetIC(app->xic); break;
        case MD_CMD_STATISTICS: modal_open(app,MD_MODAL_STATISTICS,""); break;
        case MD_CMD_HISTORY: history_open(app); break;
        case MD_CMD_CREATE_VERSION:
            ok=doc->history_root[0]!='\0'&&md_history_create(doc->history_root,doc,true,NULL,error,sizeof(error)); if (ok) app_toast(app,"Version created"); break;
        case MD_CMD_TOGGLE_FILES: app->outline_visible=false; app->sidebar_target_visible=!app->sidebar_target_visible; break;
        case MD_CMD_TOGGLE_OUTLINE: app->outline_visible=true; app->sidebar_target_visible=true; break;
        case MD_CMD_TOGGLE_SYNC: app->prefs.sync_scroll=!app->prefs.sync_scroll; app_toast(app,"Synchronized scrolling %s",app->prefs.sync_scroll?"enabled":"disabled"); break;
        case MD_CMD_TOGGLE_THEME:
            app->dark=!app->dark; app->prefs.dark_theme=app->dark; palette_init(app); if (!md_preferences_save(&app->prefs,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); break;
        case MD_CMD_PREFERENCES: modal_open(app,MD_MODAL_PREFERENCES,""); break;
        case MD_CMD_SHORTCUTS: modal_open(app,MD_MODAL_SHORTCUTS,""); break;
        case MD_CMD_PALETTE: modal_open(app,MD_MODAL_COMMAND_PALETTE,""); break;
        case MD_CMD_EXPORT_SINGLE: case MD_CMD_EXPORT_ASSETS:
            app->pending_command=command; modal_open(app,MD_MODAL_SAVE_PATH,command==MD_CMD_EXPORT_SINGLE?"Export all supported local images into one Markdown file.":"Export a Markdown file plus an independent managed-assets directory."); break;
        case MD_CMD_CLEAR_RECENT_FILES: {
            md_recent_clear_files(&app->workspace); ok=md_recent_save(&app->workspace,error,sizeof(error));
            if (ok) app_toast(app,"Recent files cleared");
            break;
        }
        case MD_CMD_CLEAR_RECENT_WORKSPACES: {
            md_recent_clear_workspaces(&app->workspace); ok=md_recent_save(&app->workspace,error,sizeof(error));
            if (ok) app_toast(app,"Recent workspaces cleared");
            break;
        }
        case MD_CMD_COUNT: ok=false; break;
    }
    if (!ok&&error[0]!='\0'&&app->modal==MD_MODAL_NONE) modal_open(app,MD_MODAL_ERROR,error);
    app->dirty_frame=true; return ok;
}

static int palette_command_at(const MdApp *app,int wanted) {
    int visible=0; size_t query_len=app->modal_input.len;
    for (size_t i=0U;i<MD_CMD_COUNT;++i) {
        bool match=query_len==0U;
        if (!match) for (size_t at=0U;app->commands[i].label[at]!='\0';++at) if (strncasecmp(app->commands[i].label+at,app->modal_input.data,query_len)==0) { match=true; break; }
        if (match) { if (visible==wanted) return (int)i; ++visible; }
    }
    return -1;
}

static void modal_submit(MdApp *app) {
    MdDocument *doc=active_document(app); char path[MD_PATH_MAX],error[1024];
    if (app->modal==MD_MODAL_HISTORY) {
        if (app->versions.count>0U) (void)history_compare_index(app,(size_t)MD_CLAMP(app->modal_selection,0,(int)app->versions.count-1));
        return;
    } else if (app->modal==MD_MODAL_HISTORY_RESTORE&&doc!=NULL) {
        int choice=app->modal_selection%3; size_t index=app->pending_history_index;
        if (choice==2) { modal_close(app); return; }
        if (choice==1) { (void)history_compare_index(app,index); return; }
        MdBuf restored; md_buf_init(&restored);
        if (index>=app->versions.count||!md_history_reconstruct(&app->versions,index,&restored,error,sizeof(error))) { md_buf_free(&restored); modal_open(app,MD_MODAL_ERROR,error); return; }
        if (!md_document_replace(doc,0U,doc->source.len,restored.data,restored.len,"Restore historical version",false,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
        else { modal_close(app); app_toast(app,"Historical version restored as one undoable change; Save is required to update disk"); }
        md_buf_free(&restored); return;
    } else if (app->modal==MD_MODAL_OPEN_PATH) {
        if (app->modal_input.len>=sizeof(path)) { modal_open(app,MD_MODAL_ERROR,"Path is too long"); return; }
        memcpy(path,app->modal_input.data,app->modal_input.len+1U); if (open_document(app,path)) modal_close(app);
    } else if (app->modal==MD_MODAL_WORKSPACE_PATH) {
        if (app->modal_input.len>=sizeof(path)) { modal_open(app,MD_MODAL_ERROR,"Path is too long"); return; }
        memcpy(path,app->modal_input.data,app->modal_input.len+1U);
        bool dirty=false; for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty) dirty=true;
        if (dirty) { modal_open(app,MD_MODAL_UNSAVED,"Resolve all dirty tabs before switching workspaces. Cancel preserves the current workspace and every buffer."); return; }
        if (open_workspace(app,path)) modal_close(app);
    } else if (app->modal==MD_MODAL_SAVE_PATH&&doc!=NULL) {
        if (app->modal_input.len>=sizeof(path)) { modal_open(app,MD_MODAL_ERROR,"Path is too long"); return; }
        memcpy(path,app->modal_input.data,app->modal_input.len+1U);
        if (app->pending_command==MD_CMD_SAVE_AS&&doc->path[0]!='\0'&&md_document_has_relative_images(doc)) {
            char old_dir[MD_PATH_MAX],new_dir[MD_PATH_MAX],old_real[MD_PATH_MAX],new_real[MD_PATH_MAX];
            bool different=true;
            if (md_path_dirname(old_dir,doc->path)&&md_path_dirname(new_dir,path)) {
                const char *old_key=realpath(old_dir,old_real); if (old_key==NULL) old_key=old_dir;
                const char *new_key=realpath(new_dir,new_real); if (new_key==NULL) new_key=new_dir;
                different=strcmp(old_key,new_key)!=0;
            }
            if (different) { (void)snprintf(app->pending_path,sizeof(app->pending_path),"%s",path); modal_open(app,MD_MODAL_RELOCATION,"Choose how relative image references should behave after Save As."); return; }
        }
        struct stat destination_state;
        bool distinct=doc->path[0]=='\0'||strcmp(path,doc->path)!=0;
        if (distinct&&lstat(path,&destination_state)==0) {
            (void)snprintf(app->pending_path,sizeof(app->pending_path),"%s",path);
            modal_open(app,MD_MODAL_OVERWRITE,"The selected destination already exists. Overwriting requires explicit confirmation.");
            return;
        }
        bool ok=false;
        if (app->pending_action==UI_PENDING_IMAGE_SAVE_AS) ok=image_save_selected(app,path,false,error,sizeof(error));
        else if (app->pending_command==MD_CMD_EXPORT_SINGLE) ok=md_export_portable_single(doc,path,error,sizeof(error));
        else if (app->pending_command==MD_CMD_EXPORT_ASSETS) ok=md_export_portable_assets(doc,path,error,sizeof(error));
        else ok=save_document(app,doc,path,false);
        if (ok) {
            app->pending_action=UI_PENDING_NONE;
            if (app->pending_close_after_save&&app->pending_close_index<app->doc_count&&app->docs[app->pending_close_index]==doc) {
                remember_closed_document(app,doc); remove_document(app,app->pending_close_index); app->pending_close_after_save=false;
            }
            modal_close(app); if (app->pending_command==MD_CMD_SAVE_ALL) save_all(app);
        }
        else if (app->modal==MD_MODAL_SAVE_PATH) modal_open(app,MD_MODAL_ERROR,error);
    } else if (app->modal==MD_MODAL_IMAGE_STORAGE) (void)insert_image_from_modal(app);
    else if (app->modal==MD_MODAL_COMMAND_PALETTE) {
        int command=palette_command_at(app,app->modal_selection); if (command>=0&&app->commands[command].enabled) { modal_close(app); (void)md_app_execute(app,(MdCommandId)command); }
        else app_toast(app,"Disabled commands cannot execute");
    } else if (app->modal==MD_MODAL_UNSAVED) {
        int choice=app->modal_selection%3;
        if (choice==2) { app->pending_exit=false; modal_close(app); return; }
        if (choice==1) {
            if (app->pending_exit) { for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty) remove_document_recovery(app->docs[i]); app->running=false; }
            else if (app->pending_close_index<app->doc_count) { remove_document_recovery(app->docs[app->pending_close_index]); remove_document(app,app->pending_close_index); modal_close(app); }
            return;
        }
        if (app->pending_exit) { save_all(app); bool still_dirty=false; for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty) still_dirty=true; if (!still_dirty) app->running=false; }
        else if (app->pending_close_index<app->doc_count) { MdDocument *closing=app->docs[app->pending_close_index]; if (closing->untitled) { app->active_doc=app->pending_close_index; app->pending_command=MD_CMD_SAVE; app->pending_close_after_save=true; modal_open(app,MD_MODAL_SAVE_PATH,"Choose a path; the tab closes only after a successful save."); } else if (save_document(app,closing,NULL,false)) { remember_closed_document(app,closing); remove_document(app,app->pending_close_index); modal_close(app); } }
    } else if (app->modal==MD_MODAL_EXTERNAL_CONFLICT&&doc!=NULL) {
        int choice=app->modal_selection%6;
        if (choice==0) { bool action_ok=false; MdDocument loaded; md_document_init(&loaded,0U); if (md_document_load(&loaded,doc->path,error,sizeof(error))) { action_ok=md_document_set_source(doc,loaded.source.data,loaded.source.len,false,error,sizeof(error)); if (action_ok) { memcpy(doc->disk_sha256,loaded.disk_sha256,32U); doc->has_disk_sha256=true; doc->conflict=false; doc->orphaned=false; modal_close(app); } } md_document_free(&loaded); if (!action_ok) modal_open(app,MD_MODAL_ERROR,error); }
        else if (choice==1) { doc->conflict=true; modal_close(app); app_toast(app,"Current buffer kept; ordinary Save remains blocked until explicit resolution"); }
        else if (choice==2) { MdBytes disk; md_bytes_init(&disk); if (md_read_file(doc->path,&disk,error,sizeof(error))) { (void)md_buf_assign(&app->diff_base,(const char *)disk.data,disk.len); (void)md_buf_assign(&app->diff_target,doc->source.data,doc->source.len); md_diff_free(&app->diff); md_diff_init(&app->diff); if (md_diff_lines(app->diff_base.data,app->diff_base.len,app->diff_target.data,app->diff_target.len,&app->diff,error,sizeof(error))) modal_open(app,MD_MODAL_DIFF,""); else modal_open(app,MD_MODAL_ERROR,error); } else modal_open(app,MD_MODAL_ERROR,error); md_bytes_free(&disk); }
        else if (choice==3) { if (save_document(app,doc,NULL,true)) modal_close(app); }
        else if (choice==4) { app->pending_command=MD_CMD_SAVE_AS; modal_open(app,MD_MODAL_SAVE_PATH,"Save the in-memory version to a different path."); }
        else modal_close(app);
    } else if (app->modal==MD_MODAL_RECOVERY) {
        MdRecoveryInfo *info=selected_recovery(app);
        int choice=app->modal_selection%4;
        if (choice==3||info==NULL) { modal_close(app); return; }
        if (choice==2) { if (!md_recovery_remove(info,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { info->valid=false; modal_close(app); } return; }
        MdBuf recovered; md_buf_init(&recovered); if (!md_recovery_open(info,&recovered,error,sizeof(error))) { md_buf_free(&recovered); modal_open(app,MD_MODAL_ERROR,error); return; }
        if (choice==1&&info->document_path[0]!='\0') { MdBytes disk; md_bytes_init(&disk); if (md_read_file(info->document_path,&disk,error,sizeof(error))) { (void)md_buf_assign(&app->diff_base,(const char *)disk.data,disk.len); (void)md_buf_assign(&app->diff_target,recovered.data,recovered.len); md_diff_free(&app->diff); md_diff_init(&app->diff); (void)md_diff_lines(app->diff_base.data,app->diff_base.len,app->diff_target.data,app->diff_target.len,&app->diff,error,sizeof(error)); modal_open(app,MD_MODAL_DIFF,""); } else modal_open(app,MD_MODAL_ERROR,error); md_bytes_free(&disk); }
        else { if (!add_new_document(app)||!md_document_set_source(active_document(app),recovered.data,recovered.len,true,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { if (info->document_path[0]!='\0') (void)snprintf(active_document(app)->path,sizeof(active_document(app)->path),"%s",info->document_path); modal_close(app); app_toast(app,"Recovered bytes opened in a dirty tab; disk was not overwritten"); } }
        md_buf_free(&recovered);
    } else if (app->modal==MD_MODAL_PREFERENCES) {
        if (!md_preferences_save(&app->prefs,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { modal_close(app); app_toast(app,"Preferences saved"); }
    } else if (app->modal==MD_MODAL_LINK_PROPERTIES&&doc!=NULL) {
        if (app->modal_input.len==0U||app->modal_secondary.len==0U) modal_open(app,MD_MODAL_ERROR,"Link display text and destination must both be non-empty");
        else if (!md_document_edit_link(doc,doc->cursor,app->modal_input.data,app->modal_secondary.data,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
        else { app->pending_action=UI_PENDING_NONE; modal_close(app); app_toast(app,"Link label and destination updated in one undo transaction"); }
    } else if (app->modal==MD_MODAL_IMAGE_PROPERTIES) {
        if (app->pending_action==UI_PENDING_NONE) modal_close(app);
        else if (!image_apply_pending(app,app->pending_action,app->modal_input.data,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
        else { app->pending_action=UI_PENDING_NONE; modal_close(app); app_toast(app,"Image source updated in one undo transaction"); }
    } else if (app->modal==MD_MODAL_TREE_ACTION) {
        bool ok=false;
        if (app->pending_action==UI_PENDING_TREE_NEW_FILE||app->pending_action==UI_PENDING_TREE_NEW_FOLDER)
            ok=md_workspace_create_file(&app->workspace,app->modal_input.data,app->pending_action==UI_PENDING_TREE_NEW_FOLDER,error,sizeof(error));
        else if (app->pending_action==UI_PENDING_TREE_RENAME) {
            char old_full[MD_PATH_MAX],new_full[MD_PATH_MAX];
            bool paths=md_path_join(old_full,app->workspace.root,app->pending_path)&&md_path_join(new_full,app->workspace.root,app->modal_input.data);
            ok=paths&&md_workspace_rename(&app->workspace,app->pending_path,app->modal_input.data,error,sizeof(error));
            if (ok) {
                size_t old_len=strlen(old_full);
                for (size_t i=0U;i<app->doc_count;++i) {
                    MdDocument *open=app->docs[i];
                    if (strcmp(open->path,old_full)!=0&&!(strncmp(open->path,old_full,old_len)==0&&open->path[old_len]=='/')) continue;
                    char updated[MD_PATH_MAX]; int n=snprintf(updated,sizeof(updated),"%s%s",new_full,open->path+old_len);
                    if (n>0&&(size_t)n<sizeof(updated)) { (void)snprintf(open->path,sizeof(open->path),"%s",updated); char base[MD_PATH_MAX]; if (md_path_basename(base,updated)&&strlen(base)<sizeof(open->display_name)) strcpy(open->display_name,base); uint8_t digest[32]; md_sha256(updated,strlen(updated),digest); md_hex_encode(digest,32U,open->id); }
                }
            }
        } else if (app->pending_action==UI_PENDING_TREE_DELETE) {
            if (strcmp(app->modal_input.data,"DELETE")!=0) (void)snprintf(error,sizeof(error),"Deletion was not confirmed; type DELETE exactly");
            else {
                char full[MD_PATH_MAX]; bool joined=md_path_join(full,app->workspace.root,app->pending_path); size_t full_len=joined?strlen(full):0U;
                ok=joined&&md_workspace_delete(&app->workspace,app->pending_path,true,error,sizeof(error));
                if (ok) for (size_t i=0U;i<app->doc_count;++i) if (strcmp(app->docs[i]->path,full)==0||(strncmp(app->docs[i]->path,full,full_len)==0&&app->docs[i]->path[full_len]=='/')) app->docs[i]->orphaned=true;
            }
        }
        if (!ok) modal_open(app,MD_MODAL_ERROR,error); else { app->pending_action=UI_PENDING_NONE; modal_close(app); app_toast(app,"Workspace filesystem action completed"); }
    } else if (app->modal==MD_MODAL_OVERWRITE) {
        int choice=app->modal_selection%3;
        if (choice==2) { app->pending_action=UI_PENDING_NONE; modal_close(app); }
        else if (choice==1) { modal_open(app,MD_MODAL_SAVE_PATH,"Choose another destination path."); }
        else {
            bool ok=false;
            if (app->pending_action==UI_PENDING_IMAGE_SAVE_AS) ok=image_save_selected(app,app->pending_path,true,error,sizeof(error));
            else if (app->pending_action==UI_PENDING_SAVE_RELOCATION) ok=save_document_relocated(app,doc,app->pending_path,app->relocation_policy,true);
            else if (app->pending_command==MD_CMD_EXPORT_SINGLE) ok=md_export_portable_single(doc,app->pending_path,error,sizeof(error));
            else if (app->pending_command==MD_CMD_EXPORT_ASSETS) ok=md_export_portable_assets(doc,app->pending_path,error,sizeof(error));
            else ok=save_document(app,doc,app->pending_path,true);
            if (ok) { app->pending_action=UI_PENDING_NONE; modal_close(app); app_toast(app,"Existing destination overwritten after confirmation"); }
            else if (app->modal==MD_MODAL_OVERWRITE) modal_open(app,MD_MODAL_ERROR,error);
        }
    } else if (app->modal==MD_MODAL_RELOCATION) {
        int choice=app->modal_selection%3;
        if (choice==2) { app->pending_action=UI_PENDING_NONE; modal_close(app); return; }
        app->relocation_policy=choice==0?MD_RELOCATE_COPY_REBASE:MD_RELOCATE_KEEP_REFERENCES;
        app->pending_action=UI_PENDING_SAVE_RELOCATION;
        struct stat st;
        if (lstat(app->pending_path,&st)==0) { modal_open(app,MD_MODAL_OVERWRITE,"The Save As destination exists. Confirm overwrite after choosing the relocation policy."); return; }
        if (save_document_relocated(app,doc,app->pending_path,app->relocation_policy,false)) { app->pending_action=UI_PENDING_NONE; modal_close(app); }
    } else modal_close(app);
}

static size_t source_offset_at(MdApp *app,UiRect area,int x,int y) {
    MdDocument *doc=active_document(app); if (doc==NULL) return 0U;
    int line_height=MD_CLAMP((int)llround((double)app->prefs.font_size*app->prefs.line_spacing*doc->zoom),16,64);
    int line=(int)floor((doc->source_scroll+(double)(y-area.y))/(double)line_height); line=MD_MAX(0,line);
    size_t start=line_start_for_number(doc,(size_t)line),end=start; while (end<doc->source.len&&doc->source.data[end]!='\n') ++end;
    int target=x-(area.x+58+14); if (target<=0) return start;
    size_t at=start,best=start; int previous=0;
    while (at<end) { size_t next=md_grapheme_next(doc->source.data,doc->source.len,at); int width=text_width(app,app->font_mono,doc->source.data+start,next-start); if (target<(previous+width)/2) break; best=next; previous=width; at=next; }
    return best;
}

static const MdBlock *rendered_block_at(MdApp *app,UiRect area,int y,int *block_y) {
    MdDocument *doc=active_document(app); if (doc==NULL) return NULL; int top=area.y+28-(int)doc->preview_scroll;
    for (size_t i=0U;i<doc->render.block_count;++i) { int height=block_height(app,&doc->render.blocks[i]); if (y>=top&&y<top+height) { if (block_y!=NULL) *block_y=top; return &doc->render.blocks[i]; } top+=height; }
    return NULL;
}

static size_t rendered_offset_at(MdApp *app,UiRect area,int x,int y) {
    int block_y=0; const MdBlock *block=rendered_block_at(app,area,y,&block_y); MdDocument *doc=active_document(app); if (block==NULL||doc==NULL) return doc==NULL?0U:doc->source.len;
    int margin=MD_CLAMP(area.w/12,24,96),text_x=area.x+margin;
    if (block->type==MD_BLOCK_TASK_ITEM) text_x+=28; else if (block->type==MD_BLOCK_UL_ITEM||block->type==MD_BLOCK_OL_ITEM) text_x+=26;
    size_t start=block->content_start,end=block->content_end; if (start>end||end>doc->source.len) return block->source_start;
    while (end>start&&doc->source.data[end-1U]=='\n') --end;
    int target=x-text_x; if (target<=0) return start; size_t at=start,best=start; int previous=0;
    XftFont *font=(block->type==MD_BLOCK_HEADING||block->type==MD_BLOCK_SETEXT_HEADING)?app->font_heading:app->font;
    while (at<end&&doc->source.data[at]!='\n') { size_t next=md_grapheme_next(doc->source.data,doc->source.len,at); int width=text_width(app,font,doc->source.data+start,next-start); if (target<(previous+width)/2) break; best=next; previous=width; at=next; }
    (void)block_y; return best;
}

static void activate_external_uri(MdApp *app,const char *uri) {
    if (strncmp(uri,"http://",7U)!=0&&strncmp(uri,"https://",8U)!=0) { app_toast(app,"Only explicit HTTP/HTTPS links can be activated externally"); return; }
    (void)snprintf(app->last_activated_uri,sizeof(app->last_activated_uri),"%.500s",uri);
    if (!app->test_mode) {
        pid_t child=fork();
        if (child==0) { execlp("xdg-open","xdg-open",uri,(char *)NULL); _exit(127); }
        if (child<0) { app_toast(app,"Could not start the system link handler: %s",strerror(errno)); return; }
    }
    app_toast(app,"Activated external link: %.300s",uri); app->dirty_frame=true;
}

static void move_vertical(MdDocument *doc,int direction,bool extend) {
    size_t current_start=doc->cursor; while (current_start>0U&&doc->source.data[current_start-1U]!='\n') --current_start;
    size_t column=doc->cursor-current_start,target_start=current_start;
    if (direction<0) { if (current_start==0U) return; target_start=current_start-1U; while (target_start>0U&&doc->source.data[target_start-1U]!='\n') --target_start; }
    else { size_t end=current_start; while (end<doc->source.len&&doc->source.data[end]!='\n') ++end; if (end==doc->source.len) return; target_start=end+1U; }
    size_t target_end=target_start; while (target_end<doc->source.len&&doc->source.data[target_end]!='\n') ++target_end;
    size_t target=MD_MIN(target_start+column,target_end); while (target>target_start&&!md_utf8_is_boundary(doc->source.data,doc->source.len,target)) --target;
    doc->cursor=target; if (!extend) doc->anchor=target;
}

static void move_word(MdDocument *doc,int direction,bool extend) {
    size_t at=doc->cursor;
    if (direction<0) {
        if (at==0U) return;
        at=md_grapheme_prev(doc->source.data,doc->source.len,at); uint32_t cp=0U; size_t p=at; bool word=md_utf8_decode(doc->source.data,doc->source.len,&p,&cp)&&md_unicode_is_word_char(cp);
        while (at>0U) { size_t prev=md_grapheme_prev(doc->source.data,doc->source.len,at),q=prev; uint32_t before=0U; bool before_word=md_utf8_decode(doc->source.data,doc->source.len,&q,&before)&&md_unicode_is_word_char(before); if (before_word!=word) break; at=prev; }
    } else {
        if (at>=doc->source.len) return;
        size_t p=at; uint32_t cp=0U; bool word=md_utf8_decode(doc->source.data,doc->source.len,&p,&cp)&&md_unicode_is_word_char(cp); at=md_grapheme_next(doc->source.data,doc->source.len,at);
        while (at<doc->source.len) { size_t q=at; uint32_t after=0U; bool after_word=md_utf8_decode(doc->source.data,doc->source.len,&q,&after)&&md_unicode_is_word_char(after); if (after_word!=word) break; at=md_grapheme_next(doc->source.data,doc->source.len,at); }
    }
    doc->cursor=at; if (!extend) doc->anchor=at;
}

static bool editor_tab(MdApp *app,bool reverse);

static bool editor_enter(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL) return false; char error[512]; const char *insert="\n"; size_t len=1U; MdBuf structural; md_buf_init(&structural);
    const MdBlock *block=NULL; for (size_t i=0U;i<doc->render.block_count;++i) if (doc->cursor>=doc->render.blocks[i].source_start&&doc->cursor<=doc->render.blocks[i].source_end) { block=&doc->render.blocks[i]; break; }
    if (doc->mode==MD_MODE_RENDERED&&block!=NULL) {
        if (block->type==MD_BLOCK_PARAGRAPH||block->type==MD_BLOCK_HEADING||block->type==MD_BLOCK_SETEXT_HEADING) { insert="\n\n"; len=2U; }
        else if (block->type==MD_BLOCK_UL_ITEM||block->type==MD_BLOCK_OL_ITEM||block->type==MD_BLOCK_TASK_ITEM) {
            size_t prefix_end=block->content_start; size_t prefix_len=prefix_end-block->source_start;
            if (block->content_start==block->content_end) { insert="\n"; len=1U; }
            else if (md_buf_append_char(&structural,'\n')&&md_buf_append(&structural,doc->source.data+block->source_start,prefix_len)) { insert=structural.data; len=structural.len; }
        } else if (block->type==MD_BLOCK_TABLE) {
            md_buf_free(&structural); return editor_tab(app,false);
        }
    }
    bool ok=md_document_insert_utf8(doc,insert,len,error,sizeof(error)); md_buf_free(&structural); if (!ok) modal_open(app,MD_MODAL_ERROR,error); return ok;
}

static bool editor_tab(MdApp *app,bool reverse) {
    MdDocument *doc=active_document(app); if (doc==NULL) return false; const MdBlock *block=NULL;
    for (size_t i=0U;i<doc->render.block_count;++i) if (doc->cursor>=doc->render.blocks[i].source_start&&doc->cursor<=doc->render.blocks[i].source_end) { block=&doc->render.blocks[i]; break; }
    char error[512];
    if (block!=NULL&&block->type==MD_BLOCK_TABLE) {
        size_t columns=table_column_count(doc,block),rows=(size_t)MD_MAX(1,block->line_end-block->line_start),row=0U,col=0U; bool found=false;
        for (size_t r=0U;r<rows&&!found;++r) for (size_t c=0U;c<columns;++c) { MdRange cell; if (table_cell_range(doc,block,r,c,&cell,NULL)&&doc->cursor>=cell.start&&doc->cursor<=cell.end) { row=r; col=c; found=true; break; } }
        size_t linear=row*columns+col;
        if (reverse) { if (linear>0U) --linear; }
        else ++linear;
        if (linear>=rows*columns) {
            size_t table_at=block->source_start;
            if (!md_document_table_action(doc,table_at,rows-1U,columns-1U,MD_TABLE_ROW_BELOW,error,sizeof(error))) return false;
            block=NULL; for (size_t i=0U;i<doc->render.block_count;++i) if (doc->render.blocks[i].type==MD_BLOCK_TABLE&&doc->render.blocks[i].source_start==table_at) { block=&doc->render.blocks[i]; break; }
            if (block==NULL) return false;
            rows=(size_t)MD_MAX(1,block->line_end-block->line_start); linear=(rows-1U)*columns;
        }
        row=linear/columns; col=linear%columns; MdRange target;
        if (table_cell_range(doc,block,row,col,&target,NULL)) doc->cursor=doc->anchor=target.start;
        return true;
    }
    if (block!=NULL&&(block->type==MD_BLOCK_UL_ITEM||block->type==MD_BLOCK_OL_ITEM||block->type==MD_BLOCK_TASK_ITEM)) {
        if (!reverse) return md_document_replace(doc,block->source_start,block->source_start,"  ",2U,"Indent list",false,error,sizeof(error));
        size_t remove=0U; while (remove<2U&&block->source_start+remove<doc->source.len&&doc->source.data[block->source_start+remove]==' ') ++remove;
        return remove==0U?true:md_document_replace(doc,block->source_start,block->source_start+remove,"",0U,"Outdent list",false,error,sizeof(error));
    }
    return reverse?true:md_document_insert_utf8(doc,"    ",4U,error,sizeof(error));
}

static void append_input(MdBuf *input,const char *text,size_t len) {
    if (len==0U||!md_utf8_validate(text,len,NULL)) return;
    (void)md_buf_append(input,text,len);
}

static void backspace_input(MdBuf *input) {
    if (input->len==0U) return;
    input->len=md_grapheme_prev(input->data,input->len,input->len); input->data[input->len]='\0';
}

static void handle_modal_key(MdApp *app,KeySym key,unsigned state,const char *text,size_t text_len) {
    bool shift=(state&ShiftMask)!=0U,ctrl=(state&ControlMask)!=0U; bool tab_key=key==XK_Tab||key==XK_ISO_Left_Tab;
    if (key==XK_ISO_Left_Tab) shift=true;
    if (key==XK_Escape) { modal_cancel(app); return; }
    if (ctrl&&(key==XK_v||key==XK_V)) { (void)md_app_execute(app,MD_CMD_PASTE); return; }
    if (app->modal==MD_MODAL_DIFF) {
        size_t count=diff_changed_count(&app->diff);
        if (tab_key||key==XK_space) { app->diff_side_by_side=!app->diff_side_by_side; app->dirty_frame=true; return; }
        if ((key==XK_Right||key==XK_Down)&&count>0U) { app->diff_change=(app->diff_change+1U)%count; app->dirty_frame=true; return; }
        if ((key==XK_Left||key==XK_Up)&&count>0U) { app->diff_change=(app->diff_change+count-1U)%count; app->dirty_frame=true; return; }
    }
    if (app->modal==MD_MODAL_HISTORY) {
        size_t selected=app->versions.count==0U?0U:(size_t)MD_CLAMP(app->modal_selection,0,(int)app->versions.count-1); char error[512];
        if ((key==XK_r||key==XK_R)&&app->versions.count>0U) { app->pending_history_index=selected; char message[512]; (void)snprintf(message,sizeof(message),"Restore Version %llu? %s",(unsigned long long)app->versions.items[selected].sequence,active_document(app)!=NULL&&active_document(app)->dirty?"Current unsaved content will remain recoverable through one-step Undo.":"The live buffer will change but disk will not be written."); modal_open(app,MD_MODAL_HISTORY_RESTORE,message); return; }
        if ((key==XK_p||key==XK_P)&&app->versions.count>0U) { bool pin=!app->versions.items[selected].pinned; if (!md_history_pin(&app->versions,selected,pin,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { (void)history_refresh(app,selected); app_toast(app,pin?"Version pinned":"Version unpinned"); } return; }
        if (key==XK_Delete&&app->versions.count>0U) { if (!md_history_delete(&app->versions,selected,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { (void)history_refresh(app,selected==0U?0U:selected-1U); app_toast(app,"History version deleted without changing the live document"); } return; }
        if (key==XK_c||key==XK_C) { MdDocument *doc=active_document(app); if (doc==NULL||!md_history_create(doc->history_root,doc,true,NULL,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else { (void)history_refresh(app,app->versions.count); app_toast(app,"Explicit version created"); } return; }
    }
    if (key==XK_Return||key==XK_KP_Enter) { modal_submit(app); return; }
    if (tab_key) {
        int count=app->modal==MD_MODAL_PREFERENCES?9:app->modal==MD_MODAL_EXTERNAL_CONFLICT?6:app->modal==MD_MODAL_LINK_PROPERTIES?2:
                  (app->modal==MD_MODAL_UNSAVED||app->modal==MD_MODAL_OVERWRITE||app->modal==MD_MODAL_RELOCATION||app->modal==MD_MODAL_HISTORY_RESTORE)?3:4;
        app->modal_selection+=shift?-1:1; if (app->modal_selection<0) app->modal_selection=count-1; app->modal_selection%=count; app->dirty_frame=true; return;
    }
    if (key==XK_BackSpace) { backspace_input(app->modal==MD_MODAL_LINK_PROPERTIES&&app->modal_selection==1?&app->modal_secondary:&app->modal_input); if (app->modal==MD_MODAL_COMMAND_PALETTE) app->modal_selection=0; app->dirty_frame=true; return; }
    if (key==XK_Up) { if (app->modal==MD_MODAL_COMMAND_PALETTE||app->modal==MD_MODAL_HISTORY) app->modal_selection=MD_MAX(0,app->modal_selection-1); else if (app->modal==MD_MODAL_IMAGE_STORAGE) app->modal_selection=0; else if (app->modal==MD_MODAL_RECOVERY) select_adjacent_recovery(app,-1); app->dirty_frame=true; return; }
    if (key==XK_Down) { if (app->modal==MD_MODAL_COMMAND_PALETTE) ++app->modal_selection; else if (app->modal==MD_MODAL_HISTORY&&app->versions.count>0U) app->modal_selection=MD_MIN((int)app->versions.count-1,app->modal_selection+1); else if (app->modal==MD_MODAL_IMAGE_STORAGE) app->modal_selection=1; else if (app->modal==MD_MODAL_RECOVERY) select_adjacent_recovery(app,1); app->dirty_frame=true; return; }
    if (key==XK_Page_Up) { app->modal_selection=MD_MAX(0,app->modal_selection-8); app->dirty_frame=true; return; }
    if (key==XK_Page_Down) { app->modal_selection+=8; if (app->modal==MD_MODAL_HISTORY&&app->versions.count>0U) app->modal_selection=MD_MIN(app->modal_selection,(int)app->versions.count-1); app->dirty_frame=true; return; }
    if (app->modal==MD_MODAL_PREFERENCES&&(key==XK_Left||key==XK_Right)) {
        int direction=key==XK_Right?1:-1;
        switch (app->modal_selection%9) {
            case 0: app->dark=!app->dark; app->prefs.dark_theme=app->dark; palette_init(app); break;
            case 1: app->prefs.font_size=MD_CLAMP(app->prefs.font_size+direction,10,32); break;
            case 2: app->prefs.line_spacing=MD_CLAMP(app->prefs.line_spacing+0.1*(double)direction,1.0,1.8); break;
            case 3: app->prefs.default_embed_images=!app->prefs.default_embed_images; break;
            case 4: app->prefs.autosave_enabled=!app->prefs.autosave_enabled; if (!app->prefs.autosave_enabled) app_toast(app,"Periodic autosave disabled; crash recovery coverage is reduced, but orderly shutdown still writes recovery"); break;
            case 5: app->prefs.autosave_interval=MD_CLAMP(app->prefs.autosave_interval+direction*10,10,300); break;
            case 6: app->prefs.default_mode=(MdEditorMode)(((int)app->prefs.default_mode+direction+4)%4); break;
            case 7: app->prefs.sync_scroll=!app->prefs.sync_scroll; break;
            case 8: app->prefs.restore_session=!app->prefs.restore_session; break;
        }
        app->dirty_frame=true; return;
    }
    if (text_len>0U&&(app->modal==MD_MODAL_OPEN_PATH||app->modal==MD_MODAL_SAVE_PATH||app->modal==MD_MODAL_WORKSPACE_PATH||app->modal==MD_MODAL_COMMAND_PALETTE||app->modal==MD_MODAL_IMAGE_STORAGE||app->modal==MD_MODAL_IMAGE_PROPERTIES||app->modal==MD_MODAL_TREE_ACTION||app->modal==MD_MODAL_LINK_PROPERTIES)) {
        append_input(app->modal==MD_MODAL_LINK_PROPERTIES&&app->modal_selection==1?&app->modal_secondary:&app->modal_input,text,text_len); if (app->modal==MD_MODAL_COMMAND_PALETTE) app->modal_selection=0; app->dirty_frame=true;
    }
}

static void find_navigate(MdApp *app,int direction) {
    if (app->find.results.count==0U) return;
    if (direction>0) app->find.results.active=(app->find.results.active+1U)%app->find.results.count;
    else app->find.results.active=(app->find.results.active+app->find.results.count-1U)%app->find.results.count;
    MdDocument *doc=active_document(app); MdRange r=app->find.results.matches[app->find.results.active]; doc->anchor=r.start; doc->cursor=r.end;
    doc->source_scroll=(double)MD_MAX(0,(int)md_document_line_for_offset(doc,r.start)-3)*(double)MD_MAX(16,(int)llround((double)app->prefs.font_size*app->prefs.line_spacing)); app->dirty_frame=true;
}

static void find_replace_all(MdApp *app) {
    MdDocument *doc=active_document(app); char error[512]; size_t replaced=0U;
    if (doc==NULL||app->find.query.len==0U) { app_toast(app,"Replace All requires a non-empty query"); return; }
    if (!md_document_replace_all(doc,app->find.query.data,app->find.replacement.data,
                                 app->find.case_sensitive,app->find.whole_word,
                                 &replaced,error,sizeof(error))) app_toast(app,"%s",error);
    else app_toast(app,"Replaced %zu source match%s in one undo transaction",replaced,replaced==1U?"":"es");
    find_refresh(app); app->dirty_frame=true;
}

static void handle_find_key(MdApp *app,KeySym key,unsigned state,const char *text,size_t text_len) {
    bool shift=(state&ShiftMask)!=0U,ctrl=(state&ControlMask)!=0U,alt=(state&Mod1Mask)!=0U;
    if (key==XK_Escape) { app->find.visible=false; app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true; return; }
    if (ctrl&&(key==XK_v||key==XK_V)) { (void)md_app_execute(app,MD_CMD_PASTE); return; }
    if (key==XK_F3) { find_navigate(app,shift?-1:1); return; }
    if (alt&&(key==XK_c||key==XK_C)) { app->find.case_sensitive=!app->find.case_sensitive; find_refresh(app); app->dirty_frame=true; return; }
    if (alt&&(key==XK_w||key==XK_W)) { app->find.whole_word=!app->find.whole_word; find_refresh(app); app->dirty_frame=true; return; }
    if (ctrl&&(key==XK_Return||key==XK_KP_Enter)&&app->find.replace) { find_replace_all(app); return; }
    if ((key==XK_Tab||key==XK_ISO_Left_Tab)&&app->find.replace) { app->find.editing_replacement=!app->find.editing_replacement; app->dirty_frame=true; return; }
    MdBuf *input=app->find.editing_replacement?&app->find.replacement:&app->find.query;
    if (key==XK_BackSpace) { backspace_input(input); if (!app->find.editing_replacement) find_refresh(app); app->dirty_frame=true; return; }
    if (key==XK_Return||key==XK_KP_Enter) {
        if (app->find.replace&&app->find.editing_replacement) { MdDocument *doc=active_document(app); char error[512]; if (!md_document_replace_active(doc,&app->find.results,app->find.replacement.data,error,sizeof(error))) app_toast(app,"%s",error); find_refresh(app); }
        else find_navigate(app,shift?-1:1);
        return;
    }
    if (text_len>0U) { append_input(input,text,text_len); if (!app->find.editing_replacement) find_refresh(app); app->dirty_frame=true; }
}

static bool supported_document_path(const char *path) {
    const char *extension=strrchr(path,'.');
    return extension!=NULL&&(strcasecmp(extension,".md")==0||
           strcasecmp(extension,".markdown")==0||strcasecmp(extension,".txt")==0);
}

static bool handle_sidebar_key(MdApp *app,KeySym key) {
    if (app->focus!=UI_FOCUS_SIDEBAR) return false;
    MdDocument *doc=active_document(app);
    int count=app->outline_visible?(doc==NULL?0:(int)doc->render.heading_count):tree_visible_count(&app->workspace);
    if (key==XK_Escape) { app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true; return true; }
    if (count<=0) return key==XK_Up||key==XK_Down||key==XK_Home||key==XK_End||key==XK_Return;
    app->focus_index=MD_CLAMP(app->focus_index,0,count-1);
    if (key==XK_Up) app->focus_index=MD_MAX(0,app->focus_index-1);
    else if (key==XK_Down) app->focus_index=MD_MIN(count-1,app->focus_index+1);
    else if (key==XK_Home) app->focus_index=0;
    else if (key==XK_End) app->focus_index=count-1;
    else if (app->outline_visible&&(key==XK_Return||key==XK_KP_Enter)) {
        const MdHeading *heading=&doc->render.headings[app->focus_index]; doc->cursor=doc->anchor=heading->source_offset;
        doc->source_scroll=(double)MD_MAX(0,doc->render.blocks[heading->block_index].line_start-2)*(double)MD_MAX(16,(int)llround((double)app->prefs.font_size*app->prefs.line_spacing));
        doc->preview_scroll=0.0; for (size_t i=0U;i<heading->block_index;++i) doc->preview_scroll+=(double)block_height(app,&doc->render.blocks[i]);
    } else if (!app->outline_visible) {
        ssize_t index=tree_index_for_visible_row(&app->workspace,app->focus_index); if (index<0) return true;
        const MdTreeEntry *entry=&app->workspace.entries[index];
        if (key==XK_Return||key==XK_KP_Enter) {
            if (entry->is_directory) (void)md_workspace_set_directory_collapsed(&app->workspace,entry->path,!md_workspace_directory_collapsed(&app->workspace,entry->path));
            else if (supported_document_path(entry->path)) { char path[MD_PATH_MAX]; if (md_path_join(path,app->workspace.root,entry->path)) (void)open_document(app,path); }
            else app_toast(app,"Unsupported file activation is non-destructive: %s",entry->path);
        } else if (key==XK_Right&&entry->is_directory) {
            if (md_workspace_directory_collapsed(&app->workspace,entry->path)) (void)md_workspace_set_directory_collapsed(&app->workspace,entry->path,false);
            else if (app->focus_index+1<count) { ssize_t child=tree_index_for_visible_row(&app->workspace,app->focus_index+1); size_t n=strlen(entry->path); if (child>=0&&strncmp(app->workspace.entries[child].path,entry->path,n)==0&&app->workspace.entries[child].path[n]=='/') ++app->focus_index; }
        } else if (key==XK_Left) {
            if (entry->is_directory&&!md_workspace_directory_collapsed(&app->workspace,entry->path)) (void)md_workspace_set_directory_collapsed(&app->workspace,entry->path,true);
            else { char parent[MD_PATH_MAX]; if (md_path_dirname(parent,entry->path)&&strcmp(parent,".")!=0) { int row=tree_visible_row_for_path(&app->workspace,parent); if (row>=0) app->focus_index=row; } }
        } else if (key==XK_F2||key==XK_Delete) {
            app->menu=(UiMenu){0}; app->menu.kind=UI_MENU_TREE; (void)snprintf(app->menu.target,sizeof(app->menu.target),"%s",entry->path); tree_menu_action(app,key==XK_F2?2:3);
        } else return false;
    } else if (key!=XK_Up&&key!=XK_Down&&key!=XK_Home&&key!=XK_End) return false;
    app->dirty_frame=true; return true;
}

static bool handle_tabstrip_key(MdApp *app,KeySym key) {
    if (app->focus!=UI_FOCUS_TABS||app->doc_count==0U) return false;
    if (key==XK_Left) app->active_doc=(app->active_doc+app->doc_count-1U)%app->doc_count;
    else if (key==XK_Right) app->active_doc=(app->active_doc+1U)%app->doc_count;
    else if (key==XK_Escape) app->focus=UI_FOCUS_EDITOR;
    else return false;
    app->capsule_target=(double)app->docs[app->active_doc]->mode; app->dirty_frame=true; return true;
}

static void handle_key(MdApp *app,XKeyEvent *event) {
    char text[UI_TEXT_CAP]; KeySym key=NoSymbol; Status status=0; int length=0;
    if (app->xic!=NULL) length=Xutf8LookupString(app->xic,event,text,(int)sizeof(text)-1,&key,&status);
    else length=XLookupString(event,text,(int)sizeof(text)-1,&key,NULL);
    if (length<0||length>=(int)sizeof(text)) length=0;
    text[length]='\0'; unsigned state=event->state; bool ctrl=(state&ControlMask)!=0U,shift=(state&ShiftMask)!=0U,alt=(state&Mod1Mask)!=0U;
    bool tab_key=key==XK_Tab||key==XK_ISO_Left_Tab; if (key==XK_ISO_Left_Tab) shift=true;
    if (app->modal!=MD_MODAL_NONE) { handle_modal_key(app,key,state,text,(size_t)length); return; }
    if (app->menu.kind!=UI_MENU_NONE) {
        if (key==XK_Escape) menu_close(app); else if (key==XK_Up) app->menu.selected=(app->menu.selected+app->menu.item_count-1)%app->menu.item_count; else if (key==XK_Down) app->menu.selected=(app->menu.selected+1)%app->menu.item_count; else if (key==XK_Return||key==XK_space) menu_activate(app,app->menu.selected); app->dirty_frame=true; return;
    }
    if (app->focus==UI_FOCUS_FIND) { handle_find_key(app,key,state,text,(size_t)length); return; }
    if (handle_sidebar_key(app,key)||handle_tabstrip_key(app,key)) return;
    if (ctrl&&shift&&key==XK_P) { (void)md_app_execute(app,MD_CMD_PALETTE); return; }
    if (ctrl&&shift&&key==XK_S) { (void)md_app_execute(app,MD_CMD_SAVE_AS); return; }
    if (ctrl&&shift&&key==XK_Z) { (void)md_app_execute(app,MD_CMD_REDO); return; }
    if (ctrl&&shift&&(key==XK_X||key==XK_x)) { (void)md_app_execute(app,MD_CMD_STRIKE); return; }
    if (ctrl&&shift&&(key==XK_T||key==XK_t)) { (void)md_app_execute(app,MD_CMD_REOPEN_CLOSED); return; }
    if (ctrl&&shift&&(key==XK_D||key==XK_d)) { (void)md_app_execute(app,MD_CMD_STATISTICS); return; }
    if (ctrl&&shift&&(key==XK_I||key==XK_i)) { (void)md_app_execute(app,MD_CMD_INSERT_IMAGE); return; }
    if (ctrl&&alt&&(key==XK_S||key==XK_s)) { (void)md_app_execute(app,MD_CMD_SAVE_ALL); return; }
    if (ctrl&&alt&&(key==XK_O||key==XK_o)) { (void)md_app_execute(app,MD_CMD_OPEN_WORKSPACE); return; }
    if (ctrl&&alt&&(key==XK_H||key==XK_h)) { (void)md_app_execute(app,MD_CMD_HISTORY); return; }
    if (ctrl&&alt&&key==XK_1) { (void)md_app_execute(app,MD_CMD_TOGGLE_FILES); return; }
    if (ctrl&&alt&&key==XK_2) { (void)md_app_execute(app,MD_CMD_TOGGLE_OUTLINE); return; }
    if (ctrl&&tab_key) { if (app->doc_count>0U) app->active_doc=shift?(app->active_doc+app->doc_count-1U)%app->doc_count:(app->active_doc+1U)%app->doc_count; MdDocument *tab=active_document(app); app->capsule_target=tab==NULL?0.0:(double)tab->mode; app->dirty_frame=true; return; }
    if (ctrl) {
        MdCommandId command=MD_CMD_COUNT;
        if (key==XK_n||key==XK_N) command=MD_CMD_NEW; else if (key==XK_o||key==XK_O) command=MD_CMD_OPEN;
        else if (key==XK_s||key==XK_S) command=MD_CMD_SAVE; else if (key==XK_w||key==XK_W) command=MD_CMD_CLOSE_TAB;
        else if (key==XK_z||key==XK_Z) command=MD_CMD_UNDO; else if (key==XK_y||key==XK_Y) command=MD_CMD_REDO;
        else if (key==XK_c||key==XK_C) command=MD_CMD_COPY; else if (key==XK_x||key==XK_X) command=MD_CMD_CUT;
        else if (key==XK_v||key==XK_V) command=MD_CMD_PASTE; else if (key==XK_f||key==XK_F) command=MD_CMD_FIND;
        else if (key==XK_h||key==XK_H) command=MD_CMD_REPLACE; else if (key==XK_b||key==XK_B) command=MD_CMD_BOLD;
        else if (key==XK_i||key==XK_I) command=MD_CMD_ITALIC; else if (key==XK_k||key==XK_K) command=MD_CMD_LINK;
        else if (key==XK_1) command=MD_CMD_MODE_SOURCE; else if (key==XK_2) command=MD_CMD_MODE_SPLIT; else if (key==XK_3) command=MD_CMD_MODE_PREVIEW; else if (key==XK_4) command=MD_CMD_MODE_RENDERED;
        else if (key==XK_comma) command=MD_CMD_PREFERENCES; else if (key==XK_grave) command=MD_CMD_INLINE_CODE;
        if (command!=MD_CMD_COUNT) { (void)md_app_execute(app,command); return; }
    }
    if (key==XK_F1) { (void)md_app_execute(app,MD_CMD_SHORTCUTS); return; }
    if ((key==XK_F10&&shift)||key==XK_Menu) {
        UiMenuKind kind=app->image_selection.selected?UI_MENU_IMAGE:UI_MENU_CONTEXT;
        size_t source_offset=app->image_selection.source_start,row=0U,col=0U; MdDocument *menu_doc=active_document(app);
        if (kind==UI_MENU_CONTEXT&&menu_doc!=NULL&&menu_doc->mode==MD_MODE_RENDERED) {
            for (size_t i=0U;i<menu_doc->render.block_count;++i) {
                const MdBlock *block=&menu_doc->render.blocks[i];
                if (block->type!=MD_BLOCK_TABLE||menu_doc->cursor<block->source_start||menu_doc->cursor>block->source_end) continue;
                kind=UI_MENU_TABLE; source_offset=block->source_start; size_t columns=table_column_count(menu_doc,block);
                size_t rows=(size_t)MD_MAX(1,block->line_end-block->line_start);
                for (size_t r=0U;r<rows;++r) for (size_t c=0U;c<columns;++c) {
                    MdRange cell; if (table_cell_range(menu_doc,block,r,c,&cell,NULL)&&menu_doc->cursor>=cell.start&&menu_doc->cursor<=cell.end) { row=r; col=c; }
                }
                break;
            }
        }
        app->menu=(UiMenu){.kind=kind,.rect={MD_MIN(app->pointer_x,app->width-250),MD_MIN(app->pointer_y,app->height-300),240,288},.selected=0,.item_count=8,.x=app->pointer_x,.y=app->pointer_y,.source_offset=source_offset,.row=row,.col=col};
        app->focus=UI_FOCUS_MENU; app->dirty_frame=true; return;
    }
    if (key==XK_F6) { app->focus=(UiFocus)(((int)app->focus+1)%4); app->focus_index=0; app->dirty_frame=true; return; }
    if (key==XK_F10&&!shift) { app->menu=(UiMenu){.kind=UI_MENU_FILE,.rect={app->width-270,62,250,288},.selected=0,.item_count=8,.x=app->width-270,.y=62}; app->focus=UI_FOCUS_MENU; app->dirty_frame=true; return; }
    MdDocument *doc=active_document(app); if (doc==NULL||doc->mode==MD_MODE_PREVIEW) return;
    bool extend=shift;
    if (key==XK_Left) { if (ctrl) move_word(doc,-1,extend); else md_document_move_left(doc,extend); }
    else if (key==XK_Right) { if (ctrl) move_word(doc,1,extend); else md_document_move_right(doc,extend); }
    else if (key==XK_Up) move_vertical(doc,-1,extend); else if (key==XK_Down) move_vertical(doc,1,extend);
    else if (key==XK_Home) md_document_move_home(doc,ctrl,extend); else if (key==XK_End) md_document_move_end(doc,ctrl,extend);
    else if (key==XK_Page_Up) { doc->source_scroll=MD_MAX(0.0,doc->source_scroll-(double)app->layout.content.h*0.85); doc->preview_scroll=MD_MAX(0.0,doc->preview_scroll-(double)app->layout.content.h*0.85); }
    else if (key==XK_Page_Down) { doc->source_scroll+=(double)app->layout.content.h*0.85; doc->preview_scroll+=(double)app->layout.content.h*0.85; }
    else if (key==XK_BackSpace) { if (!md_document_backspace(doc,text,sizeof(text))) app_toast(app,"%s",text); }
    else if (key==XK_Delete&&app->image_selection.selected) {
        const MdBlock *image=selected_image_block(app); char error[512];
        if (image==NULL||!md_document_replace(doc,image->source_start,image->source_end,"",0U,"Remove image",false,error,sizeof(error))) app_toast(app,"%s",image==NULL?"Selected image is no longer mapped":error);
        else { app->image_selection.selected=false; app->image_selection.rect=(UiRect){0,0,0,0}; }
    }
    else if (key==XK_Delete) { if (!md_document_delete(doc,text,sizeof(text))) app_toast(app,"%s",text); }
    else if (key==XK_Return||key==XK_KP_Enter) (void)editor_enter(app);
    else if (tab_key) (void)editor_tab(app,shift);
    else if (ctrl&&(key==XK_a||key==XK_A)) { doc->anchor=0U; doc->cursor=doc->source.len; }
    else if (!ctrl&&!alt&&length>0) { char error[512]; if (!md_document_insert_utf8(doc,text,(size_t)length,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); }
    app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true;
}

static UiButton *button_at(MdApp *app,int x,int y) {
    for (size_t i=app->button_count;i>0U;--i) if (point_in(app->buttons[i-1U].rect,x,y)) return &app->buttons[i-1U];
    return NULL;
}

static void activate_sidebar_at(MdApp *app,int x,int y) {
    UiRect area=app->layout.sidebar; int selector_y=area.y+12;
    if (y>=selector_y&&y<selector_y+38) { app->outline_visible=x>=area.w/2; app->sidebar_target_visible=true; app->focus=UI_FOCUS_SIDEBAR; app->dirty_frame=true; return; }
    int row_y=area.y+94; MdDocument *doc=active_document(app);
    if (app->outline_visible) {
        if (doc==NULL) return;
        int index=(y-row_y)/31; if (index>=0&&(size_t)index<doc->render.heading_count) {
            doc->cursor=doc->anchor=doc->render.headings[index].source_offset;
            doc->source_scroll=(double)MD_MAX(0,doc->render.blocks[doc->render.headings[index].block_index].line_start-2)*
                               (double)MD_MAX(16,(int)llround((double)app->prefs.font_size*app->prefs.line_spacing));
            doc->preview_scroll=0.0; for (size_t i=0U;i<doc->render.headings[index].block_index;++i) doc->preview_scroll+=(double)block_height(app,&doc->render.blocks[i]);
            app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true;
        }
        return;
    }
    if (app->workspace.root[0]=='\0') return;
    row_y+=34; int visible_row=(y-row_y)/29; ssize_t index=tree_index_for_visible_row(&app->workspace,visible_row);
    if (index>=0&&(size_t)index<app->workspace.count) {
        MdTreeEntry *entry=&app->workspace.entries[index];
        if (entry->is_directory) {
            bool collapse=!md_workspace_directory_collapsed(&app->workspace,entry->path);
            if (!md_workspace_set_directory_collapsed(&app->workspace,entry->path,collapse)) app_toast(app,"Could not update the folder expansion state");
            else app_toast(app,"%s %s",collapse?"Collapsed":"Expanded",entry->path);
        }
        else {
            if (supported_document_path(entry->path)) { char path[MD_PATH_MAX]; if (md_path_join(path,app->workspace.root,entry->path)) (void)open_document(app,path); }
            else app_toast(app,"Unsupported file activation is non-destructive: %s",entry->path);
        }
        app->focus=UI_FOCUS_SIDEBAR;
    }
}

static void selection_drop_commit(MdApp *app,bool copy) {
    MdDocument *doc=active_document(app); if (doc==NULL) return; char error[512]={0};
    if (!md_document_move_selection(doc,app->selection_drop,copy,error,sizeof(error))&&error[0]!='\0')
        modal_open(app,MD_MODAL_ERROR,error);
}

static bool html_escape_attr(MdBuf *out,const char *text) {
    for (size_t i=0U;text[i]!='\0';++i) {
        if (text[i]=='&') { if (!md_buf_append_cstr(out,"&amp;")) return false; }
        else if (text[i]=='\"') { if (!md_buf_append_cstr(out,"&quot;")) return false; }
        else if (text[i]=='<') { if (!md_buf_append_cstr(out,"&lt;")) return false; }
        else if (!md_buf_append_char(out,text[i])) return false;
    }
    return true;
}

static void image_resize_commit(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL||!app->image_selection.selected) return;
    const MdBlock *block=NULL;
    for (size_t i=0U;i<doc->render.block_count;++i) {
        const MdBlock *candidate=&doc->render.blocks[i];
        if (candidate->type==MD_BLOCK_IMAGE&&candidate->source_start==app->image_selection.source_start) { block=candidate; break; }
    }
    if (block==NULL) for (size_t i=0U;i<doc->render.block_count;++i) {
        const MdBlock *candidate=&doc->render.blocks[i];
        if (candidate->type==MD_BLOCK_IMAGE&&doc->cursor>=candidate->source_start&&doc->cursor<=candidate->source_end) { block=candidate; break; }
    }
    if (block==NULL) for (size_t i=0U;i<doc->render.block_count;++i) {
        const MdBlock *candidate=&doc->render.blocks[i];
        if (candidate->type==MD_BLOCK_IMAGE&&candidate->source_start<app->image_selection.source_end&&
            candidate->source_end>app->image_selection.source_start) { block=candidate; break; }
    }
    if (block==NULL) { modal_open(app,MD_MODAL_ERROR,"The selected image no longer maps to a source construct. No source bytes were changed."); return; }
    char alt[256],error[512]; MdBuf destination; md_buf_init(&destination);
    if (!md_buf_reserve(&destination,0U)||!image_source(doc,block,alt,sizeof(alt),&destination)) {
        md_buf_free(&destination); modal_open(app,MD_MODAL_ERROR,"Image resize could not serialize the selected Markdown source safely. No source bytes were changed."); return;
    }
    bool newline=block->source_end>block->source_start&&doc->source.data[block->source_end-1U]=='\n';
    size_t start=block->source_start,end=block->source_end;
    MdBuf html; md_buf_init(&html); bool ok=md_buf_append_cstr(&html,"<img src=\"")&&html_escape_attr(&html,destination.data)&&md_buf_append_cstr(&html,"\" alt=\"")&&html_escape_attr(&html,alt)&&md_buf_appendf(&html,"\" width=\"%d\">",app->image_selection.current_w)&&(newline?md_buf_append_char(&html,'\n'):true);
    size_t replacement_len=html.len;
    if (ok) ok=md_document_replace(doc,start,end,html.data,html.len,"Resize image",false,error,sizeof(error));
    md_buf_free(&html); md_buf_free(&destination);
    if (!ok) modal_open(app,MD_MODAL_ERROR,error); else { app->image_selection.source_start=start; app->image_selection.source_end=start+replacement_len; app_toast(app,"Image width persisted as portable inline HTML (%d px)",app->image_selection.current_w); }
}

static const MdBlock *selected_image_block(MdApp *app) {
    MdDocument *doc=active_document(app); if (doc==NULL||!app->image_selection.selected) return NULL;
    for (size_t i=0U;i<doc->render.block_count;++i)
        if (doc->render.blocks[i].type==MD_BLOCK_IMAGE&&
            doc->render.blocks[i].source_start==app->image_selection.source_start) return &doc->render.blocks[i];
    return NULL;
}

static bool append_markdown_alt(MdBuf *out,const char *alt) {
    for (size_t i=0U;alt[i]!='\0';++i) {
        if ((alt[i]=='\\'||alt[i]==']')&&!md_buf_append_char(out,'\\')) return false;
        if (!md_buf_append_char(out,alt[i])) return false;
    }
    return true;
}

static bool append_markdown_destination(MdBuf *out,const char *destination) {
    if (!md_buf_append_char(out,'<')) return false;
    for (size_t i=0U;destination[i]!='\0';++i) {
        unsigned char c=(unsigned char)destination[i];
        if (c=='\n'||c=='\r') return false;
        if ((c=='\\'||c=='<'||c=='>')&&!md_buf_append_char(out,'\\')) return false;
        if (!md_buf_append_char(out,(char)c)) return false;
    }
    return md_buf_append_char(out,'>');
}

static bool replace_selected_image(MdApp *app,const char *alt,const char *destination,
                                   int width,const char *label,char *error,size_t error_cap) {
    MdDocument *doc=active_document(app); const MdBlock *block=selected_image_block(app);
    if (doc==NULL||block==NULL) { (void)snprintf(error,error_cap,"No rendered image is selected"); return false; }
    bool html=width>0;
    size_t lead=block->source_start;
    while (lead<block->source_end&&(doc->source.data[lead]==' '||doc->source.data[lead]=='\t')) ++lead;
    if (lead+4U<=block->source_end&&memcmp(doc->source.data+lead,"<img",4U)==0) html=true;
    bool newline=block->source_end>block->source_start&&doc->source.data[block->source_end-1U]=='\n';
    size_t source_start=block->source_start,source_end=block->source_end;
    MdBuf replacement; md_buf_init(&replacement); bool ok;
    if (html) {
        ok=md_buf_append_cstr(&replacement,"<img src=\"")&&html_escape_attr(&replacement,destination)&&
           md_buf_append_cstr(&replacement,"\" alt=\"")&&html_escape_attr(&replacement,alt)&&
           (width<=0||md_buf_appendf(&replacement,"\" width=\"%d",width))&&md_buf_append_cstr(&replacement,"\">");
    } else {
        ok=md_buf_append_cstr(&replacement,"![")&&append_markdown_alt(&replacement,alt)&&
           md_buf_append_cstr(&replacement,"](")&&append_markdown_destination(&replacement,destination)&&
           md_buf_append_char(&replacement,')');
    }
    if (ok&&newline) ok=md_buf_append_char(&replacement,'\n');
    if (ok) ok=md_document_replace(doc,source_start,source_end,replacement.data,replacement.len,label,false,error,error_cap);
    if (ok) {
        app->image_selection.source_start=source_start;
        app->image_selection.source_end=source_start+replacement.len;
        app->image_selection.selected=true;
    }
    md_buf_free(&replacement); return ok;
}

static bool selected_image_values(MdApp *app,char alt[256],MdBuf *destination,int *width,
                                  char *error,size_t error_cap) {
    MdDocument *doc=active_document(app); const MdBlock *block=selected_image_block(app);
    if (doc==NULL||block==NULL||!image_source(doc,block,alt,256U,destination)) {
        (void)snprintf(error,error_cap,"The selected image reference cannot be parsed"); return false;
    }
    *width=image_persisted_width(doc,block); return true;
}

static bool selected_image_bytes(MdApp *app,MdBytes *bytes,MdImageFormat *format,
                                 char *error,size_t error_cap) {
    MdDocument *doc=active_document(app); char alt[256]; int width=0; MdBuf destination; md_buf_init(&destination); (void)md_buf_reserve(&destination,0U);
    if (!selected_image_values(app,alt,&destination,&width,error,error_cap)) { md_buf_free(&destination); return false; }
    bool ok=false;
    if (strncmp(destination.data,"data:",5U)==0) ok=md_image_parse_data_uri(destination.data,destination.len,format,bytes,error,error_cap);
    else if (strncmp(destination.data,"http://",7U)==0||strncmp(destination.data,"https://",8U)==0)
        (void)snprintf(error,error_cap,"Remote image bytes are not fetched by this offline editor");
    else {
        char path[MD_PATH_MAX];
        if (destination.data[0]=='/') (void)snprintf(path,sizeof(path),"%s",destination.data);
        else { char dir[MD_PATH_MAX]; if (!md_path_dirname(dir,doc->path)||!md_path_join(path,dir,destination.data)) { md_buf_free(&destination); (void)snprintf(error,error_cap,"Image path is too long"); return false; } }
        if (md_read_file(path,bytes,error,error_cap)) { *format=md_image_detect(bytes->data,bytes->len); ok=*format!=MD_IMAGE_UNKNOWN; if (!ok) (void)snprintf(error,error_cap,"Image data is not PNG, JPEG, or BMP"); }
    }
    (void)width; md_buf_free(&destination); return ok;
}

static bool image_save_selected(MdApp *app,const char *path,bool overwrite,char *error,size_t error_cap) {
    struct stat st;
    if (!overwrite&&lstat(path,&st)==0) { (void)snprintf(error,error_cap,"Destination already exists: %s",path); return false; }
    MdBytes bytes; md_bytes_init(&bytes); MdImageFormat format=MD_IMAGE_UNKNOWN;
    bool ok=selected_image_bytes(app,&bytes,&format,error,error_cap)&&md_write_file_atomic(path,bytes.data,bytes.len,error,error_cap);
    md_bytes_free(&bytes); return ok;
}

static bool image_apply_pending(MdApp *app,UiPendingAction action,const char *value,
                                char *error,size_t error_cap) {
    char alt[256]; int width=0; MdBuf destination; md_buf_init(&destination); (void)md_buf_reserve(&destination,0U);
    if (!selected_image_values(app,alt,&destination,&width,error,error_cap)) { md_buf_free(&destination); return false; }
    bool ok=false;
    if (action==UI_PENDING_IMAGE_ALT) ok=replace_selected_image(app,value,destination.data,width,"Edit image alt text",error,error_cap);
    else if (action==UI_PENDING_IMAGE_RELINK) ok=replace_selected_image(app,alt,value,width,"Relink image",error,error_cap);
    md_buf_free(&destination); return ok;
}

static void image_menu_action(MdApp *app,int selected) {
    MdDocument *doc=active_document(app); if (doc==NULL||selected_image_block(app)==NULL) return;
    char alt[256],error[1024]={0}; int width=0; MdBuf destination; md_buf_init(&destination); (void)md_buf_reserve(&destination,0U);
    if (selected==7) { md_buf_free(&destination); return; }
    if (!selected_image_values(app,alt,&destination,&width,error,sizeof(error))) { md_buf_free(&destination); modal_open(app,MD_MODAL_ERROR,error); return; }
    if (selected==0||selected==3) {
        app->pending_action=selected==0?UI_PENDING_IMAGE_ALT:UI_PENDING_IMAGE_RELINK;
        modal_open(app,MD_MODAL_IMAGE_PROPERTIES,selected==0?"Edit the image alternative text.":"Enter a replacement local path, relative path, or data URI.");
        (void)md_buf_append_cstr(&app->modal_input,selected==0?alt:destination.data);
    } else if (selected==1) {
        if (!replace_selected_image(app,alt,destination.data,0,"Reset image size",error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
    } else if (selected==2) {
        app->pending_action=UI_PENDING_IMAGE_SAVE_AS; app->pending_command=MD_CMD_COUNT;
        modal_open(app,MD_MODAL_SAVE_PATH,"Save the exact source image bytes (not a screenshot).");
    } else if (selected==4) {
        MdBuf replacement; md_buf_init(&replacement); bool ok=false;
        if (strncmp(destination.data,"data:",5U)==0) {
            char relative[MD_PATH_MAX];
            if (doc->path[0]=='\0') (void)snprintf(error,sizeof(error),"Save the document before externalizing an embedded image");
            else if (md_asset_externalize(app->workspace.root,doc->path,destination.data,destination.len,relative,error,sizeof(error)))
                ok=replace_selected_image(app,alt,relative,width,"Externalize image",error,sizeof(error));
        } else {
            MdBytes bytes; md_bytes_init(&bytes); MdImageFormat format=MD_IMAGE_UNKNOWN;
            if (selected_image_bytes(app,&bytes,&format,error,sizeof(error))&&md_image_make_data_uri(format,bytes.data,bytes.len,&replacement))
                ok=replace_selected_image(app,alt,replacement.data,width,"Embed image",error,sizeof(error));
            md_bytes_free(&bytes);
        }
        md_buf_free(&replacement); if (!ok) modal_open(app,MD_MODAL_ERROR,error); else app_toast(app,"Image storage converted; Undo restores the previous source form");
    } else if (selected==5) {
        const MdBlock *block=selected_image_block(app); size_t start=block->source_start,end=block->source_end;
        if (!md_document_replace(doc,start,end,"",0U,"Remove image",false,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
        else { app->image_selection.selected=false; app->image_selection.rect=(UiRect){0,0,0,0}; app_toast(app,"Image reference removed; original asset was not deleted"); }
    } else if (selected==6) {
        char message[1024]; (void)snprintf(message,sizeof(message),"Alt: %s · Width: %s · Source: %.600s",alt,width>0?"custom":"intrinsic",destination.data);
        app->pending_action=UI_PENDING_NONE; modal_open(app,MD_MODAL_IMAGE_PROPERTIES,message);
    }
    md_buf_free(&destination);
}

static void table_menu_action(MdApp *app,int selected) {
    if (selected==7) return;
    static const MdTableAction actions[]={MD_TABLE_ROW_ABOVE,MD_TABLE_ROW_BELOW,MD_TABLE_ROW_DELETE,
        MD_TABLE_COL_BEFORE,MD_TABLE_COL_AFTER,MD_TABLE_COL_DELETE,MD_TABLE_ALIGN_DEFAULT};
    MdDocument *doc=active_document(app); char error[512]={0}; if (doc==NULL) return;
    MdTableAction action=actions[selected];
    if (selected==6) {
        const MdBlock *block=NULL; for (size_t i=0U;i<doc->render.block_count;++i) if (doc->render.blocks[i].type==MD_BLOCK_TABLE&&doc->render.blocks[i].source_start==app->menu.source_offset) { block=&doc->render.blocks[i]; break; }
        MdRange marker; int alignment=0;
        if (block!=NULL&&table_physical_cell_range(doc,block,1U,app->menu.col,&marker,NULL)) { bool left=marker.start<marker.end&&doc->source.data[marker.start]==':'; bool right=marker.end>marker.start&&doc->source.data[marker.end-1U]==':'; alignment=left&&right?2:right?3:left?1:0; }
        static const MdTableAction cycle[]={MD_TABLE_ALIGN_LEFT,MD_TABLE_ALIGN_CENTER,MD_TABLE_ALIGN_RIGHT,MD_TABLE_ALIGN_DEFAULT}; action=cycle[alignment];
    }
    if (!md_document_table_action(doc,app->menu.source_offset,app->menu.row,app->menu.col,action,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
    else app_toast(app,selected==6?"Table alignment advanced through Default → Left → Center → Right":"Table structure updated in one undo transaction");
}

static void tree_menu_action(MdApp *app,int selected) {
    char error[512]={0}; const char *target=app->menu.target;
    if (selected==7) return;
    if (selected<=3) {
        app->pending_action=selected==0?UI_PENDING_TREE_NEW_FILE:selected==1?UI_PENDING_TREE_NEW_FOLDER:selected==2?UI_PENDING_TREE_RENAME:UI_PENDING_TREE_DELETE;
        (void)snprintf(app->pending_path,sizeof(app->pending_path),"%s",target);
        const char *message=selected==0?"Enter a workspace-relative Markdown filename.":selected==1?"Enter a workspace-relative folder name.":selected==2?"Enter the new workspace-relative path.":"Type DELETE to confirm recursive deletion. Open buffers remain in memory.";
        modal_open(app,MD_MODAL_TREE_ACTION,message);
        if (selected==2) (void)md_buf_append_cstr(&app->modal_input,target);
        return;
    }
    if (selected==4) {
        if (target[0]!='\0') { char path[MD_PATH_MAX]; if (md_path_join(path,app->workspace.root,target)) (void)open_document(app,path); }
    } else if (selected==5) {
        bool collapsed=md_workspace_directory_collapsed(&app->workspace,target);
        if (!md_workspace_set_directory_collapsed(&app->workspace,target,!collapsed)) app_toast(app,"Could not update tree expansion");
    } else if (selected==6) {
        if (!md_workspace_scan(&app->workspace,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); else app_toast(app,"Workspace tree refreshed");
    }
}

static void menu_activate(MdApp *app,int selected) {
    UiMenuKind kind=app->menu.kind;
    if (kind==UI_MENU_NONE||selected<0||selected>=app->menu.item_count) return;
    menu_close(app);
    if (kind==UI_MENU_OVERFLOW) {
        size_t document=app->menu.source_offset+(size_t)selected;
        if (document<app->doc_count) { app->active_doc=document; app->capsule_target=(double)app->docs[document]->mode; app->focus=UI_FOCUS_TABS; }
    } else if (kind==UI_MENU_FILE) {
        static const MdCommandId commands[]={MD_CMD_NEW,MD_CMD_OPEN,MD_CMD_SAVE,MD_CMD_SAVE_AS,MD_CMD_SAVE_ALL,MD_CMD_EXPORT_SINGLE,MD_CMD_PREFERENCES};
        if (selected<7) (void)md_app_execute(app,commands[selected]); else request_exit(app);
    } else if (kind==UI_MENU_CONTEXT) {
        static const MdCommandId commands[]={MD_CMD_UNDO,MD_CMD_REDO,MD_CMD_CUT,MD_CMD_COPY,MD_CMD_PASTE,MD_CMD_BOLD,MD_CMD_LINK};
        if (selected<7) (void)md_app_execute(app,commands[selected]);
        else (void)md_app_execute(app,MD_CMD_HEADING_4);
    } else if (kind==UI_MENU_IMAGE) image_menu_action(app,selected);
    else if (kind==UI_MENU_TABLE) table_menu_action(app,selected);
    else if (kind==UI_MENU_TREE) tree_menu_action(app,selected);
    app->dirty_frame=true;
}

static void handle_button_press(MdApp *app,XButtonEvent *event) {
    app->pointer_x=event->x; app->pointer_y=event->y;
    if (event->button==Button4||event->button==Button5) {
        MdDocument *doc=active_document(app); if (doc==NULL) return; double direction=event->button==Button4?-1.0:1.0;
        if ((event->state&ControlMask)!=0U) { doc->zoom=MD_CLAMP(doc->zoom-direction*0.1,0.5,2.5); app_toast(app,"Zoom %d%%",(int)llround(doc->zoom*100.0)); }
        else {
            double delta=direction*72.0;
            if (doc->mode==MD_MODE_SOURCE||(doc->mode==MD_MODE_SPLIT&&point_in(app->layout.source,event->x,event->y))) { doc->source_scroll=MD_MAX(0.0,doc->source_scroll+delta); if (doc->mode==MD_MODE_SPLIT&&app->prefs.sync_scroll) doc->preview_scroll=MD_MAX(0.0,doc->preview_scroll+delta); }
            else { doc->preview_scroll=MD_MAX(0.0,doc->preview_scroll+delta); if (doc->mode==MD_MODE_SPLIT&&app->prefs.sync_scroll) doc->source_scroll=MD_MAX(0.0,doc->source_scroll+delta); }
        }
        app->dirty_frame=true; return;
    }
    if (app->modal!=MD_MODAL_NONE) {
        if (event->button==Button1&&point_in((UiRect){app->modal_rect.x+app->modal_rect.w-55,app->modal_rect.y,55,55},event->x,event->y)) modal_cancel(app);
        else if (event->button==Button1&&app->modal==MD_MODAL_IMAGE_STORAGE) {
            if (event->y<app->modal_rect.y+138) app->modal_selection=0; else if (event->y<app->modal_rect.y+214) app->modal_selection=1;
        } else if (event->button==Button1&&app->modal==MD_MODAL_LINK_PROPERTIES) {
            app->modal_selection=event->y<app->modal_rect.y+153?0:1;
        } else if (event->button==Button1&&app->modal==MD_MODAL_PREFERENCES) {
            int row=(event->y-(app->modal_rect.y+50))/31; if (row>=0&&row<9) app->modal_selection=row;
        } else if (event->button==Button1&&app->modal==MD_MODAL_HISTORY) {
            size_t start=app->modal_selection>=9?(size_t)app->modal_selection-9U:0U; int row=(event->y-(app->modal_rect.y+76))/39;
            if (row>=0&&(size_t)row<10U&&start+(size_t)row<app->versions.count) app->modal_selection=(int)(start+(size_t)row);
        } else if (event->button==Button1&&(app->modal==MD_MODAL_OVERWRITE||app->modal==MD_MODAL_RELOCATION||app->modal==MD_MODAL_HISTORY_RESTORE)) {
            int top=app->modal==MD_MODAL_OVERWRITE?112:app->modal==MD_MODAL_RELOCATION?98:110;
            int step=app->modal==MD_MODAL_RELOCATION?68:app->modal==MD_MODAL_HISTORY_RESTORE?48:43;
            int choice=(event->y-(app->modal_rect.y+top))/step; if (choice>=0&&choice<3) { app->modal_selection=choice; modal_submit(app); }
        } else if (event->button==Button1&&app->modal==MD_MODAL_UNSAVED) {
            int choice=(event->x-(app->modal_rect.x+24))/MD_MAX(1,(app->modal_rect.w-48)/3); if (event->y>=app->modal_rect.y+112&&event->y<app->modal_rect.y+160&&choice>=0&&choice<3) { app->modal_selection=choice; modal_submit(app); }
        } else if (event->button==Button1&&app->modal==MD_MODAL_EXTERNAL_CONFLICT) {
            int col=(event->x-(app->modal_rect.x+22))/MD_MAX(1,(app->modal_rect.w-44)/3),row=(event->y-(app->modal_rect.y+104))/43; int choice=row*3+col;
            if (col>=0&&col<3&&row>=0&&row<2) { app->modal_selection=choice; modal_submit(app); }
        } else if (event->button==Button1&&app->modal==MD_MODAL_RECOVERY) {
            int col=(event->x-(app->modal_rect.x+24))/MD_MAX(1,app->modal_rect.w/2-20),row=(event->y-(app->modal_rect.y+212))/42; int choice=row*2+col;
            if (col>=0&&col<2&&row>=0&&row<2) { app->modal_selection=choice; modal_submit(app); }
        }
        app->dirty_frame=true; return;
    }
    if (app->menu.kind!=UI_MENU_NONE) {
        if (!point_in(app->menu.rect,event->x,event->y)) menu_close(app);
        else if (event->button==Button1) { int selected=(event->y-(app->menu.rect.y+8))/34; if (selected>=0&&selected<app->menu.item_count) menu_activate(app,selected); }
        return;
    }
    if (event->button==Button3) {
        UiMenu menu={0}; menu.kind=UI_MENU_CONTEXT; menu.rect=(UiRect){MD_MIN(event->x,app->width-250),MD_MIN(event->y,app->height-300),240,288}; menu.item_count=8; menu.x=event->x; menu.y=event->y;
        if (app->layout.sidebar.w>0&&point_in(app->layout.sidebar,event->x,event->y)&&!app->outline_visible) {
            int visible_row=(event->y-(app->layout.sidebar.y+128))/29; ssize_t index=tree_index_for_visible_row(&app->workspace,visible_row);
            menu.kind=UI_MENU_TREE;
            if (index>=0) (void)snprintf(menu.target,sizeof(menu.target),"%s",app->workspace.entries[index].path);
        } else if (app->image_selection.rect.w>0&&point_in(app->image_selection.rect,event->x,event->y)) {
            menu.kind=UI_MENU_IMAGE; menu.source_offset=app->image_selection.source_start; app->image_selection.selected=true;
        } else {
            MdDocument *doc=active_document(app);
            if (doc!=NULL&&doc->mode==MD_MODE_RENDERED) {
                UiRect rendered=doc->mode==MD_MODE_SPLIT?app->layout.preview:app->layout.preview; int block_y=0;
                const MdBlock *block=point_in(rendered,event->x,event->y)?rendered_block_at(app,rendered,event->y,&block_y):NULL;
                if (block!=NULL&&block->type==MD_BLOCK_TABLE) { size_t columns=table_column_count(doc,block); int width=MD_MIN(rendered.w-64,720),col_w=MD_MAX(1,width/(int)columns),table_x=rendered.x+32; menu.kind=UI_MENU_TABLE; menu.source_offset=block->source_start; menu.row=(size_t)MD_MAX(0,(event->y-block_y)/30); menu.col=(size_t)MD_CLAMP((event->x-table_x)/col_w,0,(int)columns-1); }
            }
        }
        app->menu=menu; app->focus=UI_FOCUS_MENU; app->dirty_frame=true; return;
    }
    if (event->button!=Button1) return;
    int visible_tabs=MD_MAX(1,(app->layout.tabstrip.w-70)/186);
    UiRect tab_overflow={app->layout.tabstrip.x+app->layout.tabstrip.w-58,app->layout.tabstrip.y+3,52,37};
    if ((int)app->doc_count>visible_tabs&&point_in(tab_overflow,event->x,event->y)) {
        int count=MD_MIN(8,(int)app->doc_count-visible_tabs);
        app->menu=(UiMenu){.kind=UI_MENU_OVERFLOW,.rect={tab_overflow.x-214,tab_overflow.y+38,260,count*34+16},.selected=0,.item_count=count,.x=event->x,.y=event->y,.source_offset=(size_t)visible_tabs};
        app->focus=UI_FOCUS_MENU; app->dirty_frame=true; return;
    }
    UiButton *button=button_at(app,event->x,event->y);
    if (button!=NULL&&button->enabled) { button->pressed=true; ripple_add(app,button->rect,event->x,event->y); app->dirty_frame=true; return; }
    if (app->doc_count==0U&&point_in(app->layout.content,event->x,event->y)) {
        UiRect area=app->layout.content; int card_w=MD_MIN(720,area.w-80),card_h=MD_MIN(430,area.h-70); UiRect card={area.x+(area.w-card_w)/2,area.y+(area.h-card_h)/2,card_w,card_h};
        int workspace_index=(event->y-(card.y+234))/25; int file_index=(event->y-(card.y+318))/24;
        if (event->y>=card.y+232&&event->y<card.y+292&&workspace_index>=0&&(size_t)workspace_index<app->workspace.recent_workspace_count) { char recent[MD_PATH_MAX]; (void)snprintf(recent,sizeof(recent),"%s",app->workspace.recent_workspaces[workspace_index]); if (!open_workspace(app,recent)) { (void)md_recent_remove_workspace(&app->workspace,recent); char ignored[128]; (void)md_recent_save(&app->workspace,ignored,sizeof(ignored)); } return; }
        if (event->y>=card.y+318&&event->y<card.y+410&&file_index>=0&&(size_t)file_index<app->workspace.recent_file_count) { char recent[MD_PATH_MAX]; (void)snprintf(recent,sizeof(recent),"%s",app->workspace.recent_files[file_index]); if (!open_document(app,recent)) { (void)md_recent_remove_file(&app->workspace,recent); char ignored[128]; (void)md_recent_save(&app->workspace,ignored,sizeof(ignored)); } return; }
    }
    for (size_t i=0U;i<app->doc_count;++i) {
        UiRect tab=tab_rect_at(app,i); if (point_in(tab,event->x,event->y)) {
            if (event->x>tab.x+tab.w-42) { app->active_doc=i; request_close_active(app); }
            else { app->active_doc=i; app->capsule_target=(double)app->docs[i]->mode; app->dragging_tab=true; app->drag_tab_from=(int)i; app->drag_tab_to=(int)i; app->focus=UI_FOCUS_TABS; }
            app->dirty_frame=true; return;
        }
    }
    if (app->layout.sidebar.w>0&&point_in(app->layout.sidebar,event->x,event->y)) { if (event->x>=app->layout.sidebar.x+app->layout.sidebar.w-5) app->dragging_sidebar=true; else activate_sidebar_at(app,event->x,event->y); return; }
    if (point_in(app->layout.divider,event->x,event->y)) { app->dragging_divider=true; return; }
    if (app->find.visible) {
        UiRect root=app->layout.content; int width=app->find.replace?610:470,height=app->find.replace?102:58;
        UiRect panel={root.x+root.w-width-22,root.y+14,width,height};
        if (point_in(panel,event->x,event->y)) {
            int local_x=event->x-panel.x,local_y=event->y-panel.y; app->focus=UI_FOCUS_FIND;
            if (local_y>=12&&local_y<48&&local_x>=14&&local_x<252) app->find.editing_replacement=false;
            else if (local_y>=12&&local_y<48&&local_x>=326&&local_x<352) find_navigate(app,-1);
            else if (local_y>=12&&local_y<48&&local_x>=352&&local_x<380) find_navigate(app,1);
            else if (local_y>=12&&local_y<48&&local_x>=380&&local_x<410) { app->find.case_sensitive=!app->find.case_sensitive; find_refresh(app); }
            else if (local_y>=12&&local_y<48&&local_x>=410&&local_x<width-34) { app->find.whole_word=!app->find.whole_word; find_refresh(app); }
            else if (local_y>=12&&local_y<48&&local_x>=width-34) { app->find.visible=false; app->focus=UI_FOCUS_EDITOR; }
            else if (app->find.replace&&local_y>=56&&local_y<94&&local_x>=14&&local_x<252) app->find.editing_replacement=true;
            else if (app->find.replace&&local_y>=56&&local_y<94&&local_x>=260&&local_x<352) {
                MdDocument *active=active_document(app); char error[512];
                if (active!=NULL&&!md_document_replace_active(active,&app->find.results,app->find.replacement.data,error,sizeof(error))) app_toast(app,"%s",error);
                find_refresh(app);
            } else if (app->find.replace&&local_y>=56&&local_y<94&&local_x>=352&&local_x<470) find_replace_all(app);
            app->dirty_frame=true; return;
        }
    }
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    UiRect editor=doc->mode==MD_MODE_SPLIT&&point_in(app->layout.preview,event->x,event->y)?app->layout.preview:app->layout.source;
    if (doc->mode==MD_MODE_RENDERED||doc->mode==MD_MODE_PREVIEW||(doc->mode==MD_MODE_SPLIT&&point_in(app->layout.preview,event->x,event->y))) {
        if (app->image_selection.rect.w>0&&point_in(app->image_selection.rect,event->x,event->y)&&doc->mode!=MD_MODE_PREVIEW) {
            app->image_selection.selected=true; app->image_selection.pointer_x=event->x; app->image_selection.pointer_y=event->y;
            UiRect handle={app->image_selection.rect.x+app->image_selection.rect.w-18,app->image_selection.rect.y+app->image_selection.rect.h-18,30,30};
            if (point_in(handle,event->x,event->y)) { app->image_selection.resizing=true; app->image_selection.start_w=app->image_selection.rect.w; app->image_selection.start_h=app->image_selection.rect.h; app->image_selection.current_w=app->image_selection.start_w; app->image_selection.current_h=app->image_selection.start_h; }
            app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true; return;
        }
        int block_y=0; const MdBlock *block=rendered_block_at(app,editor,event->y,&block_y);
        if (block!=NULL&&block->type==MD_BLOCK_TASK_ITEM&&event->x<editor.x+MD_CLAMP(editor.w/12,24,96)+28&&doc->mode!=MD_MODE_PREVIEW) { char error[512]; if (!md_document_toggle_task(doc,block->source_start,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error); app->dirty_frame=true; return; }
        if (doc->mode==MD_MODE_PREVIEW) return;
        if (block!=NULL&&block->type==MD_BLOCK_TABLE) {
            size_t columns=table_column_count(doc,block); int width=MD_MIN(editor.w-64,720),col_width=MD_MAX(1,width/(int)columns),table_x=editor.x+32;
            size_t row=(size_t)MD_MAX(0,(event->y-block_y)/30),col=(size_t)MD_CLAMP((event->x-table_x)/col_width,0,(int)columns-1); MdRange cell;
            if (table_cell_range(doc,block,row,col,&cell,NULL)) { int target=event->x-(table_x+(int)col*col_width+9),prior=0; size_t at=cell.start,best=cell.start; while (at<cell.end) { size_t next=md_grapheme_next(doc->source.data,doc->source.len,at); int measured=text_width(app,app->font,doc->source.data+cell.start,next-cell.start); if (target<(prior+measured)/2) break; best=next; prior=measured; at=next; } doc->cursor=doc->anchor=best; app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true; return; }
        }
        size_t offset=rendered_offset_at(app,editor,event->x,event->y);
        if ((event->state&ControlMask)!=0U) { MdBuf label,destination; md_buf_init(&label); md_buf_init(&destination); (void)md_buf_reserve(&label,0U); (void)md_buf_reserve(&destination,0U); if (md_document_link_at(doc,offset,&label,&destination)) activate_external_uri(app,destination.data); md_buf_free(&label); md_buf_free(&destination); return; }
        if ((event->state&ShiftMask)==0U) doc->anchor=offset;
        doc->cursor=offset;
    } else {
        size_t offset=source_offset_at(app,editor,event->x,event->y); MdRange selection=md_document_selection(doc);
        if (offset>=selection.start&&offset<=selection.end&&selection.start!=selection.end) { app->dragging_selection=true; app->selection_drag_start=offset; app->selection_drop=offset; (void)md_buf_assign(&app->selection_drag_text,doc->source.data+selection.start,selection.end-selection.start); }
        else { if ((event->state&ShiftMask)==0U) doc->anchor=offset; doc->cursor=offset; }
    }
    app->focus=UI_FOCUS_EDITOR; app->dirty_frame=true;
}

static void handle_motion(MdApp *app,XMotionEvent *event) {
    app->pointer_x=event->x; app->pointer_y=event->y; bool changed=false;
    for (size_t i=0U;i<app->button_count;++i) { bool hover=point_in(app->buttons[i].rect,event->x,event->y)&&app->buttons[i].enabled; if (hover!=app->buttons[i].hovered) { app->buttons[i].hovered=hover; changed=true; } }
    if (app->menu.kind!=UI_MENU_NONE&&point_in(app->menu.rect,event->x,event->y)) { int selected=(event->y-(app->menu.rect.y+8))/34; if (selected>=0&&selected<app->menu.item_count&&selected!=app->menu.selected) { app->menu.selected=selected; changed=true; } }
    if (app->dragging_sidebar) { app->sidebar_visual_width=(double)MD_CLAMP(event->x,180,MD_MIN(460,app->width-420)); app->sidebar_target_visible=true; changed=true; }
    if (app->dragging_divider) { MdDocument *doc=active_document(app); int start=app->layout.content.x,available=app->layout.content.w-7; if (doc!=NULL&&available>0) { doc->split_ratio=MD_CLAMP((double)(event->x-start)/(double)available,available>=480?240.0/(double)available:0.2,available>=480?1.0-240.0/(double)available:0.8); changed=true; } }
    if (app->dragging_tab) { for (size_t i=0U;i<app->doc_count;++i) { UiRect rect=tab_rect_at(app,i); if (rect.w>0&&event->x<rect.x+rect.w/2) { app->drag_tab_to=(int)i; break; } app->drag_tab_to=(int)i; } changed=true; }
    if (app->dragging_selection) { MdDocument *doc=active_document(app); if (doc!=NULL) { app->selection_drop=source_offset_at(app,app->layout.source,event->x,event->y); if (event->y<app->layout.source.y+24) doc->source_scroll=MD_MAX(0.0,doc->source_scroll-18.0); else if (event->y>app->layout.source.y+app->layout.source.h-24) doc->source_scroll+=18.0; } changed=true; }
    if (app->image_selection.resizing) { int dx=event->x-app->image_selection.pointer_x; app->image_selection.current_w=MD_CLAMP(app->image_selection.start_w+dx,48,MD_MAX(48,app->layout.content.w-40)); double ratio=(double)app->image_selection.start_h/(double)MD_MAX(1,app->image_selection.start_w); app->image_selection.current_h=MD_MAX(36,(int)llround((double)app->image_selection.current_w*ratio)); changed=true; }
    if (changed) app->dirty_frame=true;
}

static void handle_button_release(MdApp *app,XButtonEvent *event) {
    app->pointer_x=event->x; app->pointer_y=event->y;
    UiButton *activate=NULL; for (size_t i=0U;i<app->button_count;++i) if (app->buttons[i].pressed) { if (point_in(app->buttons[i].rect,event->x,event->y)&&app->buttons[i].enabled) activate=&app->buttons[i]; app->buttons[i].pressed=false; }
    if (activate!=NULL) { if (activate->command==MD_CMD_PALETTE&&strcmp(activate->label,"⋯")==0) { app->menu=(UiMenu){.kind=UI_MENU_FILE,.rect={app->width-270,62,250,288},.selected=0,.item_count=8,.x=app->width-270,.y=62}; app->focus=UI_FOCUS_MENU; } else (void)md_app_execute(app,activate->command); }
    if (app->dragging_tab) {
        if (app->drag_tab_from>=0&&app->drag_tab_to>=0&&app->drag_tab_from!=(int)app->drag_tab_to) {
            MdDocument *moving=app->docs[app->drag_tab_from]; int from=app->drag_tab_from,to=app->drag_tab_to;
            if (from<to) memmove(app->docs+from,app->docs+from+1,(size_t)(to-from)*sizeof(*app->docs)); else memmove(app->docs+to+1,app->docs+to,(size_t)(from-to)*sizeof(*app->docs));
            app->docs[to]=moving; app->active_doc=(size_t)to;
        }
        app->dragging_tab=false; app->drag_tab_from=-1; app->drag_tab_to=-1;
    }
    if (app->dragging_selection) { selection_drop_commit(app,(event->state&ControlMask)!=0U); app->dragging_selection=false; }
    if (app->image_selection.resizing) { image_resize_commit(app); app->image_selection.resizing=false; }
    if (app->dragging_sidebar) app->workspace.sidebar_width=MD_CLAMP(app->sidebar_visual_width,180.0,460.0);
    app->dragging_sidebar=false; app->dragging_divider=false; app->dirty_frame=true;
}

static void clipboard_request(MdApp *app,XSelectionRequestEvent *request) {
    XSelectionEvent reply; memset(&reply,0,sizeof(reply)); reply.type=SelectionNotify; reply.display=request->display; reply.requestor=request->requestor;
    reply.selection=request->selection; reply.target=request->target; reply.time=request->time; reply.property=None;
    Atom property=request->property==None?request->target:request->property;
    if (request->target==app->targets) {
        Atom supported[]={app->utf8_string,XA_STRING,app->targets};
        XChangeProperty(app->display,request->requestor,property,XA_ATOM,32,PropModeReplace,(const unsigned char *)supported,(int)MD_ARRAY_LEN(supported)); reply.property=property;
    } else if (request->target==app->utf8_string||request->target==XA_STRING) {
        XChangeProperty(app->display,request->requestor,property,request->target,8,PropModeReplace,(const unsigned char *)app->clipboard_text.data,(int)MD_MIN(app->clipboard_text.len,(size_t)INT_MAX)); reply.property=property;
    }
    XSendEvent(app->display,request->requestor,False,0,(XEvent *)&reply); XFlush(app->display);
}

static bool decode_uri_path(const char *uri,size_t len,char out[MD_PATH_MAX]) {
    const char *start=uri; if (len>=7U&&memcmp(uri,"file://",7U)==0) { start=uri+7U; len-=7U; if (len>=9U&&memcmp(start,"localhost",9U)==0) { start+=9U; len-=9U; } }
    size_t at=0U;
    for (size_t i=0U;i<len&&start[i]!='\r'&&start[i]!='\n';++i) {
        if (at+1U>=MD_PATH_MAX) return false;
        if (start[i]=='%'&&i+2U<len) {
            uint8_t byte=0U; char hex[2]={start[i+1U],start[i+2U]}; if (!md_hex_decode(hex,2U,&byte,1U)||byte==0U) return false; out[at++]=(char)byte; i+=2U;
        } else out[at++]=start[i];
    }
    out[at]='\0'; return at>0U;
}

static void handle_dropped_uris(MdApp *app,const char *data,size_t len) {
    size_t at=0U; while (at<len) {
        size_t end=at; while (end<len&&data[end]!='\n'&&data[end]!='\r') ++end;
        if (end>at&&data[at]!='#') {
            char path[MD_PATH_MAX]; if (decode_uri_path(data+at,end-at,path)) {
                const char *ext=strrchr(path,'.');
                if (ext!=NULL&&(strcasecmp(ext,".md")==0||strcasecmp(ext,".markdown")==0||strcasecmp(ext,".txt")==0)) (void)open_document(app,path);
                else if (ext!=NULL&&(strcasecmp(ext,".png")==0||strcasecmp(ext,".jpg")==0||strcasecmp(ext,".jpeg")==0||strcasecmp(ext,".bmp")==0)) {
                    app->modal_input.len=0U; app->modal_input.data[0]='\0'; (void)md_buf_append_cstr(&app->modal_input,path); app->modal_selection=app->prefs.default_embed_images?1:0; app->modal=MD_MODAL_IMAGE_STORAGE; (void)insert_image_from_modal(app);
                } else app_toast(app,"Dropped file type is unsupported: %s",path);
            }
        }
        while (end<len&&(data[end]=='\n'||data[end]=='\r')) ++end;
        at=end;
    }
}

static void selection_notify(MdApp *app,XSelectionEvent *event) {
    if (event->property==None) { app_toast(app,app->awaiting_xdnd?"File drop did not provide a URI list":"Clipboard paste target is unavailable"); app->awaiting_paste=false; app->awaiting_xdnd=false; return; }
    Atom type=None; int format=0; unsigned long count=0UL,remaining=0UL; unsigned char *data=NULL;
    if (XGetWindowProperty(app->display,app->window,event->property,0,1L<<24,True,AnyPropertyType,&type,&format,&count,&remaining,&data)!=Success||data==NULL) { app_toast(app,"Selection transfer failed"); return; }
    size_t bytes=format==8?(size_t)count:format==16?(size_t)count*2U:(size_t)count*4U;
    if (app->awaiting_xdnd) {
        handle_dropped_uris(app,(const char *)data,bytes); app->awaiting_xdnd=false;
        XClientMessageEvent finished; memset(&finished,0,sizeof(finished)); finished.type=ClientMessage; finished.display=app->display; finished.window=app->xdnd_source; finished.message_type=app->xdnd_finished; finished.format=32; finished.data.l[0]=(long)app->window; finished.data.l[1]=1L; finished.data.l[2]=(long)XInternAtom(app->display,"XdndActionCopy",False); XSendEvent(app->display,app->xdnd_source,False,NoEventMask,(XEvent *)&finished);
    } else if (app->awaiting_paste) {
        MdDocument *doc=active_document(app); char error[512];
        if (!md_utf8_validate((const char *)data,bytes,NULL)) modal_open(app,MD_MODAL_ERROR,"Clipboard text is not valid UTF-8; the document was not changed.");
        else if (app->modal!=MD_MODAL_NONE&&
                 (app->modal==MD_MODAL_OPEN_PATH||app->modal==MD_MODAL_SAVE_PATH||app->modal==MD_MODAL_WORKSPACE_PATH||
                  app->modal==MD_MODAL_COMMAND_PALETTE||app->modal==MD_MODAL_IMAGE_STORAGE||app->modal==MD_MODAL_IMAGE_PROPERTIES||
                  app->modal==MD_MODAL_TREE_ACTION||app->modal==MD_MODAL_LINK_PROPERTIES)) {
            MdBuf *target=app->modal==MD_MODAL_LINK_PROPERTIES&&app->modal_selection==1?&app->modal_secondary:&app->modal_input;
            append_input(target,(const char *)data,bytes);
            if (app->modal==MD_MODAL_COMMAND_PALETTE) app->modal_selection=0;
        } else if (app->focus==UI_FOCUS_FIND) {
            MdBuf *target=app->find.editing_replacement?&app->find.replacement:&app->find.query;
            append_input(target,(const char *)data,bytes); if (!app->find.editing_replacement) find_refresh(app);
        } else if (doc!=NULL&&!md_document_insert_utf8(doc,(const char *)data,bytes,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
        app->awaiting_paste=false;
    }
    XFree(data); app->dirty_frame=true;
}

static void handle_xdnd_message(MdApp *app,XClientMessageEvent *message) {
    if (message->message_type==app->xdnd_enter) {
        app->xdnd_source=(Window)message->data.l[0]; app->xdnd_version=(int)((message->data.l[1]>>24)&0xffL);
    } else if (message->message_type==app->xdnd_position) {
        XClientMessageEvent status; memset(&status,0,sizeof(status)); status.type=ClientMessage; status.display=app->display; status.window=app->xdnd_source; status.message_type=app->xdnd_status; status.format=32; status.data.l[0]=(long)app->window; status.data.l[1]=1L; status.data.l[4]=(long)XInternAtom(app->display,"XdndActionCopy",False); XSendEvent(app->display,app->xdnd_source,False,NoEventMask,(XEvent *)&status);
    } else if (message->message_type==app->xdnd_drop) {
        app->awaiting_xdnd=true; Time time=app->xdnd_version>=1?(Time)message->data.l[2]:CurrentTime;
        XConvertSelection(app->display,app->xdnd_selection,app->text_uri_list,app->clipboard_property,app->window,time);
    }
}

static void recreate_back_buffer(MdApp *app) {
    if (app->xft!=NULL) { XftDrawDestroy(app->xft); app->xft=NULL; }
    if (app->back!=None) XFreePixmap(app->display,app->back);
    app->back=XCreatePixmap(app->display,app->window,(unsigned)app->width,(unsigned)app->height,(unsigned)app->depth);
    app->xft=XftDrawCreate(app->display,app->back,app->visual,app->colormap); app->dirty_frame=true;
}

static void handle_test_client_message(MdApp *app,XClientMessageEvent *message) {
    if (!app->test_mode) return;
    if (message->message_type==app->test_command) {
        long command=message->data.l[0];
        if (command>=0L&&command<(long)MD_CMD_COUNT) (void)md_app_execute(app,(MdCommandId)command);
        else if (command==1000L) { MdDocument *doc=active_document(app); if (doc!=NULL) { doc->anchor=0U; doc->cursor=doc->source.len; app->dirty_frame=true; } }
        else if (command==1002L) { MdDocument *doc=active_document(app); if (doc!=NULL) { for (size_t at=0U;at<doc->source.len;at=md_utf8_next(doc->source.data,doc->source.len,at)) { MdBuf label,destination; md_buf_init(&label); md_buf_init(&destination); (void)md_buf_reserve(&label,0U); (void)md_buf_reserve(&destination,0U); bool found=md_document_link_at(doc,at,&label,&destination); if (found&&(strncmp(destination.data,"http://",7U)==0||strncmp(destination.data,"https://",8U)==0)) { activate_external_uri(app,destination.data); md_buf_free(&label); md_buf_free(&destination); break; } md_buf_free(&label); md_buf_free(&destination); } } }
        else if (command>=1003L&&command<=1009L) {
            static const MdBlockType types[]={MD_BLOCK_PARAGRAPH,MD_BLOCK_HEADING,MD_BLOCK_TASK_ITEM,MD_BLOCK_QUOTE,MD_BLOCK_FENCED_CODE,MD_BLOCK_TABLE,MD_BLOCK_IMAGE};
            select_first_block_type(app,types[command-1003L]); app->image_selection.selected=command==1009L;
            if (command==1008L) {
                MdDocument *doc=active_document(app); if (doc!=NULL) for (size_t i=0U;i<doc->render.block_count;++i) {
                    const MdBlock *block=&doc->render.blocks[i]; MdRange cell;
                    if (block->type==MD_BLOCK_TABLE&&table_cell_range(doc,block,0U,0U,&cell,NULL)) { doc->cursor=doc->anchor=cell.start; break; }
                }
            }
            app->dirty_frame=true;
        }
        else if (command==1010L) {
            MdDocument *doc=active_document(app); if (doc!=NULL) for (size_t i=0U;i<doc->render.block_count;++i) {
                const MdBlock *block=&doc->render.blocks[i]; if ((block->type==MD_BLOCK_HEADING||block->type==MD_BLOCK_SETEXT_HEADING)&&block->level==2) { doc->cursor=doc->anchor=block->content_start; break; }
            }
            app->image_selection.selected=false; app->dirty_frame=true;
        }
        else if (command==1011L) {
            MdDocument *doc=active_document(app); if (doc!=NULL) {
                const char *open=strchr(doc->source.data,'`');
                if (open!=NULL) { const char *close=strchr(open+1,'`'); if (close!=NULL) { doc->anchor=(size_t)(open-doc->source.data); doc->cursor=(size_t)(close-doc->source.data)+1U; } }
            }
            app->image_selection.selected=false; app->dirty_frame=true;
        }
        else if (command==1012L) {
            MdDocument *doc=active_document(app);
            if (doc!=NULL) {
                md_storage_set_fault((MdFaultInjection){MD_FAULT_ENOSPC,0U});
                (void)save_document(app,doc,NULL,false);
                md_storage_clear_fault();
            }
            app->dirty_frame=true;
        }
        else if (command==1013L) {
            MdDocument *doc=active_document(app); char error[512];
            if (doc!=NULL&&doc->recovery_root[0]!='\0') {
                if (!md_recovery_write(doc->recovery_root,doc,error,sizeof(error))) modal_open(app,MD_MODAL_ERROR,error);
                else app_toast(app,"Completed a production recovery write for crash-restart validation");
            }
            app->dirty_frame=true;
        }
        else if (command==1014L) {
            MdDocument *doc=active_document(app);
            if (doc!=NULL) for (size_t i=doc->render.block_count;i>0U;--i) {
                const MdBlock *block=&doc->render.blocks[i-1U];
                if (block->type==MD_BLOCK_IMAGE) {
                    doc->mode=MD_MODE_RENDERED;
                    doc->preview_scroll=0.0;
                    for (size_t before=0U;before+1U<i;++before)
                        doc->preview_scroll+=(double)block_height(app,&doc->render.blocks[before]);
                    doc->cursor=doc->anchor=block->content_start; app->image_selection.selected=true;
                    app->image_selection.source_start=block->source_start; app->image_selection.source_end=block->source_end; break;
                }
            }
            app->dirty_frame=true;
        }
    } else if (message->message_type==app->test_insert) {
        Atom type=None; int format=0; unsigned long count=0UL,remaining=0UL; unsigned char *data=NULL;
        if (XGetWindowProperty(app->display,app->window,app->test_insert,0,1L<<24,True,app->utf8_string,&type,&format,&count,&remaining,&data)==Success&&data!=NULL) {
            MdDocument *doc=active_document(app); char error[512]; if (doc!=NULL&&format==8) (void)md_document_insert_utf8(doc,(const char *)data,(size_t)count,error,sizeof(error)); XFree(data); app->dirty_frame=true;
        }
    }
}

static void handle_event(MdApp *app,XEvent *event) {
    if (XFilterEvent(event,app->window)) return;
    switch (event->type) {
        case Expose: app->dirty_frame=true; break;
        case ConfigureNotify:
            if (event->xconfigure.width!=app->width||event->xconfigure.height!=app->height) { app->width=MD_MAX(UI_MIN_WIDTH,event->xconfigure.width); app->height=MD_MAX(UI_MIN_HEIGHT,event->xconfigure.height); recreate_back_buffer(app); }
            if (!app->compact_nav&&app->width<900) app->compact_nav=true; else if (app->compact_nav&&app->width>960) app->compact_nav=false; break;
        case MotionNotify: handle_motion(app,&event->xmotion); break;
        case ButtonPress: handle_button_press(app,&event->xbutton); break;
        case ButtonRelease: handle_button_release(app,&event->xbutton); break;
        case KeyPress: handle_key(app,&event->xkey); break;
        case FocusIn: if (app->xic!=NULL) XSetICFocus(app->xic); app->dirty_frame=true; break;
        case FocusOut: if (app->xic!=NULL) XUnsetICFocus(app->xic); app->dirty_frame=true; break;
        case SelectionRequest: clipboard_request(app,&event->xselectionrequest); break;
        case SelectionNotify: selection_notify(app,&event->xselection); break;
        case SelectionClear: if (event->xselectionclear.selection==app->clipboard) { app->clipboard_text.len=0U; app->clipboard_text.data[0]='\0'; } break;
        case ClientMessage:
            if ((Atom)event->xclient.data.l[0]==app->wm_delete&&event->xclient.message_type==XInternAtom(app->display,"WM_PROTOCOLS",False)) request_exit(app);
            else if (event->xclient.message_type==app->xdnd_enter||event->xclient.message_type==app->xdnd_position||event->xclient.message_type==app->xdnd_drop) handle_xdnd_message(app,&event->xclient);
            else handle_test_client_message(app,&event->xclient);
            break;
        default: break;
    }
}

static void periodic_storage(MdApp *app,uint64_t now) {
    if (app->recovery_scan_pending&&app->modal==MD_MODAL_NONE&&!app->modal_closing) {
        app->recovery_scan_pending=false;
        scan_startup_recovery(app);
    }
    if (app->prefs.autosave_enabled&&now-app->last_autosave_ms>=(uint64_t)app->prefs.autosave_interval*1000U) {
        for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty&&app->docs[i]->recovery_root[0]!='\0') { char error[512]; if (!md_recovery_write(app->docs[i]->recovery_root,app->docs[i],error,sizeof(error))) app_toast(app,"Recovery autosave failed: %s; edited data remains in memory",error); }
        app->last_autosave_ms=now;
    }
    if (now-app->last_external_check_ms>=1500U) {
        for (size_t i=0U;i<app->doc_count;++i) {
            MdDocument *doc=app->docs[i]; if (doc->path[0]=='\0'||!doc->has_disk_sha256||doc->conflict) continue;
            MdBytes bytes; md_bytes_init(&bytes); char error[256];
            if (!md_read_file(doc->path,&bytes,error,sizeof(error))) { if (errno==ENOENT) { doc->orphaned=true; if (app->modal==MD_MODAL_NONE) modal_open(app,MD_MODAL_EXTERNAL_CONFLICT,"The backing file was deleted. The complete in-memory buffer is still available; choose Save As or recreate explicitly."); } }
            else { uint8_t digest[32]; md_sha256(bytes.data,bytes.len,digest); if (memcmp(digest,doc->disk_sha256,32U)!=0) { doc->conflict=true; if (app->modal==MD_MODAL_NONE) modal_open(app,MD_MODAL_EXTERNAL_CONFLICT,doc->dirty?"Both the editor and disk content changed. Neither version will be overwritten automatically.":"The clean document changed on disk. Reload, Keep Current, or Compare."); } }
            md_bytes_free(&bytes);
        }
        app->last_external_check_ms=now;
    }
}

static void animate(MdApp *app,uint64_t now) {
    double dt=app->last_frame_ms==0U?0.016:MD_CLAMP((double)(now-app->last_frame_ms)/1000.0,0.0,0.05);
    if (fabs(app->capsule_x-app->capsule_target)>0.001) { app->capsule_x+=(app->capsule_target-app->capsule_x)*MD_MIN(1.0,dt*12.0); app->dirty_frame=true; } else app->capsule_x=app->capsule_target;
    double sidebar_target=app->sidebar_target_visible?(double)MD_CLAMP((int)llround(app->workspace.sidebar_width),180,460):0.0;
    if (app->workspace.root[0]=='\0'&&!app->outline_visible) sidebar_target=0.0;
    if (fabs(app->sidebar_visual_width-sidebar_target)>0.5&&!app->dragging_sidebar) { app->sidebar_visual_width+=(sidebar_target-app->sidebar_visual_width)*MD_MIN(1.0,dt*11.0); app->dirty_frame=true; }
    for (size_t i=0U;i<app->button_count;++i) { double target=app->buttons[i].hovered&&app->buttons[i].enabled?1.0:0.0; if (fabs(app->buttons[i].hover_progress-target)>0.01) { app->buttons[i].hover_progress+=(target-app->buttons[i].hover_progress)*MD_MIN(1.0,dt*14.0); app->dirty_frame=true; } }
    MdDocument *doc=active_document(app); double scroll=doc==NULL?0.0:(doc->mode==MD_MODE_SOURCE?doc->source_scroll:doc->preview_scroll); double frost=MD_CLAMP(scroll/140.0,0.0,1.0);
    if (fabs(app->nav_frost-frost)>0.01) { app->nav_frost+=(frost-app->nav_frost)*MD_MIN(1.0,dt*9.0); app->dirty_frame=true; }
}

static bool initialize_x11(MdApp *app) {
    (void)setlocale(LC_ALL,""); (void)signal(SIGCHLD,SIG_IGN); (void)XSetLocaleModifiers(""); app->display=XOpenDisplay(NULL); if (app->display==NULL) { fputs("mdeditor: cannot open X11 display\n",stderr); return false; }
    app->screen=DefaultScreen(app->display); app->visual=DefaultVisual(app->display,app->screen); app->colormap=DefaultColormap(app->display,app->screen); app->depth=DefaultDepth(app->display,app->screen);
    app->window=XCreateSimpleWindow(app->display,RootWindow(app->display,app->screen),80,60,(unsigned)app->width,(unsigned)app->height,0,BlackPixel(app->display,app->screen),WhitePixel(app->display,app->screen));
    XSelectInput(app->display,app->window,ExposureMask|StructureNotifyMask|KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|PropertyChangeMask);
    XSizeHints hints; memset(&hints,0,sizeof(hints)); hints.flags=PMinSize; hints.min_width=UI_MIN_WIDTH; hints.min_height=UI_MIN_HEIGHT; XSetWMNormalHints(app->display,app->window,&hints);
    app->wm_delete=XInternAtom(app->display,"WM_DELETE_WINDOW",False); XSetWMProtocols(app->display,app->window,&app->wm_delete,1);
    app->utf8_string=XInternAtom(app->display,"UTF8_STRING",False); Atom net_name=XInternAtom(app->display,"_NET_WM_NAME",False); const char title[]="Lattice Markdown — C17/X11"; XChangeProperty(app->display,app->window,net_name,app->utf8_string,8,PropModeReplace,(const unsigned char *)title,(int)sizeof(title)-1); XStoreName(app->display,app->window,title);
    app->clipboard=XInternAtom(app->display,"CLIPBOARD",False); app->targets=XInternAtom(app->display,"TARGETS",False); app->clipboard_property=XInternAtom(app->display,"MDEDIT_SELECTION",False);
    app->xdnd_aware=XInternAtom(app->display,"XdndAware",False); app->xdnd_enter=XInternAtom(app->display,"XdndEnter",False); app->xdnd_position=XInternAtom(app->display,"XdndPosition",False); app->xdnd_status=XInternAtom(app->display,"XdndStatus",False); app->xdnd_drop=XInternAtom(app->display,"XdndDrop",False); app->xdnd_finished=XInternAtom(app->display,"XdndFinished",False); app->xdnd_selection=XInternAtom(app->display,"XdndSelection",False); app->text_uri_list=XInternAtom(app->display,"text/uri-list",False);
    unsigned long xdnd_version=5UL; XChangeProperty(app->display,app->window,app->xdnd_aware,XA_ATOM,32,PropModeReplace,(const unsigned char *)&xdnd_version,1);
    app->test_command=XInternAtom(app->display,"_MDEDIT_TEST_COMMAND",False); app->test_insert=XInternAtom(app->display,"_MDEDIT_TEST_INSERT",False); app->state_atom=XInternAtom(app->display,"_MDEDIT_STATE",False);
    app->gc=XCreateGC(app->display,app->window,0UL,NULL); recreate_back_buffer(app);
    char normal[128],bold[128],mono[128],heading[128]; (void)snprintf(normal,sizeof(normal),"Noto Sans CJK TC:size=%d",app->prefs.font_size); (void)snprintf(bold,sizeof(bold),"Noto Sans CJK TC:style=Bold:size=%d",app->prefs.font_size); (void)snprintf(mono,sizeof(mono),"Noto Sans Mono CJK TC:size=%d",app->prefs.font_size); (void)snprintf(heading,sizeof(heading),"Noto Sans CJK TC:style=Bold:size=%d",app->prefs.font_size+5);
    app->font=XftFontOpenName(app->display,app->screen,normal); app->font_bold=XftFontOpenName(app->display,app->screen,bold); app->font_mono=XftFontOpenName(app->display,app->screen,mono); app->font_heading=XftFontOpenName(app->display,app->screen,heading);
    app->font_symbols=XftFontOpenName(app->display,app->screen,"Noto Sans Symbols2:size=16"); app->font_emoji=XftFontOpenName(app->display,app->screen,"Noto Color Emoji:size=16");
    if (app->font==NULL||app->font_bold==NULL||app->font_mono==NULL||app->font_heading==NULL) { fputs("mdeditor: required Xft fonts could not be opened\n",stderr); return false; }
    app->xim=XOpenIM(app->display,NULL,NULL,NULL); if (app->xim!=NULL) app->xic=XCreateIC(app->xim,XNInputStyle,XIMPreeditNothing|XIMStatusNothing,XNClientWindow,app->window,XNFocusWindow,app->window,NULL);
    palette_init(app); XMapWindow(app->display,app->window); app->mapped=true; app->dirty_frame=true; return true;
}

static void ensure_demo_document(MdApp *app) {
    static const char demo[]=
        "# Lattice Markdown\n\n"
        "A **native C17** editor where rendered content remains mapped to the Markdown source.\n\n"
        "## 今日工作 / Today\n\n"
        "- [x] Build a custom X11 interface\n"
        "- [ ] Edit this checkbox in Rendered Edit mode\n"
        "- Mixed Unicode: 繁體中文, café, e\xCC\x81, ✈️, 👩‍💻\n\n"
        "> Source, preview, and rendered editing share one authoritative byte buffer.\n\n"
        "```c\n"
        "int main(void) { return 0; } /* **literal Markdown punctuation** */\n"
        "```\n\n"
        "| Feature | State | Alignment |\n"
        "| :--- | :---: | ---: |\n"
        "| Source mapping | Ready | 100 |\n"
        "| Undo transaction | Ready | 1 |\n\n"
        "## Duplicate heading\n\nFirst target.\n\n"
        "## Duplicate heading\n\nSecond target remains position-distinct.\n";
    if (app->doc_count==0U) (void)add_new_document(app);
    MdDocument *doc=active_document(app); char error[512];
    if (doc!=NULL&&doc->source.len==0U) { (void)md_document_set_source(doc,demo,sizeof(demo)-1U,false,error,sizeof(error)); doc->dirty=false; }
}

static void select_first_block_type(MdApp *app,MdBlockType type) {
    MdDocument *doc=active_document(app); if (doc==NULL) return;
    for (size_t i=0U;i<doc->render.block_count;++i) if (doc->render.blocks[i].type==type) {
        doc->cursor=doc->anchor=doc->render.blocks[i].content_start;
        if (type==MD_BLOCK_IMAGE) { app->image_selection.selected=true; app->image_selection.source_start=doc->render.blocks[i].source_start; app->image_selection.source_end=doc->render.blocks[i].source_end; }
        return;
    }
}

static void open_workspace_tabs(MdApp *app) {
    if (app->workspace.root[0]=='\0') return;
    for (size_t i=0U;i<app->workspace.count&&app->doc_count<4U;++i) {
        MdTreeEntry *entry=&app->workspace.entries[i]; const char *ext=strrchr(entry->path,'.');
        if (!entry->is_directory&&ext!=NULL&&(strcasecmp(ext,".md")==0||strcasecmp(ext,".markdown")==0)) { char path[MD_PATH_MAX]; if (md_path_join(path,app->workspace.root,entry->path)) (void)open_document(app,path); }
    }
}

static void apply_test_state(MdApp *app,const char *id) {
    if (id==NULL||id[0]=='\0') return;
    app->test_mode=true; (void)snprintf(app->test_state,sizeof(app->test_state),"%s",id);
    if (strcmp(id,"UI-EMPTY-LIGHT")==0||strcmp(id,"UI-EMPTY-DARK")==0) {
        while (app->doc_count>0U) remove_document(app,app->doc_count-1U);
        app->dark=strcmp(id,"UI-EMPTY-DARK")==0; app->prefs.dark_theme=app->dark; palette_init(app); return;
    }
    if (strcmp(id,"UI-WORKSPACE-MULTITAB")==0||strcmp(id,"UI-TAB-REORDER")==0||strcmp(id,"UI-SIDEBAR-COLLAPSED")==0) { open_workspace_tabs(app); ensure_demo_document(app); if (active_document(app)!=NULL) { char error[128]; active_document(app)->cursor=active_document(app)->source.len; (void)md_document_insert_utf8(active_document(app),"\nunsaved workspace note",23U,error,sizeof(error)); } if (strcmp(id,"UI-TAB-REORDER")==0) { app->dragging_tab=true; app->drag_tab_from=0; app->drag_tab_to=MD_MIN(2,(int)app->doc_count-1); } if (strcmp(id,"UI-SIDEBAR-COLLAPSED")==0) { app->sidebar_target_visible=false; app->sidebar_visual_width=36.0; } return; }
    ensure_demo_document(app); MdDocument *doc=active_document(app); if (doc==NULL) return;
    if (strcmp(id,"UI-SOURCE")==0) doc->mode=MD_MODE_SOURCE;
    else if (strcmp(id,"UI-SPLIT")==0) doc->mode=MD_MODE_SPLIT;
    else if (strcmp(id,"UI-PREVIEW")==0) doc->mode=MD_MODE_PREVIEW;
    else if (strcmp(id,"UI-RENDERED-EDIT")==0) { doc->mode=MD_MODE_RENDERED; select_first_block_type(app,MD_BLOCK_HEADING); doc->anchor=doc->cursor; }
    else if (strcmp(id,"UI-MARKDOWN-ALL")==0) doc->mode=MD_MODE_RENDERED;
    else if (strcmp(id,"UI-IMAGE-SELECTED")==0||strcmp(id,"UI-IMAGE-RESIZE")==0) { doc->mode=MD_MODE_RENDERED; select_first_block_type(app,MD_BLOCK_IMAGE); }
    else if (strcmp(id,"UI-TABLE-EDIT")==0) {
        doc->mode=MD_MODE_RENDERED; select_first_block_type(app,MD_BLOCK_TABLE); app->image_selection.selected=false;
        doc->preview_scroll=0.0;
        for (size_t i=0U;i<doc->render.block_count;++i) {
            const MdBlock *block=&doc->render.blocks[i];
            if (block->type==MD_BLOCK_TABLE) {
                MdRange cell; if (table_cell_range(doc,block,0U,0U,&cell,NULL)) doc->cursor=doc->anchor=cell.start;
                break;
            }
            doc->preview_scroll+=(double)block_height(app,block);
        }
    }
    else if (strcmp(id,"UI-OUTLINE")==0) { doc->mode=MD_MODE_RENDERED; app->outline_visible=true; app->sidebar_target_visible=true; app->sidebar_visual_width=UI_SIDEBAR_DEFAULT; }
    else if (strcmp(id,"UI-COMMAND-PALETTE")==0) { modal_open(app,MD_MODAL_COMMAND_PALETTE,""); (void)md_buf_append_cstr(&app->modal_input,"stat"); }
    else if (strcmp(id,"UI-STATISTICS")==0) modal_open(app,MD_MODAL_STATISTICS,"");
    else if (strcmp(id,"UI-VERSION-HISTORY")==0) { char error[512]; if (doc->history_root[0]!='\0'&&doc->path[0]!='\0') { for (int i=0;i<3;++i) (void)md_history_create(doc->history_root,doc,true,NULL,error,sizeof(error)); } history_open(app); }
    else if (strcmp(id,"UI-DIFF-SIDE-BY-SIDE")==0||strcmp(id,"UI-DIFF-INLINE")==0) {
        static const char visual_diff[]="\n## Visual diff\n\n繁體中文變更與 token refinement.\n"; char error[512];
        if (doc->history_root[0]!='\0'&&doc->path[0]!='\0') {
            (void)md_history_create(doc->history_root,doc,true,NULL,error,sizeof(error));
            doc->cursor=doc->anchor=doc->source.len;
            (void)md_document_insert_utf8(doc,visual_diff,sizeof(visual_diff)-1U,error,sizeof(error));
            history_open(app);
        }
        diff_open_current(app,strcmp(id,"UI-DIFF-SIDE-BY-SIDE")==0);
    }
    else if (strcmp(id,"UI-MODAL-BLUR")==0) { doc->source_scroll=180.0; modal_open(app,MD_MODAL_UNSAVED,"Unsaved changes stay in memory until Save or explicit Discard."); }
    else if (strcmp(id,"UI-FROSTED-SCROLLED")==0) { doc->mode=MD_MODE_SOURCE; doc->source_scroll=340.0; app->nav_frost=1.0; }
    else if (strcmp(id,"UI-IMAGE-STORAGE")==0) modal_open(app,MD_MODAL_IMAGE_STORAGE,"Choose a persistence model.");
    else if (strcmp(id,"UI-BUTTON-HOVER")==0) { app->pointer_x=600; app->pointer_y=36; }
    else if (strcmp(id,"UI-BUTTON-RIPPLE")==0) { UiRect save={568,18,62,36}; ripple_add(app,save,600,36); app->pointer_x=600; app->pointer_y=36; }
    app->capsule_x=app->capsule_target=(double)doc->mode; app->dirty_frame=true;
}

static void scan_startup_recovery(MdApp *app) {
    MdDocument temporary; md_document_init(&temporary,999U); assign_document_roots(app,&temporary); char warning[512]={0};
    md_recovery_list_free(&app->recoveries); md_recovery_list_init(&app->recoveries);
    if (temporary.recovery_root[0]!='\0'&&md_recovery_scan(temporary.recovery_root,&app->recoveries,warning,sizeof(warning))) {
        size_t valid=0U; for (size_t i=0U;i<app->recoveries.count;++i) if (app->recoveries.items[i].valid) ++valid;
        if (valid>0U) {
            for (size_t i=0U;i<app->recoveries.count;++i) if (app->recoveries.items[i].valid) { app->recovery_index=i; break; }
            char message[512]; (void)snprintf(message,sizeof(message),"%zu valid recovery record(s) are available. Corrupt entries are isolated and never trusted.",valid); modal_open(app,MD_MODAL_RECOVERY,message);
        }
        else if (warning[0]!='\0') app_toast(app,"%s",warning);
    }
    md_document_free(&temporary);
}

static void orderly_state_write(MdApp *app) {
    for (size_t i=0U;i<app->doc_count;++i) if (app->docs[i]->dirty&&app->docs[i]->recovery_root[0]!='\0') { char error[512]; (void)md_recovery_write(app->docs[i]->recovery_root,app->docs[i],error,sizeof(error)); }
    if (app->workspace.root[0]!='\0') { char error[512]; (void)save_workspace_session_snapshot(app,error,sizeof(error)); }
}

static void print_usage(void) {
    puts("usage: mdeditor [--open FILE]... [--workspace DIR] [--test-state ID] [--quit-after MILLISECONDS] [--test-mode]");
}

int md_app_run(MdApp *app,int argc,char **argv) {
    if (app==NULL||!initialize_x11(app)) return 1;
    const char *test_state=NULL; bool explicit_content=false;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--open")==0&&i+1<argc) { explicit_content=true; (void)open_document(app,argv[++i]); }
        else if (strcmp(argv[i],"--workspace")==0&&i+1<argc) { explicit_content=true; (void)open_workspace(app,argv[++i]); }
        else if (strcmp(argv[i],"--test-state")==0&&i+1<argc) { test_state=argv[++i]; app->test_mode=true; }
        else if (strcmp(argv[i],"--quit-after")==0&&i+1<argc) { char *end=NULL; unsigned long value=strtoul(argv[++i],&end,10); if (end==NULL||*end!='\0'||value<50UL) { print_usage(); return 2; } app->quit_at=md_now_millis()+(uint64_t)value; }
        else if (strcmp(argv[i],"--test-mode")==0) app->test_mode=true;
        else if (strcmp(argv[i],"--help")==0) { print_usage(); return 0; }
        else { fprintf(stderr,"mdeditor: unknown or incomplete option: %s\n",argv[i]); print_usage(); return 2; }
    }
    if (!explicit_content&&!app->test_mode&&app->prefs.restore_session) scan_startup_recovery(app);
    apply_test_state(app,test_state); app->last_autosave_ms=md_now_millis(); app->last_external_check_ms=app->last_autosave_ms;
    while (app->running) {
        while (XPending(app->display)>0) { XEvent event; XNextEvent(app->display,&event); handle_event(app,&event); }
        uint64_t now=md_now_millis(); if (app->quit_at!=0U&&now>=app->quit_at) app->running=false;
        periodic_storage(app,now); animate(app,now); if (app->dirty_frame) draw_frame(app);
        struct pollfd descriptor={ConnectionNumber(app->display),POLLIN,0}; (void)poll(&descriptor,1,16);
    }
    orderly_state_write(app); return 0;
}
