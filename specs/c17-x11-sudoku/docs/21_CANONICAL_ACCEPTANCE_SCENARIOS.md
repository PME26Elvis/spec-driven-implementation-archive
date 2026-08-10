# Canonical Acceptance Scenarios

版本：1.0.0

## 1. 目的

本文件定義最低 E2E／整合驗收場景。它只固定前置狀態、使用者行為與可觀察結果，不指定如何驅動 X11、如何截圖或使用哪種測試框架。

每個 scenario 必須：

- 有唯一 ID。
- 記錄使用的 binary/source commit。
- 從隔離資料目錄開始，除非前置狀態明確依賴前一場景。
- 對每個關鍵步驟做 state assertion。
- 失敗時輸出實際狀態、預期狀態與最後一張畫面。

## 2. 共用 fixture 規則

- Test vault 密碼固定為 `Correct Horse 42`；只可在 test mode fixture 使用。
- Reference client size 1280×800。
- Test wall clock 起點 `2026-01-02T03:04:05.000Z`。
- Test monotonic clock 起點 0 ms。
- 使用 fixed seed 時必須記錄 generator/difficulty rules version。
- Scenario 結束時不得殘留 blocking modal 或 busy request，除非預期結果就是該狀態。

## 3. Vault scenarios

### VAULT-E2E-01 First setup

前置：資料目錄不存在。

步驟：

1. 啟動 production-equivalent binary with isolated data directory。
2. 輸入兩次相同合法密碼。
3. 確認建立。

必要 assertions：

- 初始為 `VAULT_SETUP`。
- 密碼欄遮蔽。
- 建立後為 `READY_NO_GAME`。
- current vault 存在，backup 可尚不存在。
- 檔案中不得出現密碼或可讀的固定棋盤 fixture。

### VAULT-E2E-02 Mismatch and validation

- 少於 8 bytes：primary disabled 或提交後明確拒絕。
- 兩次密碼不同：不建立 vault。
- 超過 64 bytes：拒絕，不截斷。
- 合法內部空白密碼不被 trim。

### VAULT-E2E-03 Lock/unlock

1. 建立含一局遊戲的 vault。
2. 正常退出並重啟。
3. 輸入錯誤密碼。
4. 再輸入正確密碼。

Assertions：錯誤密碼不載入任何 game；正確密碼載入原狀態。

### VAULT-E2E-04 Tamper and backup recovery

1. 產生 current+backup。
2. bit-flip current ciphertext。
3. 啟動並輸入正確密碼。
4. 選擇 recovery。

Assertions：current 不被接受；backup 有效；recovery 後資料與 backup snapshot 一致；corrupt file 保留診斷副本。

## 4. New game and difficulty

### GAME-E2E-01 Create Easy

- 選 Easy。
- 觸發 New Game。
- busy state 可見。
- 完成後棋盤 9×9、clues 36–49、唯一解、trace 只含 T1/T2 且至少一個 T2。
- timer 從可互動時開始。

### GAME-E2E-02 Create Medium

- clues 30–40。
- trace 至少一個 T3–T6。
- trace 不含 T7/T8。
- score 70–260。

### GAME-E2E-03 Create Hard

- clues 24–34。
- 符合 Hard technique 條件。
- score 180–520。

### GAME-E2E-04 Generation failure

以 test hook 使 retry exhaustion：

- 顯示 Retry／Cancel。
- 不建立 game ID。
- Library count 不變。
- timer 不啟動。
- Retry 解除 failure 後可成功。

### GAME-E2E-05 Duplicate activation suppression

快速雙擊 New Game：只建立一局、只增加一個 Library item、只有一個 active generation request。

## 5. Board input scenarios

### INPUT-E2E-01 Selection and formal entry

- 選 editable cell。
- 驗證 selected、peer、same-number layers。
- 輸入 5。
- cell value=5、origin=player、notes=0、history+1、dirty=true。

### INPUT-E2E-02 Given protection

- 選 given cell。
- pointer keypad與keyboard輸入均不得改值。
- history、dirty generation、timer統計以外狀態不變。

### INPUT-E2E-03 Notes toggle

- 開 Notes。
- 同格輸入 2、7、2。
- 最終 notes只有7。
- 3×3 slot正確。
- 每次有效 toggle各一個 transaction。

### INPUT-E2E-04 Peer-note removal On

1. 在 peers 建立 digit 6 notes。
2. 在 target 正式輸入6。

Assertions：所有 peer 6 notes移除；非peer保留；輸入與移除為單一 transaction；一次 Undo完整恢復。

### INPUT-E2E-05 Peer-note removal Off

- target 自身 notes清除。
- peers notes不變。
- Undo正確。

## 6. Undo/Redo/Clear scenarios

### HIST-E2E-01 Cross-save history

- 做 player value、note、erase。
- Save、退出、重啟、Continue。
- Undo/Redo仍按原順序運作。

### HIST-E2E-02 Redo invalidation

- A、B、Undo B、輸入 C。
- redo stack清空；Redo disabled；A+C保留。

### HIST-E2E-03 Clear cancel

- 建立 values+notes。
- Clear Answers→Cancel。
- board/history/dirty generation完全不變。

### HIST-E2E-04 Clear confirm and undo

- Confirm後所有 player/hint/auto values與notes移除，given保留。
- timer不歸零。
- 一次 Undo完整恢復所有 origin與notes。

## 7. Submit scenarios

### SUBMIT-E2E-01 Incomplete no conflict

- 棋盤有 N 個空格。
- Submit顯示N。
- 空格標記。
- 無直接 conflict 的非解答值不得標為 stored-solution error。

### SUBMIT-E2E-02 Multi-conflict

建立至少：

- 一組 row duplicate。
- 一組 column duplicate。
- 一組 box duplicate。
- 一格同時參與兩類。

Assertions：所有參與格被標記且flags正確。

### SUBMIT-E2E-03 Valid completion

- 填滿合法盤。
- Submit一次。

Assertions：timer停止；In Progress移除；Completed增加一筆；completion modal；重複Submit不新增第二筆。

### SUBMIT-E2E-04 Unsatisfiable but incomplete

- 無直接 duplicate，但當前盤無解。
- Submit主要狀態仍為 incomplete，另顯示 current state has no solution。
- 不標示 stored-solution-different cells。

## 8. Hint scenarios

### HINT-E2E-01 Naked Single placement

- Hint preview不改盤。
- technique/target/reason/highlight正確。
- Dismiss只增加viewed。
- 再開並Apply：填一格、origin=hint、applied+1、可一次Undo。

### HINT-E2E-02 Hidden Single

使用固定 fixture，expected target、unit、digit與support完全比對。

### HINT-E2E-03 Locked Candidates elimination with note

- preview顯示support與elimination candidate。
- Apply只移除相符user note。
- 不改formal values。
- 一次Undo恢復。

### HINT-E2E-04 Elimination without note

- preview仍可顯示。
- Apply disabled或提交後明確 no applicable player note；不得建立transaction、不得增加applied。

### HINT-E2E-05 Direct conflict

- Hint不啟動logical step。
- 顯示conflict並高亮。
- hints_viewed不增加。

### HINT-E2E-06 Unsatisfiable state

- 不使用stored solution提供答案。
- 顯示unsatisfiable。
- board/history不變。

### HINT-E2E-07 Stalled

- 支援技巧無step但搜尋有解。
- 顯示No supported logical hint is available。
- 不回傳backtracking step。

### HINT-E2E-08 Stale result

- 啟動Hint request。
- 在結果套用前透過合法測試控制改變board generation或切換game。
- stale result丟棄，不修改任何局。

## 9. Auto Solve scenarios

### SOLVE-E2E-01 Current board solvable

- Confirm後填完剩餘格。
- 新填origin=auto-solve。
- 單一 transaction。
- 不產生player completion。

### SOLVE-E2E-02 Direct conflict

- 不改盤。
- 顯示conflicts。

### SOLVE-E2E-03 Unsatisfiable current board

- 顯示Solve Original Puzzle／Cancel。
- Cancel不改盤。
- Solve Original從clues求解，玩家衝突值被替換，整體一個transaction。

### SOLVE-E2E-04 Undo auto solve

一次Undo完整恢復values、origins、notes與assisted flags。

## 10. Timer and pause scenarios

### TIME-E2E-01 Active duration

使用虛擬monotonic time：

- active 10s。
- pause 20s。
- resume 5s。
- modal 7s。
- close app 60s wall time。
- reload active 3s。

最終 active elapsed=18s。

### TIME-E2E-02 Nested pause reasons

USER_PAUSE + BLOCKING_MODAL：關modal後仍paused；Resume後才運行。

### TIME-E2E-03 Long duration

elapsed >99h不overflow，顯示至少`100:00:00`或等效明確格式。

## 11. Save／close／switch scenarios

### SAVE-E2E-01 Manual save

- dirty=true。
- Save成功。
- current/saved generation相等。
- toast可見。
- reload資料一致。

### SAVE-E2E-02 Save failure

故障注入rename或flush failure：

- dirty維持。
- old current/backup仍可讀。
- UI顯示資料安全狀態與下一步。

### CLOSE-E2E-01 Save & Exit

- dirty game。
- close→三選項。
- Save & Exit成功才離開。

### CLOSE-E2E-02 Exit Without Saving

- 修改A但上次save為B。
- Exit Without Saving。
- reload得到B，不是A。

### CLOSE-E2E-03 Cancel

- modal關閉。
- timer依原pause reasons恢復。
- app保持原game/page。

### SWITCH-E2E-01 Dirty Save

- game A dirty，選game B。
- Save→成功後載入B。
- A reload含新狀態。

### SWITCH-E2E-02 Dirty Discard

- A dirty，Discard→載入B。
- 回A時為最後saved snapshot。

### SWITCH-E2E-03 Dirty Cancel

- 保持A與所有memory state。

## 12. Library scenarios

### LIB-E2E-01 Multiple in-progress

至少建立3局不同difficulty；Library顯示唯一game ID、摘要、排序last_played desc、tie-breaker ID。

### LIB-E2E-02 Continue most recent

Continue必須載入last_played最大者，而不是created最大者。

### LIB-E2E-03 Delete non-current

確認後刪除；reload不復活；其他record不變。

### LIB-E2E-04 Reject current delete

current game delete被拒絕並說明先切換／關閉。

### LIB-E2E-05 Frosted nav

- scroll=0：p=0，blur/shadow起始值。
- scroll=60：介於起點與終點。
- scroll>=120：達上限。
- 返回0：回起點。

### LIB-E2E-06 Empty states

In Progress與Completed各自可顯示empty state；沒有假卡片。

## 13. Settings scenarios

### SET-E2E-01 Theme persistence

切Light，重啟後仍Light；semantic roles仍可讀。

### SET-E2E-02 Settings persistence isolation

- current game先Save，再做未保存board修改。
- 切換Theme並讓global save成功。
- 模擬crash/restart。
- Theme為新值，但game board仍是上次手動Save版本，未保存修改不得被global save帶入。
- unsaved draft不得因Theme保存出現在Library。

### SET-E2E-03 Reduced motion

- 狀態變化仍存在。
- modal無scale/translation。
- duration符合reduced規則。

### SET-E2E-04 Reset data wrong password

- 密碼錯誤不刪資料。
- 第二確認不出現或不可完成。

### SET-E2E-05 Reset data success

- 正確密碼+第二確認。
- games、completed、settings重設。
- 回READY_NO_GAME。
- 新vault是否保留同密碼可由實作選擇；v1.0 canonical行為為保留vault與密碼，只清payload並重設settings。

## 14. UI interaction scenarios

### UI-E2E-01 Hover/pressed/ripple

對固定button：

- 0ms normal。
- hover 75ms為中間值。
- 150ms達-3px reference。
- pointer down產生origin一致ripple。
- disabled時無ripple與command。

### UI-E2E-02 Capsule interruption

Play→Library，130ms時再→Settings；indicator從當前插值位置連續前往Settings，無跳回Play。

### UI-E2E-03 Modal capture

open與close動畫全期間，底層click不觸發。

### UI-E2E-04 Resize stress

在960×640、1280×800、1440×900間反覆1,000次：無崩潰、buffer越界、必要control遺失。

### UI-E2E-05 Focus trap

Tab在modal內循環；Escape只關可取消modal；關閉後focus回trigger。

## 15. `locstat` scenarios

### LOC-E2E-01 Canonical fixture tree

包含source/tests/docs/config/build/results/.tinyvcs、LF/CRLF/CR、無末尾換行、binary-like偽裝檔。

Assertions：category、exclude、lines、warning與stable ordering完全匹配fixture manifest。

### LOC-E2E-02 JSON errors

對每種 malformed JSON：顯示line/column、exit 3、無部分JSON報告。

### LOC-E2E-03 Ignore semantics

驗證`*`、`**`、directory slash、literal `?`、comment、spaces。

### LOC-E2E-04 Determinism

除scan timestamp/duration外，兩次報告正規化後byte-identical。

## 16. `tinyvcs` scenarios

### VCS-E2E-01 Basic history

init→add→commit A→modify→status→add→commit B→log/show。

### VCS-E2E-02 Stage snapshot

add old content後再改working file；commit必須保存staged old content，status顯示unstaged modification。

### VCS-E2E-03 Branch switch

main與feature各有不同內容；切換後working tree、HEAD、status一致。

### VCS-E2E-04 Dirty collision

tracked dirty與untracked collision兩種都在preflight完整拒絕，working tree零改動。

### VCS-E2E-05 Restore/reset

restore不動HEAD/index；reset --hard --yes同步branch/index/tree。

### VCS-E2E-06 Content dedup

不同path相同bytes只對應一個blob object ID與一份object file。

### VCS-E2E-07 Corruption

分別bit-flip envelope、compressed payload、tree object ID、delete blob；verify回5並分類問題。

### VCS-E2E-08 Interrupted update

stale temp/lock不被當正式ref/object；repository仍可讀或給明確recovery。

### VCS-E2E-09 Self-host evidence

Workstream B history至少8個實質commits、main+feature branch、verify clean。

## 17. Release scenario

### REL-E2E-01 Clean-room acceptance

從交付來源與文件開始：

1. clean build。
2. unit。
3. integration。
4. E2E。
5. failure injection。
6. batch。
7. locstat final。
8. tinyvcs verify。
9. evidence audit。
10. manual checklist。

全部exit 0且catalog無缺口才可產生完成宣告。
