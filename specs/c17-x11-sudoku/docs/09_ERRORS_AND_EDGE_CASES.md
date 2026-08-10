# 錯誤處理與邊界案例

## 1. 一般原則

- 錯誤不得造成無限迴圈、崩潰或靜默資料遺失。
- 使用者可恢復的錯誤需提供下一步。
- 內部詳細錯誤可寫入 log，但不得洩漏密碼或 plaintext。
- 顯示錯誤後，UI 必須保持可操作或安全關閉。

## 2. 視窗與圖形

必須處理：

- X display 無法開啟。
- 視窗建立失敗。
- pixel buffer allocation 失敗。
- resize 至最小尺寸。
- 快速連續 resize。
- expose event。
- 視窗關閉 protocol。
- focus lost/gained。
- 重複按鍵。
- 滑鼠在動畫中移出元件。

失敗時不得 dereference null buffer 或繼續繪製已釋放資源。

## 3. 輸入

- 未選取格時輸入數字：不得改變棋盤。
- 選取 given clue 時輸入：不得修改。
- Notes mode 對 given clue：不得修改。
- 對已填正式值格切 note：需明確忽略或要求先 erase；本版本定義為忽略並給短促 feedback。
- 超出 1–9 的按鍵：不得改變棋盤。
- 快速連點 Undo/Redo：不得越界。
- key repeat 不得產生不可控制的 modal activation。

## 4. 對話框

- modal 開啟時底層不可接收 click/key activation。
- 重複觸發同一 modal 不得堆疊無限副本。
- Escape 只能關閉可取消 modal。
- destructive modal 預設 focus 不得放在危險按鈕上。
- 關閉動畫未完成前不得觸發底層按鈕。

## 5. 遊戲建立

- 隨機來源失敗：顯示錯誤，不建立遊戲。
- 產生器達嘗試上限：可重試或取消。
- 產生結果非唯一：不得進入 Play。
- 產生結果與解盤不合法：視為內部錯誤並拒絕保存。
- game ID collision：重新產生，不覆蓋既有遊戲。

## 6. Submit

- 空棋盤：顯示 81 格未完成，不崩潰。
- 只有 notes：仍視為空白正式答案。
- 多組 duplicate：標示全部。
- 完整合法：只完成一次。
- 完成 modal 開啟時再按 Submit：不得建立重複紀錄。
- 完成後不得繼續增加 timer。

## 7. Difficulty 與 Hint

- 指定難度產生失敗：不以其他難度冒充；提供 Retry 或 Cancel。
- classification trace 與 label 不一致：視為內部錯誤並拒絕保存。
- Hint 在 direct conflict 下：不產生 deduction，改為顯示 conflicts。
- Hint 在無直接 conflict 但無解下：回報 unsatisfiable current state。
- logical solver stalled：顯示沒有支援的邏輯 Hint，不得以 backtracking 補答案。
- elimination hint 沒有對應 user note：可預覽，但 Apply 不建立空 transaction。
- 重複按 Hint：同一 busy request 只執行一次。
- Hint 計算完成前切換遊戲：結果必須綁定原 game ID；不得套用至另一局。
- Hint modal 關閉後過期結果不得再套用。

## 8. Auto Solve

- 當前直接 conflict：不覆蓋棋盤。
- 當前無直接 conflict 但無解：提示 solve original 或 cancel。
- 求解器內部超時：保留原棋盤。
- 使用者在 busy state 重複按 Auto Solve：只執行一次。
- Auto Solve 完成前切換頁面：結果套用到正確 game，不得誤寫另一局。

## 9. Undo/Redo

- 空 history：按鈕 disabled。
- 載入損毀 history：整個 record 拒絕或安全捨棄 history，不能越界讀取。
- transaction 太大：仍需原子處理。
- Clear Answers 後 Undo：完整恢復 values 與 notes。
- Auto Solve 後 Undo：完整恢復 assisted mask。
- Undo 後新輸入：redo 清空。

## 10. Timer

- 系統時間往回調整：active elapsed 不倒退。
- Pause 重複呼叫：idempotent。
- Resume 重複呼叫：不得重複啟動多個 timer。
- Modal 開啟與關閉：時間只暫停一次。
- Save 時間點跨秒：不得重複加算。
- elapsed 超過 99 小時：顯示仍正確，不 overflow。

## 11. Library

- 0 筆：empty state。
- 1 筆：無不必要 scrollbar。
- 大量紀錄：捲動仍可操作。
- 刪除目前正在玩的遊戲：必須先離開或明確確認；本版本要求拒絕並提示先關閉目前遊戲。
- 刪除完成紀錄：本版本不提供，避免誤刪；Reset Data 可全部刪除。
- duplicate game ID：載入時視為資料錯誤，不得合併猜測。

## 12. 儲存

- 無寫入權限。
- 磁碟空間不足。
- temporary file 建立失敗。
- flush 失敗。
- rename 失敗。
- current vault 截斷。
- backup 截斷。
- authentication failure。
- 未知格式版本。

任何保存失敗都不得把 dirty state 標成 clean。

## 13. Close flow

- Save & Exit 保存失敗：留在應用程式。
- 連續點擊 Save & Exit：不得重複寫入或 double free。
- Exit Without Saving：保留上一份有效存檔。
- Cancel：完整回到原狀態。
- window manager 再次送 close event：modal 已開啟時不得建立第二個 modal。

## 14. tinyvcs

- 在非 repository 執行命令。
- repository metadata 權限不足。
- object missing/corrupt。
- index corrupt。
- branch ref corrupt。
- invalid branch name。
- path traversal，例如 `../outside`。
- symlink 指向 repository 外。
- file 在掃描期間改變。
- commit message 太長。
- disk full during object write。

不得讓版本控制操作寫入 repository root 之外。

## 15. locstat

- root 是檔案而非目錄。
- 深層目錄。
- symlink loop。
- filename 非 UTF-8。
- 巨大單行檔。
- binary file 偽裝成 `.c`。
- CR-only line endings。
- 無權限目錄。
- config category overlap。

category overlap 必須採固定優先順序並在報告中標示。

## 16. 資源限制

所有模組需檢查：

- allocation failure。
- integer overflow。
- size multiplication overflow。
- file length 超過可表示範圍。
- recursion depth。
- collection growth 上限。

求解器與 parser 不得因惡意或損毀輸入造成無限 recursion。

## 17. 錯誤訊息品質

錯誤訊息至少回答：

- 哪個操作失敗。
- 大致原因。
- 資料是否仍安全。
- 使用者能做什麼。

不得只顯示 `Error`、`Failed` 或數字代碼。


## 18. Canonical limits與拒絕語意

所有固定上限見`19_CANONICAL_FORMATS_AND_LIMITS.md`。超限時：

- 在配置前拒絕。
- 不截斷檔案、message、record、history或path後繼續。
- 顯示實際limit與操作類型，但不需顯示敏感內容。
- CLI使用規定exit code。
- GUI保持memory state與最後save。

## 19. Focus與頁面切換

- 離開Play或focus lost超過500ms時，加入APP_NOT_INTERACTIVE。
- 棋盤在focus lost時遮蔽，避免背景可見但timer停止。
- focus事件抖動不得重複加入pause reason；使用bit/ref semantics。
- current game不存在時focus/page不影響timer。

## 20. Close during busy

Canonical行為：

- close request設`pending_close=true`。
- 不free current request/state。
- 顯示`Finishing current operation…`或等效persistent status。
- current busy operation結束或安全失敗後，自動重新執行一般close flow。
- 第二次close只保持pending flag，不建立modal。
- 若operation有明確安全cancel且實作選擇cancel，仍必須等待worker確認停止後才close。

## 21. Parser differential edge cases

所有binary parser測試至少涵蓋：

- 每個header byte截斷。
- count乘size overflow。
- length小於、等於、大於remaining。
- duplicate ID/path/tree entry。
- nonzero reserved。
- unknown enum/version。
- trailing bytes。
- valid prefix + corrupt suffix。

不得只測單一`corrupt file`fixture。

## 22. User-visible error taxonomy

GUI至少區分：

- validation error：輸入可修正。
- operation error：操作失敗但資料仍安全。
- recovery warning：使用backup或需使用者選擇。
- fatal data error：current/backup皆不可讀，禁止覆寫。
- internal invariant error：停止該操作並保留診斷資訊。

每類使用不同semantic icon/border/text，不得只顯示相同`Error`。
