# UI Reference Contract：固定視覺語言與互動幾何

版本：1.0.0

## 1. 目的

本文件不是要求逐像素複製設計稿，而是固定足以讓不同實作者產生同一類現代桌面產品的視覺語言、幾何、狀態與驗收閾值。實作者不得以「審美自由」省略必要效果，也不得因微小差異被要求達到不可能的跨機器完全像素一致。

## 2. 設計方向

- 視覺風格：乾淨、低雜訊、圓角、分層表面、克制的發光與模糊。
- 禁止擬物遊戲素材、像素風、終端風、網頁瀏覽器外觀或 OS 原生 widget 拼裝感。
- 主要資訊永遠優先於裝飾效果。
- 所有效果必須由同一套 token、renderer 與 component state 產生。

## 3. Canonical client sizes

- Reference：1280×800。
- Minimum：960×640。
- Additional acceptance：1440×900。
- 低於 minimum 的 resize request 可 clamp 或拒絕，但不得進入不可操作狀態。
- client area 不含 window manager decoration。

## 4. Reference layout：1280×800

### 4.1 Top navigation

- x=32、y=20、height=52。
- 左側 app mark/title 區寬 220。
- 中央 capsule tabs 寬 360、高 40。
- 右側狀態／theme 快捷區最大寬 220。
- nav 與 client 左右至少 32 px margin。

### 4.2 Play content

- content top=92。
- 棋盤 reference size=612×612。
- 棋盤位於 x=64、y=112。
- 右側 control panel x=724、寬 492。
- panel 內至少包含 timer/status、number pad、action groups。
- 棋盤和 panel 間距至少 40。

### 4.3 Minimum layout

在 960×640：

- nav 左右 margin 可降至 20。
- 棋盤最大 500×500，最小 450×450。
- control panel 可縮為 390 寬。
- 次要說明可截斷，但所有必要按鈕仍須可到達。
- 若高度不足，control panel 可成為內部 scroll viewport；棋盤本身不得垂直裁切。

### 4.4 Library／Settings

- content viewport 起始於 nav 下方。
- list 最大內容寬 1120，水平置中。
- card 高度 112–144，依內容而定。
- 每張 card 的主要 action 位於右側，不得覆蓋摘要。

## 5. Fixed spacing tokens

Reference pixel tokens：

| Token | px |
|---|---:|
| `space-1` | 4 |
| `space-2` | 8 |
| `space-3` | 12 |
| `space-4` | 16 |
| `space-5` | 24 |
| `space-6` | 32 |
| `space-7` | 48 |
| `space-8` | 64 |

只能使用 token 或由 layout 明確計算出的值；不得在同類元件散落近似 magic numbers。

## 6. Radius tokens

| Token | px | 用途 |
|---|---:|---|
| `radius-small` | 8 | 小輸入、tag |
| `radius-medium` | 12 | 一般 button/card |
| `radius-large` | 20 | modal、主 panel |
| `radius-pill` | height/2 | capsule/toggle |

Sudoku 外框 radius=18；cell 本身不逐格圓角，避免破壞網格。

## 7. Typography contract

實作者自行提供 alpha-mask bitmap glyph atlas，至少包含 ASCII 32–126。正式 UI 字級：

| Role | Reference height | Weight treatment |
|---|---:|---|
| display | 32 px | semibold/bold mask |
| title | 24 px | semibold |
| heading | 18 px | semibold |
| body | 15 px | regular |
| label | 13 px | medium |
| caption | 11 px | regular |
| Sudoku given/player | 30–34 px | given semibold, player regular |
| Sudoku note | 10–12 px | regular |

- baseline 與 line-height 必須一致。
- body line-height 1.4–1.6 倍 glyph height。
- 不得以每個字元獨立不規則縮放造成抖動。
- password field 顯示 bullet 或 `*`，不得顯示明文。

## 8. Dark theme palette

Reference sRGB：

| Semantic token | Hex |
|---|---|
| background | `#0B1020` |
| surface | `#131A2A` |
| surface-raised | `#1A2336` |
| surface-hover | `#222D43` |
| border | `#2D3952` |
| text-primary | `#F4F7FF` |
| text-secondary | `#AAB5CA` |
| text-muted | `#77839A` |
| accent | `#7C6CFF` |
| accent-strong | `#9B8CFF` |
| success | `#39D98A` |
| warning | `#F5B942` |
| danger | `#FF5C74` |
| focus | `#B7AEFF` |

## 9. Light theme palette

| Semantic token | Hex |
|---|---|
| background | `#F4F6FB` |
| surface | `#FFFFFF` |
| surface-raised | `#F9FAFF` |
| surface-hover | `#EEF1FA` |
| border | `#D5DBE8` |
| text-primary | `#172033` |
| text-secondary | `#526079` |
| text-muted | `#7B879D` |
| accent | `#6254E8` |
| accent-strong | `#4F42CF` |
| success | `#168A58` |
| warning | `#A96B00` |
| danger | `#C9344F` |
| focus | `#6254E8` |

可在 ±8 sRGB units 內微調，但 semantic contrast、角色與一致性必須保持。

## 10. Contrast and non-color cues

- body text 對主要背景 contrast ratio 目標至少 4.5:1。
- large text 與 icon 目標至少 3:1。
- disabled 不得只降低到不可讀；最低 2.5:1，並搭配 opacity/interaction state。
- conflict 除 danger 色外，必須有 2 px border 或角落 marker。
- Hint elimination 必須有 strike/cross marker。
- focus 必須有可見 ring，不得只改文字顏色。

## 11. Buttons

### 11.1 Geometry

- primary/secondary button 高 44。
- compact button 高 36。
- horizontal padding 16–20。
- minimum hit target 36×36；主要 action 44×44。
- icon 與 label gap=8。

### 11.2 State precedence

視覺優先順序：disabled > pressed > focused > hover > normal。

Focus 與 hover 可同時存在，但 pressed feedback 必須最明顯。

### 11.3 Hover elevation

- translation Y：0 → -3 px。
- shadow y：2 → 7 px。
- shadow blur：8 → 18 px。
- transition：150 ms，Bezier `ease-out`。
- 元件 layout box 不移動；只改 render transform。

### 11.4 Pressed

- translation Y 回到 0 或 +1 px。
- scale 0.985–0.995。
- pressed transition 70 ms。

### 11.5 Ripple

- pointer-down local coordinate 為 origin。
- initial radius 0；end radius = origin 到四角最遠距離 + 2 px。
- duration 360 ms。
- opacity 0 → 0.24 at 12% → 0 at 100%。
- radius 使用 ease-out；opacity 使用分段線性或等效。
- 每個 ripple 完成即釋放；超過 64 active instances 時丟棄最舊已超過 50% 的 instance。

### 11.6 Glow

- hover glow outer radius=8，alpha 0.10–0.18。
- keyboard focus glow outer radius=10，alpha 0.22–0.32。
- danger action glow 使用 danger semantic color，但預設 focus 不得落在 destructive action。

## 12. Navigation capsule

- outer container 360×40，pill radius。
- 三個 tab equal width。
- indicator inset=3，高=34。
- indicator transition=260 ms。
- cubic Bézier control points `(0.22, 1.0, 0.36, 1.0)`。
- indicator x 與 width 均插值。
- 快速重定向以目前 computed x/width 作新起點。
- page state 在 activation 時立即更新；indicator 必須朝 active page 移動，不得暫時顯示錯頁。

## 13. Modal contract

### 13.1 Geometry

- 一般 modal width=480；內容較多可至 620。
- minimum horizontal viewport margin=24。
- padding=24 或 32。
- title/body/action spacing 使用固定 token。
- actions 右對齊；primary 最右，Cancel 在其左；destructive action不得為 Enter 預設。

### 13.2 Open animation

- duration=280 ms full motion。
- backdrop dim alpha 0 → 0.48 dark／0.32 light。
- backdrop blur radius 0 → 16 px。
- modal opacity 0 → 1。
- modal scale 0.94 → 1。
- modal Y 8 → 0 px。
- curve `(0.16, 1.0, 0.30, 1.0)`。

### 13.3 Close animation

- duration=190 ms。
- opacity 1 → 0。
- scale 1 → 0.97。
- blur/dim 反向至 0。
- close 開始後 modal controls disabled，但 backdrop 持續 capture input。

### 13.4 Reduced motion

- open/close duration=80 ms。
- 無 position/scale transform。
- opacity 與 blur 可短暫過渡。

## 14. Blur implementation contract

- blur 操作在 application-owned offscreen background 上。
- 最低算法：三次 separable box blur，近似 Gaussian。
- edge handling 採 clamp-to-edge。
- modal final effective radius=16 px reference。
- Dynamic nav radius=0–12 px。
- blur 期間 foreground modal/nav text 必須在 blur 後重新繪製，不能一起失焦。
- 測試至少對 impulse image、uniform image、edge image 驗證 symmetry、energy bounds 與 clipping。

## 15. Dynamic frosted nav mapping

Library scroll offset `s`：

- normalization `p = clamp(s / 120, 0, 1)`。
- blur radius = `12 * smoothstep(p)` px。
- tint alpha：dark 0.30 → 0.82；light 0.20 → 0.88。
- bottom shadow alpha = `0.20 * smoothstep(p)`。
- nav vertical padding 可由 20 → 10 px。
- decorative subtitle opacity 1 → 0；主要 tabs 不淡出。
- scroll 返回 0 時所有值回 reference 起點。

## 16. Sudoku board geometry

- 9×9 equal cells。
- reference board inner size=594，cell=66。
- outer frame + padding 組成 612。
- thin line=1 px；3×3 separator=3 px。
- cell hit rectangle等於 cell visual rectangle。
- selected fill、peer fill、same-number fill 依優先順序 composite：
  1. conflict overlay。
  2. selection border。
  3. same-number。
  4. peer。
  5. base surface。
- selection border=3 px accent。
- given text、player text、assisted text 使用不同 semantic token。

## 17. Notes geometry

- 每格內部劃分 3×3 slots。
- digit n 固定在 `(n-1)/3, (n-1)%3`。
- slot center 必須穩定，不隨其他 note 數量重新排列。
- elimination preview 在對應 slot 畫 strike/cross。
- note 不能接觸 cell border；內縮至少 3 px。

## 18. Number pad

- 3×3 digits 1–9。
- button reference size 64×56，gap=10。
- Notes active 時顯示明確 active capsule／indicator。
- given cell selected 時 number pad disabled，但仍可選取其他格。
- keyboard與 pointer activation 共用同一 command path。

## 19. Scroll contract

- mouse wheel 每 notch reference 48 px。
- high-resolution scroll 可累積 fractional delta。
- offset clamp 0..max。
- list content不足 viewport 時 max=0，無 scrollbar。
- scrollbar thumb minimum height=36。
- drag interaction非最低必要，但若實作必須與 wheel 共用 offset。

## 20. Focus navigation

- Tab 在主要 controls 依 visual order 前進，Shift+Tab 反向。
- Sudoku board 作為一個 composite focus region；進入後 arrows 移動 cell。
- modal open 時 focus trap。
- modal close 後回到觸發 control；若 control 已不存在，回 page first primary control。
- Escape 不關閉不可取消的 Save-in-progress 或 fatal modal。

## 21. Toast and inline status

- success toast duration=2.5 seconds；滑鼠 hover 可延長但非必要。
- warning/error 若需要決策，使用 modal或persistent inline message，不使用短暫 toast。
- 同類重複 toast 合併，不建立無限 queue。
- toast 最大同時 3 個。

## 22. Busy state

- 超過 100 ms 的 operation 顯示 spinner/progress affordance。
- spinner 必須 time-based，不依 frame count。
- busy control disabled；狀態 label 顯示 `Generating…`、`Analyzing hint…`、`Solving…` 或 `Saving…`。
- 未知進度不得顯示假百分比。
- resize/expose/nav rendering 在 busy 時仍運作。

## 23. Stable semantic IDs

所有主要可操作元件與驗收場景元素必須有穩定 semantic ID，至少：

- `nav.play`, `nav.library`, `nav.settings`
- `play.new_game`, `play.continue`
- `game.board`, `game.cell.rNcM`
- `game.digit.1` … `game.digit.9`
- `game.notes`, `game.undo`, `game.redo`, `game.clear`
- `game.save`, `game.submit`, `game.hint`, `game.auto_solve`
- `game.pause`, `game.timer`
- `modal.primary`, `modal.secondary`, `modal.cancel`
- `library.in_progress`, `library.completed`
- `settings.theme`, `settings.motion`, `settings.peer_notes`, `settings.reset`

ID 可用於 test probe，不必顯示給使用者。

## 24. Visual acceptance tolerances

- 元件存在、文字、semantic state、z-order、clipping 與 interaction 不允許 tolerance。
- Reference geometry位置/尺寸允許 ±4 px；minimum layout允許 ±6 px。
- palette channel允許 ±8，但 contrast 不得失敗。
- animation duration允許 ±12%。
- blur radius估計允許 ±2 px，但不得為 0 或固定不變。
- screenshot pixel diff門檻由實作者提出，不能高到遺漏整個元件仍通過。

## 25. 明確不要求

- subpixel font hinting 與完整 Unicode shaping。
- GPU acceleration。
- OS desktop transparency。
- native accessibility API integration。
- touch gestures。
- high-DPI 多螢幕完整支援；但同一 client pixel size 行為必須一致。
