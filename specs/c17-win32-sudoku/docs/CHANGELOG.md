# Task Pack Changelog

## v1.0.0-win32 — 2026-08-06

正式Windows原生fork，基於C17/X11 v1.0.0的完整產品與驗收基線。

### 平台替換

- 目標改為Windows 10 22H2／Windows 11 x64 Unicode Win32 desktop application。
- User32僅負責top-level HWND、message與真實input。
- GDI僅負責blit application-owned top-down BGRA framebuffer。
- 禁止native child controls、Direct2D/DirectWrite/GDI+、DWM blur、WinUI/WPF/Forms/MFC與WebView。
- 固定Kernel32/User32/GDI/Bcrypt RNG import boundary。

### Windows工程契約

- 新增`26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`。
- 固定Per-Monitor V2、96/125/150/200% DPI與WM_DPICHANGED。
- 固定WM_PAINT、minimize/restore、capture loss、Alt+F4與WM_CLOSE生命週期。
- 固定Unicode UTF-16 API、UTF-8 canonical paths、case-insensitive identity、case-only rename、reparse point、junction、ADS與reserved paths。
- tinyvcs executable bit替換為Windows read-only flag。
- 固定CreateFileW／FlushFileBuffers／MoveFileExW／ReplaceFileW／rollback契約。
- Bcrypt僅允許`BCryptGenRandom`；所有crypto primitive仍須手刻。
- Production data directory固定`%LOCALAPPDATA%\C17Win32Sudoku\`。

### 建置、測試與驗收

- Canonical build入口改為`build.cmd`。
- 增加WIN-E2E-01至WIN-E2E-07。
- 增加WIN-01至WIN-15 requirement IDs。
- Release Gate擴充為G0–G15，G15為Windows native platform audit。
- 加入PE/import、DPI matrix、Unicode path與atomic replacement evidence。
- 新增根目錄`INITIAL_PROMPT.md`，供不同實作者使用相同啟動指令。

### 保留內容

- locstat、tinyvcs、C test harness。
- Play／Library／Settings、Easy／Medium／Hard、T1–T8 Hint。
- 自製renderer、layout、ripple、glow、modal blur、dynamic frosted nav。
- vault、XChaCha20-Poly1305、PBKDF2-HMAC-SHA-256、recovery。
- 原有canonical formats、scenario、manual checklist與DoD，除必要Windows平台差異外均保留。
