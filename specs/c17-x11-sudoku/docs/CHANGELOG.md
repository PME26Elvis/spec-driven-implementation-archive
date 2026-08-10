# Changelog

## 1.0.0 — 2026-08-06

正式範圍凍結版，可作為不同實作者之間的共同任務基線。

在 0.2 scope-freeze內容上完成：

- 將規範關鍵字、術語、版本與衝突優先順序正式化。
- 固定允許的C17/Linux/X11/Xlib/POSIX工程邊界與link-time allowlist。
- 固定`locstat` CLI、traversal、ignore、line counting、JSON schema與limits。
- 固定`tinyvcs` repository layout、blob/tree/commit identity、object envelope、index、ref與exact LZSS tokens。
- 固定vault outer header、AAD、payload framing、Settings/Game/Undo/Completed binary records。
- 固定所有parser的length、overflow、version、reserved與trailing-data拒絕規則。
- 固定UI reference layout、palette、typography、contrast、animation數值、blur與dynamic frosted nav mapping。
- 固定stable semantic IDs、UI probe與visual tolerance。
- 補齊unsaved draft、首次Save進Library、timer dirty、離開Play／失焦pause等產品語意。
- 固定Auto Solve保留Undo、Submit後archive與completion classification。
- 固定Clear Answers移除全部非given formal values與notes。
- 固定elimination Hint無對應player note時的明確no-op行為。
- 補齊T1–T8 subset formal conditions、candidate state與deterministic tie-break。
- 固定atomic vault replacement、background snapshot generation與Reset Data行為。
- 建立canonical vault/game/input/history/Hint/Solve/Timer/Save/Library/UI/locstat/tinyvcs scenarios。
- 建立G0–G14正式release gates、traceability matrix與completion report模板。
- Requirement Catalog擴充為124個穩定ID。
- 建立task-pack self-audit與文件完整性檢查。

v1.0不再新增主要功能。後續1.x只可消除矛盾或改善可驗收性。

## 0.2.0 — 2026-08-06

範圍凍結候選版：

- Easy／Medium／Hard。
- T1–T8人類邏輯難度分類。
- 可解釋Hint。
- 產品狀態機。
- UI golden scenes。
- scope freeze與requirement catalog。

## 0.1.0 — 2026-08-05

第一版規格文件包：

- C17 + Linux + Xlib。
- 自製software renderer與GUI效果。
- 9×9 Sudoku產品功能。
- encrypted vault。
- `locstat`與`tinyvcs`。
- unit/integration/E2E/failure tests。
- Definition of Done與人工checklist。
