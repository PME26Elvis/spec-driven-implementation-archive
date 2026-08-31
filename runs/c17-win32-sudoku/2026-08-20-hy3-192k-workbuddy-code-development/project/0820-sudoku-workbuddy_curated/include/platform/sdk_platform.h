/* sdk_platform.h - Windows 10/11 native host (docs/26).
 * Single top-level HWND; only Kernel32/User32/GDI/Bcrypt are used.
 * GDI is used ONLY to blit the software framebuffer (StretchDIBits).
 * No native controls, no D2D/DWrite/GDI+. */
#ifndef SDK_PLATFORM_H
#define SDK_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sdk_app;
typedef struct sdk_app sdk_app;

/* Runs the message loop until the app requests quit. Returns 0 on clean exit. */
int sdk_platform_run(sdk_app *app, const wchar_t *title);

#ifdef __cplusplus
}
#endif
#endif /* SDK_PLATFORM_H */
