# 測試、驗證工具與證據

## 1. 核心要求

實作者必須自己寫出足以驗證本作業的測試與輔助工具。
沒有測試證據的核心功能不得只靠文字宣稱完成。

全部測試程式碼屬於正式交付物，不能在最終版本刪除。

## 2. 測試層級

至少包含：

1. Unit tests。
2. Integration tests。
3. End-to-end tests。
4. Corruption/failure-injection tests。
5. Deterministic visual-state evidence。

## 3. 共用 C 測試執行器

需自行提供 C17 測試執行器或共用 test harness，至少支援：

- 註冊測試名稱。
- assertion equal/not equal。
- memory/byte comparison。
- expected failure。
- temporary directory fixture。
- 每測試 pass/fail。
- suite summary。
- 非零 exit status 表示失敗。
- 可輸出人類可讀與 JSON 結果。

不得以 shell script 只檢查程式有無崩潰取代斷言。
Shell 可作為啟動包裝，但核心判定必須有明確 assertions。

## 4. Unit tests

### locstat

- lexer 狀態。
- line ending。
- ignore matching。
- JSON parser。
- category assignment。

### tinyvcs

- SHA-256。
- CRC32。
- LZSS。
- object serialization。
- tree canonical ordering。
- ignore matching。
- ref validation。

### Sudoku core

- board validation。
- conflict masks。
- notes bit operations。
- undo transactions。
- solver。
- solution count。
- generator fixed seed。
- T1–T8 logical technique detectors。
- difficulty scoring and classification。
- structured Hint generation and application。

### Storage/security

- binary serializer/parser。
- length validation。
- KDF/cipher primitives。
- AEAD tamper rejection。
- atomic save state transitions。

### UI engine

- alpha blend。
- clipping。
- rounded geometry bounds。
- cubic Bézier evaluator。
- animation progress。
- layout calculations。
- hit-testing。
- scroll clamping。

## 5. Integration tests

至少涵蓋：

- locstat 讀 config、掃描 fixture tree、產出 JSON。
- tinyvcs init→add→commit→modify→status→restore。
- tinyvcs branch→switch→不同內容→切回。
- tinyvcs object corruption→verify failure。
- generator→unique check→save→load→solve。
- player actions→undo/redo→save→load→history preserved。
- complete game→move to Completed→restart→record remains。
- wrong password／tampered vault→拒絕載入。
- current vault corrupt→backup recovery。

## 6. E2E 測試

E2E 必須操作真實應用程式流程，而不是直接呼叫內部函式後宣稱 UI 已通過。

至少要有可重現 scenario：

1. 首次建立 vault。
2. 建立新遊戲。
3. 輸入正式答案與 notes。
4. Undo/Redo。
5. Save。
6. 關閉並重新啟動。
7. Continue 正確遊戲。
8. 建立第二局並在 Library 切換。
9. Clear Answers→Cancel。
10. Clear Answers→Confirm→Undo。
11. Submit incomplete。
12. 建立 row/column/box conflict 並 Submit。
13. Auto Solve。
14. assisted completion 不計入 player success。
15. 完成合法題目並出現在 Completed。
16. 關閉視窗 Save & Exit。
17. 關閉視窗 Exit Without Saving。
18. 修改設定並重啟確認保存。

## 7. E2E 可觀測性

實作者可自行決定E2E runner架構，但最終必須啟動實際Win32 executable、取得真實HWND，並包含：

- scenario 名稱。
- 步驟結果。
- 最終 state assertions。
- 失敗步驟。
- 程式 exit status。
- 對應 screenshot path，若該 scenario 要求畫面證據。

不得只輸出「E2E passed」而沒有逐步或最終斷言。

## 8. 測試模式

為可重現性，應用程式可提供 test-only 啟動選項，但：

- 不得改變 production 核心規則。
- 固定 seed、虛擬時間或 fixture vault 必須明確標記。
- production 預設不得使用固定 seed。
- test hooks 不得讓一般使用者繞過密碼或偽造完成紀錄。

## 9. Screenshot 證據

固定截圖至少包含：

- 1280×800 Dark Play。
- 1280×800 Light Play。
- notes 狀態。
- incomplete Submit。
- 多重 conflict。
- Clear Answers modal。
- close Save/Discard/Cancel modal。
- Library 多個 unfinished games。
- Completed record。
- Library scroll=0。
- Library scroll>threshold 的 frosted nav。
- 最小視窗尺寸。

截圖必須來自實際執行 frame。
不得使用設計稿或後製合成。

## 10. 動畫證據

至少提交短錄影或 frame sequence：

- hover elevation。
- click ripple。
- border glow transition。
- navigation capsule slide。
- modal scale+opacity open/close。
- blur progressive change。
- dynamic nav blur/shadow with scrolling。

只提交起點與終點截圖不足以證明動畫連續性。

## 11. Failure injection

需能建立並驗證：

- 截斷 vault。
- vault ciphertext bit flip。
- wrong password。
- write failure simulation。
- tinyvcs blob bit flip。
- tinyvcs missing tree object。
- malformed locstat config。
- generator forced retry exhaustion。

Failure injection 工具不得修改 production binary 的核心判定結果，只控制輸入或故障點。

## 12. Batch 驗證

至少執行：

- Easy、Medium、Hard 各 50 個不同 seed 的 puzzle generation。
- 每題 uniqueness count limit=2。
- 每題 logical solver 完整求解、difficulty label、score range 與 technique requirement 驗證。
- 每題 search solver round trip。
- 100 次 encrypt/decrypt with unique nonce。
- 100 組 tinyvcs random/repeated blob LZSS round trip。
- 反覆 resize/paint 至少 1,000 iterations 的無崩潰測試。

## 13. 測試資料

fixture 必須包含：

- 已知 Sudoku unique/no-solution/multiple-solution boards。
- T1–T8 technique 正例與 near-miss fixtures。
- Easy／Medium／Hard 固定分類 fixtures。
- C comments/strings edge cases。
- ignore directory tree。
- repository history fixture。
- crypto known-answer vectors。
- corrupted binary samples。

不得在執行測試時從網路下載 fixture。

## 14. 結果格式

最終 `results/` 至少包含：

- `unit-results.txt`
- `unit-results.json`
- `integration-results.txt`
- `integration-results.json`
- `e2e-results.txt`
- `e2e-results.json`
- `batch-results.json`
- `failure-injection-results.json`
- `locstat/`
- `screenshots/`
- `recordings/`

## 15. 測試獨立性

- 測試不得依賴既有使用者資料。
- 每個 suite 使用隔離 temporary directory。
- 測試失敗後仍需清楚標示殘留 fixture path，或安全清除。
- 測試順序改變不應改變結果。
- 可重跑。

## 16. 不接受的測試

- 永遠為真的 assertion。
- expected value 由同一被測函式計算。
- 只測 happy path。
- 只測一個 Sudoku 題目。
- 只測檔案存在。
- 只測 UI 程式啟動。
- 將失敗測試標記 skip 後仍宣稱全通過。
- 產生報告但實際未執行測試。

## 17. 證據對照表

交付需提供 requirement-to-test matrix，且至少包含 `17_ACCEPTANCE_REQUIREMENT_CATALOG.md` 的全部 ID：

- requirement ID／章節。
- 實作模組。
- unit test。
- integration/E2E test。
- screenshot/recording，若適用。
- 結果檔路徑。

所有 MUST requirement 至少有一個證據項目。

## 18. 狀態機測試

需以 table-driven 測試覆蓋 `15_PRODUCT_STATE_MACHINE.md`：

- 每個合法 event 的 next state。
- 每個非法 event 被拒絕且 state 不變。
- dirty、timer、modal、busy flags 的組合。
- stale async result 依 game ID／request generation 被丟棄。
- close、switch game、new game 三條 unsaved-change 流程一致。

## 19. 規格一致性檢查

交付時必須執行一個可重現檢查，至少驗證：

- requirement-to-test matrix 中所有 MUST 有證據。
- 所有列出的 screenshot/recording 路徑存在。
- JSON test result 可解析，且 summary 與詳細案例數一致。
- 沒有必要測試被 skip。
- fixed seed fixture 版本與 generator/difficulty rules version 相符。
- final binary 所對應的 source commit 可由 `tinyvcs show` 識別。


## 20. Canonical scenarios

`21_CANONICAL_ACCEPTANCE_SCENARIOS.md`中的scenario ID為v1.0最低E2E／整合集合。實作者可增加案例，但不得刪除、合併後遺失assertion或以單元測試替代真實UI流程。

## 21. 測試命名與 requirement IDs

- 每個test case至少引用一個catalog ID。
- 名稱需穩定且描述行為，例如`unit.sudoku.hidden_pair.near_miss_union_too_large`。
- 同一ID可有多個case。
- 不得用單一`test_everything`掩蓋失敗位置。
- JSON result schema見`19` §26。

## 22. 最低 assertion品質

對state-changing case至少assert：

- command/result status。
- primary state。
- 不應變動的invariants。
- history/dirty/ref/count等副作用。

對failure case至少assert非零/error category，以及正式資料未被修改。

## 23. UI E2E真實性

E2E的pointer/key activation必須透過`SendInput`或等效OS input injection進入正式Win32 hit-test/focus/dispatch路徑。允許semantic probe定位元件；可直接觸發`WM_CLOSE`、resize、DPI等window lifecycle事件，但不得直接發送產品command message或呼叫內部handler。

不得：

- 直接呼叫`new_game()`後宣稱點了按鈕。
- 直接改state struct後截圖。
- 用test-only hidden command跳過modal。

## 24. Golden scene construction

固定scene可使用test fixture直接載入已驗證vault/board，以避免每次經長流程建立畫面；但fixture載入仍需經正式parser，且scene只用於視覺證據，不取代對應產品E2E。

每張scene需保存：scene ID、seed/fixture hash、client size、theme、motion mode、virtual time、source commit與binary hash。

## 25. Independent expected values

至少採下列方式之一建立expected：

- 公開標準known-answer bytes。
- 人工固定小fixture與明確計算。
- 與production不同的簡單reference implementation，只存在test source。
- 不變量／round-trip加上獨立corruption判定。

單純round-trip不足以證明兩端共享錯誤格式的primitive正確，因此crypto/hash至少需要known-answer。

## 26. Allocation failure testing

可透過test allocator在第N次allocation失敗。至少對：

- UI buffer/blur buffer resize。
- generator/solver state。
- vault serialize/encrypt buffer。
- tinyvcs object/index construction。
- locstat traversal/report growth。

驗證無double free、leak-like lost ownership、partial formal state或誤報成功。

## 27. Release gates

測試與證據只有在`22_RELEASE_GATE_AND_AUDIT.md` G0–G15全PASS時才構成正式完成。單一suite全綠不代表整體完成。

## 28. Windows platform verification

Windows fork還必須驗證：

- PE import table只含allowlist與已說明CRT/system dependency。
- 無native child control class與禁止API import。
- 96/125/150/200% DPI場景。
- Unicode root/data path、case collision、case-only rename、reparse point與ADS。
- `SendInput`真實pointer/keyboard E2E。
- minimize/restore/occlusion、pointer capture loss與Alt+F4。
- Win32 atomic replacement stage-by-stage failure injection。

結果需對應`WIN-E2E-01`至`WIN-E2E-07`與`WIN-01`至`WIN-15`。

Windows平台細節以`26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`為準。
