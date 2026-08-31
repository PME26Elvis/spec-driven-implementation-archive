# C17/Win32 數獨工程任務包

版本：1.0.0（正式範圍凍結版）

## 1. 文件目的

本任務包定義一份可交由不同實作者完成的固定軟體工程作業。它只規定產品範圍、工程限制、可觀察行為、資料格式、錯誤處理、驗收證據、交付物與停止條件。

它不規定模型、代理框架、MCP、提示策略、開發節奏、工具調用方法、環境建置步驟或套件取得方式。

本任務由兩個相依但可分別驗收的工作流構成：

1. Workstream A：自行實作開發工具。
2. Workstream B：自行實作 Windows/Win32 現代化數獨桌面應用。

Workstream A 的工具必須實際用於 Workstream B 的來源、歷史與最終驗證。

## 2. 核心目標

本題不是視覺原型或概念展示，而是完整可建置、可操作、可保存、可復原、可測試、可稽核的軟體。

- 畫面上的必要控制項必須連接真實行為。
- 演算法必須處理正例、反例與損毀輸入。
- 資料保存必須有完整性、加密與故障復原。
- 每項核心要求必須有可重現證據。
- 不得以 placeholder、假資料、硬編碼答案或高階替代函式庫規避工程量。

## 3. 建議閱讀順序

1. `01_ASSIGNMENT_BRIEF.md`
2. `18_NORMATIVE_CONVENTIONS_AND_GLOSSARY.md`
3. `02_SCOPE_AND_TECHNICAL_BOUNDARIES.md`
4. `16_SCOPE_FREEZE_AND_ACCEPTANCE_POLICY.md`
5. Workstream A：`03`、`04`
6. Workstream B：`05`–`09`、`15`、`19`、`20`
7. Windows 平台契約：`26`
8. 驗證：`10`、`13`、`17`、`21`、`23`
9. 交付與停止：`11`、`12`、`22`、`24`

## 4. 文件索引

- `01_ASSIGNMENT_BRIEF.md`：作業總綱與固定工程範圍。
- `02_SCOPE_AND_TECHNICAL_BOUNDARIES.md`：語言、平台、允許 API 與禁止依賴。
- `03_DEV_TOOL_LOCSTAT.md`：行數統計工具。
- `04_DEV_TOOL_TINYVCS.md`：簡化版本控制系統。
- `05_PRODUCT_REQUIREMENTS.md`：數獨應用頁面與功能。
- `06_UI_ENGINE_AND_VISUAL_SPEC.md`：自製 UI 引擎與效果。
- `07_SUDOKU_RULES_AND_ALGORITHMS.md`：規則、產生、求解、難度與 Hint。
- `08_STATE_STORAGE_AND_SECURITY.md`：狀態、vault、加密與 recovery。
- `09_ERRORS_AND_EDGE_CASES.md`：錯誤與邊界案例。
- `10_TESTING_AND_EVIDENCE.md`：測試層級與證據。
- `11_DELIVERABLES_AND_DOD.md`：交付物與 Definition of Done。
- `12_PROHIBITED_SHORTCUTS.md`：禁止替代實作。
- `13_MANUAL_ACCEPTANCE_CHECKLIST.md`：人工驗收清單。
- `14_FINALIZED_PRODUCT_DECISIONS.md`：已定案範圍。
- `15_PRODUCT_STATE_MACHINE.md`：狀態與事件轉移。
- `16_SCOPE_FREEZE_AND_ACCEPTANCE_POLICY.md`：範圍凍結與驗收政策。
- `17_ACCEPTANCE_REQUIREMENT_CATALOG.md`：穩定 requirement ID。
- `18_NORMATIVE_CONVENTIONS_AND_GLOSSARY.md`：規範語意與術語。
- `19_CANONICAL_FORMATS_AND_LIMITS.md`：精確格式、CLI、上限與排序。
- `20_UI_REFERENCE_CONTRACT.md`：固定視覺 token、幾何與動畫契約。
- `21_CANONICAL_ACCEPTANCE_SCENARIOS.md`：固定 E2E／整合場景。
- `22_RELEASE_GATE_AND_AUDIT.md`：正式 release gate。
- `23_REQUIREMENT_TRACEABILITY_TEMPLATE.md`：需求證據矩陣模板。
- `24_FINAL_COMPLETION_REPORT_TEMPLATE.md`：最終完成報告模板。
- `25_SPECIFICATION_SELF_AUDIT.md`：任務包自身的範圍、格式與一致性稽核。
- `26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`：Win32、DPI、Unicode 路徑、檔案原子性與真實輸入契約。
- `CHANGELOG.md`：文件包版本紀錄。
- `DOCUMENT_MANIFEST.md`：所有人類閱讀文件的逐檔行數。
- 根目錄 `INITIAL_PROMPT.md`：跨實作者統一啟動指令。

## 5. 規格優先順序

依 `18_NORMATIVE_CONVENTIONS_AND_GLOSSARY.md`：release gate、DoD、禁止事項、canonical 格式、各領域 MUST、requirement catalog、人工 checklist、說明文字。

## 6. 已固定邊界

- C17、Windows 10 22H2／Windows 11 x64、Unicode Win32、User32/GDI presentation boundary。
- 自製 software renderer、layout、widgets、動畫、glow 與 blur。
- 經典 9×9 Sudoku。
- Easy／Medium／Hard deterministic 邏輯分級。
- T1–T8 可解釋 Hint。
- 玩家 notes、Undo/Redo、Clear Answers、Pause、Timer。
- 多個未完成遊戲、Completed library、加密 vault。
- 自製 `locstat`、`tinyvcs` 與 C test harness。
- Unit、integration、真實 GUI E2E、failure injection、batch 與 visual evidence。

v1.0 已正式凍結。未列入 MUST 的擴充不得取代或延後既有要求。
