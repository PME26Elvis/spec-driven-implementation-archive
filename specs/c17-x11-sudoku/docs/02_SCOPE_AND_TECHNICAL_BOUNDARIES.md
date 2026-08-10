# 範圍與技術邊界

## 1. 目標平台

- 作業系統：Linux。
- 視窗系統：X11。
- GUI API：Xlib／libX11。
- 程式語言：C17。
- 主要應用型態：本機桌面應用。

本文件描述的是產品與工程邊界，不規定實作者如何取得編譯器、如何啟動顯示環境或如何安排開發流程。

## 2. 允許使用

### 2.1 語言與基礎函式

- ISO C17 標準函式庫。
- Xlib，用於：
  - 建立與關閉視窗。
  - 接收鍵盤、滑鼠、焦點、視窗尺寸與關閉事件。
  - 建立呈現目標。
  - 將完成的畫面呈現到 X11 視窗。
- Linux `getrandom()` 或等效核心安全亂數介面，僅限密碼學 salt、nonce 與識別碼。
- Linux/POSIX 基礎介面，僅限檔案、目錄、權限、原子 rename、flush/sync、monotonic/wall clock、process exit、環境變數與安全亂數。
- POSIX threads 可選用於背景工作；若使用，UI/Xlib 呼叫必須集中於 UI thread，worker 只處理純計算或明確所有權的 I/O。
- C math library。

### 2.2 建置描述

- 必須提供 Makefile，且有明確的 production、test 與 clean targets；target 名稱見 `11_DELIVERABLES_AND_DOD.md`。
- 可將程式拆分為多個 `.c` 與 `.h`。
- 可提供純資料資產，但必須符合本文件的資產規則。

## 3. 禁止使用

- GTK、Qt、SDL、SFML、GLFW、Cairo、Skia、Dear ImGui。
- OpenGL、Vulkan 或其他 GPU 圖形 API。
- Electron、WebView、瀏覽器 UI 或 HTML/CSS/JavaScript 前端。
- ncurses 或純終端 UI 代替桌面 GUI。
- SQLite 或其他資料庫引擎。
- OpenSSL、libsodium、mbedTLS 或其他密碼學套件。
- zlib、LZ4、zstd 或其他壓縮套件。
- Git、libgit2 或其他版本控制核心作為 `tinyvcs` 的實作後端。
- 現成數獨題庫套件、求解器或產生器。
- 現成 UI 動畫、模糊、陰影或 layout engine。

## 4. Xlib 的使用界線

Xlib 是視窗與輸入／輸出邊界，不是 UI 元件庫。

允許：

- 開視窗與處理事件。
- 取得滑鼠與鍵盤事件。
- 將應用程式產生的像素畫面送至視窗。
- 處理視窗 resize、expose 與 close protocol。

不得：

- 以 Xlib 內建控制項替代自製按鈕、對話框、選單或文字輸入。
- 依賴外部 widget toolkit。
- 將主要 UI 拆成多個作業系統原生 child widgets 來規避自製 layout 與 hit-testing。

## 5. UI renderer 邊界

應用程式必須自行維護軟體像素緩衝區，並至少自行實作：

- 矩形、圓角矩形、圓形與線段。
- Alpha blending。
- 裁切區域。
- 基本文字 glyph rasterization。
- 陰影與光暈。
- 背景模糊。
- 元件 layout 與 hit-testing。
- 動畫時間線與 easing。

Xlib 最終呈現之前的畫面，必須可被視為應用程式自行算出的完整 frame。

## 6. 字型與文字

- 不得依賴 Pango、FreeType 或其他字型排版／光柵化函式庫。
- 必須提供應用程式所需 glyph 的自有 rasterization 路徑。
- 可內嵌自行建立的 bitmap font 或簡化 glyph atlas。
- 必須至少支援應用程式實際使用的 ASCII 字元、數字與符號。
- 不得以圖片取代整個頁面或將按鈕文字烘焙成完整按鈕圖片。
- 字型資產必須在交付內容中，且不得於執行時下載。

## 7. 網路與外部服務

- 應用程式不得依賴網路。
- 題目、設定、存檔與紀錄均為本機資料。
- 不得使用雲端 API 產生或求解數獨。
- 不得回傳遙測資料。

## 8. 單執行緒與多執行緒

- 規格不要求多執行緒。
- 若實作者自行使用執行緒，仍不得引入第三方 threading framework。
- UI 不得在產生題目或存檔時永久凍結。
- 單次可感知操作若可能超過 100 ms，必須顯示進度或 busy state，或將工作切割於 event loop 中執行。

## 9. 資產規則

允許：

- 小型圖示的自行定義向量路徑或點陣資料。
- 字型 glyph atlas。
- 測試 fixture。
- 固定的 UI 比對參考數值。

禁止：

- 預先渲染整頁 UI。
- 預先渲染所有互動狀態並依狀態切圖。
- 內嵌大量數獨題目與答案，取代真正產生器。
- 內嵌第三方二進位函式庫。

## 10. 可攜性

本版本只要求 Linux/X11。
不要求 Windows、macOS 或 Wayland。
不得因自行增加跨平台抽象而降低 Linux/X11 必要功能的完成度。


## 11. v1.0 Link-time allowlist

正式 Sudoku binary 可直接連結：

- C runtime／system libc。
- `libX11`。
- `libm`。
- `libpthread`，只有實際使用背景 worker 時。
- compiler runtime 所需的標準低階支援。

Workstream A CLI 不得依賴 `libX11`。

不得連結未在本節列出的高階 library。若作業系統或 toolchain 自動加入動態 linker、loader 或 compiler runtime，不視為違規，但 dependency audit 必須列出。

## 12. Xlib 呈現界線

Canonical 呈現路徑為 application-owned pixel buffer 經 `XImage`／`XPutImage` 或等效 Xlib core path 顯示。v1.0 不允許 XShm、XRender、Xft 或其他 extension 作為必要 rendering shortcut。

Xlib core text drawing不得用於正式 UI 文字；文字必須經自有 glyph mask renderer進入同一 pixel buffer。

## 13. 檔案與目錄安全

- 所有 user-controlled path 必須正規化並阻擋 `..` traversal。
- 不得寫入應用程式資料目錄、repository root或指定output以外的位置。
- Workstream A 對 symlink 的行為由 `19` 固定。
- 不得執行或載入 repository／vault 中的任意程式碼。

## 14. Test hooks 邊界

可提供 fixed seed、virtual clock、isolated data directory、UI probe與failure injection，但 production預設必須關閉。Test hook不能：

- 跳過 vault tag 驗證。
- 直接寫入偽造 Completed record。
- 讓 generator 接受非唯一或錯誤難度題目。
- 讓 E2E 直接呼叫內部 command 而未操作真實 UI。
