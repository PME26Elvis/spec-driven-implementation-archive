# 正式 Release Gate、稽核與停止條件

版本：1.0.0

## 1. Gate 原則

v1.0只有一種正式完成狀態：G0–G15全部必要gate通過。

不得以「主要功能完成」「只剩 polish」「測試大多通過」或「受限於環境」代替完成。環境問題可標示 BLOCKED，但 BLOCKED 不是 PASS。

## 2. Gate 清單

| Gate | 名稱 | 失敗效果 |
|---|---|---|
| G0 | Scope and dependency audit | 禁止進入正式驗收 |
| G1 | Clean source build | 整體 FAIL |
| G2 | Static completeness audit | 整體 FAIL |
| G3 | Unit tests | 整體 FAIL |
| G4 | Integration tests | 整體 FAIL |
| G5 | E2E scenarios | 整體 FAIL |
| G6 | Algorithm batch | 整體 FAIL |
| G7 | Failure/corruption injection | 整體 FAIL |
| G8 | Security known-answer tests | 整體 FAIL |
| G9 | Visual and animation evidence | 對應 UI requirements FAIL |
| G10 | tinyvcs repository audit | Workstream A/B FAIL |
| G11 | locstat and document audit | 交付 FAIL |
| G12 | Requirement traceability | 未對應 requirements FAIL |
| G13 | Manual acceptance | 未通過項目 FAIL |
| G14 | Final consistency audit | 禁止宣告完成 |
| G15 | Windows native platform audit | 整體 FAIL |

## 3. G0：Scope and dependency audit

必須確認：

- 來源語言為 C17；建置不得偷偷下載或 vendor 禁止函式庫。
- GUI只使用允許的Kernel32/User32/GDI/Bcrypt RNG boundary，無native child controls或禁止renderer。
- 沒有 GTK、Qt、SDL、Cairo、OpenGL、WebView、SQLite、OpenSSL、libsodium、Git backend 等。
- 沒有預置大量題庫或答案表。
- 沒有未揭露 prebuilt object/library。
- 額外功能沒有改變 v1.0 必要行為。

必要輸出：dependency audit report，列出所有 linked libraries、資產來源與用途。

## 4. G1：Clean source build

從交付來源與空的 build/output 目錄開始：

- 建立全部必要 binary。
- 不依賴先前 build artifact。
- 不依賴使用者 home 中未交付的檔案。
- 不依賴網路。
- warning policy：至少啟用常見警告；所有剩餘 warning 必須在報告逐項說明。
- build 失敗、缺檔或 binary 與來源不一致即 FAIL。

## 5. G2：Static completeness audit

掃描 source、tests、docs：

- 無指向 MUST 的 TODO/FIXME/XXX/NOT IMPLEMENTED。
- 無空函式、固定成功、未連接 UI command。
- 所有 requirement matrix 中的實作模組存在。
- 所有主要按鈕 semantic ID 有 command handler。
- 所有 parser／serializer 有版本、長度與錯誤路徑。
- test-only bypass 不會在 production default 啟用。

字串出現在禁止事項文件或測試 fixture 不應誤判；audit 需能列出人工確認例外。

## 6. G3：Unit tests

- 全部必要 unit tests pass。
- failed=0、skipped=0。
- 每個 case assertion_count >=1。
- crypto known-answer、LZSS、SHA-256、CRC32、solver、T1–T8、serializer、layout純函式均須覆蓋。
- sanitizer 非硬性依賴，但若提供，其失敗不得忽略。

最低數量不是唯一品質門檻；不接受把大量相同 trivial cases拆成數量。

## 7. G4：Integration tests

必須執行真實 filesystem、object store、vault、generator與跨模組流程。

- 不得全部替換為 mocks。
- I/O fixture 使用隔離目錄。
- 每個 suite 可重跑且順序無關。
- failed=0、skipped=0。

## 8. G5：E2E scenarios

`21_CANONICAL_ACCEPTANCE_SCENARIOS.md` 中所有 MUST scenarios都要有結果。

每個 scenario：

- status=PASS。
- 有步驟 assertions。
- 對應實際 UI binary。
- 需要畫面者有 screenshot path。
- 需要動畫者有 recording/frame path。
- 不得只執行內部 API。

## 9. G6：Algorithm batch

### 9.1 Sudoku generation

每難度恰好至少50個不同 seed成功樣本：

- valid complete solution rate=100%。
- puzzle direct validity=100%。
- uniqueness=100%。
- logical classification=100%。
- requested label match=100%。
- search solver round trip=100%。
- no stalled/unsupported technique in accepted output。

允許 generator retry，但報告必須包含 attempts、reject reasons與latency。

### 9.2 Crypto and compression

- 100次vault encrypt/decrypt，nonce全不同。
- 100組LZSS varied fixture round-trip。
- 100組object hash/dedup consistency。

### 9.3 UI stress

- 至少1,000次resize/layout/paint sequence。
- 至少10,000次random合法UI純狀態事件或等效state-machine fuzz。
- 無崩潰、越界、invalid state。

## 10. G7：Failure/corruption injection

全部指定故障須得到預期失敗，不得被當成功：

- vault header/ciphertext/tag truncation與bit flip。
- wrong password。
- temp write、flush、rename failure。
- backup recovery。
- tinyvcs missing/corrupt objects、index/ref corruption。
- malformed locstat config、unreadable path、oversized file。
- generator exhaustion、solver guard。
- allocation failure hooks至少覆蓋關鍵高階操作。

必要資料不遺失，dirty/ref/saved generation維持正確。

## 11. G8：Security known-answer tests

至少通過：

- SHA-256 empty、`abc`與multi-block vector。
- HMAC-SHA-256至少三個不同key/message長度。
- PBKDF2 iteration 1、2與multi-block output。
- ChaCha20 block。
- HChaCha20。
- Poly1305 one-time authenticator。
- XChaCha20-Poly1305 encrypt/decrypt vector。
- AAD tamper、nonce tamper、ciphertext tamper、tag tamper拒絕。

Expected bytes必須固定在fixture或test source，不可由production函式生成。

## 12. G9：Visual and animation evidence

必須有：

- `06`與`20`指定的golden scenes。
- full motion與reduced motion。
- Dark/Light。
- 1280×800、960×640、1440×900代表場景。
- 動畫中間幀，不只起終點。
- blur輸入與輸出證據。
- UI probe與screenshot scene名稱一致。

人工檢查視覺是否符合品質門檻；自動probe只輔助幾何與狀態。

## 13. G10：tinyvcs repository audit

- `tinyvcs verify` exit 0。
- corrupt=missing=malformed=0。
- 至少main與一個非main分支。
- 至少8個實質commit。
- commit message可理解且非湊數。
- Workstream B來源在history中演進。
- final binary/source commit可識別。
- 重複blob有實際dedup證據。
- branch switch/restore/reset有scenario證據。

## 14. G11：locstat and document audit

- `locstat`對A、B、全專案與docs產出報告。
- `results/`、screenshots、recordings、build、`.tinyvcs`不計入。
- JSON可解析，summary等於per-file加總。
- 文件manifest與實際Markdown行數一致。
- 所有文件UTF-8可讀、internal links存在。
- task implementation的文件總行數由其自己的locstat報告提供；任務包本身的行數見本包manifest。

## 15. G12：Requirement traceability

- Catalog每個ID恰有一列或多列明確mapping。
- 每個MUST至少一項有效證據。
- 演算法、安全與資料一致性不得只用manual evidence。
- UI動畫不得只用unit test。
- evidence path存在且結果PASS。
- module/test不存在或path錯誤視為缺口。

## 16. G13：Manual acceptance

使用`13_MANUAL_ACCEPTANCE_CHECKLIST.md`：

- 每項記錄PASS/FAIL/BLOCKED/N/A。
- N/A只允許規格明確標記選配。
- 每個FAIL附復現步驟。
- 不能用自動測試報告直接整表勾選而未操作UI。

## 17. G14：Final consistency audit

最後檢查：

- 所有結果檔的source commit相同。
- binary hash與完成報告一致。
- batch規則版本與save fixture版本一致。
- screenshot/recording來自final binary。
- test count summary一致。
- known limitations不含任何MUST缺口。
- changelog、README、version string一致。
- 未完成項目清單為空。

## 18. Gate 重跑規則

修正後至少重跑：

- 直接受影響的unit/integration/E2E。
- 所有依賴該模組的batch/failure tests。
- G12 traceability與G14 final audit。
- 若修改serialization、crypto、generator、difficulty或UI renderer，需重跑其完整gate，不得只跑單一case。

## 19. PASS／FAIL 宣告

正式完成宣告只能在G0–G15全部PASS後產生。

允許報告非必要 known limitations，但必須：

- 明確不屬於MUST。
- 不影響安全、資料、核心操作與驗收證據。
- 不被用來掩蓋失敗。

## 20. 停止條件

G0–G15全部PASS後，實作者應停止範圍擴張並交付。不得因自行新增聲音、跨平台、更多技巧、多人或額外工具而延後v1.0。

## 21. G15：Windows native platform audit

必須全部PASS：

- x64 PE executable與Windows 10 22H2／Windows 11啟動驗證。
- Import table符合`02` allowlist；Bcrypt只使用`BCryptGenRandom`。
- 無native child controls、GDI text/shape/alpha、Direct2D/DirectWrite/GDI+/DWM blur。
- Per-Monitor V2、96/125/150/200% DPI驗證。
- `WIN-E2E-01`至`WIN-E2E-07`全部PASS。
- Unicode path、case、reparse、ADS、read-only metadata與long-path manifest符合`26`。
- `WM_CLOSE`、Alt+F4、paint lifecycle、capture loss與atomic replacement正確。

必要輸出：PE/import audit、Windows version/toolchain report、DPI matrix、Windows scenario JSON與atomic replacement failure report。

Windows平台細節以`26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`為準。
