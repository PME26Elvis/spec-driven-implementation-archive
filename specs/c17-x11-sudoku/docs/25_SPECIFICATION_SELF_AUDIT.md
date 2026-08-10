# v1.0 Specification Self-Audit

日期：2026-08-06

## 1. Audit purpose

本文件記錄任務包自身的正式發放前檢查。它不替代實作者的 release gate，也不表示任何未來實作已完成。

## 2. Scope audit

v1.0 必要範圍僅包含：

- Workstream A1：`locstat`。
- Workstream A2：`tinyvcs`。
- 共用 C test harness與驗證輔助工具。
- Workstream B：C17/Linux/X11數獨桌面應用。
- 對上述內容的測試、資料格式、證據、文件與release gate。

沒有加入：

- 網路、帳號、同步、遙測。
- 聲音、多人、排行榜、成就。
- 額外數獨尺寸或變體。
- Windows、macOS、Wayland。
- tinyvcs remote、merge、rebase、stash、tag、完整diff。
- 進階T9+技巧或完整教學課程。

結果：PASS。

## 3. Agent／環境中立性

文件沒有指定：

- model或供應商。
- agent framework。
- MCP或工具調用方式。
- 開發迭代流程。
- 套件安裝命令。
- 特定容器或CI平台。
- token、時間或成本紀錄方法。

文件只定義工程產品、允許OS/API邊界、交付與驗收。

結果：PASS。

## 4. Dependency boundary audit

正式允許範圍已固定：

- C17 standard library/runtime。
- Linux/POSIX基礎檔案、時間、亂數與可選pthread。
- Xlib core window/input/presentation。
- libm。

禁止高階GUI、renderer、database、crypto、compression、VCS與Sudoku套件。

結果：PASS。

## 5. Product behavior closure

已明確定義：

- Play、Library、Settings。
- Easy/Medium/Hard。
- player values、notes、origins、Undo/Redo。
- Clear Answers、Pause、Submit、Hint、Auto Solve。
- timer、dirty、page/focus pause。
- unsaved draft、首次Save、多個In Progress。
- Close/New/Switch Save/Discard/Cancel。
- Completed分類與archive時機。
- Reset Data與Confirm Auto Solve。

沒有仍標示待決的必要產品行為。

結果：PASS。

## 6. Algorithm closure

已固定：

- 0/1/multiple solution搜尋solver。
- uniqueness驗證與generator流程。
- T1–T8 formal conditions。
- deterministic scan/tie-break。
- technique weight、score與difficulty範圍。
- candidate state延續。
- Hint preview/apply/no-op行為。
- stored solution isolation。
- fixed seed與batch欄位。

結果：PASS。

## 7. Format closure

`19_CANONICAL_FORMATS_AND_LIMITS.md`已固定：

- common limits與CLI exit codes。
- locstat traversal/config/JSON。
- ignore semantics。
- tinyvcs repository、object、index、ref、LZSS。
- vault outer header、AAD、payload framing。
- settings/game/history/completed records。
- UI probe與test result JSON。

結果：PASS。

## 8. UI closure

`20_UI_REFERENCE_CONTRACT.md`已固定：

- 1280×800、960×640、1440×900行為。
- spacing/radius/type tokens。
- Dark/Light palette與contrast。
- hover/ripple/glow/capsule。
- modal scale/opacity/blur。
- dynamic frosted nav mapping。
- Sudoku/notes/keypad geometry。
- focus、scroll、toast、busy與semantic IDs。
- visual tolerances。

結果：PASS。

## 9. Verification closure

已定義：

- unit、integration、E2E、failure injection、batch、visual evidence。
- canonical scenario IDs。
- requirement-to-evidence matrix。
- G0–G14 release gates。
- final completion report template。
- manual checklist。

結果：PASS。

## 10. Automated document checks

建立v1.0時執行下列檢查：

- Markdown檔案均可UTF-8讀取。
- 所有文件以newline結尾。
- code fences成對。
- 文件中引用的`.md`檔名均存在。
- Requirement Catalog ID無重複。
- 非Changelog文件無v0.2／candidate殘留標記。
- ZIP建立後需再執行完整性測試。

初步結果：

- 人類閱讀Markdown文件：28份（建立本audit前）；本文件加入後為29份。
- Requirement IDs：125。
- missing file references：0。
- duplicate requirement IDs：0。
- unbalanced code fences：0。
- old-version markers outside changelog：0。

## 11. Known specification constraints

以下是刻意固定的題目限制，不是未完成：

- 密碼只支援可列印ASCII 8–64 bytes，以避免額外Unicode shaping與normalization範圍。
- UI固定英文；規格文件繁體中文。
- Xlib core與software rendering，不要求GPU。
- 不要求完整accessibility API或Unicode font shaping。
- Hard只接受T1–T8可解範圍，超出者由generator拒絕。
- Elimination Hint無對應player note時不建立state change，可能再次出現同一步。
- 無週期autosave；由dirty與明確Save流程管理。

這些限制均已在正式文件與禁止擴張政策中說明。

## 12. Final audit actions

正式封裝流程要求：

1. 更新README與Changelog。
2. 重新計算逐檔與總行數。
3. 產生`DOCUMENT_MANIFEST.md`。
4. 執行所有internal link、ID、fence、version checks。
5. 建立ZIP。
6. 對ZIP執行完整性測試與內容比對。

上述步驟由任務包發布者在封裝時執行；最終結果與SHA-256會在交付訊息中回報。
