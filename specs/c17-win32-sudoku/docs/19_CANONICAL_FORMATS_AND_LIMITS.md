# Canonical 格式、命令契約與固定限制

版本：1.0.0

## 1. 目的

本文件固定跨實作可驗證的 binary layout、CLI exit status、排序、長度上限與 resource guard。實作者可使用不同內部資料結構，但正式輸出與持久化資料必須符合本文件。

所有 multi-byte integer 除非另有說明，一律使用 little-endian。所有長度在配置記憶體前必須先檢查上限與 overflow。

## 2. 共用數值上限

| 項目 | v1.0 固定上限 |
|---|---:|
| 單一一般檔案 | 64 MiB |
| `locstat` 掃描檔案數 | 200,000 |
| `locstat` 路徑深度 | 128 components |
| `tinyvcs` tracked files | 100,000 |
| `tinyvcs` 單一canonical UTF-8 pathname | 4096 bytes |
| Win32 absolute path | 32,767 UTF-16 code units |
| `tinyvcs` tree entries | 100,000 |
| `tinyvcs` commit message | 4096 bytes |
| `tinyvcs` author | 128 bytes |
| branch name | 255 bytes |
| vault ciphertext | 64 MiB |
| in-progress games | 1,000 |
| completed records | 100,000 |
| Undo transactions per game | 10,000 |
| changes per transaction | 512 |
| UI active ripple instances | 64 |
| UI list items materialized | 2,000 |
| JSON nesting depth | 64 |
| JSON string length | 1 MiB |

超過上限時必須安全拒絕，不能截斷後當成成功。

## 3. CLI 通用行為

Workstream A 執行檔必須：

- `--help` 回傳 0。
- 正常成功回傳 0。
- 使用者輸入或 usage error 回傳 2。
- 資料格式／repository／config 損毀回傳 3。
- I/O、權限、資源或系統錯誤回傳 4。
- 完整性驗證發現問題回傳 5。
- 未預期 internal invariant failure 回傳 70。
- 錯誤訊息寫至 stderr；正式資料輸出寫至 stdout 或指定檔案。
- 不得在 stdout 混入 debug log，使 JSON 無法解析。
- 任何失敗都不得回傳 0。
- CLI正式人類可讀輸出固定英文，避免驗收依locale變動。

## 4. `locstat` 命令契約

Canonical usage：

```text
locstat [--config PATH] [--json PATH] [--category NAME]
        [--fail-on-error] [--no-default-excludes] ROOT
```

### 4.1 Option 規則

- ROOT 恰好一個。
- 不認識的 option：usage error。
- option 缺少 value：usage error。
- `--category` 不存在：usage error。
- `--json -` 表示 JSON 寫至 stdout；此時人類可讀摘要寫至 stderr。
- 未提供 `--config` 時使用內建預設。
- 未提供 `--json` 時不建立 JSON 檔。
- 相同 option 重複時，以最後一個 scalar value 為準；boolean option 重複無額外效果。

### 4.2 Traversal

- 預設不follow reparse point／junction。
- reparse point本身列為excluded，reason=`reparse_point`。
- root本身若為reparse point，只允許以directory handle解析一次final path；掃描期間不follow內部reparse point。
- entry path先轉嚴格UTF-8、separator正規化為`/`。
- directory entries依Windows ordinal ignore-case排序；若ignore-case相等則以exact UTF-16 ordinal tie-break。
- 報告中的 per-file 順序與 traversal 順序一致。
- hidden file 不因名稱以 `.` 開頭而自動排除，除非符合預設或 config rule。

### 4.3 Text／binary 判定

- 先依 category extension 判斷是否候選文字檔。
- 讀取前 8 KiB；含 NUL byte 則標記 binary-like 並排除 line statistics。
- binary-like `.c`／`.md` 必須產生 warning，不能當作 0 行正常檔。
- UTF-8 BOM 若存在，不屬於第一行內容。
- LF、CRLF、CR 都視為單一 line terminator。

### 4.4 Physical line algorithm

- zero-byte file：0。
- 非空檔：line count = terminator count +（最後 byte 不是 terminator時的 1）。
- CRLF 只算一個 terminator。
- 最後為 CRLF、LF 或 CR 時不額外增加空行。

### 4.5 Category 優先順序

1. tests path rule。
2. 明確 config category，依 config 中 category declaration order。
3. built-in source。
4. built-in docs。
5. built-in config。
6. unclassified。

一個檔案恰好屬一個 category。若多規則匹配，報告需含 `matched_rules` 與 `selected_category`。

### 4.6 JSON config grammar

只需支援標準 JSON：object、array、string、number、boolean、null。Config root 必須是 object。

允許 keys：

- `include_extensions`: string array。
- `exclude_extensions`: string array。
- `exclude_paths`: string array。
- `categories`: object，value 為 string array。
- `max_file_bytes`: integer 1–67,108,864。
- `follow_reparse_points`: 必須為 false；true 在 v1.0 視為 unsupported config error。

未知 key 必須拒絕，避免拼字錯誤被靜默忽略。

### 4.7 Canonical JSON report

Top-level keys 依以下順序輸出：

```json
{
  "schema_version": 1,
  "tool_version": "1.0.0",
  "root": "...",
  "config_digest_sha256": "...",
  "scan_started_epoch_ms": 0,
  "scan_duration_ms": 0,
  "categories": [],
  "files": [],
  "excluded": [],
  "warnings": [],
  "errors": [],
  "totals": {}
}
```

必要 category object：

- `name`。
- `file_count`。
- `physical_lines`。
- `blank_lines`。
- `comment_only_lines`。
- `code_lines`。
- `mixed_code_comment_lines`。

非 C category 的 lexical 欄位為 0，不得省略。

必要 file object：

- `path`，相對 root，`/` 分隔。
- `category`。
- `bytes`。
- `physical_lines`。
- C lexical 欄位。
- `encoding_warning` boolean。

所有 JSON object key 順序固定；數字使用十進位整數；不得輸出 NaN/Infinity。

`config_digest_sha256`：指定config時為config檔原始bytes的SHA-256；使用內建預設時為工具內固定canonical default-config JSON bytes的SHA-256。

## 5. Ignore pattern canonical semantics

適用於 `.tinyignore` 與 `locstat` path rules：

- path先以Unicode Win32 API解析為root-relative，嚴格轉UTF-8，separator正規化為`/`。
- 不允許 `..` component。
- pattern 不以 `/` 開頭時仍從 repository／scan root 比對完整相對 path。
- `*` 匹配零個以上非 `/` bytes。
- `?` 在 v1.0 不具 wildcard 意義，視為 literal `?`。
- `**` 匹配包含 `/` 的零個以上 bytes。
- 末尾 `/` 只匹配目錄及其所有 descendants。
- `#` 只有在行第一個非空白字元時表示 comment。
- trailing spaces 視為 pattern 內容，不自動 trim；leading spaces 亦然，除非整行全空白。
- 本版本不支援 escaping 與 `!` negate。
- 規則由上到下處理；由於沒有 negate，任一 exclude 命中即排除。

## 6. `tinyvcs` repository layout

Canonical metadata：

```text
.tinyvcs/
  FORMAT
  HEAD
  index
  objects/
    aa/
      bbbbb...      # 64-hex object ID 的前兩碼為目錄
  refs/
    heads/
      main
  locks/
  tmp/
```

- `FORMAT` 內容恰為 ASCII `tinyvcs 1\n`。
- `HEAD` 內容為 `ref: refs/heads/<branch>\n`。
- ref file 內容為 64 lowercase hex + `\n`；unborn branch 為空檔。
- 不允許 detached HEAD。
- metadata file 不得是 reparse point。

## 7. `tinyvcs` Windows path model

- 外部path使用UTF-16 `*W` API；object/index/report內使用嚴格UTF-8與`/`separator。
- 只追蹤regular disk files與directories。
- reparse point、junction、device namespace、alternate data stream與非-disk file必須拒絕`add`並列出path。
- pathname不得含NUL；component不得為空、`.`、`..`、末尾space/dot、colon或Win32 reserved device basename。
- `.tinyvcs` component永久禁止追蹤。
- path identity為Windows ordinal case-insensitive、case-preserving。
- 同一tree/index中若兩條path case-insensitive相等，視為collision；不得以exact case區分兩個tracked path。
- entries使用`CompareStringOrdinal(ignoreCase=TRUE)`語意排序；不依user locale。理論tie以exact ordinal排序，但case collision會先拒絕。
- tree/index保存原始case的UTF-8名稱。
- `file_flags` bit0保存`FILE_ATTRIBUTE_READONLY`；bits1–7必須0。其他attribute、ACL、owner、timestamp與hard-link identity不進object identity。
- 詳細root containment、long path與case-only rename見`26`。

## 8. `tinyvcs` object identity

Canonical unhashed object bytes：

```text
<object-type-ascii> SP <payload-length-decimal> NUL <payload>
```

Object type 只能為：

- `blob`
- `tree`
- `commit`

Object ID = SHA-256(canonical unhashed object bytes)，顯示為 64 lowercase hex。

On-disk object envelope中被raw/LZSS保存的`uncompressed payload`只包含本object type的payload，不包含`<type> SP <length> NUL`前綴；驗證ID時必須依envelope type與解壓payload重建canonical unhashed bytes。CRC32亦計算payload bytes。

## 9. Blob payload

Blob payload 恰為檔案原始 bytes，不做換行轉換。

## 10. Tree payload

每筆 entry 依序序列化：

```text
u8 entry_type      # 1=file, 2=directory
u8 file_flags     # bit0=read-only；directory必須0；其他bits必須0
u16 name_length
name bytes
32-byte raw object id
```

- entry count 不另外寫入；由 payload length 解析至結尾。
- `name_length` 1–255。
- name 只能是一個 component，不能含 `/` 或 NUL。
- entries必須依Windows ordinal ignore-case嚴格遞增；case-collision、重複或亂序tree視為malformed。

## 11. Commit payload

Commit 使用 canonical UTF-8-like byte fields，但 author/message 只允許規定字元：

```text
u8 format_version          # 1
u8 parent_count            # 0 or 1
[parent 32 bytes]
root_tree 32 bytes
i64 timestamp_epoch_ms
u16 author_length
author bytes
u32 message_length
message bytes
```

- author：可列印 ASCII 32–126，1–128 bytes，首尾不得為空白。
- message：UTF-8，1–4096 bytes；不得全為 Unicode/ASCII whitespace。
- message 可含 newline；不得含 NUL。
- v1.0 每個 commit 至多一個 parent。

## 12. On-disk object envelope

`.tinyvcs/objects/aa/...` 內容：

```text
8 bytes magic              # "TVCSOBJ1"
u8 storage_mode            # 0=raw, 1=lzss
u8 object_type             # 1=blob, 2=tree, 3=commit
u16 reserved               # must be 0
u64 uncompressed_length
u64 stored_length
u32 crc32_uncompressed
32 bytes object_id
stored payload bytes
```

- header 所有欄位（除 magic）little-endian。
- reserved 非 0 視為 unsupported/malformed。
- 檔案總長必須恰好等於 header + stored_length。
- 解壓後長度必須等於 uncompressed_length。
- CRC、object ID 與 canonical object type 必須全部驗證。
- raw/compressed 選擇：只有 LZSS payload 嚴格小於 raw payload 才使用 mode 1。

## 13. Canonical LZSS token format

- sliding window：最近 4096 bytes。
- search candidate 起點由最接近目前位置往較舊位置掃描。
- 最長 match 優先；同長度時選最近的 match。
- minimum length 3；maximum length 18。
- 每 8 個 token 前置一個 flag byte，least-significant bit 對應第一個 token。
- flag bit 1 = literal，後接 1 byte。
- flag bit 0 = match，後接 2 bytes：
  - high 12 bits = distance - 1，範圍 0–4095。
  - low 4 bits = length - 3，範圍 0–15。
- 16-bit match word 以 little-endian 寫入。
- 最後一組不足 8 tokens，未使用 flag bits 必須為 0。
- decoder 必須拒絕 distance 超過已輸出 bytes、越過 declared length 或 trailing garbage。

## 14. Index format

`.tinyvcs/index`：

```text
8 bytes magic              # "TVCSIDX1"
u32 entry_count
entries...
u32 crc32_of_all_previous_bytes
```

Entry：

```text
u16 path_length
path bytes
u8 file_flags
u8 stage_state             # 0=present, 1=deleted
32-byte blob id            # deleted 時全 0
u64 file_size_snapshot
i64 mtime_100ns_snapshot
```

- entries依完整path的Windows ordinal ignore-case排序；case-collision禁止。
- index內容是staged snapshot；Windows file size與`FILETIME` 100ns值只作快速dirty check，最終判定仍需content hash。
- malformed index 不得部分載入。

## 15. `tinyvcs` CLI 詳細契約

### 15.1 Repository discovery

從 current working directory 往父目錄尋找 `.tinyvcs/`。到 filesystem root 停止。所有 path argument 轉為 repository-root-relative path。

### 15.2 `status`

固定 section 順序：

1. `On branch`。
2. `Staged changes`。
3. `Unstaged changes`。
4. `Untracked files`。
5. summary。

每section path依Windows ordinal ignore-case排序。Machine-readable mode非必要，但text output必須穩定可測且與user locale無關。

### 15.3 `add`

- 至少一個 path，或使用 `--all`，不可同時省略。
- directory 遞迴加入。
- ignore 命中 path 不加入；明確指定 ignored path 時顯示 warning。
- path 不存在時，只有 `--all` 或已 tracked path 可 stage deletion；其他為 usage/data error。
- 操作要麼完整更新 index，要麼完全不改 index。

### 15.4 `commit`

- `-m` 恰好一個 message。
- author 從 `TINYVCS_AUTHOR` 取得；缺少或不合法時失敗。
- timestamp 取 wall clock；test mode 可提供固定時間。
- object全部成功寫入並`FlushFileBuffers`後才以`ReplaceFileW`／`MoveFileExW`原子更新ref。
- ref lock使用`CreateFileW(..., CREATE_NEW, share=0)`；既有有效lock視為busy error。

### 15.5 `branch`

- 無名稱：列出所有分支，依ASCII branch name bytewise ascending；目前分支前加`* `。
- 有名稱：從目前 HEAD 建立；unborn HEAD 時拒絕。
- 已存在名稱拒絕。

### 15.6 `switch`

- 只接受既有 branch。
- preflight 必須先計算所有會新增、修改、刪除的 paths。
- 任何會覆蓋 dirty tracked file 或 untracked collision 時，在改動前完整拒絕。
- 實際套用使用same-directory temporary sibling files，依`26`以Win32 atomic move/replace套用；case-only rename使用temporary intermediate。失敗時回復已改動paths。
- 全部工作目錄成功後才更新 HEAD。

### 15.7 `restore`

- 預設 source = index。
- `--source COMMIT` source = commit tree。
- path 不存在於 source 時，若目前 tracked 則刪除；若從未 tracked 則拒絕。
- 不改 index、HEAD、branch ref。

### 15.8 `reset --hard`

Canonical syntax：

```text
tinyvcs reset --hard COMMIT --yes
```

- `--yes` 必須存在，避免互動差異。
- preflight 檢查 untracked collision。
- 成功後 branch ref、index、working tree 一致指向 COMMIT。

### 15.9 `verify`

必須檢查：

- FORMAT、HEAD、refs。
- index framing/CRC。
- 所有 refs reachable commit graph。
- commit/tree/blob envelope、CRC、SHA-256。
- referenced object existence/type。
- tree ordering/path rules。
- unreachable objects 只列 warning，不使 verify 失敗。
- `.tmp` 與 stale lock 列 warning。

Summary 至少包含 scanned、reachable、unreachable、corrupt、missing、malformed counts。

## 16. Vault outer format

正式 vault 檔名由實作者決定，但 current 與 backup 必須位於同一資料目錄。

Outer bytes：

```text
8 bytes magic              # "SDKVLT01"
u16 header_version         # 1
u16 kdf_id                  # 1 = PBKDF2-HMAC-SHA-256
u32 pbkdf2_iterations       # production exactly 200000
16 bytes salt
u16 cipher_id               # 1 = XChaCha20-Poly1305
u16 nonce_length            # exactly 24
24 bytes nonce
u64 ciphertext_length
ciphertext bytes
16 bytes authentication_tag
```

AAD = 從 magic 開始至 ciphertext_length 欄位結束的全部 header bytes。

- 檔案總長必須恰好匹配。
- unknown version/id/length 必須在 KDF 前拒絕。
- ciphertext_length 不得超過 64 MiB。
- tag 驗證成功前不得解析 payload。

## 17. Vault payload framing

Plaintext payload：

```text
8 bytes magic              # "SDKPAY01"
u16 payload_version        # 1
u16 difficulty_rules_ver   # 1
u16 generator_format_ver   # 1
u16 reserved               # 0
u32 settings_length
settings bytes
u32 in_progress_count
framed game records...
u32 completed_count
framed completed records...
u32 payload_crc32
```

每個 framed record：

```text
u16 record_type
u16 record_version
u32 record_length
record bytes
```

- payload CRC32 不是安全邊界，只用於解析診斷；AEAD tag 才是完整性來源。
- count 與所有 length 必須在 remaining bytes 內。
- 不允許 trailing bytes。

## 18. Settings record

v1.0 settings：

```text
u8 theme                    # 0=dark, 1=light
u8 motion                   # 0=full, 1=reduced
u8 auto_remove_peer_notes   # 0=off, 1=on
u8 confirm_auto_solve       # 0=off, 1=on
u8 last_difficulty          # 0=easy, 1=medium, 2=hard
u8 reserved[11]             # all zero
```

## 19. Game record canonical fields

Record type 1, version 1：

- 16-byte game ID。
- `u8 difficulty`。
- `u16 difficulty_rules_version`。
- `u16 generator_format_version`。
- `u64 generator_seed`。
- 81 bytes original clues，0–9。
- 81 bytes current values，0–9。
- 81 × `u16` notes mask，僅 bits 0–8 可用。
- 81 bytes value origin：0=empty, 1=given, 2=player, 3=hint, 4=auto-solve。
- `u64 active_elapsed_ms`。
- `i64 created_epoch_ms`。
- `i64 last_played_epoch_ms`。
- `u8 paused`。
- `u32 hints_viewed`。
- `u32 hints_applied`。
- `u8 highest_hint_technique`，0 表示 none。
- `u8 used_auto_solve`。
- `u64 current_generation`。
- `u64 saved_generation`。
- `u32 undo_count` + transactions。
- `u32 redo_count` + transactions。

解析時必須驗證 given origin 與 original clues 一致；given cell current value 必須等於 clue。

## 20. Undo transaction framing

Transaction：

```text
u8 action_kind
u8 assisted_reason         # 0=none,1=hint,2=auto-solve
u16 change_count
u64 sequence_number
changes...
```

Change：

```text
u8 cell_index              # 0..80
u8 before_value
u8 after_value
u8 before_origin
u8 after_origin
u16 before_notes
u16 after_notes
```

- 同一 transaction 每個 cell 最多出現一次。
- cell index 必須 ascending，確保 deterministic serialization。
- before/after 相同的 no-op change 禁止保存。
- sequence number 在每局內嚴格遞增。

## 21. Completed record

Record type 2, version 1：

- game ID。
- difficulty 與版本欄位。
- original clues。
- completed grid。
- 81 value origins。
- active elapsed。
- created、last played、completed timestamps。
- hints viewed/applied。
- highest hint technique。
- used auto solve。
- completion classification：
  - 0=`PLAYER_UNASSISTED`
  - 1=`PLAYER_HINT_ASSISTED`
  - 2=`AUTO_SOLVED`
- logic score。
- max technique。
- clue count。

`AUTO_SOLVED` 不得同時標記為 player completion。

## 22. Game ID 與 nonce

- Game ID為16 bytes，由`BCryptGenRandom(NULL, ..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)`取得。
- 全 0 ID 無效。
- collision 時重取，最多 8 次；仍 collision 則失敗。
- XChaCha nonce為24 bytes，每次成功或失敗的保存嘗試都重新呼叫system-preferred `BCryptGenRandom`。
- 不得使用 game ID、timestamp 或 counter 直接當 nonce。

## 23. Sudoku canonical board text fixture

測試 fixture 可使用 9 行，每行恰好 9 個字元：

- `1`–`9` = formal value。
- `.` = empty。
- 空白行與 `#` comment line 可忽略。
- 其他字元、行數或行長錯誤必須拒絕。

Canonical cell order為 row-major。

## 24. Difficulty trace canonical order

Technique enum：

1. Naked Single
2. Hidden Single
3. Locked Candidates Pointing
4. Locked Candidates Claiming
5. Naked Pair
6. Hidden Pair
7. Naked Triple
8. Hidden Triple

Placement/elimination target cells、support cells、affected candidates 均以 cell index ascending、digit ascending 序列化。相同 logical state 必須得到相同第一步與相同完整 trace。

## 25. UI probe output

測試模式可輸出結構化 UI probe，最低 schema：

```json
{
  "schema_version": 1,
  "scene": "play_easy_dark",
  "client_width": 1280,
  "client_height": 800,
  "page": "play",
  "modal": null,
  "focused_control": "...",
  "elements": []
}
```

每個 element 至少包含：

- stable semantic ID。
- role。
- x、y、width、height。
- visible、enabled、focused。
- text 或 semantic value。
- z-order。

UI probe 是 E2E assertion 的輔助，不能取代實際畫面證據。

## 26. Test result JSON

所有 suite 共用最低 schema：

```json
{
  "schema_version": 1,
  "suite": "unit",
  "source_commit": "...",
  "started_epoch_ms": 0,
  "duration_ms": 0,
  "total": 0,
  "passed": 0,
  "failed": 0,
  "skipped": 0,
  "exit_status": 0,
  "cases": []
}
```

每個 case：name、requirement_ids、status、duration_ms、assertion_count、failure_message、evidence_paths。

Summary 數字必須與 cases 一致。

## 27. Log 與 report 安全

不得出現在任何 log/report：

- vault password。
- derived key。
- plaintext serialized payload。
- 完整敏感 memory dump。

可記錄：game ID hex、非秘密格式版本、錯誤階段、fixture 名稱與 hash。

## 28. Canonical limits 測試

每個 parser／serializer 至少測試：

- 0、1、最大合法值。
- 最大值 + 1。
- integer overflow framing。
- truncated each header field。
- declared length 小於、等於、大於 remaining bytes。
- trailing garbage。
- unknown version／enum。
- reserved bits 非 0。

## 29. Windows canonical path and error mapping

- Win32 API failure對CLI exit status的mapping：usage/path validation→2；malformed repository/config→3；`ERROR_ACCESS_DENIED`、`ERROR_DISK_FULL`、`ERROR_SHARING_VIOLATION`、allocation/system I/O→4；verify mismatch→5；internal rollback failure→70。
- stderr必須同時包含穩定英文stage name與numeric Win32 error code；不得只輸出localized`FormatMessage`文字。
- Canonical report path使用`/`separator，不輸出`\\?\`prefix。
- Unicode path、case、reparse、ADS、read-only flag與atomic replacement的規範以`26`為準。

Windows平台細節以`26_WINDOWS_NATIVE_PLATFORM_CONTRACT.md`為準。
