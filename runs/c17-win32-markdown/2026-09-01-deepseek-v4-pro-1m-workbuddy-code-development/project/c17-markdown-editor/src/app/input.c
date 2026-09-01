/* input.c - window procedure, input handling, editing, WinMain. */
#include "app.h"
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"
#include "winutil.h"
#include "stats.h"
#include "search.h"
#include <wchar.h>
#include <shellapi.h>
#include <windowsx.h>

/* defined in view.c */
void app_render(App *a);
void put_px_border(App *a, int x, int y);

/* ---------------- text editing operations ---------------- */

static void caret_left(App *a, DocTab *t){
    const char *src = md_document_text(&t->doc);
    size_t pos = t->caret;
    if(t->has_sel){ t->caret = t->sel_start < t->sel_end ? t->sel_start : t->sel_end; t->has_sel = false; return; }
    t->caret = ce_grapheme_prev((const uint8_t*)src, pos);
    (void)a;
}
static void caret_right(App *a, DocTab *t){
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    if(t->has_sel){ t->caret = t->sel_start < t->sel_end ? t->sel_end : t->sel_start; t->has_sel = false; return; }
    t->caret = ce_grapheme_next((const uint8_t*)src, len, t->caret);
    (void)a;
}

static void insert_text_at(App *a, DocTab *t, const char *text, size_t len){
    if(t->has_sel){
        size_t s = t->sel_start < t->sel_end ? t->sel_start : t->sel_end;
        size_t e = t->sel_start < t->sel_end ? t->sel_end : t->sel_start;
        md_document_edit_begin(&t->doc);
        md_document_edit_op(&t->doc, s, e - s, text, len);
        md_document_edit_end(&t->doc);
        t->caret = s + len;
        t->has_sel = false;
    } else {
        md_document_insert(&t->doc, t->caret, text, len);
        t->caret += len;
    }
    t->sel_start = t->sel_end = t->caret;
    t->parsed_dirty = true;
    (void)a;
}

static void backspace(App *a, DocTab *t){
    const char *src = md_document_text(&t->doc);
    if(t->has_sel){
        size_t s = t->sel_start < t->sel_end ? t->sel_start : t->sel_end;
        size_t e = t->sel_start < t->sel_end ? t->sel_end : t->sel_start;
        md_document_delete(&t->doc, s, e - s);
        t->caret = s; t->has_sel = false;
    } else if(t->caret > 0){
        size_t p = ce_grapheme_prev((const uint8_t*)src, t->caret);
        md_document_delete(&t->doc, p, t->caret - p);
        t->caret = p;
    }
    t->sel_start = t->sel_end = t->caret;
    t->parsed_dirty = true;
    (void)a;
}

static void delete_fwd(App *a, DocTab *t){
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    if(t->has_sel){
        size_t s = t->sel_start < t->sel_end ? t->sel_start : t->sel_end;
        size_t e = t->sel_start < t->sel_end ? t->sel_end : t->sel_start;
        md_document_delete(&t->doc, s, e - s);
        t->caret = s; t->has_sel = false;
    } else if(t->caret < len){
        size_t n = ce_grapheme_next((const uint8_t*)src, len, t->caret);
        md_document_delete(&t->doc, t->caret, n - t->caret);
    }
    t->sel_start = t->sel_end = t->caret;
    t->parsed_dirty = true;
    (void)a;
}

/* ---------------- source hit-testing ---------------- */

int app_source_hit(App *a, DocTab *t, int mx, int my, size_t *pos){
    (void)a;
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    int base = a->prefs.font_size;
    int line_h = base + 4 + a->prefs.line_spacing * 2;
    int gutter = 48;
    /* line = scroll_y/line_h + (my)/line_h */
    int line = t->scroll_y / line_h + my / line_h;
    /* compute byte offset for line, col */
    int cur_line = 0;
    size_t i = 0;
    while(i < len && cur_line < line){
        if(src[i] == '\n') cur_line++;
        i++;
    }
    size_t ls = i;
    /* column from mx */
    int col = (mx - gutter) / (base - 2);
    if(col < 0) col = 0;
    size_t p = ls;
    int cc = 0;
    while(p < len && src[p] != '\n' && cc < col){
        p = ce_utf8_next((const uint8_t*)src, len, p);
        cc++;
    }
    *pos = p;
    return 0;
}

/* ---------------- keyboard ---------------- */

static void select_all(App *a, DocTab *t){
    t->sel_start = 0;
    t->sel_end = md_document_len(&t->doc);
    t->has_sel = true;
    t->caret = t->sel_end;
    (void)a;
}

static void copy_selection(App *a, DocTab *t){
    if(!t->has_sel) return;
    const char *src = md_document_text(&t->doc);
    size_t s = t->sel_start < t->sel_end ? t->sel_start : t->sel_end;
    size_t e = t->sel_start < t->sel_end ? t->sel_end : t->sel_start;
    char *sel = ce_strndup(src + s, e - s);
    wchar_t *w = wu_u8_to_w(sel);
    ce_free(sel);
    if(!w) return;
    size_t wl = wcslen(w);
    if(OpenClipboard(a->hwnd)){
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (wl + 1) * sizeof(wchar_t));
        if(h){
            wchar_t *p = GlobalLock(h);
            memcpy(p, w, (wl + 1) * sizeof(wchar_t));
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        }
        CloseClipboard();
    }
    ce_free(w);
}

static void cut_selection(App *a, DocTab *t){
    if(!t->has_sel) return;
    copy_selection(a, t);
    /* cut only if clipboard ownership succeeded */
    if(IsClipboardFormatAvailable(CF_UNICODETEXT)){
        size_t s = t->sel_start < t->sel_end ? t->sel_start : t->sel_end;
        size_t e = t->sel_start < t->sel_end ? t->sel_end : t->sel_start;
        md_document_delete(&t->doc, s, e - s);
        t->caret = s; t->has_sel = false;
        t->parsed_dirty = true;
    }
}

static void paste_clipboard(App *a, DocTab *t){
    if(!OpenClipboard(a->hwnd)) return;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if(h){
        wchar_t *w = GlobalLock(h);
        if(w){
            char *u8 = wu_w_to_u8(w);
            GlobalUnlock(h);
            if(u8){
                /* normalize CRLF -> LF */
                ce_buf nb; ce_buf_init(&nb);
                for(char *p = u8; *p; p++){
                    if(*p == '\r'){ if(p[1] == '\n') p++; ce_buf_append_c(&nb, '\n'); }
                    else ce_buf_append_c(&nb, *p);
                }
                insert_text_at(a, t, nb.data, nb.len);
                ce_buf_free(&nb);
                ce_free(u8);
            }
        }
    }
    CloseClipboard();
}

/* ---------------- WinMain + window proc ---------------- */

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

static App *g_app = NULL;

static void handle_mouse(App *a, int x, int y, int button, bool down, bool move);

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE prev, PWSTR cmdline, int show){
    (void)prev;
    /* parse command line: --screenshot <id> <out.png> */
    int argc = 0;
    LPWSTR *wargv = CommandLineToArgvW(cmdline, &argc);
    char *shot_out = NULL;
    const char *shot_id = NULL;
    for(int i = 0; i < argc - 2; i++){
        if(wcscmp(wargv[i], L"--screenshot") == 0){
            char *id = wu_w_to_u8(wargv[i+1]);
            if(id){ shot_id = id; }
            char *out = wu_w_to_u8(wargv[i+2]);
            if(out){ shot_out = out; }
        }
    }

    App app;
    g_app = &app;
    app_init(&app);
    app.hinst = hinst;
    app.shot_id = shot_id;
    app.shot_out = shot_out;

    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = wndproc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"C17MarkdownEditor";
    wc.hbrBackground = NULL;
    RegisterClassW(&wc);

    RECT rc = {0, 0, app.width, app.height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    app.hwnd = CreateWindowExW(0, L"C17MarkdownEditor", L"C17 Markdown Editor",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hinst, NULL);

    render_init(&app);
    render_resize(&app, app.width, app.height);

    /* screenshot automation: set up state */
    if(shot_id){
        app_setup_screenshot(&app, shot_id);
    }

    ShowWindow(app.hwnd, show);
    UpdateWindow(app.hwnd);

    /* render loop */
    MSG msg;
    while(app.running && GetMessageW(&msg, NULL, 0, 0) > 0){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if(app.running && msg.message == WM_PAINT){
            /* single paint per frame handled in wndproc */
        }
    }
    if(wargv) LocalFree(wargv);
    return 0;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
    App *a = g_app;
    switch(msg){
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            app_render(a);
            BitBlt(hdc, 0, 0, a->fb_w, a->fb_h, a->memdc, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            /* screenshot */
            if(a->shot_id){
                app_capture_shot(a);
            }
            return 0;
        }
        case WM_SIZE:
            render_resize(a, LOWORD(lp), HIWORD(lp));
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_LBUTTONDOWN: handle_mouse(a, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1, true, false); return 0;
        case WM_LBUTTONUP: handle_mouse(a, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1, false, false); return 0;
        case WM_MOUSEMOVE: handle_mouse(a, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0, false, true); return 0;
        case WM_MOUSEWHEEL: {
            DocTab *t = app_active(a);
            if(t){
                int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
                if(GetKeyState(VK_CONTROL) & 0x8000){
                    t->zoom += delta * 0.1;
                    if(t->zoom < 0.5) t->zoom = 0.5;
                    if(t->zoom > 3.0) t->zoom = 3.0;
                } else {
                    t->scroll_y -= delta * 40;
                    if(t->scroll_y < 0) t->scroll_y = 0;
                    a->nav_scroll = t->scroll_y > 0 ? 1.0 : 0.0;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN: {
            DocTab *t = app_active(a);
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            switch(wp){
                case VK_LEFT: if(t){ if(shift){ if(!t->has_sel){ t->sel_start=t->sel_end=t->caret; t->has_sel=true; } caret_left(a,t); if(t->sel_start > t->caret) t->sel_start = t->caret; } else caret_left(a,t); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case VK_RIGHT: if(t){ if(shift){ if(!t->has_sel){ t->sel_start=t->sel_end=t->caret; t->has_sel=true; } caret_right(a,t); if(t->sel_end < t->caret) t->sel_end = t->caret; } else caret_right(a,t); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case VK_BACK: if(t){ backspace(a,t); InvalidateRect(hwnd,NULL,FALSE); } return 0;
                case VK_DELETE: if(t){ delete_fwd(a,t); InvalidateRect(hwnd,NULL,FALSE); } return 0;
                case VK_HOME: if(t){ t->caret = 0; if(shift){ t->has_sel=true; t->sel_start=0; t->sel_end=t->caret;} else t->has_sel=false; InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case VK_END: if(t){ t->caret = md_document_len(&t->doc); if(shift){ t->has_sel=true; t->sel_end=t->caret;} else t->has_sel=false; InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'Z': if(ctrl && !shift && t){ md_document_undo(&t->doc); t->parsed_dirty=true; InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'Y': if(ctrl && t){ md_document_redo(&t->doc); t->parsed_dirty=true; InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'S': if(ctrl && t){ app_save(a, shift); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'O': if(ctrl){ app_do_command(a, "open"); } return 0;
                case 'N': if(ctrl){ app_do_command(a, "new"); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'A': if(ctrl && t){ select_all(a,t); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'C': if(ctrl && t){ copy_selection(a,t);} return 0;
                case 'X': if(ctrl && t){ cut_selection(a,t); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'V': if(ctrl && t){ paste_clipboard(a,t); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'F': if(ctrl){ app_do_command(a, shift ? "replace" : "find"); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'B': if(ctrl && t){ app_apply_fmt(a, 1); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'I': if(ctrl && t){ app_apply_fmt(a, 2); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'K': if(ctrl && t){ app_apply_fmt(a, 0); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case 'W': if(ctrl){ app_close_tab(a, a->active); InvalidateRect(hwnd,NULL,FALSE);} return 0;
                case VK_TAB: if(ctrl){ a->active = (a->active + (shift ? -1 : 1)); if(a->active < 0) a->active = (int)a->ntabs - 1; if(a->active >= (int)a->ntabs) a->active = 0; InvalidateRect(hwnd,NULL,FALSE); } return 0;
                case VK_ESCAPE: if(a->modal){ a->modal = 0; InvalidateRect(hwnd,NULL,FALSE); } else if(a->find_open){ a->find_open = false; InvalidateRect(hwnd,NULL,FALSE); } return 0;
                case VK_F3: app_find_next(a, shift ? -1 : 1); InvalidateRect(hwnd,NULL,FALSE); return 0;
                case VK_RETURN: if(t && a->find_open){ app_replace_one(a); InvalidateRect(hwnd,NULL,FALSE);} return 0;
            }
            return 0;
        }
        case WM_CHAR: {
            DocTab *t = app_active(a);
            if(t && !a->modal && wp >= 0x20 && wp != 0x7F){
                if(a->find_open){
                    /* type into find query */
                    if(wp == '\r') return 0;
                    size_t ql = strlen(a->find_query);
                    if(ql < sizeof(a->find_query)-1){
                        a->find_query[ql] = (char)wp; a->find_query[ql+1] = 0;
                    }
                    app_find_next(a, 1);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                wchar_t wc = (wchar_t)wp;
                uint8_t enc[4];
                int n = ce_utf8_encode((uint32_t)wc, enc);
                insert_text_at(a, t, (const char*)enc, (size_t)n);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_DROPFILES: {
            HDROP hdrop = (HDROP)wp;
            UINT n = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
            for(UINT i = 0; i < n; i++){
                wchar_t path[MAX_PATH];
                DragQueryFileW(hdrop, i, path, MAX_PATH);
                char *u8 = wu_w_to_u8(path);
                if(u8){
                    if(ce_ends_with(u8, ".md") || ce_ends_with(u8, ".markdown") || ce_ends_with(u8, ".txt")) app_open_file(a, path);
                    else if(ce_ends_with(u8, ".png") || ce_ends_with(u8, ".jpg") || ce_ends_with(u8, ".jpeg") || ce_ends_with(u8, ".bmp")) app_image_insert(a, path);
                    ce_free(u8);
                }
            }
            DragFinish(hdrop);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_CLOSE:
            a->running = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ---------------- mouse handling ---------------- */

static void handle_mouse(App *a, int x, int y, int button, bool down, bool move){
    (void)move;
    Theme *th = a->theme;
    /* modal consumes input */
    if(a->modal){
        if(down && button == 1){
            /* click OK/Cancel in modal */
            a->modal = 0;
        }
        return;
    }
    if(down && button == 1){
        /* nav buttons */
        int nav_h = 44, tab_h = 30, status_h = 22;
        if(y < nav_h){
            if(x >= 160 && x < 220) app_do_command(a, "new");
            else if(x >= 226 && x < 286) app_do_command(a, "open");
            else if(x >= 292 && x < 348) app_do_command(a, "save");
            else if(x >= a->fb_w - 92) app_do_command(a, "theme");
            else {
                /* mode capsule */
                int cw = 280, cx = a->fb_w/2 - cw/2;
                if(x >= cx && x < cx + cw){
                    int m = (x - cx) / (cw/4);
                    app_set_mode(a, m);
                    a->capsule_anim = (double)m;
                }
            }
            InvalidateRect(a->hwnd, NULL, FALSE);
            return;
        }
        if(y < nav_h + tab_h){
            /* tab strip */
            int tx = (a->sidebar_visible && a->workspace_root) ? a->sidebar_width : 0;
            int idx = -1;
            int txx = tx;
            for(size_t i = 0; i < a->ntabs; i++){
                int w = 140;
                if(x >= txx && x < txx + w){ idx = (int)i; break; }
                txx += w;
            }
            if(idx >= 0){
                /* close button area */
                if(x >= txx + 124){ app_close_tab(a, idx); }
                else app_switch_tab(a, idx);
            }
            InvalidateRect(a->hwnd, NULL, FALSE);
            return;
        }
        /* sidebar */
        int sw = (a->sidebar_visible && a->workspace_root) ? a->sidebar_width : 0;
        if(x < sw){
            /* tree click: open file / toggle dir */
            int top = nav_h + tab_h + 28;
            int idx = (y - top) / (a->prefs.font_size + 4);
            (void)idx;
            /* simple: expand/collapse handled in tree_draw; open file not wired here */
            return;
        }
        /* editor area */
        DocTab *t = app_active(a);
        if(t && y >= nav_h + tab_h && y < a->fb_h - status_h){
            if(t->mode == MODE_SOURCE || t->mode == MODE_SPLIT){
                size_t pos = 0;
                app_source_hit(a, t, x - sw, y - nav_h - tab_h, &pos);
                t->caret = pos;
                t->has_sel = false;
                t->sel_start = t->sel_end = pos;
            } else {
                /* rendered: approximate */
            }
            InvalidateRect(a->hwnd, NULL, FALSE);
        }
        (void)th;
    }
}

/* ---------------- start surface interactions (delegated) ---------------- */

void app_capture_shot(App *a);

/* put_px_border is defined in view.c; here we only reference it. */
