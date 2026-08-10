# 人工驗收 Checklist

## A. 建置與啟動

- [ ] 所有必要執行檔可由來源建置。
- [ ] 數獨程式建立真正Unicode Win32 top-level window，且client area沒有native child controls。
- [ ] 無網路也可啟動與使用。
- [ ] 預設視窗約 1280×800。
- [ ] resize 至 960×640 DIP 仍能操作。
- [ ] 125%、150%、200% DPI不裁切必要控制項，hit-test與畫面一致。
- [ ] minimize/restore與遮蔽後畫面可完整重繪。

## B. Vault

- [ ] 首次啟動要求建立密碼。
- [ ] 兩次密碼不同會拒絕。
- [ ] 密碼未以明文顯示。
- [ ] 重啟後要求解鎖。
- [ ] 錯誤密碼不會載入資料。
- [ ] 存檔中找不到可直接閱讀的棋盤明文。
- [ ] 修改 ciphertext byte 後載入失敗。

## C. 導覽與視覺

- [ ] Play/Library/Settings 可切換。
- [ ] active capsule 平滑滑動。
- [ ] 快速切換不跳回舊起點。
- [ ] 按鈕 hover 有升起與陰影。
- [ ] 點擊有由指標位置擴張的 ripple。
- [ ] focus/hover 有柔和 glow。
- [ ] modal 開啟有 scale+opacity。
- [ ] 背景實際逐步模糊並變暗。
- [ ] modal 關閉期間底層不能點擊。
- [ ] Reduced Motion 不影響功能。
- [ ] Dark/Light 均可讀。

## D. New Game

- [ ] 可選 Easy、Medium、Hard。
- [ ] 首次預設 Easy，之後記住上次選擇。

- [ ] New Game 會產生題目。
- [ ] 題目是 9×9、3×3 宮清楚。
- [ ] clues 不可修改。
- [ ] 不同建立操作可得到不同題目。
- [ ] 題目產生期間有 busy feedback。
- [ ] 產生器報告證明每難度 50 題皆唯一解且分類正確。

## E. 棋盤輸入

- [ ] 點擊格子會選取。
- [ ] 同列、同欄、同宮有次要高亮。
- [ ] 相同數字有高亮。
- [ ] 1–9 可輸入正式值。
- [ ] Erase 可清除。
- [ ] given clue 不可修改。
- [ ] 鍵盤方向鍵可移動。
- [ ] Backspace/Delete 可清除。

## F. Notes

- [ ] Notes mode 可切換。
- [ ] 候選數以 3×3 位置顯示。
- [ ] 同數字再次輸入會移除 note。
- [ ] notes 不參與完成判定。
- [ ] 正式輸入會清除同格 notes。
- [ ] notes 可 Undo/Redo。

## G. Undo/Redo/Clear

- [ ] Undo 還原正式輸入。
- [ ] Redo 重做。
- [ ] Undo 後新操作會清空 redo。
- [ ] Clear Answers 有確認。
- [ ] Cancel 不改變棋盤。
- [ ] Confirm 清除全部 player entries 與 notes。
- [ ] given clues 保留。
- [ ] Clear Answers 一次 Undo 可完整恢復。

## H. Submit

- [ ] 未填滿時顯示空格數。
- [ ] 未填滿不會把無衝突但與答案不同的格標錯。
- [ ] row duplicate 的所有格都標示。
- [ ] column duplicate 的所有格都標示。
- [ ] box duplicate 的所有格都標示。
- [ ] 同格可顯示多類 conflict。
- [ ] 完整合法棋盤才成功。
- [ ] 完成後 timer 停止。
- [ ] 完成紀錄只建立一次。

## I. Hint

- [ ] Hint 在 preview 階段不修改棋盤。
- [ ] 顯示 technique、target、support、digit/candidates 與可驗證理由。
- [ ] Hidden Single fixture 的說明與高亮正確。
- [ ] Locked Candidates fixture 的 elimination 與 support 正確。
- [ ] Dismiss 後棋盤、notes、history 不變。
- [ ] Apply placement 後標示 hint-assisted。
- [ ] Apply Hint 可一次 Undo。
- [ ] peer-note removal 與設定、Undo 一致。
- [ ] direct conflict 時不提供 Hint。
- [ ] unsatisfiable state 不從 stored solution 猜測提示。
- [ ] logical stalled 顯示無支援 Hint。
- [ ] hints viewed/applied 在 Save/Load/Completed 正確。

## J. Auto Solve

- [ ] 可求解目前一致棋盤。
- [ ] 直接 conflict 時不覆寫。
- [ ] 無直接 conflict 但無解時提示。
- [ ] 可選擇 Solve Original Puzzle。
- [ ] Auto Solve 結果標示 assisted。
- [ ] assisted 不算 player completion。
- [ ] Auto Solve 可一次 Undo。

## K. Timer/Pause

- [ ] 題目可操作後 timer 開始。
- [ ] Pause 後 timer 停止。
- [ ] Pause 時棋盤被遮蔽。
- [ ] Resume 後從原時間繼續。
- [ ] modal 開啟期間 timer 暫停。
- [ ] 重啟載入後 elapsed 保留。

## L. Save/Close

- [ ] Save 真正保存棋盤與 notes。
- [ ] Save 保存 Undo/Redo。
- [ ] 重啟後 Continue 正確。
- [ ] 有未保存變更時 close 顯示三選項。
- [ ] Save & Exit 成功後關閉。
- [ ] Exit Without Saving 保留舊存檔。
- [ ] Cancel 返回原遊戲。
- [ ] 保存失敗不會關閉或標示成功。

## M. 多個未完成遊戲

- [ ] 可保存至少兩個未完成遊戲。
- [ ] Continue 開啟最近一局。
- [ ] Library 可選其他局。
- [ ] 切換遊戲前處理未保存變更。
- [ ] 刪除有確認。
- [ ] 刪除後重啟不會復活。

## N. Library

- [ ] 分成 In Progress 與 Completed。
- [ ] unfinished item 顯示必要摘要。
- [ ] completed item 顯示時間與 assisted 狀態。
- [ ] Completed 詳細畫面唯讀。
- [ ] 空清單有 empty state。
- [ ] 長清單可捲動。
- [ ] scroll=0 導覽 blur/shadow 接近零。
- [ ] 往下捲動後 blur/shadow 平滑增加。

## O. Settings

- [ ] Dark/Light 保存。
- [ ] Auto-remove peer notes 預設 On 且保存。
- [ ] Reduced Motion 保存。
- [ ] Reset Data 要求密碼。
- [ ] Reset Data 有第二次危險確認。
- [ ] Reset 後所有 game/record 消失。

## P. locstat

- [ ] 可掃描 fixture tree。
- [ ] `.tinyvcs`、build、results、log 被排除。
- [ ] Markdown 文件被計入 docs。
- [ ] C source/test 正確分類。
- [ ] LF/CRLF 正確。
- [ ] 字串內 `//` 不被誤判。
- [ ] JSON 報告可解析。
- [ ] 最終文件總行數已輸出。

## Q. tinyvcs

- [ ] init 建立 main。
- [ ] status 顯示 untracked/staged/unstaged。
- [ ] add 後再修改可區分。
- [ ] commit 建立真實 snapshot。
- [ ] 相同 blob 去重。
- [ ] branch 可建立。
- [ ] switch 改變工作目錄。
- [ ] dirty collision 會阻擋。
- [ ] Unicode path與case-only rename可用。
- [ ] case-collision、reparse point與ADS被安全拒絕。
- [ ] read-only flag於commit/restore後保留。
- [ ] restore 可還原單檔。
- [ ] reset 可回舊 commit。
- [ ] `.tinyignore` 生效。
- [ ] bit flip 後 verify 失敗。
- [ ] 最終 Workstream B history 至少 8 個實質 commits。

## R. 測試與證據

- [ ] Unit test 有明確 assertions。
- [ ] Integration test 跑完整跨模組流程。
- [ ] E2E以SendInput或等效OS injection操作真實Win32 HWND。
- [ ] 失敗注入有報告。
- [ ] Easy／Medium／Hard 各 50 題 batch 有統計。
- [ ] 截圖來自實際執行畫面。
- [ ] 動畫有錄影或連續幀。
- [ ] requirement-to-test matrix 完整。
- [ ] 無必要測試被 skip。
- [ ] PE import audit、DPI、Unicode path與atomic replace reports存在。

## S. 最終停止檢查

- [ ] 沒有必要功能 TODO/FIXME。
- [ ] 沒有 placeholder 或假資料。
- [ ] 沒有 forbidden library。
- [ ] 全部必要測試通過。
- [ ] tinyvcs verify 通過。
- [ ] locstat 最終報告存在。
- [ ] 未完成清單為空。


## T. Canonical format與限制

- [ ] CLI success/error exit status符合`19`。
- [ ] locstat JSON keys、file ordering與totals一致。
- [ ] tinyvcs blob/tree/commit ID可由canonical bytes重算。
- [ ] LZSS fixture可由獨立decoder round-trip。
- [ ] tree亂序、duplicate entry、trailing bytes被拒絕。
- [ ] vault header、AAD、payload framing符合`19`。
- [ ] unknown version/reserved bits/trailing bytes被拒絕。
- [ ] Undo transaction cell order與no-op規則正確。
- [ ] 超過固定上限時安全拒絕，不截斷。

## U. v1.0產品封口細節

- [ ] 新產生未Save遊戲不出現在持久化Library。
- [ ] 首次Save後才成為In Progress item。
- [ ] timer使clean game跨秒後變dirty，但不每秒增加generation。
- [ ] 離開Play與focus lost會暫停且pause reasons不互相覆蓋。
- [ ] elimination Hint無對應note時Apply disabled且不增加applied。
- [ ] Auto Solve填滿後仍可Undo，Submit後才archive。
- [ ] Clear Answers移除所有非given origin與notes。
- [ ] Reset Data保留vault密碼並建立空payload。
- [ ] Confirm Auto Solve Off不關閉其他必要確認。
- [ ] Theme等global settings立即保存，但不順便保存current dirty game。
- [ ] global save不會把unsaved draft加入Library。

## V. Release gates與一致性

- [ ] G0–G15皆有報告且PASS。
- [ ] linked dependency audit無禁止library。
- [ ] final source commit、binary hash、test results、visual evidence一致。
- [ ] canonical scenarios無缺漏。
- [ ] catalog新增FMT/UXR/ACC/REL ID全部在matrix。
- [ ] 完成報告使用`24`模板。
- [ ] known limitations不含MUST缺口。

## T. Windows Native

- [ ] executable為x64 PE32+且支援Windows 10 22H2／Windows 11。
- [ ] Process在建立HWND前啟用Per-Monitor V2 DPI awareness。
- [ ] `WM_CLOSE`、Alt+F4與title-bar close共用保存流程。
- [ ] `WM_CAPTURECHANGED`不留下stuck pressed state。
- [ ] GDI只blit完成BGRA framebuffer，沒有用於文字、shape、blur或widget。
- [ ] Password、filesystem與command line使用Unicode path API。
- [ ] `%LOCALAPPDATA%\C17Win32Sudoku\`為production default data directory。
- [ ] RNG失敗時不fallback到`rand()`或timestamp。
- [ ] Win32 write/flush/replace失敗不造成current與backup同時失效。
