# 禁止的替代實作與偷懶方式

## 1. GUI

禁止：

- GTK/Qt/SDL/Cairo/WebView 等替代自製 UI。
- 以 HTML/CSS 在瀏覽器中顯示。
- 用 ncurses 取代桌面視窗。
- 將整個頁面預先渲染成圖片。
- 用多張狀態圖片模擬按鈕與動畫。
- 只在 console 實作功能，GUI 按鈕不接行為。
- 以固定透明底色冒充 blur。
- 以較粗邊框冒充 glow。
- 以瞬間切換冒充 capsule slide。
- 只改 opacity、不做規格要求的 scale。

## 2. Sudoku

禁止：

- 只提供固定一題。
- 內嵌大量題庫後隨機挑一題，冒充 generator。
- 內嵌答案表冒充 solver。
- Submit 直接與 stored solution 比較來標示錯格。
- 題目未驗證唯一解。
- Auto Solve 只是顯示預存答案。
- Clear Answers 重新生成另一題。
- Undo 只支援最後一個格子而不支援 notes/批次操作。
- 完成紀錄使用假資料。

## 3. Storage/security

禁止：

- plaintext JSON save。
- Base64 當成加密。
- XOR encryption。
- 固定 key、固定 salt 或固定 nonce。
- password 直接作為 cipher key。
- 解密後不驗證 authentication tag。
- 保存失敗後仍標示成功。
- 每次啟動都清空資料來避開 migration/recovery。
- 將密碼寫入設定檔。

## 4. tinyvcs

禁止：

- 呼叫 Git CLI。
- 使用 `.git` 作為後端。
- 每個 commit 完整複製整個工作目錄且無內容定址。
- 只保存最新版本。
- branch 只是標籤文字，實際不能切換。
- restore 只是重新 checkout 最新檔。
- ignore 只硬編碼固定目錄。
- hash 使用檔名或時間。
- CRC/hash 驗證永遠回傳成功。
- `verify` 只檢查檔案存在。

## 5. locstat

禁止：

- 直接包裝 `wc -l` 後忽略分類與排除規則。
- 將 binary、log、results、screenshots 計入。
- CRLF 重複計數。
- 只依副檔名，不處理 tests 路徑優先。
- C 註解統計不處理字串內註解符號。
- JSON config 存在但實際不讀取。

## 6. Tests

禁止：

- 空測試。
- assertion 永遠成立。
- 所有 test expected value 由同一被測函式生成。
- 測試只印文字、不檢查結果。
- E2E 只啟動 app。
- 把 failing tests 移除或 skip 後宣稱通過。
- 用 screenshot 代替 solver/crypto/storage unit tests。
- 用 unit tests 代替所有 UI E2E。
- 只測 happy path。

## 7. 文件與交付

禁止：

- 文件聲稱功能存在，但 source 中沒有路徑。
- binary 與 source 不是同一版本。
- 截圖來自設計稿、其他 app 或後製。
- 只交付部分 source。
- 將主要實作標示為 future work。
- 將必要錯誤處理列成 known limitation。

## 8. Placeholder 判定

以下均視為未完成：

- TODO/FIXME 指向必要功能。
- 空函式。
- 固定回傳成功。
- 固定回傳測試 expected value。
- `not implemented` 路徑。
- 點擊後只顯示 toast「coming soon」。
- 假資料列表。
- 不保存的 Save 按鈕。
- 不改變狀態的 Undo。

## 9. 等效實作

實作者可採不同內部架構，但必須同時滿足：

- 外部行為相同。
- 工程難度沒有被高階依賴取代。
- 測試可證明相同要求。
- 沒有違反明確禁止事項。

若等效方案改變可驗收行為，不能自行視為等效。

## 10. Difficulty 與 Hint

禁止：

- 只依 clue 數貼 Easy／Medium／Hard 標籤。
- 產生任意題目後隨機指定難度。
- 使用搜尋節點數但不執行規格中的邏輯 technique trace。
- Hint 只顯示 stored solution 中的一格。
- 先從 stored solution 得知答案，再反向捏造 Hidden Single／Pair 說明。
- logical solver stalled 時偷偷使用 backtracking 並宣稱是人類邏輯。
- elimination Hint 宣稱移除候選，但沒有 target/support/affected candidate 證據。
- Hint Apply 建立空 transaction 或無法 Undo。
- 將所有 Hint 與 Auto Solve 合併成單一 assisted boolean，遺失原因與計數。
- Hard 題需要未列入規格的技巧或猜測才能完成。


## 11. Canonical format與CLI shortcut

禁止：

- 宣稱使用SHA-256，但object ID實際hash非canonical object bytes。
- 使用不同LZSS格式而不符合`19`，使驗收fixture無法解析。
- JSON key/file ordering不穩定並以「JSON無順序」規避deterministic report。
- parser忽略unknown version/reserved bits/trailing bytes。
- 超限資料默默截斷。
- CLI失敗仍exit 0。
- `reset --hard`缺少`--yes`仍執行。
- reparse point/path traversal寫出root。

## 12. UI shortcut

禁止：

- UI probe輸出理想bounds，但實際畫面不同。
- 用GDI `TextOut`／`DrawText`、DirectWrite或系統font rasterizer取代自有glyph renderer。
- 用Direct2D、GDI+、DirectComposition、OpenGL或其他高階renderer取代software renderer。
- 對blur只模糊固定測試圖片，正式modal不模糊live background。
- animation只在test mode存在。
- Reduced Motion直接關閉modal/input capture等功能。
- 將所有按鈕使用同一點作ripple origin。
- resize只縮放整張bitmap而不重新layout。

## 13. State與persistence shortcut

禁止：

- 把selection/hover當唯一dirty變更，卻漏掉timer/value/history。
- Save啟動即把dirty標clean，而未等待atomic write成功。
- background save成功時覆蓋其後新增變更的saved generation。
- Auto Solve後直接建立player-unassisted紀錄。
- Clear Answers只清player origin而保留Hint/Auto值。
- navigation/focus pause reason互相覆蓋，造成timer錯算。
- 遇到history損毀就靜默清空history並載入其餘資料。

## 14. Evidence shortcut

禁止：

- 同一張screenshot複製成多個scene名稱。
- 修改image metadata冒充不同source commit。
- 測試報告summary與cases不一致。
- requirement matrix列出不存在的test或path。
- 將BLOCKED/N/A計入PASS。
- 修正source後沿用舊binary的visual evidence。

## 19. Windows 原生禁止替代

下列皆視為未完成自製UI工程：

- 使用BUTTON、EDIT、LISTVIEW、TABCONTROL、TaskDialog、MessageBox等native controls/dialog完成產品互動。
- 使用DWM/Mica/Acrylic或desktop compositor blur冒充application-owned progressive blur。
- 使用GDI shape/text/alpha APIs畫正式元件。
- 使用ANSI Win32 API造成Unicode path不完整。
- 忽略Per-Monitor DPI並只在100% scaling可用。
- 以`PostMessage`自訂command或直接handler call冒充E2E使用者操作。
- 以BCrypt/CNG現成hash、KDF、cipher、MAC或AEAD取代手刻密碼學；Bcrypt只准RNG。
- 將reparse point、ADS、case collision或atomic replace問題列為「Windows限制」而不實作規格行為。
