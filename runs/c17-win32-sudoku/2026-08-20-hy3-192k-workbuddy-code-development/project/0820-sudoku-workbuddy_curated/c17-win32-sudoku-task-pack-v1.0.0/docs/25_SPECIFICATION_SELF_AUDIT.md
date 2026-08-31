# v1.0 Windows Fork Specification Self-Audit

日期：2026-08-06

## 1. Audit purpose

本文件記錄Windows原生fork正式發放前的規格稽核。它不替代實作者release gate，也不表示未來實作已完成。

## 2. Fork baseline

本fork以C17/X11 v1.0.0的產品、Sudoku演算法、vault、locstat、tinyvcs、測試與DoD為基線，只替換平台邊界並補齊Windows特有契約。

保留：

- Workstream A1 `locstat`。
- Workstream A2 `tinyvcs`。
- 共用C test harness。
- Play／Library／Settings數獨應用。
- Easy／Medium／Hard與T1–T8 Hint。
- 自製renderer、動畫、blur、frosted nav。
- vault、crypto、failure recovery、全部驗收與證據。

平台替換：

- Linux/X11→Windows 10 22H2／Windows 11 x64 Win32。
- Xlib presentation→User32 HWND/input + GDI framebuffer blit。
- POSIX path/rename/random/time→Unicode Win32 path、ReplaceFile/MoveFile、BCrypt RNG、QueryPerformanceCounter。
- executable bit→Windows read-only file flag。

結果：PASS。

## 3. Agent／環境中立性

文件未指定model、agent framework、MCP、提示策略、開發流程、套件安裝命令、CI平台、token或耗時紀錄方法。

Windows版本、API allowlist、PE architecture、DPI與build entry屬產品工程契約，不是agent環境指示。

結果：PASS。

## 4. Windows dependency boundary

正式允許：

- ISO C17與compiler/CRT runtime。
- Kernel32。
- User32。
- GDI final framebuffer blit。
- Bcrypt `BCryptGenRandom` only。

禁止：native child controls、MFC/ATL、WinUI/WPF/Forms、Direct2D/DirectWrite/GDI+、DWM blur、OpenGL、WebView、database、crypto/compression/VCS/Sudoku套件。

結果：PASS。

## 5. Windows platform closure

`26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`已固定：

- OS版本、x64 PE與manifest。
- HWND/message lifetime。
- frame scheduling與WM_PAINT。
- BGRA DIB presentation。
- Per-Monitor V2與DIP rounding。
- pointer capture、wheel、keyboard、focus與Alt+F4。
- Unicode UTF-16↔UTF-8。
- case identity、reparse point、ADS、reserved path。
- read-only metadata。
- CreateFile/Flush/MoveFile/ReplaceFile/rollback。
- lock、RNG、default data directory與真實E2E boundary。

結果：PASS。

## 6. Product、algorithm與format closure

原v1.0產品、狀態、Sudoku、difficulty、Hint、vault與測試要求均保留。Windows fork另將tinyvcs tree/index的Linux executable bit替換為Windows read-only flag，並固定case-insensitive canonical ordering。

結果：PASS。

## 7. Build與deliverable closure

- Canonical entry改為`build.cmd` subcommands。
- Executable名稱固定`.exe`。
- Application manifest與resource可交付。
- PE/import/DPI/Windows scenario evidence納入G15。

結果：PASS。

## 8. Requirement closure

原125個requirement ID保留；新增WIN-01至WIN-15。

Windows ID涵蓋：PE、HWND、framebuffer、DPI、message pump、capture/focus、close、Unicode、case、reparse/ADS、read-only、atomic replace、RNG、data directory與platform audit。

結果：PASS。

## 9. Known intentional constraints

- UI固定英文；規格文件繁體中文。
- Password只支援printable ASCII 8–64 bytes。
- 不要求IME composition、完整Unicode shaping、native accessibility、touch或custom title bar。
- 不要求GPU、DWM效果、Windows Store packaging或ARM64。
- Golden screenshot固定96 DPI，但其他指定DPI功能必須可用。
- GDI只作最終blit。

上述皆為範圍凍結，不是未完成。

## 10. Automated document checks

正式封裝必須檢查：

- 所有Markdown為UTF-8且以newline結尾。
- code fences成對。
- `.md`引用存在。
- requirement ID無重複。
- 不殘留Linux/X11/Xlib/POSIX實作要求；Changelog中的歷史基線敘述除外。
- `executable bit`已完整替換為Windows read-only flag。
- G0–G15、WIN-E2E-01–07與WIN-01–15一致。
- manifest行數與實際一致。
- ZIP完整性與內容hash一致。

最終數量與檢查結果由`DOCUMENT_MANIFEST.md`與交付訊息回報。
