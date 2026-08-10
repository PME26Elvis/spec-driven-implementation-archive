# 範圍凍結與正式驗收政策

## 1. 狀態

版本 1.0.0 為本任務包的正式 scope freeze。
v1.0 發布後，同一major版本的文件修改只可：

- 消除矛盾。
- 補足既有功能的可判定行為。
- 修正測試或資料格式缺漏。
- 提高驗收可重現性。

不得在同一 major task 中新增主要頁面、平台、網路服務或大型演算法領域。

## 2. 固定工程量原則

比較不同實作者時，必須使用相同版本文件包。
不得針對某一實作者額外放寬依賴、刪除測試或提供私有 fixture。

實作者自行選擇：

- 原始碼模組切分。
- 資料結構細節。
- 測試驅動工具與截圖方法。
- 建置過程與除錯方法。

但不可改變產品行為、禁止事項或證據門檻。

## 3. 驗收優先順序

發生宣告衝突時依序採信：

1. 可重現的執行結果與資料完整性。
2. 自動測試中的明確 assertions。
3. 人工 checklist 的實際操作。
4. source code inspection。
5. requirement-to-test matrix。
6. 實作者文字說明。

文字宣稱不能推翻可重現的失敗。

## 4. 完成度判定

每個 MUST requirement 只可標記：

- PASS：實作存在且證據通過。
- FAIL：缺少、錯誤或證據失敗。
- BLOCKED：交付環境本身無法啟動驗收；仍不等於 PASS。
- NOT APPLICABLE：只有規格明確允許時可用。

不得使用「大致完成」「UI 已有」「核心完成」代替逐項狀態。

## 5. 測試證據可信度

測試結果要被接受，至少滿足：

- 測試 source 一併交付。
- 有可辨識 assertion 與 expected value 來源。
- 執行報告包含案例數與 exit status。
- 結果檔與 source commit 對應。
- 必要測試沒有 skip。
- fixture 不依賴網路。

測試本身若錯誤、空洞或只重複 production 邏輯，可判該 requirement 無證據。

## 6. 視覺證據可信度

- 截圖／錄影來自實際 binary。
- 使用固定 scene 資料與尺寸。
- 截圖證明狀態；錄影或 frame sequence 證明連續動畫。
- 後製標註可另存副本，但原始畫面必須保留。
- 設計稿不能作為完成證據。

## 7. 允許的 known limitations

只能列出不影響 MUST 的限制，例如：

- 不支援規格外平台。
- 不支援規格外 Sudoku technique。
- Hard generation 在較慢 CPU 上可能接近規格 timeout，但仍有 busy/retry。

不得列為 limitation：

- Hint 尚未實作。
- 加密改成明文。
- UI 動畫只做一部分。
- E2E 無法操作真實 UI。
- Hard 實際只是 clue 數標籤。

## 8. 發布候選流程

正式宣告完成前至少執行：

1. clean build。
2. unit tests。
3. integration tests。
4. E2E scenarios。
5. corruption/failure injection。
6. Easy/Medium/Hard batch。
7. `tinyvcs verify`。
8. `locstat` final scan。
9. screenshot/recording scene collection。
10. requirement-to-test completeness check。
11. manual acceptance checklist。

任一步驟失敗都必須修正並重跑受影響範圍。

## 9. 停止與禁止過度開發

當全部 MUST 為 PASS、全部必要測試通過且交付完整時，實作者應停止。
不得以重構、美化或新增功能為由無限延後可驗收版本。

不應加入：

- 規格外依賴。
- 無驗收需求的大型抽象層。
- 額外平台。
- 不使用的 framework imitation。
- 與任務無關的 telemetry、plugin 或 extension system。

## 10. 人工比較建議

本任務包本身不定義學術分數，但人工比較可記錄：

- MUST checklist pass/total。
- 首次交付即可通過的項目比例。
- 測試真實性與缺陷。
- 修正回合數。
- 最終已知 bug 數。
- 完成時間、token 或成本；這些由外部比較者記錄，不寫入實作產品。

上述比較資訊不應改變實作者收到的工程規格。


## 11. v1.0 Finalization status

本版不是candidate。產品、工具、演算法、資料格式、視覺reference、acceptance scenarios與release gates均已凍結。

任何比較不同實作者的活動必須提供完全相同的v1.0.0內容；不得額外口頭允許某一方使用framework、跳過crypto、縮減batch或替換UI。

## 12. Clarification versus scope change

屬於clarification：

- 修正章節連結。
- 補上已明確行為的錯誤碼或排序。
- 消除兩文件矛盾而不增加功能。

屬於scope change：

- 新增command/page/platform/technique。
- 改變canonical binary layout。
- 放寬禁止dependency。
- 降低test/batch/evidence門檻。
- 改變difficulty分類。

Scope change需新任務包版本，不能私下修改後仍稱v1.0.0。

## 13. External comparison metrics

完成時間、token、成本、修正輪次、model/framework名稱與評分表屬外部比較資料，不得寫入或改變產品行為。實作者只需交付工程證據，不需內建benchmark telemetry。
