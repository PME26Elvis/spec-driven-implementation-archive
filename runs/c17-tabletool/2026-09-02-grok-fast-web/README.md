# 2026-09-02 — Grok Web Fast

Implementation run for the **TableTool** portable C17 specification.

## Run metadata

| Field | Value |
|---|---|
| Execution surface | Grok Web |
| Mode | Fast |
| Backend model | Not exposed by the Fast-mode frontend |
| Run date | 2026-09-02 |
| Total implementation time | 約 28 分鐘 |
| Submitted project | `0902-grok-tabletool-1.0.1.zip` |
| Archived project | `project/tabletool/` |
| Conversation exports | Markdown + MHTML |

## 模型型號備註

本 run 實際可觀測到的產品設定是 **Grok Web · Fast**。執行當時
Fast 模式的網頁前端沒有直接標示具體 backend model，因此本 archive
不把「Grok 4.6」記錄成已確認的 run metadata。

綜合 2026-09-02 當時可取得的資訊，目前最合理的推測是 Fast
高機率同樣基於 **Grok 4.6 family**，但採偏低延遲、較低推理預算的
快速配置。

主要依據包括：

- xAI 已於 2026-08-12 發布 Grok 4.6，且當時已是現行 Grok 世代。
- xAI 官方資料確認 Grok 4.6 支援不同程度的 reasoning effort，
  包括 low、medium、high 與 xhigh。
- xAI 的 Grok 4.6 發布資料亦明確提到另有 fast variant。
- 同期產品 UI 中 Expert / Heavy 已明確標示為 Grok 4.6。
- Fast 在先前仍會顯示 Grok 4.5，但 4.6 上線後反而移除了具體
  型號標示；這較像是產品不再對外固定揭露 Fast 的 backend routing，
  而不能據此認定仍固定使用 Grok 4.5。

因此，本 archive 採用：

**Grok Web · Fast (backend model undisclosed; likely Grok 4.6 family)**

作為這次 run 的描述。這是一項有根據的推定，而不是 xAI 對
consumer Fast routing、serving configuration 或 reasoning parameters
的官方確認。

## Project

Submitted snapshot：

`project/tabletool/`

原始 ZIP 解開後的 TableTool project 共 **31 個檔案**。檢查 snapshot
後未發現 compiled binary、compiler/intermediate output、runtime log、
generated evidence 或其他需要從 Git snapshot 移除的可重建產物，
因此 project tree 原樣保留。

內容包含 implementation source、tests、human-authored documentation
與 small authored examples。

`src/ops_extra.c` 雖然只有 placeholder comment，但它仍是實作者提交的
source file，因此保留於 project snapshot 並納入 authored-corpus 統計。

## Implementer-reported final state

Submitted `tabletool/README.md` 將成果標示為 **TableTool 1.0.1**，
並宣稱 supplied acceptance cases A–H 均通過。

同一份 README 亦主動註明部分 secondary edges 相較完整 specification
有所簡化。因此這裡保存的是 **implementer-reported final state**；
本次 archive 上架流程本身不把該聲明等同於獨立 specification review
或 Release Gate 重新驗證。

## Repomix

依 archive 的 normalized implementer-authored corpus policy，
以 Repomix 1.18.0 與 `o200k_base` 計算：

- Total Files: **31 files**
- Total Tokens: **39,490 tokens**
- Total Chars: **127,949 chars**

本 snapshot 經 review 後不需要 custom exclusions。

完整量測規則位於 `analysis/repomix/`。

## Conversation

完整對話同時保存：

- `conversations/grok-export.md` — 9,306 lines / 324,765 bytes
- `conversations/grok-export.mhtml` — 61,237 lines / 3,881,903 bytes

MHTML 保存較接近原始 Grok Web 頁面狀態的瀏覽器備援，
Markdown 則提供方便閱讀、搜尋與版本控制的文字表示。

## Archive provenance

Original submission：

`0902-grok-tabletool-1.0.1.zip`

SHA-256：

```text
7fb9f5c190f20a2329e4bfaf8e57bd26d1a11607c4bddbbd7cdf4c4b315102ab
```

原始 ZIP 與 conversation 的詳細 provenance 位於
`analysis/archive/import-manifest.md`。
