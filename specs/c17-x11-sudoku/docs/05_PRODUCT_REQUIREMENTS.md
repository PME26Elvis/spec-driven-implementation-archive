# Workstream B：數獨產品需求

## 1. 產品定位

一個離線、現代化、可保存多局進度的經典 9×9 數獨桌面應用。
使用者可以選擇 Easy／Medium／Hard、建立隨機題目、手動解題、使用候選數、取得一次一個可解釋 Hint、復原操作、自動求解、保存多個未完成遊戲，並查看成功完成紀錄。

## 2. 主要資訊架構

應用程式包含三個主要頁面：

1. Play
2. Library
3. Settings

頂部導覽列使用膠囊式滑動標籤切換頁面。
切換頁面不得重新建立或遺失目前遊戲狀態。

## 3. 啟動畫面

應用程式啟動後：

- 若本機 vault 尚未建立，顯示建立密碼流程。
- 若 vault 已存在且未解鎖，顯示解鎖流程。
- 解鎖後進入首頁／Play landing state。

Play landing state 至少顯示：

- `New Game`。
- 難度選擇：Easy、Medium、Hard。
- `Continue`。
- 最近未完成遊戲摘要。
- 未完成遊戲數量。
- 最近一次成功完成紀錄摘要。

若沒有未完成遊戲：

- `Continue` 為 disabled。
- disabled 狀態須有不同視覺，不能只是不回應。

## 4. New Game

建立新遊戲時：

- 使用者必須先選擇 Easy、Medium 或 Hard；預設為上次成功選擇的難度，首次使用預設 Easy。
- 產生新的 9×9 題目。
- 題目必須通過該難度的邏輯分類器，不得只依 clues 數標示。
- 題目必須有且僅有一個解。
- 顯示 loading/busy state，直到產生完成。
- 不得在 UI event loop 中呈現永久無回應。
- 每局有唯一 game ID。
- 記錄建立時間。
- 計時器在題目可互動後開始。

若目前已有未儲存變更：

- 建立新遊戲前需詢問 Save、Discard 或 Cancel。

## 5. Play 頁面

Play 頁面至少包含：

- 9×9 棋盤。
- 3×3 宮格的視覺分隔。
- 計時器。
- 遊戲狀態標籤。
- 數字輸入 1–9。
- Erase。
- Notes mode。
- Undo。
- Redo。
- Clear Answers。
- Save。
- Submit。
- Hint。
- Auto Solve。
- Pause／Resume。

## 6. 棋盤互動

### 6.1 選取

點擊可編輯格子後：

- 顯示選取狀態。
- 同列、同欄與同 3×3 宮顯示次要高亮。
- 棋盤內相同數字顯示一致高亮。
- given clue 與 player entry 必須有可辨識的視覺差異。

點擊 given clue：

- 可以選取。
- 不得修改。
- 輸入控制需顯示不可編輯狀態或忽略並給予明確視覺回饋。

### 6.2 正式答案輸入

Notes mode 關閉時：

- 輸入 1–9 設定該格正式值。
- 新值取代舊值。
- 設定正式值後清除該格全部 notes。
- 相同操作需進入 undo history。

### 6.3 候選數／筆記模式

Notes mode 開啟時：

- 每格可保存 1–9 的集合。
- 輸入數字切換該候選數存在與否。
- 候選數在格內以 3×3 小型排列顯示。
- 候選數不得視為正式答案。
- given clue 不得有 notes。
- 已有正式答案的格子不能同時顯示 notes。
- Notes mode 狀態切換本身不必進入 undo history。

## 7. Undo 與 Redo

Undo/Redo 必須支援：

- 正式數字輸入。
- 清除單格。
- 候選數新增／移除。
- Clear Answers。
- Auto Solve 造成的整體改變。

規則：

- 每個使用者意圖是一個 transaction。
- Clear Answers 必須一次 Undo 即完整恢復。
- 新操作發生後清空 redo stack。
- given clues 永遠不進入修改 history。
- history 必須隨存檔保存，載入後仍可 Undo/Redo。

## 8. Clear Answers

按下 Clear Answers：

- 顯示確認對話框。
- 確認後移除全部 player entries 與 notes。
- given clues 保留。
- 計時器不歸零。
- 操作記為單一 undo transaction。
- 取消不得改變任何狀態。

## 9. Pause

Pause 時：

- 計時器停止累加。
- 棋盤內容被遮蔽，不能以 pause 偷看。
- 不能輸入數字或執行遊戲操作。
- 顯示 Resume。
- 導覽至 Library/Settings 不得自動清除 pause 狀態。

Resume 後：

- 計時器從原 elapsed time 繼續。
- 不得把暫停期間算入 active play time。

## 10. Submit

Submit 不得將玩家棋盤逐格與內部完整答案比較。
提交檢查必須依數獨規則與完成狀態判定。

### 10.1 未完成

若存在空白正式答案格：

- 不算完成。
- 顯示尚缺格數。
- 標示所有空白可填格。
- 同時仍標示已存在的規則衝突。
- 不得把「與解答不同但目前無衝突」的格子標示為錯誤。

### 10.2 規則衝突

若任一列、欄或 3×3 宮有重複正式數字：

- 不算完成。
- 所有參與衝突的格子都必須被標示。
- 顯示衝突來源類型：row、column、box，可合併顯示。
- 不得只標示後輸入的格子。

### 10.3 成功

只有在：

- 81 格都有正式值。
- 每列符合 1–9。
- 每欄符合 1–9。
- 每個 3×3 宮符合 1–9。

才算成功。
由於題目保證唯一解，完整且無衝突的棋盤自然是該題唯一解，不需要逐格比對預存答案。

## 11. Hint

Hint 必須分析目前棋盤並一次提出一個可解釋的合法推理步驟。

必要行為：

- 不得直接只顯示答案數字而不說明理由。
- 顯示 technique 名稱、目標格或候選數、必要的支持格，以及自然語言說明。
- 以視覺高亮對應說明中的 cells、units 與 candidates。
- 預覽 Hint 不得立即修改棋盤。
- 使用者可選擇 `Apply Hint` 或 `Dismiss`。
- 套用 placement hint 時，填入一個 assisted value。
- 套用 elimination hint 時，只移除與該 deduction 對應的使用者 notes；若沒有可移除的 note，仍可顯示 deduction，但不得捏造棋盤變更。
- 每次套用視為一個 undo transaction。
- 每次成功取得 Hint 都增加 `hints_viewed`；套用時另增加 `hints_applied`。
- 使用過任何 Hint 的完成紀錄必須標示 assisted，但仍與 Auto Solve completion 分開記錄。
- 若目前有 direct conflict，先顯示衝突而不提供 Hint。
- 若目前無衝突但已無解，顯示目前狀態不可解，不得從 stored solution 猜測一個步驟。
- 若沒有支援範圍內的邏輯步驟，明確顯示 `No supported logical hint is available`；不得偷偷使用 backtracking 產生假的人類推理說明。

詳細 technique 與優先順序見 `07_SUDOKU_RULES_AND_ALGORITHMS.md`。

## 12. Auto Solve

Auto Solve：

- 顯示確認對話框。
- 使用真正求解器處理目前棋盤。
- 若目前玩家輸入仍可導向解，填入剩餘格。
- 若目前玩家輸入使棋盤無解，顯示兩個選項：
  - `Solve Original Puzzle`：忽略玩家輸入，從原始 clues 求解。
  - `Cancel`。
- 自動填入結果以不同狀態標記為 assisted。
- Auto Solve 後不得計為玩家成功紀錄。
- Auto Solve 的棋盤改變視為單一 undo transaction。
- Auto Solve 與 Hint 必須以不同 assisted reason bit 記錄。

## 13. 計時器

每局至少保存：

- active elapsed time。
- created time。
- last played time。
- completed time，若完成。

active elapsed time：

- 題目可互動後開始。
- Pause 時停止。
- blocking confirmation dialog 開啟期間停止。
- 應用程式關閉期間停止。
- 載入未完成遊戲後繼續。
- 顯示至少精確至秒。
- 不得使用系統時鐘倒退造成負時間。

## 14. Save

手動 Save：

- 保存棋盤、notes、history、timer 與 UI 必要狀態。
- 成功後顯示非阻塞成功回饋。
- 失敗時顯示可理解錯誤，且保留記憶體中的遊戲狀態。
- 不得在寫入失敗時覆蓋最後一份有效存檔。

## 15. 關閉視窗

當有正在進行的遊戲且自上次保存後有變更時，關閉視窗必須顯示：

- `Save & Exit`
- `Exit Without Saving`
- `Cancel`

行為：

- Save & Exit：保存成功後關閉；保存失敗則保持開啟。
- Exit Without Saving：放棄本次未保存變更；既有舊存檔保持不變。
- Cancel：關閉對話框並返回遊戲。

若是尚未曾保存的新遊戲：

- Exit Without Saving 後不得在 Library 留下半成品紀錄。

若沒有進行中的遊戲或沒有未保存變更：

- 可以直接關閉。

## 16. Continue 與多個未完成遊戲

- Library 可保存多個未完成遊戲。
- `Continue` 開啟最近一次遊玩的未完成遊戲。
- 使用者可從 Library 選擇其他未完成遊戲。
- 載入 A 遊戲前，若 B 遊戲有未保存變更，須先 Save/Discard/Cancel。
- 同一 game ID 不得產生重複 Library item。

## 17. Library 頁面

Library 分成：

- In Progress。
- Completed。

### In Progress item

至少顯示：

- 題目縮圖或簡化棋盤預覽。
- 建立時間。
- 最後遊玩時間。
- elapsed time。
- 已填正式答案格數。
- notes 數量。
- difficulty。
- hints viewed／applied。
- assisted 標記。

必要操作：

- Continue。
- Delete。
- 顯示刪除確認。

### Completed item

至少顯示：

- 完成時間。
- active elapsed time。
- difficulty。
- hints viewed／applied。
- 是否使用 Hint。
- 是否使用 Auto Solve。
- 是否為有效 player completion。
- 題目識別資訊。
- difficulty 與 logical rating summary。

完成紀錄不可再以 Play 模式修改。
可開啟 read-only 詳細檢視。

## 18. Library 捲動

- 內容超過可視範圍時可捲動。
- mouse wheel 與拖曳 scrollbar 至少支援一種；本版本要求 mouse wheel。
- 捲動位置需限制在有效範圍。
- 頂部導覽列的 frosted blur 與陰影依捲動距離平滑變化。
- 空清單顯示明確 empty state。

## 19. Settings 頁面

本版本至少包含：

- UI animation：On/Reduced。
- Confirm Auto Solve：On／Off，預設 On；關閉時只略過 Auto Solve 的第一層確認，不影響 Clear Answers、Delete、Reset Data 或 unsaved-change 對話框。
- Auto-remove peer notes：On／Off，預設 On。
- Theme：Dark／Light。
- Reset application data。

Reset application data：

- 必須要求重新輸入 vault 密碼。
- 再次顯示破壞性確認。
- 成功後刪除全部進行中、完成紀錄與設定。
- 保留既有 vault 與密碼，建立空的加密 payload，設定回到預設值。
- 不得刪除應用程式本身。

## 20. 鍵盤操作

至少支援：

- Arrow keys：移動選取格。
- 1–9：輸入正式值或 note。
- Backspace/Delete：清除選取格。
- N：切換 Notes mode。
- Ctrl+Z：Undo。
- Ctrl+Y 或 Ctrl+Shift+Z：Redo。
- Ctrl+S：Save。
- Space：Pause/Resume。
- Enter：在對話框啟動 primary action。
- Escape：關閉可取消對話框。

鍵盤與滑鼠必須操作同一份狀態，不得有不同規則。

## 21. 成功完成流程

成功 Submit 後：

- 停止計時器。
- 顯示完成 modal。
- 顯示 active elapsed time。
- 將遊戲移出 In Progress。
- 建立 Completed record。
- 清除該遊戲的 dirty state。
- 提供回到 Library 或開始新遊戲的選項。
- player completion、hint-assisted completion 與 auto-solved completion 必須可區分。
- 完成紀錄保存 difficulty、hints viewed、hints applied 與最高使用 technique。

## 22. 自動清除 peer notes

Settings 的 `Auto-remove peer notes` 預設開啟。

當玩家或 Apply Hint 在某格填入正式數字時：

- 若設定開啟，從同列、同欄與同宮其他可編輯格移除相同 note。
- 同一 note 即使同時屬於列、欄與宮，也只處理一次。
- 正式值輸入與所有 peer-note removals 必須是同一 undo transaction。
- Undo 必須完整恢復原正式值與被移除 notes。
- 若設定關閉，只清除目標格自己的 notes。
- given clues 與 assisted digits 同樣觸發此規則。

## 23. 現代桌面行為定義

- 所有 destructive action 都必須有明確名稱，不能只寫 `OK`。
- primary action 只能有一個；危險 action 不得預設取得鍵盤 focus。
- busy operation 期間相關按鈕 disabled，其他不衝突的導覽可依狀態機規則使用。
- 成功 toast 不得遮住棋盤主要互動，且自動消失；失敗訊息在使用者處理前不得過早消失。
- 重複快速操作必須 idempotent 或被抑制，不得建立兩個遊戲、兩筆完成紀錄或兩次保存。
- 日期時間顯示採本機時區；持久化格式採明確 epoch 整數。
- Library 預設以 `last played` 由新到舊排列；Completed 預設以 completion time 由新到舊排列。
- 同值排序時使用 game ID 作為固定 tie-breaker，確保畫面與測試可重現。


## 24. Current game 與 Library 持久化語意

- 同一時間最多一個 current game載入記憶體。
- 新產生遊戲在首次成功Save前是unsaved draft，不列入已持久化In Progress數量。
- 首次Save後才成為Library In Progress item。
- 已保存遊戲載入後的修改只存在記憶體，直到再次Save。
- Library顯示最後成功vault snapshot，不得將未保存current memory state冒充已保存item。
- current game若clean，Library摘要可直接對應save snapshot；若dirty，current item可加`Unsaved changes`標記，但reload後仍以最後save為準。
- Continue只選擇已保存的In Progress；若目前已有unsaved draft，Play頁直接顯示該draft，而不是將它誤算為Continue候選。

## 25. Timer、dirty與頁面互動

- active elapsed內部保存毫秒；UI顯示完整秒。
- timer從clean跨過下一個完整秒時，遊戲因時間變更成為dirty，但不得每一frame新增generation。
- elapsed造成clean→dirty時只增加一次current generation；後續時間累積不重複增加，直到Save後再次跨秒。
- current page離開Play時加入`APP_NOT_INTERACTIVE` pause reason；返回Play時只移除此reason。
- X11 focus lost超過500ms時加入`APP_NOT_INTERACTIVE`並遮蔽棋盤；focus regained只移除此reason。
- 若同時USER_PAUSE或modal存在，返回Play／focus不得誤啟動timer。

## 26. Auto Solve 與完成流程

- Auto Solve填滿棋盤後不立即建立Completed record，以保留一次Undo能力。
- 畫面顯示`Solved with Auto Solve — Submit to archive`或等效明確狀態。
- 使用者可Undo回到原盤，或按Submit進入完成流程。
- Submit合法盤時依origin判斷completion classification；只要任何非given格仍為auto-solve origin，分類為`AUTO_SOLVED`。
- Hint-assisted盤若沒有auto-solve origin，分類為`PLAYER_HINT_ASSISTED`。
- 完全無Hint/Auto Solve origin且hints_applied=0，分類為`PLAYER_UNASSISTED`。
- viewed但未applied的Hint不改completion classification，但completed record仍保存hints_viewed。

## 27. Clear Answers 精確範圍

Clear Answers確認後移除：

- 所有player values。
- 所有hint-assisted values。
- 所有auto-solved values。
- 所有player notes。

它不移除given clues、不改difficulty、seed、created time、elapsed或Hint計數。整體為單一transaction。

## 28. Hint no-op 行為

對 elimination Hint，若目標candidate未存在於任何player note：

- Preview仍顯示完整推理。
- `Apply Hint` disabled，旁邊顯示`No matching player note to remove`。
- Dismiss可用。
- 不建立transaction、不增加hints_applied。
- 再次要求Hint可能得到同一步；這是v1.0明確行為，不得為避免重複而偷偷填值。

## 29. Confirmation dialog 行為

- `Confirm Auto Solve=On`：Auto Solve先顯示一般確認。
- Off：若current board可解，可直接進入busy solve；若無解仍必須顯示Solve Original／Cancel決策。
- Clear Answers、Delete In Progress、Reset Data、unsaved close/new/switch永遠不可被設定關閉。
- 所有destructive primary label必須描述行為，不得只用`OK`。

## 30. 日期、排序與摘要

- 日期顯示本機時區。
- In Progress：last played descending，再game ID ascending。
- Completed：completed time descending，再game ID ascending。
- 已填格數只計非given formal values；clues另顯示或不計。
- notes數量為所有player note bits總數，不是含notes格數。
- Library board preview不得顯示password或hidden internal solution；可顯示original clues與saved current values。

## 31. 產品文字與狀態完整性

- 正式UI固定英文。
- 所有error、empty、busy、disabled與success state必須有可理解文字或semantic label。
- 不得以tooltip作為唯一必要說明。
- 所有主要控制項stable semantic ID見`20_UI_REFERENCE_CONTRACT.md`。
- 固定E2E產品行為見`21_CANONICAL_ACCEPTANCE_SCENARIOS.md`。


## 32. Global settings persistence

Theme、motion、peer-note removal、Confirm Auto Solve與last difficulty是global settings，不屬於單一game Undo history。

- 使用者修改Theme/Motion/Peer Notes/Confirm Auto Solve時，應用程式立即以原子vault save持久化。
- 保存成功後才顯示設定已套用；Theme/Motion可先preview，但失敗時必須回復最後persisted值。
- global settings save不得順便保存current dirty game。它必須使用最後persisted game snapshot，再只套用global settings變更。
- 若current game尚未首次Save，global settings save不得把draft加入Library。
- last difficulty在新遊戲成功建立後更新並持久化；generation失敗或Cancel不更新。
- 沒有current game時關閉app不需要額外settings prompt，因設定已即時持久化。
- 設定保存busy時對同一setting的重複切換須序列化或只保留最後意圖，不得讓較舊結果覆蓋較新選擇。

## 33. Game metadata dirty rules

下列也使current game dirty：

- Hint preview成功後`hints_viewed`增加。
- Hint Apply。
- Pause/Resume state改變。
- last played時間更新。
- completion前的assisted分類狀態改變。

純hover、selection、focus、page、scroll與modal animation不使game dirty。
