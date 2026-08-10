# 範圍與技術邊界

## 1. 目標平台

- 作業系統：64-bit Windows 10 version 22H2 或 Windows 11。
- 應用程式型態：傳統 Unicode Win32 desktop application。
- 視窗 API：User32。
- 最終像素呈現 API：GDI，僅限 `StretchDIBits`、`SetDIBitsToDevice` 或語意等價的 framebuffer blit。
- 程式語言：ISO C17。
- 目標架構：x86-64。

不得以 UWP、WinUI、WPF、Windows Forms、MFC、ATL、WebView、Electron 或瀏覽器應用代替傳統 Win32 executable。

本文件描述產品與工程邊界，不規定實作者如何取得編譯器、Windows SDK、如何安排工作流程或使用何種 agent／工具。

## 2. 允許使用的系統邊界

### 2.1 語言與 runtime

- ISO C17 standard library。
- compiler／CRT 所需的標準 runtime support。
- C math functions；不要求額外連結獨立 C math runtime。

### 2.2 Win32 base API

允許直接呼叫：

- Kernel32：檔案、目錄、process、thread、synchronization、environment、high-resolution timing、wall clock、memory allocation 與 error retrieval。
- User32：註冊 window class、建立一個主要 top-level window、message loop、鍵盤／滑鼠／focus／DPI／resize／close event、pointer capture、cursor 與 `SendInput` 測試驅動。
- GDI：建立或描述 DIB、取得 paint DC，以及將完成 framebuffer blit 至 client area。
- Bcrypt：只允許 `BCryptGenRandom(..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)` 作為 OS CSPRNG。

可使用 Win32 threads，但所有 HWND、message、paint 與 User32/GDI 操作必須由 UI thread 擁有。Worker thread 只可處理純計算或具明確所有權的 I/O，並透過明確訊息／同步機制回報結果。

## 3. 明確禁止的高階替代實作

- Common Controls、CreateWindowEx 建立的 `BUTTON`、`EDIT`、`LISTVIEW`、`TABCONTROL`、`PROGRESS_CLASS` 或其他 child widget。
- Common Dialogs、TaskDialog、MessageBox 作為產品 modal；致命啟動錯誤可輸出 stderr，不得用它替代正常 UI 流程。
- Direct2D、DirectWrite、GDI+、WIC、DirectComposition、Skia、Cairo、OpenGL、Vulkan、ANGLE。
- DWM blur、Mica、Acrylic、`DwmEnableBlurBehindWindow` 或 undocumented composition API 代替自製背景模糊。
- GDI 的 `TextOut`、`DrawText`、`ExtTextOut`、`RoundRect`、`GradientFill`、`AlphaBlend` 或其他 primitive 代替自製 renderer。
- GTK、Qt、SDL、SFML、GLFW、Dear ImGui、MFC、ATL、WinUI、WPF、Windows Forms。
- Electron、WebView2、HTML/CSS/JavaScript UI。
- SQLite 或其他資料庫引擎。
- OpenSSL、libsodium、CNG hash/cipher/KDF、CryptoAPI 或其他密碼學套件；Bcrypt 只可取亂數。
- zlib、LZ4、zstd 或其他壓縮套件。
- Git、libgit2 或其他版本控制核心作為 `tinyvcs` 後端。
- 現成數獨題庫、solver、generator、difficulty classifier 或 Hint engine。

## 4. Win32 視窗使用界線

Win32 是 OS boundary，不是 UI toolkit。

允許：

- `RegisterClassExW`、`CreateWindowExW` 建立一個 resizable top-level window。
- 使用標準 non-client frame、title bar、minimize/maximize/close buttons。
- 處理 `WM_PAINT`、`WM_SIZE`、`WM_DPICHANGED`、`WM_GETMINMAXINFO`、`WM_CLOSE`、`WM_DESTROY`、focus、mouse、wheel 與 keyboard messages。
- 使用 `SetCapture`／`ReleaseCapture` 實作正確 pressed interaction。
- 使用 `TrackMouseEvent` 或等效邏輯取得 hover leave。
- 使用標準 system cursor。

不得：

- 建立產品用 child controls、native menu、native dialog 或 native text input。
- 將 Play、Library、Settings 拆成多個 HWND 以規避自製 layout、z-order、focus 與 hit-testing。
- 直接以 `PostMessage`／`SendMessage` 呼叫產品 command 來冒充使用者 E2E。
- 自訂 non-client title bar 並非必要；若自行實作，不得因此降低任何 client UI requirement。

## 5. Software renderer 邊界

應用程式必須維護 application-owned、top-down、32-bit BGRA framebuffer。Framebuffer 中的 alpha 僅供應用程式內部 composite；送到 opaque HWND 前必須得到完整最終畫面。

至少自行實作：

- filled/stroked rectangle、rounded rectangle、circle、line。
- clipping 與 nested clip intersection。
- alpha blending、gradient、mask composite。
- glyph mask renderer 與文字 layout。
- shadow、glow、separable blur。
- layout、hit-testing、focus、modal capture。
- animation timeline、cubic Bézier evaluator。

Canonical presentation：在 `WM_PAINT` 或等效 invalidation path 中，使用 `BeginPaint`／`EndPaint`，以 `StretchDIBits` 或 `SetDIBitsToDevice` 顯示完成的 framebuffer。GDI 不得參與正式 UI primitive、文字、alpha、blur、shadow 或 widget rendering。

## 6. Unicode、文字與路徑

- 所有 Win32 window、filesystem、environment 與 command-line operation 必須使用 wide-character `*W` API 或語意等價的 Unicode entry point。
- 不得使用 ANSI `*A` API 處理路徑或使用者輸入。
- 內部持久化文字與 canonical report 使用 UTF-8。
- UTF-16↔UTF-8 轉換必須拒絕 invalid surrogate／invalid UTF-8，不得 silent replacement。
- UI 正式標籤固定英文；password 僅支援規格定義的 printable ASCII，因此不要求 IME 或完整 shaping。
- UI glyph 必須來自交付的自有 bitmap／mask atlas；不得使用 GDI、DirectWrite 或系統 font rasterization。

## 7. DPI 與尺寸

- Process 必須在建立任何 HWND 前啟用 Per-Monitor DPI Awareness V2。
- Layout token 使用 device-independent pixel（DIP）；96 DPI 時 1 DIP = 1 physical pixel。
- Framebuffer 使用實際 client pixel size。
- 必須處理 `WM_DPICHANGED`，採用 suggested window rectangle、重建 buffer 並重新 layout。
- Golden scenes 固定 96 DPI；另必須驗證 125%、150%、200% 不裁切必要控制項。
- 最小 client area 以 DIP 表示，需透過 `WM_GETMINMAXINFO` 與當前 DPI 正確換算 non-client frame。

精確契約見 `26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`。

## 8. 計時、亂數與背景工作

- animation/game active time 使用 `QueryPerformanceCounter` 與固定 frequency；不得使用 wall clock 計算 elapsed。
- persisted timestamps 使用 UTC wall clock，轉為 Unix epoch milliseconds。
- salt、nonce、game ID 使用 `BCryptGenRandom` system-preferred RNG。
- 不得使用 `rand()`、timestamp、GUID v1、process ID 或 counter 產生密碼學值。
- 單次可感知操作可能超過 100 ms 時，必須顯示 busy state，並以 worker 或 event-loop slicing 保持 window message pump 可回應。

## 9. Filesystem 與資料安全

- 使用 Unicode Win32 file APIs。
- 正式路徑必須阻擋 `..` traversal、device namespace、alternate data stream 與 reparse-point escape。
- 內部 reparse point／junction／symbolic link 不得被 traversal 或 tracked。
- Atomic replacement 與 backup 必須遵循 `08`、`19`、`26` 的 `CreateFileW`／`FlushFileBuffers`／`ReplaceFileW`／`MoveFileExW` 契約。
- 不得執行或載入 repository／vault 中的任意程式碼。

## 10. 網路與外部服務

- 應用程式不得依賴網路。
- 不得連結 Winsock 來提供產品功能。
- 題目、設定、存檔與紀錄均為本機資料。
- 不得使用雲端 API 產生、分類、提示或求解 Sudoku。
- 不得回傳 telemetry。

## 11. 建置交付界面

必須提供 repository-root `build.cmd`，支援 `11_DELIVERABLES_AND_DOD.md` 固定的 subcommands。Script 不得下載依賴。

可另提供 NMake Makefile、CMake 或 IDE project，但它們不能取代 canonical `build.cmd`，也不得成為唯一可重現入口。

## 12. Link-time allowlist

正式 Sudoku executable 只可直接依賴：

- Windows system loader 與 compiler／CRT runtime。
- `kernel32.dll`。
- `user32.dll`。
- `gdi32.dll`。
- `bcrypt.dll`，且 import audit 只允許 RNG 路徑。

Workstream A CLI 原則上只可依賴 compiler／CRT runtime 與 `kernel32.dll`。若測試 runner 驅動 Win32 UI，可額外使用 `user32.dll`／`gdi32.dll`。

任何額外 imported DLL/API 必須在 dependency audit 中逐項列出；若提供被禁止的高階能力，即使未明顯使用也視為 FAIL。

## 13. Assets

允許：

- 自有 glyph atlas。
- 自行定義的小型 icon vector/path 或 point data。
- test fixtures 與 reference geometry。
- Windows application manifest、`.rc` 與 icon resource。

禁止：

- 預先渲染整頁或所有互動狀態。
- 內嵌大量 Sudoku 題目／答案取代 generator。
- 內嵌第三方 DLL、static library 或 opaque object file。

## 14. Test hooks 邊界

可提供 fixed seed、virtual clock、isolated data directory、semantic UI probe、failure injection 與 deterministic DPI fixture，但 production 預設必須關閉。Test hook 不得：

- 跳過 vault authentication tag。
- 直接寫入偽造 Completed record。
- 讓 generator 接受非唯一或錯誤難度題目。
- 讓 E2E 直接呼叫內部產品 command 而未經真實 Win32 input path。
- 改用 GDI／native widget 繪出僅供截圖的假 UI。

## 15. 可攜性

本 fork 只要求 Windows 10 22H2／Windows 11 x64 Win32。
不要求 Linux、X11、Wayland、macOS、ARM64 或 Windows Store packaging。
不得因自行增加跨平台 abstraction 而降低 Windows 原生必要功能的完成度。
