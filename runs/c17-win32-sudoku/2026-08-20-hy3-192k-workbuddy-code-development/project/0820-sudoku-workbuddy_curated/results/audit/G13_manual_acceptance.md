# G13 — Manual Acceptance

版本：1.0.0 ｜ 依据：`docs/13_MANUAL_ACCEPTANCE_CHECKLIST.md`（A–V 共约 120 项）
源提交：`d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`

> 按 `22 §16`：每項須記錄 PASS/FAIL/BLOCKED/N/A；不能用自動測試報告直接整表勾選而未操作 UI。本環境**無真實 Windows 桌面 / 顯示器 / 輸入會話**，無法執行人工 UI 操作，故全部依賴桌面交互的項目標記 **BLOCKED**，並註明其行為已被自動化證據（E2E / 視覺 / 單元 / 集成 / 故障注入 / G15 import）覆蓋，待真實桌面最終確認。

## 状态汇总

| 区块 | 项数(约) | 状态 | 说明 |
|---|---:|---|---|
| A 建置与启动 | 11 | BLOCKED | 启动/窗口/DPI/minimize 需桌面；import/架构已由 G15 自动 PASS |
| B Vault | 7 | BLOCKED | 密码/解锁/密文篡改已由 `testvault`(3/3)+`testfailure`(3/3) 自动覆盖 |
| C 导航与视觉 | 12 | BLOCKED | 已由 `results/screenshots/*`(3 分辨率 17 帧) + `e2e` 覆盖；reduced-motion 证据变体见 G14 §7 |
| D New Game | 8 | BLOCKED | 生成/唯一解/分类已由 `testsudoku`+`testbatch`(150 题) 自动覆盖 |
| E 棋盘输入 | 9 | BLOCKED | 已由 `e2e.keyboard_play` + `testsudoku` 覆盖 |
| F Notes | 6 | BLOCKED | 已由 `testsudoku`(SDK-08) + `integration.notes` 覆盖 |
| G Undo/Redo/Clear | 8 | BLOCKED | 已由 `integration.undo_redo` + `testsudoku`(SDK-09/10/20) 覆盖 |
| H Submit | 10 | BLOCKED | 已由 `testsudoku`(SDK-02) + `integration` 覆盖 |
| I Hint | 11 | BLOCKED | 已由 `testsudoku`(HNT-01/03/05) + `e2e.mistake_then_win` 覆盖 |
| J Auto Solve | 7 | BLOCKED | 已由 `testsudoku`(SDK-12/19) + `integration.auto_solve` 覆盖 |
| K Timer/Pause | 6 | BLOCKED | 已由 `integration`(SDK-13/18) + `testsudoku` 覆盖 |
| L Save/Close | 8 | BLOCKED | 已由 `integration`(STA-03) + `testfailure`(atomic) 覆盖 |
| M 多个未完成游戏 | 6 | BLOCKED | 已由 `testsudoku`(SDK-14) 覆盖 |
| N Library | 7 | BLOCKED | 已由 `e2e` + screenshots(nav) 覆盖 |
| O Settings | 6 | BLOCKED | 已由 `testsudoku`(SDK-16) + `integration`(SEC-09/11) 覆盖 |
| P locstat | 8 | PASS(自动) | `testlocstat`(12/12) + `results/locstat/*` 直接验证 |
| Q tinyvcs | 16 | PASS(自动) | `testtinyvcs`(6/6) + `tinyvcs verify`(0 corrupt) 直接验证 |
| R 测试与证据 | 10 | PASS(自动) | 全部 result JSON + screenshots + manifest + 本审计存在 |
| S 最终停止检查 | 7 | PASS(自动) | G2(无 TODO/FIXME) + G0(无禁止库) + G10/G11 直接验证 |
| T Canonical format | 10 | PASS(自动) | `testcrypto`/`testvault`/`testlocstat`/`testtinyvcs` + `failure` 覆盖 |
| U v1.0 封口细节 | 11 | BLOCKED/PASS混 | 多数由集成/E2E/单测覆盖；纯桌面交互项 BLOCKED |
| V Release gates | 8 | PASS(自动) | 本审计 G0–G15 报告齐备；G13 自身为 BLOCKED 项 |
| W(T) Windows Native | 9 | BLOCKED | import/PE/Bcrypt 已由 G15 自动 PASS；DPI/WIN-E2E 需桌面 |

## 关键声明

- **整体 G13：BLOCKED（环境限制）**。沙箱无真实 Windows 桌面，无法由人工操作 UI 完成 A–V 中依赖桌面交互的项。
- **非功能缺口**：所有被 BLOCKED 的行为均有对应自动化证据（E2E / 视觉截图 / 单元 / 集成 / 故障注入 / G15 import 审计），说明功能已实现并通过自动验证；仅「人工目视/手动操作」这一验收动作本身未能在沙箱内执行。
- **可直接 PASS 的项（P/Q/R/S/T）**：locstat、tinyvcs、测试证据、停止检查、canonical format 已由自动化套件与审计报告直接验证，无需桌面。
- **后续动作**：在目标 Windows 10 22H2 / Windows 11 上按 `docs/13` 逐项人工确认，将 BLOCKED 翻为 PASS/FAIL；预期全部 PASS（功能已自动验证）。

## G13 结论

⛔ **BLOCKED**（仅因沙箱无真实 Windows 桌面，环境限制；非 MUST 功能缺口）。自动化证据已覆盖绝大多数行为；建议目标平台人工终验。
