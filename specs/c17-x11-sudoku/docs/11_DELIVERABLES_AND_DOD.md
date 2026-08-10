# 交付內容、Definition of Done 與停止條件

## 1. 必要目錄

v1.0 必須提供下列語意等價的頂層結構；目錄可再細分，但主要類別不得混淆：

```text
/
  README.md
  Makefile
  src/
  include/
  tests/
  dev_tools/
    locstat/
    tinyvcs/
    test_harness/
  assets/
  fixtures/
  docs/
  results/
    locstat/
    screenshots/
    recordings/
  .tinyignore
  locstat.json
```

可細分目錄，但不得把正式原始碼藏在 `results/`、fixture generated output或版本控制物件中。若採不同名稱，README必須提供一對一mapping。

## 2. 必要執行檔

至少產生：

- 數獨桌面應用。
- `locstat`。
- `tinyvcs`。
- unit test runner。
- integration test runner。
- E2E/scenario runner 或等效可重現工具。

## 3. 必要來源內容

- 全部 `.c`／`.h`。
- Makefile。
- 所需字型／圖示資產。
- fixture。
- 測試。
- 文件。
- tinyvcs repository metadata 的可驗證交付副本，若打包格式允許。

不得只交付 binary。

## 4. 必要文件

至少包含：

- Architecture overview。
- Module map。
- Persistent format specification。
- UI engine design。
- Sudoku algorithm notes。
- tinyvcs object format。
- locstat configuration reference。
- Test plan。
- Requirement-to-test matrix，包含全部 catalog ID。
- Known limitations；完成時不得列出違反 MUST 的 limitation。
- Build/run usage。

Build/run usage 可以說明命令，但不得把缺少依賴或未完成流程推給驗收者。

## 5. 必要驗證產物

- 全部測試結果。
- batch generation report。
- corruption/failure report。
- locstat 報告。
- tinyvcs verify 報告。
- 截圖。
- 動畫錄影或連續幀。

## 6. Workstream A Definition of Done

### locstat

- 可解析規格中的 JSON config。
- 可正確分類與排除。
- 可處理 C comments/string edge cases。
- 可輸出 text 與 JSON。
- 測試全部通過。
- 已用於最終專案統計。

### tinyvcs

- init/status/add/commit/log/branch/switch/restore/reset/show/verify 可用。
- 內容定址、SHA-256、LZSS、CRC32 皆為真實實作。
- staging 與 dirty working tree 行為正確。
- 可偵測 object corruption。
- 測試全部通過。
- 已管理 Workstream B。

## 7. Workstream B Definition of Done

### 產品

- 可建立、進行、保存、載入與完成 Easy／Medium／Hard 遊戲。
- 可保存多個未完成遊戲。
- 候選數、Undo/Redo、Clear Answers 可用。
- Submit 按規則驗證，不逐格比 stored solution。
- Hint 使用真實、結構化且可解釋的 T1–T8 邏輯步驟。
- Auto Solve 使用真實 solver。
- 完成紀錄與 timer 正確。
- 關閉保存流程正確。
- vault 加密與 recovery 正確。

### GUI

- 所有主要元件為自製。
- resize 可用。
- hover/ripple/glow/capsule/modal/blur/frosted nav 有真實效果。
- Dark/Light 與 Reduced Motion 可用。
- modal 正確攔截輸入。

### 演算法

- solver 處理 0/1/多解。
- generator 產生唯一解且符合所選 Easy／Medium／Hard 分類的題目。
- logical solver、difficulty trace 與 Hint 不依賴 stored solution 偽造。
- 每難度 50 題、共 150 題 batch 通過。
- conflict mask 標示全部相關格。
- crypto known vectors 通過。

### 測試

- unit、integration、E2E 全部通過。
- 沒有被 skip 的必要測試。
- requirement-to-test matrix 完整。
- failure injection 結果符合預期。

## 8. 品質門檻

- 編譯時不得有未處理的重要 warning。
- 不得含未使用的重大 stub。
- 不得有已知資料遺失路徑。
- 不得在正常操作崩潰。
- 不得將密碼或 plaintext 寫入 log。
- 不得把測試模式默認開啟。
- 不得需要網路才能使用。

## 9. 停止條件

只有以下全部成立才可宣告完成：

- 所有 MUST requirements 已實作。
- 所有人工 checklist 項目可執行。
- 所有必要測試已實際通過。
- `tinyvcs verify` 通過。
- `locstat` 最終報告已產生。
- 成果中沒有 TODO/FIXME/placeholder/stub。
- 未完成清單為空。
- 沒有以禁止替代實作規避要求。
- 最新提交對應到交付 binary 與證據。

## 10. 宣告格式

最終完成報告必須明確列出：

- 完成的 requirement 範圍。
- 測試總數、通過數、失敗數、skip 數。
- Easy／Medium／Hard 各 50 題的產生、唯一解、邏輯分類與效能統計。
- tinyvcs object verify 統計。
- locstat 分類與總行數。
- 已知非必要限制。
- 每個證據檔案的位置。

不得只以「全部完成」作為報告。

## 11. Scope-freeze 完成門檻

以下均不得列為「之後再補」：

- 三種難度分類與固定 technique trace。
- 可解釋 Hint 的 preview、Apply、Dismiss、Undo 與紀錄。
- 產品狀態機與 stale-result 防護。
- 規格指定的 golden scenes。
- 測試／證據一致性檢查。

同時，實作者不需為以下功能延後交付：聲音、網路、多人、排行榜、其他數獨變體、完整 Git 相容性、多語系。


## 12. Makefile 必要 targets

頂層Makefile至少提供：

```text
make all
make sudoku
make dev-tools
make unit
make integration
make e2e
make batch
make failure-tests
make evidence-audit
make test
make clean
```

- `all`建置全部production與test binaries，不執行測試。
- `test`依序執行unit、integration、必要非互動E2E/batch/failure/evidence checks；若完整GUI E2E需顯示環境，README可說明入口，但不得省略。
- 任一子target失敗，make回傳非零。
- `clean`只刪可重建產物，不刪source、fixtures、evidence或`.tinyvcs`。
- target不得下載依賴。

## 13. 必要 binary與名稱

README必須列出實際路徑，至少對應：

- `sudoku`。
- `locstat`。
- `tinyvcs`。
- `unit_tests`。
- `integration_tests`。
- `e2e_runner`。
- `batch_validator`。
- `failure_injector`或整合於test runner的明確入口。
- `evidence_audit`。

名稱可有prefix，但功能不能合併成無法單獨驗收的單一opaque binary。

## 14. Documentation completeness

實作交付文件需說明：

- source tree與module ownership。
- public/internal interfaces。
- event/state flow。
- object/vault formats與version。
- memory ownership與error propagation。
- build/run/test入口。
- test hooks與production隔離。
- evidence inventory。

文件不得只複製本任務包；必須描述實際完成的架構與路徑。

## 15. Evidence source identity

所有正式result、screenshot、recording與report必須包含或伴隨：

- task pack version。
- final source commit。
- product version。
- binary SHA-256。
- generation時間。

若證據來自不同commit，必須重做或明確標為非正式。

## 16. 正式完成報告

使用`24_FINAL_COMPLETION_REPORT_TEMPLATE.md`。Release gate任一非PASS時不得使用完成宣告。
