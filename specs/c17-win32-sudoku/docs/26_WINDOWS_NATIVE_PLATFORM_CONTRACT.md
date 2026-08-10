# Windows Native Platform Contract

版本：1.0.0

## 1. 目的

本文件固定 Windows fork 的 native platform boundary，使不同實作者在 Win32 視窗、DPI、輸入、路徑、檔案替換與 E2E 驅動上承擔相近工程量。產品功能、Sudoku 演算法、vault 格式與 UI 視覺仍由其他文件規範。

## 2. 支援矩陣

正式支援：

- Windows 10 22H2 x64。
- Windows 11 x64。
- 96、120、144、192 DPI。
- 單一 top-level Unicode HWND。
- standard resizable overlapped non-client frame。

不要求：ARM64、Windows 8.1、UWP、MSIX、touch、pen、IME composition、native accessibility provider、system high-contrast integration 或 custom title bar。

## 3. Process 與 executable

- Production binary 為 PE32+ x86-64 executable。
- 必須含 manifest，宣告 `longPathAware=true` 與 Per-Monitor V2 DPI awareness，或在 process initialization 以官方 API 達成等效結果。
- 任何 HWND 建立前完成 DPI awareness initialization。
- Production default 不顯示 console window；CLI 與 test runner 可為 console subsystem。
- Product window class name必須穩定，例如 `C17Win32SudokuMainWindow`，實際名稱寫入 README 與 test evidence。

## 4. HWND 與 message lifetime

Canonical lifetime：

1. 初始化 runtime、DPI、clock、data directory、vault state。
2. `RegisterClassExW`。
3. 以 `CreateWindowExW` 建立唯一主要 top-level HWND。
4. 顯示 window 並進入 message／frame loop。
5. `WM_CLOSE` 只發出產品 close request；若有 dirty state，開啟自製 modal，不得立即 `DestroyWindow`。
6. 只有產品允許離開後才呼叫 `DestroyWindow`。
7. `WM_DESTROY` 只做最後資源釋放與 `PostQuitMessage`。

重複 `WM_CLOSE` 在 close modal 已存在時不得堆疊 modal 或重複保存。

## 5. Message loop 與 frame scheduling

- 不得使用永久 busy-spin message loop。
- 無動畫且無 invalidation 時，thread 應阻塞等待 message／worker completion。
- 有動畫時，frame target 以 elapsed time 驅動；可使用 waitable timer 或 `MsgWaitForMultipleObjectsEx`。
- 所有 queued input/state command 先處理，再更新 animation、layout、paint。
- `WM_PAINT` 必須驗證 update region 並呈現最新完整 framebuffer。
- `WM_ERASEBKGND` 不得造成白色 flicker；全畫面由 application framebuffer 覆蓋。
- Minimized 時停止非必要 redraw；game active timer依產品 focus/interactive規則處理，不因 frame停止而遺失時間。

## 6. Canonical pixel presentation

- Internal format：top-down BGRA8，row pitch 至少 `width*4` 且經 overflow 檢查。
- Final frame 為 opaque；每 pixel 的 RGB 已完成所有 alpha composite。
- `BITMAPINFOHEADER.biHeight` 使用負值表示 top-down DIB。
- `biCompression=BI_RGB`，`biBitCount=32`。
- Presentation 只可使用 `StretchDIBits` 或 `SetDIBitsToDevice` 類型 blit。
- 禁止 GDI text、shape、gradient、alpha、blur 或 widget drawing。
- Resize／DPI change 時先安全配置新 buffer，再交換；配置失敗保留舊 buffer並顯示安全錯誤狀態。

## 7. DPI contract

- Reference geometry 使用 DIP。
- `scale = dpi / 96.0`。
- Layout 尺寸由 DIP 轉 physical pixel 時採一致 rounding policy；同一 edge 不得由不同元件各自四捨五入造成裂縫。
- `WM_DPICHANGED`：讀取新 DPI、採用 suggested rectangle、重建 framebuffer、重新 layout、invalidate。
- `WM_GETMINMAXINFO`：確保 960×640 DIP 的最小 client area，而非整個 outer window。
- 96 DPI 的 1280×800 client screenshot 是 canonical golden scene。
- 125%、150%、200% 至少驗證：文字可讀、棋盤正方、modal不出界、主要操作可到達、pointer hit target 與視覺一致。

## 8. Pointer input

- `WM_MOUSEMOVE` 更新 hover；離開 window使用 `TrackMouseEvent(TME_LEAVE)` 或等效。
- `WM_LBUTTONDOWN` 依 z-order hit-test並 `SetCapture`。
- `WM_LBUTTONUP` 完成 activation後 `ReleaseCapture`；若 release 在 target外，依 `06` input dispatch規則取消或明確處理 capture action。
- `WM_CAPTURECHANGED` 必須清理 pressed state，不能留下卡住按鈕。
- `WM_MOUSEWHEEL` 使用 `GET_WHEEL_DELTA_WPARAM`，累積高解析 delta；每 `WHEEL_DELTA=120` 對應 48 DIP reference scroll。
- Pointer coordinate須在當前 DPI/client coordinate中一致。

## 9. Keyboard 與 focus

- `WM_KEYDOWN` 處理 navigation、shortcut、Delete、Escape、Enter。
- `WM_CHAR` 處理 printable ASCII password/input；不得以 virtual-key 假設鍵盤配置。
- Alt+F4 最終進入與 close button 相同 `WM_CLOSE` 流程。
- `WM_KILLFOCUS`／inactive 超過500ms加入 `APP_NOT_INTERACTIVE:FOCUS`；`WM_SETFOCUS`只移除此 reason。
- 自製 modal 開啟時 focus trap；不建立 native dialog HWND。
- Windows key、Alt-Tab、screen lock後返回，不得造成 stuck pressed/ripple或一次累加失焦期間 active timer。

## 10. Text input 與 secrets

- Password只接受規格允許的 printable ASCII 32–126，8–64 UTF-8 bytes；leading/trailing spaces保留。
- Backspace刪除一個已接受 character。
- Password field不得提供明文copy；paste非必要。
- Mask glyph由自有 renderer繪製。
- 暫存 password、derived key與plaintext payload在不再需要時使用不會被compiler省略的清零方法，例如 `SecureZeroMemory` 或自製 volatile wipe。

## 11. Windows path model

### 11.1 API 與編碼

- 外部 command line與filesystem path使用UTF-16。
- Canonical report／tinyvcs object path使用UTF-8與`/` separator。
- UTF-16↔UTF-8必須使用嚴格轉換 flags並驗證 round trip。

### 11.2 正規化與拒絕

- 將 relative path對repository／scan root解析後，必須仍位於root。
- 拒絕 `.`、`..`、空 component、NUL、alternate-data-stream colon、device namespace與reserved device basename。
- 內部 component末尾的space或dot拒絕，避免Win32 normalization alias。
- 內部 reparse point（symbolic link、junction、mount point等）不follow。
- Root本身若為reparse point，只可解析一次到final directory；之後以final root做containment check。

### 11.3 Case identity

- Windows canonical path identity為case-insensitive、case-preserving。
- 使用 `CompareStringOrdinal(..., TRUE)` 或語意等價的ordinal ignore-case比較，不使用user locale。
- 同一tree／index中若兩條path case-insensitive相等但bytes不同，視為collision並拒絕。
- Case-only rename在checkout時必須經temporary intermediate path完成，或以其他原子安全方式處理。

## 12. Reparse points 與特殊檔案

- `FILE_ATTRIBUTE_REPARSE_POINT` 路徑在 traversal中列為excluded/rejected。
- `.tinyvcs` metadata、object、ref、index、lock不得為reparse point。
- 只追蹤 `FILE_TYPE_DISK` regular files與directories。
- Alternate data streams、named pipes、device paths、sparse reparse semantics不在v1.0 tracked model。
- Hard link可作為一般file path讀取，但tinyvcs不保存file identity/link relationship；每個tracked path以content獨立表示。

## 13. Read-only metadata

Linux executable-bit欄位在本Windows fork改為 `file_flags`：

- bit 0：`READ_ONLY`。
- bits 1–7：保留且必須為0。
- directory entry的flags必須為0。

Checkout／restore後應依tree flag設定或清除 `FILE_ATTRIBUTE_READONLY`。其他ACL、owner、timestamps、compression、encryption、hidden/system/archive flags不屬於object identity。

## 14. Atomic file creation與replacement

### 14.1 Temporary file

- Temp file必須位於與target相同directory與volume。
- 以create-new語意建立，避免覆寫既有temp。
- 寫完後檢查所有byte count，呼叫`FlushFileBuffers`，再close handle。

### 14.2 Initial create

Target不存在時，以 `MoveFileExW(temp, target, MOVEFILE_WRITE_THROUGH)` 或等效same-volume atomic rename完成；若target競爭出現，操作失敗且不得覆寫未知檔案。

### 14.3 Replace with backup

Target存在時，使用 `ReplaceFileW(target, temp, backup, REPLACEFILE_WRITE_THROUGH, ...)` 或經測試證明等效的same-volume sequence。成功前不得更新memory `saved_generation` 或ref。

### 14.4 Failure semantics

任何write、flush、close、replace、attribute或rollback failure：

- 回傳失敗。
- 不得宣稱save/commit成功。
- 舊current或backup至少一份維持可驗證。
- temp／recovery path在report中列出。
- failure injection必須覆蓋每個stage。

Windows沒有一般directory fsync契約；本題以same-volume atomic replace、`WRITE_THROUGH`、handle flush及current/backup故障測試作為完成標準。

## 15. Locking

- Lock file以 `CreateFileW(..., CREATE_NEW, share=0)` 建立。
- Lock內容含process ID、UTC timestamp與operation，但不得含secret。
- 既有lock預設視為busy；stale lock recovery必須明確命令或確認，不得自動刪除仍可能有效的lock。
- 正常／失敗離開都應移除自己建立的lock；移除失敗需報告。

## 16. Secure random boundary

- 唯一允許的OS crypto function是`BCryptGenRandom` system-preferred RNG。
- 不得呼叫BCrypt SHA、HMAC、PBKDF2、ChaCha、Poly1305或AEAD provider；這些皆須依規格自行實作。
- RNG呼叫失敗必須使game ID、nonce、salt生成操作失敗，不得fallback到弱亂數。

## 17. Default data directory

Production default：

```text
%LOCALAPPDATA%\C17Win32Sudoku\
```

- `%LOCALAPPDATA%`缺失或無法建立時，啟動顯示可恢復錯誤並不得fallback到current working directory。
- Vault current、backup、lock與diagnostic corrupt copy位於同一app data directory。
- Test mode可指定isolated data directory；production default不得接受任意未揭露environment override。

## 18. Win32 E2E boundary

- E2E必須啟動正式Win32 product executable並取得實際top-level HWND。
- Pointer／keyboard activation必須經 `SendInput` 或等效OS input injection到正式hit-test/focus/dispatch path。
- 可直接發送 `WM_CLOSE`、resize、DPI等window-system lifecycle event，但不得直接發送自訂command message或呼叫內部handler。
- Semantic probe可透過test-only named pipe、memory-mapped file或明確IPC讀取；不得用probe改state或activatecommand。
- 每個失敗scenario保留最後screenshot、probe與Win32 error code。

## 19. Windows-specific mandatory scenarios

至少包含：

- `WIN-E2E-01`：96→144 DPI change，layout／hit-test／buffer重建正確。
- `WIN-E2E-02`：minimize、restore、occlude後 `WM_PAINT` 仍顯示完整frame。
- `WIN-E2E-03`：pointer capture中拖出window再release，pressed state清除且不誤activate。
- `WIN-E2E-04`：Alt+F4與title-bar close走相同Save/Discard/Cancel流程。
- `WIN-E2E-05`：Unicode root path下locstat、tinyvcs、vault/test data均可運作。
- `WIN-E2E-06`：case-collision、case-only rename、reparse point與ADS path按契約處理。
- `WIN-E2E-07`：ReplaceFile／MoveFile／FlushFileBuffers failure injection維持old/backup一致性。

## 20. 平台完成條件

只有在：

- import audit符合allowlist。
- 無native child controls與高階renderer。
- 96/125/150/200% DPI驗證通過。
- Unicode path、case、reparse、atomic replacement scenarios通過。
- 真實Win32 E2E通過。

Windows native platform requirement才算PASS。
