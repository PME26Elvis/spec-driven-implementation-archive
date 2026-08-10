# 規範語意、術語與解讀規則

版本：1.0.0

## 1. 文件角色

本文件固定整份任務包的規範語意。任何實作、測試、缺陷報告與人工驗收均應使用本文件的術語。

本任務包是產品與工程作業規格，不是實作教學。它不指定模型、代理框架、MCP、提示方式、開發環境配置、套件安裝流程或工作排程。

## 2. 規範關鍵字

- **MUST／必須**：缺少、錯誤或無法證明即為 FAIL。
- **MUST NOT／不得**：一旦出現即為 FAIL，除非文件明確提供例外。
- **SHOULD／應**：預期必須達成；只有在交付報告說明等效行為、理由與證據時才能偏離。
- **SHOULD NOT／不應**：原則上禁止；偏離時必須證明不降低安全、正確性、工程量與可驗收性。
- **MAY／可**：選配，不影響最低完成判定。
- **至少**：數量下限，不能以等價敘述降低。
- **恰好**：數量或狀態必須完全相等。
- **固定**：同一規格版本下不得由實作者自行變動。
- **確定性**：相同規格版本、相同輸入與相同 seed 必須得到相同語意輸出；時間戳、隨機 nonce 與檔案位置等明確例外除外。

## 3. 規格來源優先順序

發生矛盾時依下列順序處理：

1. `22_RELEASE_GATE_AND_AUDIT.md` 的正式 release gate。
2. `11_DELIVERABLES_AND_DOD.md` 的 Definition of Done。
3. `12_PROHIBITED_SHORTCUTS.md` 的禁止事項。
4. `19_CANONICAL_FORMATS_AND_LIMITS.md` 的 canonical 格式與數值限制。
5. 各領域文件中的 MUST／MUST NOT。
6. `17_ACCEPTANCE_REQUIREMENT_CATALOG.md` 的 requirement 索引。
7. `13_MANUAL_ACCEPTANCE_CHECKLIST.md` 的人工操作。
8. 說明性文字、範例與建議。

若同一優先層仍矛盾，採取較嚴格、較可驗證且不擴大產品範圍的解讀，並在完成報告列為規格問題。

## 4. 版本與相容性

- 任務包正式版本為 `1.0.0`。
- `1.x` 只允許澄清、錯字修正、矛盾消除與驗收可重現性改善，不新增主要產品功能。
- 新增平台、頁面、數獨變體、網路服務、主要演算法或工具命令需提升 major version。
- 實作者與驗收者必須記錄實際使用的完整任務包版本。
- 不得混用不同版本文件後宣稱符合 v1.0。

## 5. 核心術語

### 5.1 Product

指 Workstream B 的 Linux/X11 數獨桌面應用。

### 5.2 Development Utilities

指 Workstream A 的：

- `locstat`。
- `tinyvcs`。
- 共用 C 測試執行器與必要驗證輔助工具。

### 5.3 Production binary

以正式設定建置、未預設啟用 test hook、未繞過安全或持久化規則的可執行檔。

### 5.4 Test mode

僅為重現 seed、虛擬時間、固定資料目錄、故障注入或 UI probe 而提供的明確模式。Test mode 不得改變數獨規則、難度分類、加密驗證、存檔一致性或完成紀錄語意。

### 5.5 Given／Clue

題目原始不可修改數字。

### 5.6 Player value

由玩家直接輸入的正式數字。

### 5.7 Hint-assisted value

由 `Apply Hint` 放入的正式數字。

### 5.8 Auto-solved value

由 Auto Solve 放入的正式數字。

### 5.9 Formal value

Given、player value、hint-assisted value 或 auto-solved value 中實際佔據格子的正式數字。Notes 不屬於 formal value。

### 5.10 Player notes

玩家自行維護的候選數集合。它不是求解器的 derived candidates，也不參與盤面合法性判斷。

### 5.11 Derived candidates

依目前 formal values 與邏輯求解器內部 elimination state 計算出的演算法候選數。

### 5.12 Conflict

同一列、欄或宮中，同一正式數字出現至少兩次。所有參與重複的格子都屬 conflict。

### 5.13 Unsatisfiable current state

目前 formal values 沒有直接 duplicate conflict，但不存在任何完整合法解。

### 5.14 Dirty

記憶體中的可持久化狀態與最後成功保存 snapshot 不一致。

### 5.15 Transaction

一次使用者意圖造成的所有狀態變更集合。一次 Undo 必須完整逆轉整個 transaction。

### 5.16 Blocking modal

攔截所有底層輸入並使遊戲 timer 暫停的對話框。

### 5.17 Busy operation

可能跨多個 frame 或背景工作完成的 generation、Hint、Solve 或 Save 操作。Busy 不等於程式失去回應。

### 5.18 Evidence

可重現的自動測試、固定輸入輸出、實際 binary 截圖／錄影、故障注入結果、格式驗證或人工 checklist 紀錄。

### 5.19 Placeholder

任何外觀存在但不執行正式行為、固定回傳成功、以假資料替代真資料、以 hard-coded expected output 假裝演算法完成的內容。

## 6. 數量與索引慣例

- Sudoku row、column 對使用者顯示為 1–9。
- 文件中的 `R1C1` 表示第一列第一欄。
- 程式內部可使用 0-based index，但序列化與測試文件必須說明轉換。
- 3×3 box 依左到右、上到下編號 1–9。
- cell linear index 的 canonical 次序為 row-major：R1C1、R1C2……R9C9。
- bit mask 的 digit 1 對應 bit 0，digit 9 對應 bit 8。

## 7. 時間慣例

- 持久化 wall-clock timestamp 使用 Unix epoch milliseconds，signed 64-bit little-endian integer。
- active elapsed 使用 unsigned 64-bit milliseconds。
- UI 顯示時間採四捨五入向下至完整秒。
- animation 使用 monotonic time。
- 測試模式的虛擬時間必須明確與 production wall clock 分離。

## 8. 字串與編碼

- 正式 UI 文案固定英文。
- 規格文件為 UTF-8 繁體中文。
- 檔案路徑在 Workstream A 內視為原始 byte sequence；可顯示時以 UTF-8 解讀，無效序列需以安全 escaping 顯示。
- Commit author、message 與 branch name 限制見 `19_CANONICAL_FORMATS_AND_LIMITS.md`。
- Vault 密碼 v1.0 限制為可列印 ASCII 32–126，長度 8–64 bytes；不得自動 trim 內部空白。

## 9. 錯誤與狀態不變原則

除文件明確指定部分成功外，任何失敗操作都必須遵守：

- 不建立半成品正式紀錄。
- 不把 dirty 誤標為 clean。
- 不移動 branch ref。
- 不覆蓋最後 known-good vault。
- 不建立空 Undo transaction。
- 不增加成功統計。
- 不讓 stale result 修改新狀態。

## 10. 等效實作判定

實作者可自行設計模組、資料結構與內部函式，但等效實作必須同時滿足：

1. 所有可觀察行為相同。
2. 沒有使用禁止依賴或高階替代品。
3. 沒有移除本題刻意要求的底層工程量。
4. 所有 canonical 格式、排序、限制與錯誤契約相容。
5. 既有驗收案例無需特殊放寬即可通過。

## 11. 非規範內容

文件中的「例如」「建議」「可採」若未伴隨 MUST，不是唯一實作方式。範例不能推翻同章的明確規則。

## 12. 規格疑義處理

實作者遇到未定義行為時：

- 優先採安全、保守、可恢復、確定性且不擴大範圍的行為。
- 在實作文件記錄該決策。
- 為該決策加入測試。
- 不得以疑義為由省略整個功能。
- 不得自行引入外部服務或高階函式庫作為解法。
