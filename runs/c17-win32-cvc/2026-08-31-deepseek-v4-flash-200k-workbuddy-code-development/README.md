## 模型設定

Tencent WorkBuddy，任務選擇「代碼開發」，模型使用
**Deepseek-V4-Flash**；上下文窗口 **200K**；思考強度使用預設的
**關閉**。

本次任務約消耗 **300 WorkBuddy credits**。截圖當下
Deepseek-V4-Flash 顯示 **0.17×** credit 消耗倍率（夜間折扣）。
這裡的倍率是 WorkBuddy 平台 credit 消耗速率，不是 API 單價。

同一時點截圖可見的其他模型 / 倍率包括：
Hy4 preview 0.00×、Hy3 0.00× / 0.05×、GLM-5.3 0.79×、
GLM-5.3-Flash 0.06×、GLM-5.1 0.79×、GLM-5v-Turbo 0.71×、
MiniMax-M3 0.25×、Kimi-K3 1.62×、Kimi-K2.7-Code 0.57×、
Kimi-K2.6 0.52×、Deepseek-V4-Pro 0.51×。這只是當時 UI 的可見
snapshot，平台供應與倍率可能變動。

## Project

本 run 的 curated project snapshot 位於 `project/cvc/`。

原始 ZIP 中只有 `tests/run/__pycache__/` 的 16 個衍生 Python
bytecode cache 沒有寫入 Git；完整 raw ZIP 以 checksum 保留 provenance。

## Repomix

使用 archive 統一的 reviewed implementer-authored corpus 規則
（Repomix 1.18.0、`o200k_base`）。

Total Files: **54 files**

Total Tokens: **152,176 tokens**

## Conversation

WorkBuddy 對話保存為：

- `conversations/workbuddy-export.md`
- `conversations/workbuddy-output-only.txt`

後者為另外保留的 output-only corpus，方便後續模型輸出 token 分析。

## Archive

原始 submission：`0831-cvc.zip`

SHA-256：

```text
ee8f5fa813e7d76a7a3ede163d2d779c576136709dca4195c72df3f4f67c60f5
```

詳細 ingestion / omission provenance 位於 `analysis/archive/`。
