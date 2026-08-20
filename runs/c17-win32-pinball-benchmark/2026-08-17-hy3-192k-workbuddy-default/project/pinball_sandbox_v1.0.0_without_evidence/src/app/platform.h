/* platform.h — Win32 platform-layer glue (User32 / GDI32 / Imm32).
 * Real, used: IME (Imm32) association on the editor window so that IME
 * composition (e.g. CJK input) routes to our window. GDI32 is used by the
 * framebuffer presenter in editor.c.
 */
#ifndef PB_PLATFORM_H
#define PB_PLATFORM_H
#include <windows.h>

/* Initialize any process-wide IME state (currently a no-op placeholder that
 * keeps the Imm32 dependency live and documents intent). */
void platform_init_ime(void);

/* Associate/dissociate the IME input context with the given window. */
void platform_enable_ime(HWND hwnd, int enable);

#endif /* PB_PLATFORM_H */
