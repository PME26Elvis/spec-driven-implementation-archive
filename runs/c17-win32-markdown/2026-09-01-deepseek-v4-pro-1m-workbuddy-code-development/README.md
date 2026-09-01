## 模型設定

Tencent WorkBuddy，任務選擇 **「代碼開發」**，模型使用
**Deepseek-V4-Pro**；上下文窗口 **1M**；思考強度選擇
**「超高」**。

本次任務約消耗 **1,659 WorkBuddy credits**。

先前保存的 WorkBuddy 模型選擇 UI snapshot 中，
Deepseek-V4-Pro 顯示 **0.51×** 並標示夜間折扣。本次模型倍率表沒有
改變，但本 run 的實際使用時段 **不在 WorkBuddy 系統提示的非尖峰
五折優惠時段**。因此這裡只把 0.51× 保存為平台 UI 的折扣倍率資訊，
不把它解讀為本 run 的實際有效 billing multiplier。

## Project

Curated project snapshot：

`project/c17-markdown-editor/`

Git snapshot 移除了可重建的 compiler / intermediate output 與 runtime
cache；完整 byte-exact submission 仍由 raw ZIP checksum 保存。

## Implementer-reported final state

WorkBuddy 自己的最終報告並未宣稱完整通過 v1.0 Release Gates。

最後的 `evidencecheck` 仍記錄 **3 errors**：

- 1 個 mandatory test failure；
- 缺少 `UI-OUTLINE` screenshot；
- 缺少 `UI-FROSTED-SCROLLED` screenshot。

因此本 archive 將這份交付視為 substantial implementation，
但不是完整通過原 specification Definition of Done 的 release。

## 人工測試

2026-09-01 於實際 Win32 GUI 進行人工操作。

觀察結果：

- 上方 Source / Split / Preview / Rendered 模式切換時，按鈕位置會飄移。
- 藍色 New Document 的操作回饋異常；觸發 New 後分頁實際可建立，
  但輸入第一個字即會閃退。
- Open 可以正常瀏覽檔案並成功開啟。
- Preview 的呈現效果很勉強。
- Split 模式存在文字溢出。
- 左下角即時游標位置顯示看起來正常。
- Light / Dark mode 切換正常。

**整體人工使用體驗目前尚未達到可用狀態。**

## Repomix

使用 archive 的 normalized implementer-authored corpus 規則：

- Repomix 1.18.0
- `o200k_base`
- generated `evidence/**` 不納入 authored corpus

Total Files: **56 files**

Total Tokens: **116,606 tokens**

## Conversation

- `conversations/workbuddy-export.md`
- `conversations/workbuddy-output-only.txt`

## Archive

Original submission：

`0901-c17-markdown-editor-workbuddy.zip`

SHA-256：

```text
b99b21bf14cbdcbd2a0cd8ddbbaf945a267189d71e00dac55e0a207ea1534532
```

詳細 archive / omission provenance 位於 `analysis/archive/`。
