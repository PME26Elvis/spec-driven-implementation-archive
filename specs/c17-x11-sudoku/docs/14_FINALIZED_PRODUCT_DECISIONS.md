# 已定案產品決策

版本：1.0.0 final scope freeze

本文件將先前待決項目定案。除非未來建立新的任務包 major version，實作者不得自行擴大或縮小這些決策。

## 1. 難度

- 必須提供 Easy、Medium、Hard。
- 依人類邏輯 technique trace 與固定 score 分類。
- clue 數只作 sanity range。
- 詳細規則見 `07_SUDOKU_RULES_AND_ALGORITHMS.md`。

## 2. Hint

- 必須一次提供一個可解釋步驟。
- 必須顯示 technique、target、support 與 reason。
- 預覽不修改；Apply 才修改。
- Hint 與 Auto Solve 分開記錄。

## 3. Peer-note removal

- 提供 On／Off 設定。
- 預設 On。
- 正式輸入與 note removals 為單一 undo transaction。

## 4. 錯誤模式

- 採自由輸入模式。
- 允許玩家輸入會造成 duplicate 的值，並以 conflict 標示。
- 不提供依 stored solution 計算的 Mistake Limit。
- 不在輸入當下把「與答案不同」當成錯誤。

## 5. 保存模型

- 保留手動 Save、dirty state 與關閉 Save/Discard/Cancel。
- 本版本不加入週期性 autosave 或每步 debounce autosave。
- 新遊戲首次Save前是unsaved draft，不列入已持久化Library。
- 切換遊戲、建立新遊戲與關閉視窗共用相同未保存變更決策語意。

## 6. 統計

- Library Completed item 顯示必要紀錄。
- 不新增獨立 Statistics 頁面或圖表。
- 完成紀錄保留 difficulty、elapsed、Hint/Auto Solve 使用資訊。

## 7. 聲音與語言

- 不提供聲音。
- 正式應用 UI 固定英文。
- 規格文件維持繁體中文。
- 不要求中文字型 rasterization。

## 8. Completed record 刪除

- 不提供單筆刪除 Completed record。
- Reset Application Data 可清除全部資料。
- In Progress game 可經確認刪除。

## 9. tinyvcs 邊界

- 不要求 merge、rebase、remote、push、pull、stash、tag 或完整文字 diff。
- `show` 的 changed file list 足夠。
- 不加入 Myers diff，避免 Workstream A 蓋過主應用。

## 10. 視覺驗收

- 使用固定 golden scenes、截圖與動畫證據。
- 不要求所有平台像素完全一致。
- 元件存在、layout、狀態、blur 與動畫不得因 tolerance 而被忽略。

## 11. 明確不納入

- 4×4、6×6、16×16 或不規則數獨。
- Killer、Diagonal、Samurai 等變體。
- 每日挑戰、排行榜、成就、帳號與同步。
- 多人或分享功能。
- 題庫匯入／匯出。
- PDF/圖片辨識數獨。
- 教學文章、完整解題課程或超出 T1–T8 的進階技巧。
- Windows、macOS、Wayland。


## 12. Timer互動

- 離開Play頁時active timer暫停。
- X11 focus lost超過500ms時active timer暫停且棋盤遮蔽。
- 兩者與USER_PAUSE、modal、busy、completed使用可組合pause reasons。

## 13. Confirmation setting

- Settings提供`Confirm Auto Solve` On/Off，預設On。
- Off只略過可解盤的第一層Auto Solve確認。
- Clear Answers、Delete、Reset與unsaved-change確認永遠保留。

## 14. Reset data

- Reset清除payload資料與設定。
- 保留vault與同一密碼。
- 以原子方式寫入空payload，失敗時保留舊資料。

## 15. Auto Solve archive

- Auto Solve填盤後不立即archive，以保留Undo。
- Submit時才建立`AUTO_SOLVED` Completed record。
