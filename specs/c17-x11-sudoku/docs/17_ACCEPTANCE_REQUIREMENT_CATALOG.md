# 驗收 Requirement Catalog

## 1. 使用方式

本表提供穩定 requirement ID，供 requirement-to-test matrix、測試名稱、缺陷報告與人工比較引用。

- 本表是索引，不取代各專題文件的細節。
- 一個 catalog item 只有在其引用章節的全部 MUST 行為通過時才算 PASS。
- 實作者不得只測本表的短句而忽略引用文件中的邊界案例。

## 2. Workstream A：locstat

| ID | Requirement | 主要規格 |
|---|---|---|
| LOC-01 | 可掃描 root 並依 source/tests/docs/config 分類 | `03` §2–3 |
| LOC-02 | 預設與 config ignore 規則正確 | `03` §4–6 |
| LOC-03 | 物理行數、LF/CRLF 與無末尾換行正確 | `03` §7.1 |
| LOC-04 | C comment/string lexical counting 正確 | `03` §7.2 |
| LOC-05 | text 與 deterministic JSON 報告完整 | `03` §8–9 |
| LOC-06 | 無效 config、權限、symlink loop 等錯誤安全 | `03` §10–11、`09` §15 |
| LOC-07 | 實際產出三個 workstream 與文件規模報告 | `03` §12 |

## 3. Workstream A：tinyvcs

| ID | Requirement | 主要規格 |
|---|---|---|
| VCS-01 | init、repository discovery 與 main branch | `04` §3–4 |
| VCS-02 | SHA-256 content-addressed blob/tree/commit | `04` §5 |
| VCS-03 | LZSS raw/compressed mode 與 CRC32 | `04` §6 |
| VCS-04 | staging 正確區分 staged/unstaged/deleted/untracked | `04` §7 |
| VCS-05 | commit 原子更新且拒絕空 commit | `04` §8 |
| VCS-06 | branch 建立、列出與名稱驗證 | `04` §9 |
| VCS-07 | switch 保護 dirty 與 untracked collision | `04` §9 |
| VCS-08 | restore 與 reset --hard 行為正確 | `04` §10 |
| VCS-09 | `.tinyignore` pattern 正確 | `04` §11 |
| VCS-10 | log/show 真實反映 tree 差異 | `04` §12 |
| VCS-11 | verify 偵測 CRC/hash/missing/malformed object | `04` §6、§13 |
| VCS-12 | Workstream B 由 tinyvcs 管理且 history 有實質內容 | `04` §15 |

## 4. 平台與 UI 引擎

| ID | Requirement | 主要規格 |
|---|---|---|
| UI-01 | C17 + Linux + Xlib，無禁止依賴 | `02` |
| UI-02 | 自製 software pixel buffer renderer | `02` §5、`06` §3–5 |
| UI-03 | 自製 layout、hit-testing、focus、modal capture | `06` §3、§6–7 |
| UI-04 | resize 1280×800 至 960×640 仍可操作 | `06` §2 |
| UI-05 | button hover elevation | `06` §8 |
| UI-06 | pointer-origin ripple 且正確 clipping | `06` §9 |
| UI-07 | spatial border glow | `06` §10 |
| UI-08 | capsule navigation interruption-safe animation | `06` §11 |
| UI-09 | dynamic collapse 與 scroll mapping | `06` §12 |
| UI-10 | modal scale+opacity 與 input capture | `06` §13 |
| UI-11 | 真實背景 blur 與 progressive darkening | `06` §14 |
| UI-12 | Library dynamic frosted nav | `06` §15 |
| UI-13 | cubic Bézier evaluator 與 time-based animation | `06` §16、§21 |
| UI-14 | Dark/Light、Reduced Motion 與 semantic states | `06` §17–19、§23 |
| UI-15 | Hint target/support/elimination 視覺 | `06` §24 |
| UI-16 | 固定 golden scenes 與視覺品質門檻 | `06` §25–26 |

## 5. 數獨核心與產品

| ID | Requirement | 主要規格 |
|---|---|---|
| SDK-01 | 9×9 rules、given/player/note/assisted 狀態分離 | `07` §1–2 |
| SDK-02 | 完整 conflict masks 與 Submit 規則判定 | `05` §10、`07` §3–5 |
| SDK-03 | 搜尋 solver 處理 0/1/多解 | `07` §6–7 |
| SDK-04 | generator 產生合法完整盤 | `07` §13 |
| SDK-05 | clue removal 每步維持唯一解 | `07` §14 |
| SDK-06 | 產生器 bounded retry，不建立未驗證遊戲 | `07` §15 |
| SDK-07 | player input、erase 與 clue protection | `05` §6 |
| SDK-08 | notes bitset、3×3 顯示與 toggle | `05` §6.3 |
| SDK-09 | Undo/Redo transaction 與持久化 | `05` §7 |
| SDK-10 | Clear Answers 確認且一次 Undo | `05` §8 |
| SDK-11 | Pause 遮蔽棋盤且 timer 不累加 | `05` §9 |
| SDK-12 | Auto Solve 真實求解且 assisted 分流 | `05` §12、`07` §18 |
| SDK-13 | timer 使用 monotonic active duration | `05` §13、`08` §13 |
| SDK-14 | 多個未完成遊戲與 Continue | `05` §16–17 |
| SDK-15 | 完成紀錄原子建立且不重複 | `05` §21、`15` §12 |
| SDK-16 | auto-remove peer notes 設定與 transaction | `05` §22 |

## 6. 難度與 Hint

| ID | Requirement | 主要規格 |
|---|---|---|
| DIF-01 | deterministic 邏輯 solver 與固定 scan order | `07` §8 |
| DIF-02 | 正確實作 T1–T8 technique | `07` §9 |
| DIF-03 | 結構化 technique step 可獨立驗證 | `07` §10 |
| DIF-04 | 固定 weight、logic score、max technique | `07` §11 |
| DIF-05 | Easy 分類符合 technique/score/clue 條件 | `07` §12 |
| DIF-06 | Medium 分類符合 technique/score/clue 條件 | `07` §12 |
| DIF-07 | Hard 分類符合 technique/score/clue 條件 | `07` §12 |
| DIF-08 | 不可邏輯解或超範圍題目被 generator 拒絕 | `07` §12、§14 |
| HNT-01 | Hint 先驗證 conflict、solvability 與 current state | `07` §16 |
| HNT-02 | Hint preview 不修改棋盤 | `05` §11、`07` §16 |
| HNT-03 | 說明包含 technique、target、support、reason | `07` §17 |
| HNT-04 | placement Apply 填一格並可 Undo | `07` §16 |
| HNT-05 | elimination Apply 只移除真實 user notes | `07` §16 |
| HNT-06 | Hint 不依 stored solution 偽造推理 | `07` §8、`12` §10 |
| HNT-07 | viewed/applied/assisted 狀態保存與完成紀錄正確 | `05` §11、§21；`08` §7–8 |

## 7. 狀態、資料與安全

| ID | Requirement | 主要規格 |
|---|---|---|
| STA-01 | 頂層狀態與非法事件安全 | `15` §2、§15 |
| STA-02 | timer pause reasons 可組合且不互相誤解除 | `15` §4、§9 |
| STA-03 | New/Switch/Close 共用正確 Save/Discard/Cancel | `15` §5–8 |
| STA-04 | Hint/Solve stale result 不寫錯 game/generation | `15` §10–11 |
| STA-05 | busy operation 不重入且 UI 保持可呈現 | `15` §14 |
| SEC-01 | PBKDF2-HMAC-SHA-256 production 200,000 iterations | `08` §3 |
| SEC-02 | XChaCha20-Poly1305、fresh nonce、AAD、tag-first parse | `08` §4–5 |
| SEC-03 | versioned binary payload 與嚴格 length validation | `08` §6、§16 |
| SEC-04 | game/completed records 保存全部必要欄位 | `08` §7–8、§18 |
| SEC-05 | atomic write、known-good backup 與 recovery | `08` §9–10 |
| SEC-06 | password/key plaintext 不進磁碟或 log | `08` §11–12 |
| SEC-07 | multi-game snapshot 與 completion move 原子一致 | `08` §15 |

## 8. 測試、證據與交付

| ID | Requirement | 主要規格 |
|---|---|---|
| TST-01 | 自製 C test harness 有 assertions 與 JSON summary | `10` §3 |
| TST-02 | unit tests 覆蓋各演算法與 UI 純函式 | `10` §4 |
| TST-03 | integration tests 覆蓋跨模組真實資料流程 | `10` §5 |
| TST-04 | E2E 操作真實 X11 UI | `10` §6–7 |
| TST-05 | failure injection 可重現且 production 判定不被繞過 | `10` §11 |
| TST-06 | 每難度 50 題 batch 與完整統計 | `10` §12 |
| TST-07 | golden screenshots 與 animation evidence 完整 | `10` §9–10 |
| TST-08 | state machine table-driven tests | `10` §18 |
| TST-09 | requirement-to-test matrix 無 MUST 缺口 | `10` §17、§19 |
| DEL-01 | 全部 source、assets、fixtures、tests、docs 交付 | `11` §1–4 |
| DEL-02 | test/batch/corruption/locstat/VCS/visual evidence 交付 | `11` §5 |
| DEL-03 | 無 placeholder、forbidden shortcut 或未完成 MUST | `11` §8–11、`12` |
| DEL-04 | clean build 與正式 release gates 全部通過 | `16` §8 |

## 9. Canonical 格式與封口行為

| ID | Requirement | 主要規格 |
|---|---|---|
| FMT-01 | CLI exit status、stdout/stderr與usage契約固定 | `19` §3–4、§15 |
| FMT-02 | locstat traversal、分類、JSON schema與limits固定 | `19` §2、§4–5 |
| FMT-03 | tinyvcs repository/path/object/index/ref格式固定 | `19` §6–15 |
| FMT-04 | LZSS token、tie-break、CRC/hash envelope正確 | `19` §12–13 |
| FMT-05 | vault outer/AAD/payload framing正確 | `19` §16–17 |
| FMT-06 | Settings/Game/Undo/Completed records正確 | `19` §18–21 |
| FMT-07 | 所有parser執行上限、overflow、version與trailing驗證 | `19` §2、§28；`09` §18、§21 |
| SDK-17 | unsaved draft與首次Save進Library語意正確 | `05` §24；`15` §22 |
| SDK-18 | timer dirty、page/focus pause正確 | `05` §25；`15` §17–18 |
| SDK-19 | Auto Solve保留Undo並於Submit分類archive | `05` §26；`15` §20 |
| SDK-20 | Clear Answers移除全部非given formal values與notes | `05` §27 |
| HNT-08 | elimination無player note時為明確no-op preview | `05` §28 |
| SEC-08 | Save snapshot concurrency與atomic replacement正確 | `08` §20–21 |
| SEC-09 | Reset保留vault密碼並原子寫入空payload | `08` §22 |
| SEC-10 | tag固定長度比較與plaintext lifetime處理 | `08` §24–25 |
| SEC-11 | global settings save不隱含保存dirty game/draft | `05` §32；`08` §26–27；`15` §23 |
| VCS-13 | switch/reset完整preflight、rollback與原子ref順序 | `04` §17；`19` §15 |

## 10. UI Reference Contract

| ID | Requirement | 主要規格 |
|---|---|---|
| UXR-01 | reference/minimum/large layout與spacing/radius固定 | `20` §3–6 |
| UXR-02 | typography、Dark/Light palette與contrast正確 | `20` §7–10 |
| UXR-03 | button/ripple/glow/capsule數值與中斷行為正確 | `20` §11–12 |
| UXR-04 | modal、blur、dynamic nav mapping正確 | `20` §13–15 |
| UXR-05 | Sudoku/notes/keypad geometry正確 | `20` §16–18 |
| UXR-06 | focus、scroll、toast、busy與stable IDs正確 | `20` §19–23 |
| UXR-07 | visual tolerance不掩蓋缺失元件或效果 | `20` §24 |

## 11. Acceptance與Release

| ID | Requirement | 主要規格 |
|---|---|---|
| ACC-01 | canonical vault/game/input/history/submit scenarios通過 | `21` §3–7 |
| ACC-02 | canonical Hint/Solve/Timer/Save scenarios通過 | `21` §8–11 |
| ACC-03 | canonical Library/Settings/UI scenarios通過 | `21` §12–14 |
| ACC-04 | canonical locstat/tinyvcs/release scenarios通過 | `21` §15–17 |
| ACC-05 | scenario使用隔離fixture、真實UI與明確assertions | `21` §1–2；`10` §20–24 |
| REL-01 | G0 dependency/scope與G1 clean build通過 | `22` §3–4 |
| REL-02 | G2–G8 static/test/batch/failure/security通過 | `22` §5–11 |
| REL-03 | G9–G13 visual/VCS/locstat/trace/manual通過 | `22` §12–16 |
| REL-04 | G14 final consistency與evidence source identity通過 | `22` §17；`11` §15 |
| REL-05 | 最終完成報告符合模板且無MUST limitation | `24`; `22` §19–20 |

## 12. Catalog 完整性規則

最終 matrix 至少包含本表每一個 ID。
若一個測試覆蓋多個 ID，可重複引用，但必須說明 assertion 對應關係。
人工 checklist 可以作為輔助證據，不能取代 solver、crypto、storage、difficulty 或 Hint 的自動測試。
