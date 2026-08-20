/* platform.c — Win32 IME (Imm32) integration. Real Win32 calls. */
#define WIN32_LEAN_AND_MEAN
#include "platform.h"
#include <imm.h>

void platform_init_ime(void) {
  /* Nothing process-global required; Imm32 is initialized on first use. */
}

void platform_enable_ime(HWND hwnd, int enable) {
  if (!hwnd) return;
  HIMC imc = ImmGetContext(hwnd);
  if (imc) {
    ImmAssociateContext(hwnd, enable ? imc : NULL);
    ImmReleaseContext(hwnd, imc);
  }
}
