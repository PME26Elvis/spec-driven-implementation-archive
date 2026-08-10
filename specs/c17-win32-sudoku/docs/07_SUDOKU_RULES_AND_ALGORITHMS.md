# 數獨規則、難度、Hint 與演算法規格

## 1. 經典 9×9 規則

棋盤為 9 列 × 9 欄；每格正式值為空或 1–9。

有效完整棋盤必須滿足：

- 每列恰好包含 1–9 各一次。
- 每欄恰好包含 1–9 各一次。
- 每個 3×3 宮恰好包含 1–9 各一次。

原始 clues 為不可修改值。

## 2. 核心資料表示

實作必須清楚區分：

- original clues。
- current player values。
- notes bitset。
- assisted values 與 assisted reason。
- selection/UI state。
- derived logical candidates。

不可用單一模糊欄位混合 given、player、hint-assisted 與 auto-solved origin。
建議 notes 與 candidates 使用 9-bit mask，但可採等效表示。

## 3. 規則驗證器

規則驗證器必須能獨立於 UI 執行。

輸入：

- 81 格正式值。
- 可選的 clue mask。

輸出至少包含：

- 是否完整。
- 空格數。
- row conflict mask。
- column conflict mask。
- box conflict mask。
- 每格 conflict flags。
- 是否為完整有效棋盤。

## 4. 衝突標記算法

對每一列、欄與宮：

- 對數字 1–9 計數。
- 若某數字出現超過一次，所有出現位置都標記衝突。
- 同一格可同時有 row/column/box 多種衝突。
- 空格不構成 duplicate conflict。

不得只比較新輸入格，也不得只保留第一個或最後一個衝突位置。

## 5. Submit 判定

Submit 使用規則驗證器：

- 有空格：incomplete。
- 有 duplicate conflicts：invalid。
- 無空格且無 conflict：valid completion。

不得使用 stored solution 的逐格 equality 作為主要 Submit 判定。
內部可保存 solution 供產生器驗證、測試與 Auto Solve，但不得用它直接標示玩家目前「錯在哪格」。

## 6. 搜尋求解器

必須自行實作可處理任意合法 9×9 經典數獨的完整求解器。

最低要求：

- constraint propagation。
- candidate mask。
- backtracking search。
- 選擇候選數最少的空格，或等效有效策略。
- 可計算 0、1、至少 2 個解。
- 可在找到第二解後提早停止唯一性檢查。

至少支援：

- `solve_one(board)`：回傳一組解或無解。
- `count_solutions(board, limit)`：計算至 limit。
- `is_unique(board)`：恰好一解。
- `validate_partial(board)`：偵測直接規則衝突。

不得以全域隱藏狀態讓連續呼叫互相污染。

## 7. 搜尋求解器輸入案例

必須正確處理：

- 已完成有效棋盤。
- 已完成但有重複數字。
- 空棋盤。
- 無解棋盤。
- 多解棋盤。
- 唯一解棋盤。
- clues 自相矛盾。
- 玩家輸入造成無解但沒有立即 duplicate 的棋盤。

## 8. 邏輯求解器

難度分類與 Hint 使用另一個可重現的「人類邏輯求解器」。它不得依賴搜尋求解器的完整答案來偽造推理步驟。

邏輯求解器必須：

- 從目前正式值重新計算 derived candidates。
- 每次只套用一個 deterministic technique step。
- 產生結構化 step record。
- 依固定 technique priority 與 cell/unit scan order 選擇第一個步驟。
- 能以純邏輯解完整題目，或明確回報 stalled。
- 不得把 backtracking node 包裝成人類 Hint。

固定掃描順序：

1. technique priority 由低至高。
2. row 1–9。
3. column 1–9。
4. box 1–9，以由左至右、由上至下排序。
5. cell index R1C1 至 R9C9。
6. digit 1 至 9。

此順序用於固定 seed 測試與相同題目的穩定評分。

## 9. 必須支援的邏輯技巧

### T1 Naked Single

某空格 derived candidate 只有一個。

Step record 至少包含：

- target cell。
- placed digit。
- 原 candidate mask。

### T2 Hidden Single

某數字在一個 row、column 或 box 的候選位置只有一格。

Step record 至少包含：

- unit type 與 index。
- target cell。
- placed digit。
- 該 unit 中被檢查的候選位置。

### T3 Locked Candidates: Pointing

某 box 中某 digit 的候選都落在同一 row 或 column，可從該 row／column 的 box 外格移除。

### T4 Locked Candidates: Claiming

某 row 或 column 中某 digit 的候選都落在同一 box，可從該 box 的其他格移除。

### T5 Naked Pair

同一 unit 中恰有兩格具有相同的兩個 candidates，可從 unit 其他格移除這兩個 candidates。

### T6 Hidden Pair

同一 unit 中兩個 digits 只出現在相同兩格，可從該兩格移除其他 candidates。

### T7 Naked Triple

同一 unit 中兩至三格的 candidate union 恰為三個 digits，且符合 naked subset 條件，可從其他格移除。

### T8 Hidden Triple

同一 unit 中三個 digits 的所有候選位置合計只落在三格，可從該三格移除其他 candidates。

本版本不要求 X-Wing、Swordfish、coloring、chains 或猜測式解說。

## 10. Technique step 結構

每個 placement 或 elimination step 至少保存：

- technique enum。
- step kind：placement 或 elimination。
- target cells。
- affected candidates。
- supporting cells。
- supporting unit(s)。
- before candidate masks。
- after candidate masks。
- deterministic textual parameters，不直接保存不可驗證的自由文字。
- score weight。

UI 說明由結構化 step 產生，例如：

> Hidden Single in row 4: only R4C7 can contain 5, so place 5 at R4C7.

不得僅輸出：

> Put 5 here.

## 11. 難度評分

難度不得只依 clue 數。
每題必須由空白玩家盤面開始，使用第 8–10 節的邏輯求解器完整求解並留下 technique trace。

固定 technique weight：

| Technique | Weight |
|---|---:|
| Naked Single | 1 |
| Hidden Single | 2 |
| Locked Candidates: Pointing | 4 |
| Locked Candidates: Claiming | 4 |
| Naked Pair | 6 |
| Hidden Pair | 7 |
| Naked Triple | 10 |
| Hidden Triple | 12 |

總分計算：

- 每個實際改變至少一個 value 或 candidate 的 step 計一次 weight。
- 無效果的重複掃描不計分。
- placement 與 elimination 都計分。
- `logic_score` 為所有 step weight 總和。
- `max_technique` 為 trace 中最高技巧。

## 12. Easy／Medium／Hard 定義

所有難度共同條件：

- 題目恰有一解。
- 邏輯求解器能完整解出，不得 stalled。
- 不需要 backtracking 或未支援技巧。
- clue 數只作 sanity range，不作主要分類依據。

### Easy

- 只使用 Naked Single 與 Hidden Single。
- 至少出現一個 Hidden Single，避免全為機械式 naked singles。
- `logic_score` 20–120。
- clue sanity range：36–49；超出範圍即拒絕。

### Medium

- 至少出現一個 T3–T6 技巧。
- 不得使用 T7 或 T8。
- `logic_score` 70–260。
- clue sanity range：30–40。

### Hard

- 至少出現一個 T7 或 T8；或至少出現三個 T5/T6 且 `logic_score >= 220`。
- 可使用 T1–T8。
- `logic_score` 180–520。
- clue sanity range：24–34。

若題目同時符合兩個區間，以最高實際 technique 與上述必要條件決定；仍模糊時取較高難度。
若 `logic_score` 超過 Hard 上限、邏輯 stalled 或需要搜尋，該題不屬於本產品可發出的三種難度，產生器必須放棄並重試。

## 13. 隨機完整盤產生

產生完整解盤時：

- 使用安全或高品質隨機 seed。
- 搜尋候選順序可隨機化。
- 產生結果必須通過完整規則驗證。
- 不同 seed 應可產生不同盤面。
- 測試模式必須允許固定 seed，以重現結果。

不得只對一個固定解盤做有限少量位置交換。
允許合法 row/column/band/stack permutation 作額外隨機化，但不得作為唯一產生方式。

## 14. 題目挖空與分類

產生器至少執行：

1. 產生合法完整盤。
2. 依隨機順序選擇候選格。
3. 暫時移除 clue。
4. 使用 `count_solutions(limit=2)` 驗證唯一性。
5. 只有仍唯一解時才保留移除。
6. 到達目標 clue sanity range 後執行邏輯求解與難度分類。
7. 不符合所選難度則繼續調整或重新開始。
8. 最終再次獨立驗證完整規則、唯一解與難度 trace。

不得先生成任意題目後只改 UI label。

## 15. 產生器失敗處理

- 每個候選盤與整局流程均有嘗試上限。
- 達上限後重新 seed／重新產生完整盤。
- 若整體時間超過內部上限，UI 顯示可重試錯誤。
- 不得無限迴圈。
- 失敗不得建立空白、錯誤難度或未驗證 save record。
- error report 應包含 requested difficulty 與失敗階段，不包含敏感資料。

## 16. Hint 選擇

Hint 對目前棋盤執行：

1. direct conflict validation。
2. 確認目前棋盤至少有一解。
3. 從目前正式值計算 derived candidates。
4. 依固定 technique priority 找出第一個邏輯 step。
5. 將 step 交給 UI 預覽，不立即修改棋盤。

Hint 不依賴使用者 notes 的完整性；notes 可以缺少、過多或錯誤。
Hint 的 candidate reasoning 必須依 row/column/box 正式值重新推導。

### Placement Hint

- Apply 後只填入該 step 的一個 target value。
- 填入值標記為 hint-assisted。
- 套用自動 peer-note removal 設定。
- 形成單一 undo transaction。

### Elimination Hint

- 預覽顯示被排除的 candidate、support cells 與 unit。
- Apply 只移除使用者 notes 中實際存在且與 step 相符的 candidates。
- 不得新增猜測性 notes。
- 即使沒有對應 note 可移除，Hint 仍算 viewed，但不算 applied，也不得建立空 undo transaction。

## 17. Hint 文字與視覺語意

每個 Hint 必須顯示：

- technique display name。
- 一句結論。
- 至少一句可驗證理由。
- cell 名稱採 RnCm。
- digit 與 candidate list。
- supporting unit。

高亮角色至少分為：

- target。
- support。
- elimination。
- related unit。

不得只靠顏色區分；需搭配 border style、marker 或 candidate strike-through。

## 18. Auto Solve

Auto Solve 對目前棋盤：

- 先執行 direct conflict validation。
- 若有直接衝突，回報 conflicts，不直接覆蓋。
- 若無直接衝突但無解，回報 unsatisfiable current state。
- 若至少一解，使用搜尋求解器填入結果。
- 正常產生的原始 clues 必須唯一，因此忽略玩家輸入後必須恰有一解。
- Auto Solve 的填入來源標記為 auto-solved，不得與 hint-assisted 混為同一原因。

## 19. 不得將求解答案當成玩家錯誤檢查

對部分填答：

- 只要尚未直接違反 row/column/box，該格不能因與 stored solution 不同而標紅。
- 即使該輸入最終導致無解，Submit 主要結果仍是 incomplete；可額外顯示目前狀態無解。
- Hint 可先判斷無解而拒絕提供步驟，但不得以 stored solution 標出「錯格」。
- 找出最小矛盾集合不是本版本必要功能。

## 20. Notes 邏輯

- notes 是玩家輔助資料，不參與 Sudoku validity。
- derived candidates 是演算法資料，不得直接覆寫玩家 notes。
- 正式值輸入後清除該格 notes。
- 清除正式值不自動恢復過去 notes，除非透過 Undo。
- Auto Solve 後畫面不得在已填格顯示 notes。
- peer-note removal 必須符合 Settings 與 transaction 規格。

## 21. 確定性測試模式

- 搜尋產生器接受明確 seed。
- 相同規格版本、seed、difficulty 必須得到相同題目與 difficulty trace。
- 邏輯求解器固定 scan order。
- 正常 UI 建立新遊戲不得固定 seed。
- 若演算法版本變更導致 fixed-seed output 改變，必須提升 generator format version 並更新 fixture。

## 22. 效能基準

在一般桌面 CPU 上：

- 解一般唯一解題目目標 1 秒內。
- `count_solutions(limit=2)` 有明確 node／time guard。
- Easy/Medium 單題產生目標 3 秒內。
- Hard 單題產生目標 8 秒內。
- 超時顯示 busy/retry，不得永久無回應。
- batch 報告記錄每難度成功率、唯一解率、分類通過率、中位數與第 95 百分位時間。

此處定義產品門檻，不規定測量工具。

## 23. 必要演算法測試

至少包含：

- 已知唯一解、無解、多解題目。
- 完整合法盤。
- row/column/box duplicate 與多類 conflict。
- 部分棋盤與 stored solution 不同但無直接 conflict。
- 每一種 T1–T8 technique 的正例 fixture。
- 每一種 technique 的 near-miss 反例，避免過度套用。
- Hint 結構化 step 的固定 expected result。
- Hint 不讀取 stored solution 的隔離測試。
- Easy/Medium/Hard 固定 fixture 的分類。
- boundary score 與 clue sanity range。
- fixed seed 重現。
- 每難度至少 50 題 uniqueness＋difficulty batch。
- solver 連續呼叫無狀態洩漏。
- Apply Hint、Auto Solve 後 Undo 完整還原。


## 24. Candidate state 精確語意

搜尋求解器與邏輯求解器必須使用分離state。

邏輯求解起始時：

1. 對每個空格以row/column/box formal values計算base mask。
2. 若任何空格mask=0，回報unsatisfiable/stalled原因。
3. elimination step只修改此次logic run的candidate masks。
4. placement後更新formal value，重新移除所有peers對應digit；既有合法elimination保持。
5. 完成或stalled前不得重新從formal values重建並遺失先前eliminations。

每次新的Hint request從目前formal values重新開始一個logic run；player notes不作candidate truth source。

## 25. Technique enumeration tie-break

同一technique有多個合法step時：

- Placement：target cell index ascending，再digit ascending，再unit order row/column/box。
- Pointing/Claiming：digit ascending、source unit order、target unit order、affected cell list lexicographic。
- Naked subset：unit orderrow→column→box，subset size固定於technique，cell combination lexicographic，digit union ascending。
- Hidden subset：unit order，digit combination lexicographic，target cell list lexicographic。
- 只有實際造成至少一個placement或candidate removal的step才合法。
- elimination list必須去重並ascending。

這些規則與`19`共同保證固定trace。

## 26. Subset technique formal conditions

### Naked Pair/Triple

選定k個空格（k=2或3）：

- 每格candidate count介於2與k。
- union candidate count恰為k。
- 至少一個unit內其他空格含union digit可移除。
- 不得把同一個已被更小subset完整解釋的集合重複當更大subset；technique priority自然先套用Pair。

### Hidden Pair/Triple

選定k個digits：

- 這些digits在unit內出現的候選位置union恰為k格。
- 每個選定digit至少出現一次。
- 目標格至少有一個非選定digit可移除。

near-miss fixture必須涵蓋union過大、位置不足、無實際elimination與已填格誤納入。

## 27. Difficulty acceptance edge rules

- clue count包含given cells。
- logic score使用完整trace，包含最後一個placement。
- 同一step移除多個candidate仍只計一次weight。
- 一個placement因peer propagation自然移除candidate，不另計elimination step。
- Hard第二條件「至少三個T5/T6」指trace中三個有實際效果的獨立steps。
- score位於區間端點視為合法。
- 若Medium題trace含T7/T8，即使score低也拒絕。
- 若Hard題只含T1–T6，必須同時T5/T6 step count>=3且score>=220。
- accepted puzzle完整trace必須解至81格，不能只分類前綴。

## 28. Generator guards

每次production generation至少設定：

- full-grid search node guard。
- uniqueness count node guard。
- candidate puzzle attempts guard。
- whole request wall/monotonic guard。

具體內部數值可依演算法設計，但不得無限；report必須記錄guard hit。固定seed test不得因thread scheduling改變結果。

產生器可使用對稱挖空，但v1.0不要求；若使用，仍逐次驗證唯一解。

## 29. Search solver determinism

`solve_one`在測試模式：

- cell selection使用minimum candidates，tie取cell index最低。
- digit順序1→9。

production generator可隨機化digit/cell候選，但`count_solutions`的結果不得受隨機性影響。

## 30. Stored solution isolation

為證明Hint/Submit未讀stored solution，測試必須可：

- 將stored solution替換為另一個完整合法但不對應的盤，Submit conflict/incomplete結果不變。
- 在Hint模組不可取得solution pointer的build/test boundary執行fixture。
- 對同一current board與不同hidden solution bytes，structured Hint完全相同。

Auto Solve與generator verification可使用搜尋求解器；UI錯格標示與Hint解說不可讀預存答案。

## 31. Batch report必要欄位

每題至少記錄：seed、requested/actual difficulty、clue count、logic score、max technique、各technique count、generation attempts、uniqueness result、search nodes、logic step count、duration、accepted/reject reason。

Aggregate至少記錄median、p95、max與所有reject reason counts。
