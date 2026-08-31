# Workstream A2：tinyvcs 版本控制工具

## 1. 目的

`tinyvcs` 是一個本機、單使用者、快照式版本控制系統。
它不需要 remote、push、pull、merge 或 rebase，但必須真正保存版本、分支與工作目錄狀態。

## 2. 基本模型

`tinyvcs` 必須包含：

- repository metadata directory：`.tinyvcs/`。
- working tree。
- staging index。
- content-addressed blob objects。
- tree objects。
- commit objects。
- branch references。
- HEAD reference。
- ignore rules。

不得只將每個 commit 完整複製成一個資料夾而不做內容定址去重。

## 3. 必要命令

```text
tinyvcs init
tinyvcs status
tinyvcs add <path>...
tinyvcs add --all
tinyvcs commit -m <message>
tinyvcs log
tinyvcs branch
tinyvcs branch <name>
tinyvcs switch <name>
tinyvcs restore <path>...
tinyvcs restore --source <commit> <path>...
tinyvcs reset --hard <commit>
tinyvcs show <commit>
tinyvcs verify
tinyvcs help
```

## 4. init

- 在目前目錄建立 `.tinyvcs/`。
- 建立預設分支 `main`。
- 不得覆寫既有 repository。
- 若上層目錄已有 repository，需明確拒絕巢狀初始化，或明確以文件定義其行為；本任務預設拒絕。

## 5. 內容定址物件

### 5.1 Hash

- 使用 SHA-256。
- SHA-256 必須自行實作。
- object ID 為 canonical serialized content 的 SHA-256 hex digest。
- 不得使用檔名或時間作為 object ID。

### 5.2 Blob

- blob 代表單一檔案內容。
- 相同內容必須得到相同 blob ID。
- 相同 blob 不得重複儲存多份。

### 5.3 Tree

每筆 entry 至少包含：

- entry type。
- normalized relative name。
- object ID。
- Windows read-only file flag。

entries 必須依 `19` 定義的 Windows ordinal case-insensitive canonical order序列化；case-collision必須拒絕，確保相同目錄得到相同ID。

### 5.4 Commit

至少包含：

- root tree ID。
- parent commit ID，可為空。
- author name。
- timestamp。
- commit message。
- format version。

## 6. 壓縮與完整性

### 6.1 LZSS

object payload 必須使用自行實作的簡化 LZSS：

- 4096-byte sliding window。
- greedy longest match。
- minimum match length 3。
- maximum match length 18。
- literal/match flag groups。
- deterministic output。

壓縮後若不比原始內容小，可使用 raw storage，但 header 必須標示模式。

### 6.2 CRC32

- 每個物件保存 uncompressed payload 的 CRC32。
- CRC32 必須自行實作。
- 讀取物件時必須驗證。
- `verify` 必須掃描所有 reachable objects 並回報 hash mismatch、CRC mismatch、missing object 與 malformed object。

## 7. staging

- `add` 將目前檔案內容放入 staging index。
- 後續工作目錄再改動，不得改變已 staged 的內容。
- 刪除檔案後 `add --all` 必須 stage deletion。
- `status` 必須區分：
  - untracked。
  - staged added。
  - staged modified。
  - staged deleted。
  - modified but unstaged。
  - deleted but unstaged。

## 8. commit

- 空 staging 不得建立一般 commit。
- commit message 不得為空白。
- 成功 commit 後更新目前 branch reference。
- commit 後 staging 與 HEAD 一致。
- commit 失敗不得留下半寫入 reference。
- object 寫入需先寫同目錄temporary file、`FlushFileBuffers`，再依`26`以same-volume atomic move完成。

## 9. branch 與 switch

### branch

- 列出分支時標示目前分支。
- 建立分支時指向目前 HEAD。
- 分支名稱至少允許 ASCII 字母、數字、`-`、`_`、`/`。
- 禁止空名稱、`.`、`..`、前後 `/`、連續 `//`。

### switch

- 切換分支後，工作目錄必須符合目標 commit。
- 若未提交變更會被覆蓋，必須拒絕。
- 不受影響的 untracked file 應保留。
- 若 untracked file 會被目標版本覆蓋，必須拒絕並列出檔案。
- 切換途中失敗不得留下半套工作目錄。

## 10. restore 與 reset

### restore

- 預設從 staging 還原工作目錄檔案。
- `--source <commit>` 從指定 commit 還原指定路徑。
- 不得改變 branch pointer。

### reset --hard

- 將目前 branch、staging 與 working tree 一起移至指定 commit。
- 必須使用 canonical `--yes` 旗標；缺少時拒絕，不進行互動猜測。
- 不得刪除不衝突的 untracked files。

## 11. ignore

ignore 檔名為 `.tinyignore`。
至少支援：

- 空行。
- `#` 開頭註解。
- 相對路徑。
- `*`。
- `**`。
- 結尾 `/` 目錄規則。
- 規則順序。

本版本不要求 `!` negate pattern。
`.tinyvcs/` 必須永久忽略，不能由規則取消。

## 12. log 與 show

### log

至少顯示：

- 完整 commit ID。
- 短 commit ID。
- author。
- timestamp。
- message。
- branch labels。

按 parent chain 由新到舊顯示。

### show

至少顯示：

- commit metadata。
- parent。
- changed file list。
- added/modified/deleted 狀態。

不要求完整文字 diff，但 changed file 判定必須真實比較 tree。

## 13. repository recovery

- reference 更新必須可偵測 interrupted write。
- 若 temporary object 存在，正常讀取不得誤認為正式 object。
- malformed index 必須拒絕並提供 recovery 指示。
- object 損毀不得靜默忽略。
- `verify` 必須回傳非零狀態表示 repository 不完整。

## 14. 必要測試

至少涵蓋：

- init 與重複 init。
- add/commit/status。
- staged 與 unstaged 差異。
- 檔案新增、修改、刪除。
- 相同 blob 去重。
- branch 建立與切換。
- dirty working tree 阻擋 switch。
- untracked collision 阻擋 switch。
- restore 單一檔案。
- reset 至舊 commit。
- ignore pattern。
- SHA-256 已知測試向量。
- CRC32 已知測試向量。
- LZSS round trip、空資料、隨機資料、重複資料。
- object bit flip 損毀。
- missing object。
- interrupted reference update fixture。

## 15. 自我託管使用要求

Workstream B 必須由 `tinyvcs` 管理，且最終 history 至少包含：

- `main` 分支。
- 一個非 `main` 功能分支。
- 至少 8 個具有實質內容的 commits。
- 分支切換與回復功能的實際使用證據。
- `tinyvcs verify` 成功報告。

不得為滿足數量而建立只有空白、時間戳或無意義訊息的 commits。


## 16. v1.0 Canonical repository format

`19_CANONICAL_FORMATS_AND_LIMITS.md` §6–15 固定：

- `.tinyvcs` layout。
- repository discovery。
- Unicode path、case identity、reparse point與ADS規則。
- blob/tree/commit canonical identity。
- on-disk object envelope。
- exact LZSS token format與tie-break。
- index framing。
- branch/ref/HEAD內容。
- CLI exit status、Win32 error mapping與preflight atomicity。

若本文件概念描述與 `19` 的byte layout不同，以 `19` 為準。

## 17. Atomic checkout要求

`switch`與`reset --hard`必須先完成完整preflight，再套用變更。套用中任一I/O失敗時：

- HEAD/ref不得更新。
- index不得更新。
- 已替換檔案必須從temporary backup回復。
- 若回復本身失敗，命令回傳internal/recovery error並列出受影響paths與`GetLastError()` code，不得宣稱成功。

## 18. History 實質性判定

八個最低commits中，每個commit至少符合一項：

- 新增或完成一個可辨識模組。
- 新增對應測試。
- 修正可重現缺陷。
- 完成一個明確文件／格式契約。

只有格式化、時間戳、空白、產生結果或改message不算實質commit。至少一個非main分支必須含main當時不存在的實質commit，並有切換證據。
