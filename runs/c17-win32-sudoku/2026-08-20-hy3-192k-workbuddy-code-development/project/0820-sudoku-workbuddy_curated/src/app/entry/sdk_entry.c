/* sdk_entry.c - application entry point (GUI subsystem, WinMain). */
#include "app/sdk_app.h"
#include "platform/sdk_platform.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int nshow) {
    (void)hInst; (void)hPrev; (void)cmdline; (void)nshow;

    const char *vault = NULL, *pw = NULL;
    int prod = 1;   /* production PBKDF2 iterations by default */
    for (int i = 1; i < __argc; ++i) {
        if (i + 1 < __argc && strcmp(__argv[i], "--vault") == 0) { vault = __argv[++i]; }
        else if (i + 1 < __argc && strcmp(__argv[i], "--pw") == 0) { pw = __argv[++i]; }
        else if (strcmp(__argv[i], "--test-iters") == 0) { prod = 0; }
    }

    sdk_app *app = sdk_app_create(960, 720, vault, pw, prod);
    if (!app) return 1;

    int rc = sdk_platform_run(app, L"Sudoku");
    sdk_app_destroy(app);
    return rc;
}
