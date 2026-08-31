## 模型設定
大致為預設，使用 Hy3 限時免費（上下文窗口：192k；思考強度：高），任務選擇代碼開發，沒有手動開啟任何專家、技能、連接器。

## Repomix 統計
### Project
使用 archive 統一的 reviewed implementer-authored corpus 規則，排除 generated results 與 auto-generated font tables（`o200k_base`）。

Total Files: 71 files

Total Tokens: 218,447 tokens

### Conversation
使用不同 Tokenizer 估算大約 670,000 Tokens。

## Archive
完整原始 submission 為 `0820-sudoku-workbuddy.zip`。由於原始 workspace 含大量編譯產物、PDB/OBJ、runtime state 與 BMP evidence，Git 中的 `project/` 是 review-friendly curated snapshot；完整 raw ZIP 會在本 run merge 後以 GitHub Release asset 保存。

原始 ZIP SHA-256：

```text
f0c6fdc701786bf4c9d12dc0cbbfe567c1aaef9fa8a1bb3db60a58ef717ced14
```

裁剪與 Repomix 選檔規則分別記錄於 `analysis/archive/` 與 `analysis/repomix/`。