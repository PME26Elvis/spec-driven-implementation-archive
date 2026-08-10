# C17/Win32 Sudoku Engineering Task Pack v1.0.0

這是一份正式範圍凍結的 Windows 原生規格驅動軟體實作任務包。

任務包含兩個工作流：自行實作 `locstat`／`tinyvcs`／C 測試工具，以及以 C17 + Unicode Win32 API 完成自製 software UI engine 與現代化 9×9 數獨桌面應用。

Win32 僅作為作業系統邊界：建立頂層視窗、接收真實輸入、取得時間與安全亂數、操作檔案，以及將 application-owned BGRA framebuffer 呈現到 HWND。所有 UI 元件、文字 rasterization、layout、hit-testing、動畫、陰影、光暈、模糊與 frosted glass 效果仍須自行實作。

本包只定義產品、工程、資料格式、測試、證據、交付與停止條件；不定義 model、agent framework、MCP、工具調用或開發流程。

建議將 [`INITIAL_PROMPT.md`](INITIAL_PROMPT.md) 與完整任務包一併交給實作者，並從 [`docs/00_README.md`](docs/00_README.md) 開始閱讀。正式完成必須通過 [`docs/22_RELEASE_GATE_AND_AUDIT.md`](docs/22_RELEASE_GATE_AND_AUDIT.md) 的 G0–G15。

逐檔行數與文件總規模見 [`docs/DOCUMENT_MANIFEST.md`](docs/DOCUMENT_MANIFEST.md)。
