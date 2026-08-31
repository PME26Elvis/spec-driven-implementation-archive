/* sdk_platform.c - Windows native host (docs/26).
 * Single HWND, DPI Per-Monitor V2, framebuffer blit via StretchDIBits only. */
#include "platform/sdk_platform.h"
#include "app/sdk_app.h"
#include "ui/sdk_ui.h"

#include <windows.h>
#include <stdio.h>

/* module state (single window is intentional per docs/26) */
static sdk_app   *g_app = NULL;
static sdk_fb    *g_fb  = NULL;
static wchar_t    g_class[64];
static int        g_dpi  = 96;

static int _dpi_scale(int v) { return (int)((long long)v * g_dpi / 96); }

static void _ensure_fb(int w, int h) {
    if (g_fb && g_fb->width == w && g_fb->height == h) return;
    if (!g_fb) g_fb = sdk_fb_create(w, h);
    else sdk_fb_resize(g_fb, w, h);
}

static void _blit(HWND hwnd) {
    if (!g_fb || !g_app) return;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (hdc) {
        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof bi);
        bi.biSize = sizeof bi;
        bi.biWidth = g_fb->width;
        bi.biHeight = -g_fb->height;     /* top-down */
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        StretchDIBits(hdc, 0, 0, g_fb->width, g_fb->height,
                      0, 0, g_fb->width, g_fb->height,
                      g_fb->pixels, (BITMAPINFO *)&bi, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(hwnd, &ps);
    }
}

static LRESULT CALLBACK _wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_DPICHANGED:
        g_dpi = (int)LOWORD(wp);
        {
            RECT *r = (RECT *)lp;
            SetWindowPos(hwnd, NULL, r->left, r->top,
                         r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_SIZE: {
        int w = (int)(short)LOWORD(lp);
        int h = (int)(short)HIWORD(lp);
        if (w > 0 && h > 0) {
            _ensure_fb(w, h);
            sdk_app_set_size(g_app, w, h);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_PAINT:
        if (g_fb && g_app) {
            sdk_app_render(g_app, g_fb);
        }
        _blit(hwnd);
        return 0;
    case WM_TIMER:
        if (g_app) sdk_app_on_timer(g_app, sdk_monotonic_ms());
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        if (g_app) sdk_app_on_mouse_down(g_app, (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        if (g_app) sdk_app_on_mouse_up(g_app, (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (g_app) sdk_app_on_mouse_move(g_app, (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_KEYDOWN:
        if (g_app) sdk_app_on_key(g_app, (unsigned int)wp, 0);
        return 0;
    case WM_CHAR:
        if (g_app) sdk_app_on_key(g_app, 0, (int)wp);
        return 0;
    case WM_CLOSE:
        if (g_app) sdk_app_quit(g_app);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

int sdk_platform_run(sdk_app *app, const wchar_t *title) {
    g_app = app;
    g_dpi = 96;
    /* Per-Monitor V2 DPI awareness (docs/26) */
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    _snwprintf(g_class, 64, L"SDKWin32_%p", (void *)app);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = _wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = g_class;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int w = _dpi_scale(app->width), h = _dpi_scale(app->height);
    HWND hwnd = CreateWindowExW(0, g_class, title ? title : L"Sudoku",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                                NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) return -1;

    _ensure_fb(app->width, app->height);
    SetTimer(hwnd, 1, 16, NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
        if (g_app && sdk_app_wants_quit(g_app)) { PostQuitMessage(0); break; }
    }
    if (g_fb) { sdk_fb_destroy(g_fb); g_fb = NULL; }
    return 0;
}
