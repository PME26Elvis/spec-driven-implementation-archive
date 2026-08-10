# 產品狀態機與事件契約

## 1. 目的

本文件固定跨頁面、遊戲、modal、busy、dirty 與 timer 的行為。實作者可採不同內部架構，但外部狀態轉移必須等效。

## 2. 頂層狀態

應用程式恰處於下列之一：

- `BOOTING`
- `VAULT_SETUP`
- `VAULT_LOCKED`
- `READY_NO_GAME`
- `READY_GAME_ACTIVE`
- `READY_GAME_PAUSED`
- `BUSY_GENERATING`
- `BUSY_HINT`
- `BUSY_SOLVING`
- `BUSY_SAVING`
- `FATAL_ERROR`
- `EXITING`

Modal 不是獨立頂層狀態，而是附加的 modal layer；同一時間最多一個 blocking modal。

## 3. Session 狀態

解鎖後至少維護：

- current page：Play／Library／Settings；非Play時加入APP_NOT_INTERACTIVE pause reason。
- current game ID 或 none。
- current game dirty generation。
- current request generation。
- modal kind 或 none。
- timer pause reasons bitset。
- selected cell。
- notes mode。
- scroll offsets。

## 4. Timer pause reasons

Timer 是否運行不得由多個 boolean 互相覆蓋。至少使用下列原因集合或等效引用計數：

- USER_PAUSE
- BLOCKING_MODAL
- APP_NOT_INTERACTIVE
- BUSY_OPERATION
- GAME_COMPLETED

只要任一原因存在，active elapsed 不累加。移除其中一個原因不得誤解除其他原因。

## 5. New Game 轉移

### 無 dirty game

`READY_*` + New Game：

1. 顯示 difficulty chooser 或使用已選難度。
2. 進入 `BUSY_GENERATING`。
3. 建立 request generation 與 requested difficulty。
4. 成功且 request 仍有效：建立新 game ID，進入 `READY_GAME_ACTIVE`。
5. 失敗：回原狀態並顯示 Retry/Cancel。

### 有 dirty game

先開 Save／Discard／Cancel modal：

- Save：成功後繼續 New Game；失敗停留原遊戲。
- Discard：恢復最後保存版本或丟棄未保存新局，再繼續。
- Cancel：不產生新局。

## 6. Switch Game 轉移

- 目標 game ID 與目前相同：不做事。
- 目前 game clean：直接載入目標。
- 目前 game dirty：Save／Discard／Cancel。
- 載入失敗：目前記憶體遊戲保持不變。
- 成功後更新 last played，timer 依目標 pause state 決定。

## 7. Save 轉移

- clean game 的 Save 可顯示 `Already saved`，不得重寫造成 nonce reuse。
- dirty game：進入 `BUSY_SAVING`，加入 BUSY_OPERATION pause reason。
- 成功：更新 last saved generation，回原 ready state。
- 失敗：generation 不變，維持 dirty，顯示可操作錯誤。
- 重複 Save request 在同一 busy generation 中只執行一次。

## 8. Close 轉移

- 無 dirty game：進入 EXITING。
- 有 dirty game：Save & Exit／Exit Without Saving／Cancel。
- Save & Exit：保存成功後 EXITING；失敗回原狀態。
- Exit Without Saving：不改動最後有效 vault，進入 EXITING。
- Cancel：移除 modal pause reason，回原狀態。
- 第二個 window-manager close event 不建立第二個 modal。

## 9. Pause 與 modal

- User Pause 加入 USER_PAUSE 並遮蔽棋盤。
- Resume 只移除 USER_PAUSE。
- blocking modal 加入 BLOCKING_MODAL。
- modal 關閉只移除 BLOCKING_MODAL。
- 若玩家原本已 Pause，關閉 modal 後仍保持 paused。
- 完成 modal 加入 GAME_COMPLETED，不能 Resume。

## 10. Hint 轉移

1. 只能對 current game 發出。
2. 進入 `BUSY_HINT`，記錄 game ID、board generation、request generation。
3. 結果返回時三者都必須仍匹配，否則丟棄 stale result。
4. 成功後回 ready state並顯示 preview。
5. Dismiss：不改 board/history；`hints_viewed` 已增加。
6. Apply：重新驗證 step 對目前 generation 仍有效，再建立一個 transaction。
7. Apply 後更新 dirty generation；placement 增加 hints_applied，無效果 elimination 不增加。

## 11. Auto Solve 轉移

- 與 Hint 同樣綁定 game ID、board generation、request generation。
- busy 期間不得再次啟動 solver。
- 直接 conflict 或 unsatisfiable 不修改棋盤。
- Apply solve 結果為單一 transaction。
- stale result 不得覆寫新輸入或另一局。

## 12. Submit 轉移

- incomplete/invalid：保持 current game，顯示結果；不建立 record。
- valid completion：原子地從 In Progress 移除並加入 Completed。
- 同一 game ID 已完成：後續 Submit 為 no-op，不新增紀錄。
- 完成後加入 GAME_COMPLETED pause reason。

## 13. Delete 轉移

- 刪除非 current In Progress：確認後原子保存。
- 刪除 current game：拒絕並提示先關閉／切換目前遊戲。
- Completed 不提供單筆刪除。
- Reset Data：要求密碼與第二次確認，成功後回 `READY_NO_GAME`。

## 14. Busy 與輸入

BUSY_GENERATING/HINT/SOLVING/SAVING 時：

- 對應觸發按鈕 disabled。
- 棋盤修改事件不得執行。
- close event設定`pending_close`，等待operation完成或安全取消確認後再執行一般close flow；不得直接free正在使用的state。
- resize/expose 仍必須處理。
- 不得呈現 OS `not responding` 的永久狀態。

## 15. 非法事件

任何目前狀態不允許的事件：

- 不改變資料。
- 不建立 history。
- 不增加 timer 或統計。
- 可給短促 disabled feedback。
- 測試模式必須可斷言事件被拒絕。

## 16. 狀態持久化邊界

必須保存：game、timer、pause、history、difficulty、Hint/assisted 統計與設定。

不需保存：hover、pressed、ripple instances、modal animation progress、toast、當前 focus。

selection 與 current page 可保存但不是完成必要條件；若不保存，啟動時使用明確預設。


## 17. Page與focus轉移

- `Play → Library/Settings`：加入APP_NOT_INTERACTIVE。
- `Library/Settings → Play`：移除PAGE來源的APP_NOT_INTERACTIVE；若focus仍lost，reason仍存在。
- X11 FocusOut持續超過500ms：加入FOCUS來源的APP_NOT_INTERACTIVE並遮蔽棋盤。
- FocusIn：移除FOCUS來源；不移除PAGE、USER_PAUSE、MODAL、BUSY或COMPLETED。
- 實作可用source-specificbit或refcount；不得只用單一boolean。

## 18. Dirty generation rules

- 每個有效state-changing transaction增加current generation一次。
- Undo/Redo各自也是一次generation變更。
- timer從clean後累積跨過下一完整秒，增加generation一次並維持dirty；之後每秒不再增加。
- Save snapshot記錄其generation；成功只將saved generation設為該snapshot generation。
- 若Save期間current generation變大，完成後仍dirty。
- selection/page/hover/focus/ripple/modal animation不增加game generation。

## 19. Hint counters

- successful structured preview建立時增加hints_viewed。
- conflict、unsatisfiable、stalled或internal failure不增加。
- Dismiss不回退viewed。
- placement Apply成功增加hints_applied。
- elimination只有實際移除至少一個player note才增加applied。
- stale preview/result不增加任何counter。

## 20. Completion classification

valid Submit時：

1. 若任一非given formal value origin=auto-solve，`AUTO_SOLVED`。
2. 否則若任一origin=hint或hints_applied>0，`PLAYER_HINT_ASSISTED`。
3. 否則`PLAYER_UNASSISTED`。

Completed move與vault save同一原子operation。若save失敗，game仍保持current/in-progress且completion modal顯示保存失敗，不得產生memory-only正式紀錄。

## 21. Pending close

BUSY_*收到close：

- `pending_close=true`。
- 忽略後續重複close。
- operation結束後先套用成功/失敗state，再進一般close判定。
- 若operation結果stale，丟棄後仍處理pending close。
- pending close本身不等於Exit Without Saving。

## 22. Unsaved draft

新產生但未首次Save的game：

- current game存在、dirty=true、persisted=false。
- 不出現在vault Library。
- New/Switch/Close的Discard會丟棄整個draft。
- Save成功後persisted=true並成為In Progress。


## 23. Global setting save

Settings change事件：

1. 建立新的settings request generation。
2. 可先更新preview value。
3. 進入或排入BUSY_SAVING，但snapshot來源為persisted store，不含current dirty game/draft。
4. 成功且request仍為最新：更新persisted setting。
5. 成功但已過期：不得覆蓋較新preview；下一個queued save決定最終值。
6. 失敗：回復最後persisted value並顯示錯誤。

Global save不得修改game saved generation。

## 24. Persisted versus live library

Library資料來源以persisted store為主。若current game已persisted但dirty，可在對應card顯示`Unsaved changes`badge；card摘要的board/count仍可顯示live preview，但必須明確標示未保存，且其他games完全來自persisted store。
