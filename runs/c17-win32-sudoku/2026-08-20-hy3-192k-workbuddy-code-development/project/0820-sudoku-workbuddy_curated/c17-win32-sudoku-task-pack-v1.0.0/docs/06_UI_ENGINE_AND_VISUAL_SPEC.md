# 自製 UI 引擎與視覺規格

## 1. 目標

UI 必須是實際運作的現代化桌面介面，不得只是靜態 mockup。
所有效果必須由自製軟體 renderer 與元件系統產生。

## 2. 視窗

- 預設 client area：1280×800。
- 最小尺寸：960×640。
- 可調整尺寸。
- resize 後內容重新 layout，不得只裁切固定畫面。
- 棋盤需維持正方形。
- 最小尺寸下不可遮住必要操作。
- 關閉事件須進入產品定義的保存流程。

## 3. Frame 與 event loop

UI 引擎至少具備：

- event queue handling。
- frame timing。
- monotonic animation clock。
- invalidation／dirty state。
- layout pass。
- painting pass。
- hit-testing。
- focus management。
- modal input capture。

動畫進行中必須持續 redraw。
靜止時可阻塞等待message，但不得漏掉 `WM_PAINT`、resize、DPI change或worker completion。

## 4. 像素與混色

- 使用明確定義的 RGBA 或 BGRA pixel format。
- Alpha blending 必須有測試。
- 須處理 buffer pitch。
- resize 時安全重建 buffer。
- 不得越界寫入。
- clipping region 必須套用於所有 primitive。

## 5. 基本繪圖 primitive

至少自行實作：

- filled rectangle。
- stroked rectangle。
- rounded rectangle。
- circle／disc。
- line。
- gradient。
- image/glyph blit。
- alpha mask composite。
- clipping rectangle。

圓角不得只用方形近似。

## 6. Layout

至少支援：

- 固定 spacing token。
- padding／margin。
- horizontal stack。
- vertical stack。
- overlay。
- content alignment。
- minimum/maximum size。
- proportional expansion。
- scroll viewport。

不得為每個視窗尺寸各自硬編碼一套座標。
可有設計基準尺寸，但 resize 必須經 layout 規則計算。

## 7. 元件

至少自行完成：

- Button。
- Icon button。
- Toggle／segmented control。
- Navigation capsule tabs。
- Modal dialog。
- Toast／non-blocking status message。
- Scrollable list。
- List card。
- Sudoku cell/grid。
- Numeric keypad。
- Text/password input。
- Scrollbar indicator。
- Empty state。

每個互動元件至少具有：

- normal。
- hover。
- pressed。
- focused。
- disabled，若適用。

## 8. 按鈕 hover elevation

滑鼠進入按鈕時：

- 按鈕視覺位置向上移動 2–4 px。
- 陰影高度與透明度增加。
- 過渡 120–180 ms。
- 滑鼠離開後平滑回復。
- 不得改變 layout 造成周邊元件跳動。

## 9. 點擊 ripple

- pointer down 位置為 ripple 起點。
- ripple 半徑擴張至覆蓋按鈕。
- opacity 先出現後衰減。
- 動畫 280–450 ms。
- ripple 必須裁切於按鈕圓角形狀內。
- 快速連點允許多個 ripple 同時存在，或明確合併；不得導致記憶體無限增長。
- disabled button 不產生 ripple。

## 10. Border glow

hover 或 keyboard focus 時：

- 邊框外側出現柔和光暈。
- 光暈需有空間衰減，不得只是較粗實線。
- pressed state 可短暫提高 glow intensity。
- modal primary action 與危險操作使用不同語意強度，但不得只依顏色傳達 disabled/error。

## 11. 膠囊滑動標籤

Play、Library、Settings 導覽：

- 具有共同 capsule container。
- active indicator 在標籤之間平滑位移與縮放。
- indicator 不得瞬間切換。
- 動畫期間文字仍可讀。
- 快速切換時從目前插值位置轉向新目標，不得跳回上一動畫起點。
- active page 必須與指示器一致。

建議過渡：220–320 ms cubic Bézier。

## 12. 動態收合

導覽列在內容捲動時：

- 可縮小垂直 padding。
- 次要標題或 decorative element 可淡出。
- 主導航操作必須保留。
- 收合程度以 scroll distance 連續映射，不得只有兩個突變狀態。
- 往上捲動時平滑展開。

## 13. Modal 開啟與關閉

### 開啟

- backdrop opacity 由 0 漸增。
- 背景 blur 由 0 漸增。
- modal scale 由約 0.92–0.96 到 1.0。
- modal opacity 由 0 到 1。
- 使用具有輕微彈性的 cubic Bézier，不得嚴重 overshoot。

### 關閉

- 反向 opacity 與 scale transition。
- 關閉動畫期間 backdrop 仍攔截底層輸入。
- 動畫結束後才釋放 modal state。

建議時長：

- open 220–320 ms。
- close 160–240 ms。

## 14. 背景模糊

Modal 開啟時：

- 背景內容逐步變暗。
- 背景內容逐步模糊。
- 必須模糊應用程式自己已繪製的背景 frame。
- 不要求模糊其他桌面視窗。
- 不得僅覆蓋半透明深色矩形並聲稱是 blur。

至少實作 separable box blur、多次 box blur 近似 Gaussian，或等效可驗證演算法。
blur radius 必須隨 animation progress 改變。

## 15. Dynamic Frosted Glass Nav

在 Library 頁面：

- 導覽列背景取自其後方已渲染內容。
- scroll offset 由 0 增加時，blur radius 平滑增加至上限。
- tint opacity 可同步增加。
- bottom shadow opacity 與 blur 由無到有。
- scroll 回頂端後效果平滑歸零。
- 需保證導覽文字對比。

驗收不得以固定 blur、固定 shadow 配合內容移動冒充動態效果。

## 16. Easing

需自行實作 cubic Bézier evaluator。
至少支援：

- linear。
- ease-out。
- ease-in-out。
- emphasized／spring-like preset。

必須處理 time progress 0–1。
若 cubic Bézier x 軸非線性，需正確求取對應 y，不得直接以 t 當 x。

## 17. Reduced Motion

Settings 的 Reduced 模式：

- 保留狀態變化，但大幅縮短或取消位移／縮放。
- ripple 可改成短 opacity feedback。
- blur 可保留但縮短過渡。
- 不得讓功能失效。

## 18. 主題

至少支援 Dark 與 Light。

兩個主題都必須：

- 文字可讀。
- given clue、player entry、notes、conflict、selection 可區分。
- disabled 狀態可辨識。
- modal backdrop 有足夠區隔。
- frosted nav 仍可見。

不得只反轉整張畫面顏色。

## 19. Sudoku 視覺細節

- 9×9 cell 尺寸一致。
- 3×3 宮分隔線明顯但不壓過內容。
- given clue 字重或色彩與 player entry 不同。
- notes 使用固定 3×3 子位置。
- 選取格、peer cells、same-number cells 使用不同層級高亮。
- conflict 不得只靠紅色；需搭配 border、icon、pattern 或文字提示之一。
- assisted digits 必須與 player digits 可區分，但不影響可讀性。

## 20. 鍵盤焦點

- Tab 或方向鍵可到達主要控制項，或提供明確鍵盤導航方案。
- focus ring 必須可見。
- modal 開啟時 focus 留在 modal。
- modal 關閉後 focus 回到合理控制項。
- disabled 元件不可取得 activation focus。

## 21. 動畫正確性

- 動畫以 elapsed time 計算，不得依 frame count。
- frame rate 波動不得改變總動畫時長。
- progress 必須 clamp 於 0–1。
- 視窗失焦後返回，不得一次跳過造成錯誤狀態。
- 被中斷的動畫要從目前視覺值續接。

## 22. 視覺證據

需提交實際執行程式的固定尺寸截圖與短錄影，至少涵蓋：

- Dark／Light 主題。
- Play 主畫面。
- notes 顯示。
- conflict 顯示。
- hover elevation。
- ripple 過程。
- modal open/close。
- backdrop blur。
- Library 頂端與捲動後 frosted nav。
- navigation capsule 滑動。
- resize 後 layout。

本文件只定義要看到什麼，不規定使用何種截圖或錄影工具。

## 23. Theme token 與一致性

實作必須集中定義而非散落 magic numbers：

- spacing scale：4、8、12、16、24、32、48 px，允許依 resize 比例調整但需保持階層。
- corner radius 至少包含 small、medium、large、pill。
- elevation 至少包含 rest、hover、modal。
- semantic colors 至少包含 surface、surface-raised、text-primary、text-secondary、accent、danger、success、warning、focus。
- animation duration 至少包含 fast、normal、slow。

相同語意元件應使用相同 token。不得每個按鈕任意使用不同圓角、陰影或時間。

## 24. Hint 視覺

Hint 預覽必須：

- 保持棋盤可見。
- 清楚區分 target、support、elimination 與 related unit。
- 在側邊 panel 或 modal 顯示 technique、reason 與 Apply/Dismiss。
- elimination candidate 需以刪除線、叉號或明確 marker 呈現，不得只改淡色。
- Apply Hint 為 assisted action，視覺語意不得與 Submit 混淆。
- Reduced Motion 下仍要完整顯示所有推理角色。

## 25. 固定視覺驗收場景

Golden scene 使用固定 1280×800 client area、固定 test font asset、固定 seed 與固定時間。至少建立：

- `play_easy_dark`。
- `play_hard_notes_light`。
- `hint_hidden_single`。
- `hint_locked_candidates`。
- `submit_multi_conflict`。
- `library_top`。
- `library_scrolled`。
- `close_unsaved_modal`。
- `minimum_size_play`。

Golden image 可採像素差異或結構化 probe 驗證；若採像素差異，需允許小幅平台像素差，但不得放寬到遺漏元件仍通過。

## 26. 視覺品質最低門檻

- 主要內容不得互相重疊、裁切或超出視窗。
- 文字不得貼邊；互動區與視覺邊界需一致。
- 至少有清楚的主、次、危險 action hierarchy。
- 同頁不得出現三種以上互相矛盾的按鈕風格。
- 列表長文字須截斷或換行，不能覆蓋相鄰欄位。
- disabled、hover、pressed、focused 必須可從實際畫面區分。
- 視覺效果不能犧牲可讀性；blur/glow 不得使文字失焦。


## 27. v1.0 Reference contract

`20_UI_REFERENCE_CONTRACT.md`固定reference layout、palette、typography、button/ripple/glow數值、modal時間、blur半徑、dynamic nav mapping、focus與visual tolerance。

本文件定義效果種類；`20`定義正式驗收數值。發生差異時以`20`為準。

## 28. Renderer pipeline

每個frame canonical順序：

1. 處理已排隊input/state commands。
2. 更新monotonic animations。
3. 計算layout。
4. 建立或清除offscreen pixel buffer。
5. 繪製page background/content。
6. 若需要frosted region，從已繪背景取樣並blur/tint。
7. 繪製foreground nav/content。
8. 若modal存在，保存background、執行progressive blur/dim，再繪modal。
9. 繪製toast/focus/debug probe overlay；production不得顯示debug overlay。
10. 於`WM_PAINT`經GDI framebuffer blit呈現完整buffer；GDI不得繪製UI primitive或文字。

任何primitive都必須接受clip rectangle；nested clips取intersection。

## 29. Input dispatch

- pointer hit-test依z-order由上到下。
- pointer down鎖定pressed target；pointer up只有仍在target或元件明確允許capture時才activate。
- modal/backdrop優先於所有page element。
- keyboard command經focus manager與command dispatch，不得直接繞過disabled/busy/state-machine規則。
- key repeat對digits/arrow可用；對Submit、Delete、New Game、Save & Exit等one-shot command必須抑制。

## 30. Memory and bounds

- pixel buffer width×height×bytes-per-pixel配置前檢查乘法overflow。
- blur temporary buffer失敗時不得退化成假blur並宣稱成功；應顯示可恢復錯誤或使用已測試的低記憶體等效blur。
- active animation/ripple instances有固定上限，見`19`。
- list virtualization非必要，但不得materialize超過上限後越界；大量records可分頁式繪製可視項。

## 31. Text input

- password field支援可列印ASCII、Backspace/Delete、左右移動、Home/End。
- 不要求system clipboard；若實作paste，仍須經Unicode clipboard API、套用8–64 byte與字元限制，且不得提供password明文copy。
- 不得顯示或log實際password。
- commit/config文字輸入屬CLI，不經UI engine。

## 32. Screenshot與probe一致性

固定scene的semantic IDs、bounds、text/state probe必須與同frame screenshot一致。不得先輸出理想probe再繪製不同畫面。Scene建立、時間、seed與尺寸可由test mode固定，但不得更改production component code path。

## 33. Windows native integration

- Product只有一個top-level HWND；所有client widgets皆為自製semantic element。
- `WM_PAINT`只blit完整framebuffer。
- `WM_SIZE`、`WM_DPICHANGED`與`WM_GETMINMAXINFO`依`26`處理。
- Pointer capture遺失、Alt+F4、minimize/restore、occlusion與DPI切換不得破壞pressed/focus/modal/animation狀態。
- 96 DPI golden scene使用精確client pixel size；其他DPI使用DIP layout。
- 不得使用DWM/Mica/Acrylic/GDI alpha或native shadow冒充application blur/glow。
