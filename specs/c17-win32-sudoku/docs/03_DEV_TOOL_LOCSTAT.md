# Workstream A1：locstat 行數統計工具

## 1. 目的

`locstat` 用來統計本任務產生的人類可讀文件、程式碼、測試與設定檔規模。
它不得把 build 產物、log、測試結果、截圖、錄影、資料庫或版本控制物件計入。

## 2. 必要命令

```text
locstat [options] <root-path>
```

必要選項：

- `--config <path>`：指定 JSON 設定檔。
- `--json <path>`：額外輸出機器可讀報告。
- `--category <name>`：只顯示指定分類。
- `--fail-on-error`：遇到無法讀取檔案時回傳非零狀態。
- `--help`：顯示用法。

## 3. 預設分類

### source

- `.c`
- `.h`

### tests

- 位於 `tests/` 下的 `.c`、`.h`。
- 位於 `test/` 下的 `.c`、`.h`。

### docs

- `.md`
- `.txt`

### config

- `.json`
- `.yaml`
- `.yml`
- `build.cmd`
- `.rc`
- `.manifest`

同一檔案只能被計入一個分類。
測試路徑優先於副檔名的一般 source 分類。

## 4. 預設排除

至少排除：

- `.git/`
- `.tinyvcs/`
- `build/`
- `dist/`
- `out/`
- `logs/`
- `results/`
- `screenshots/`
- `recordings/`
- `tmp/`
- `temp/`
- `coverage/`
- 二進位執行檔。
- object files。
- archives。
- 圖片、音訊與影片。
- 應用程式存檔與資料庫。

## 5. 設定檔

設定檔為 JSON，至少支援：

```json
{
  "include_extensions": [".c", ".h", ".md", ".json", ".yaml", ".yml"],
  "exclude_paths": ["build/**", "results/**"],
  "exclude_extensions": [".log", ".bin"],
  "categories": {
    "source": [".c", ".h"],
    "docs": [".md", ".txt"],
    "config": [".json", ".yaml", ".yml"]
  }
}
```

必須自行實作足以解析此設定結構的 JSON parser。
不得使用外部 JSON 函式庫。
不要求支援 JSON 的任意擴充語法。

## 6. Ignore 規則

- 支援相對路徑。
- 支援 `*` 匹配單一路徑片段內任意字元。
- 支援 `**` 跨目錄匹配。
- 支援結尾 `/` 表示目錄。
- 規則按設定順序套用。
- 本版本不要求否定規則 `!`。

## 7. 行數定義

### 7.1 所有文字檔

- `physical_lines`：物理行數。
- 最後一行沒有換行仍算一行。
- 空檔案為 0 行。
- 支援 LF 與 CRLF。
- 不得因 CRLF 將一行算成兩行。

### 7.2 C source/test 額外統計

至少分出：

- `blank_lines`
- `comment_only_lines`
- `code_lines`
- `mixed_code_comment_lines`

必須正確處理：

- `//` 註解。
- 跨行 `/* ... */`。
- 字串中的 `//` 或 `/*` 不得視為註解。
- 字元常值中的註解符號不得誤判。
- escaped quote。

此工具是 lexical counter，不要求完整 C parser。

## 8. 人類可讀輸出

至少包含：

- 掃描根目錄。
- 掃描檔案數。
- 排除檔案數。
- 每分類檔案數與物理行數。
- source/test 的 code/comment/blank 統計。
- 全部人類可讀文件總行數。
- 無法讀取檔案清單。

## 9. JSON 輸出

JSON 報告至少包含：

- 工具版本。
- 掃描時間。
- root path。
- config digest。
- category totals。
- per-file 統計。
- errors。
- grand totals。

輸出順序必須固定，讓相同輸入可得到可比較的結果。

## 10. 錯誤行為

- 根目錄不存在：失敗。
- 設定檔不存在：失敗。
- JSON 無效：顯示行列位置並失敗。
- reparse point／junction：不得follow；root只允許解析一次。
- unreadable file：記錄錯誤；是否整體失敗依 `--fail-on-error`。
- 非 UTF-8 文字檔：仍可依 byte newline 統計，但須標示 encoding warning。

## 11. 必要測試

至少涵蓋：

- 空檔案。
- 單行無結尾換行。
- LF／CRLF。
- 空白與註解混合。
- 字串中的註解符號。
- 巢狀目錄與 ignore pattern。
- `.tinyvcs` 物件庫排除。
- JSON 設定解析錯誤。
- reparse point、junction與root reparse resolution。
- unreadable path。
- 兩次掃描同內容輸出相同 totals。

## 12. 本任務中的使用要求

最終交付必須用 `locstat` 產生：

- Workstream A 報告。
- Workstream B 報告。
- 全專案報告。
- 文件總行數報告。

報告必須放在 `results/locstat/`，而 `results/` 自身不得被計入統計。


## 13. v1.0 Canonical 契約

以下行為以 `19_CANONICAL_FORMATS_AND_LIMITS.md` §3–5 為準：

- exit status。
- option 重複與 `--json -`。
- 不 follow reparse point／junction。
- CR-only line ending。
- binary-like detection。
- category priority。
- unknown config key拒絕。
- deterministic traversal與JSON key/order。
- max files、path depth、file bytes與JSON limits。

## 14. 文件總行數定義

本任務要求的「所有人類閱讀文件總行數」為：

- category=`docs`。
- 只計正式 source tree 中的 `.md`／`.txt`。
- 排除 build、results、logs、screenshots、recordings、fixture generated output與`.tinyvcs`。
- 每個檔案按physical line定義計算。

報告須同時提供逐檔與總計，不能只提供單一總數。

## 15. 最低 CLI 驗收

至少直接驗證：

- help/usage/invalid option exit code。
- text output。
- JSON file output。
- JSON stdout output。
- `--category docs`。
- unreadable file在有無`--fail-on-error`的不同結果。
- 相同fixture的stable ordering。
