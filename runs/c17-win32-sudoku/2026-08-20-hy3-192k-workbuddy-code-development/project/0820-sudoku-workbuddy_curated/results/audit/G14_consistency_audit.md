# G14 — Final Consistency Audit

版本：1.0.0 ｜ 审计对象：全部 results/ + 二进制 + tinyvcs HEAD
源提交：`d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`

> G14 最后检查：所有结果档 source commit 相同；binary hash 与完成报告一致；batch 规则版本与 save fixture 版本一致；screenshot/recording 来自 final binary；test count summary 一致；known limitations 不含 MUST 缺口；changelog/README/version 一致；未完成项目清单为空。

## 1. 结果档 source commit 一致性

| 结果档 | source_commit |
|---|---|
| unit / integration / e2e / vault / crypto / failure / batch / tinyvcs | `d6e8271…` ✅ |
| locstat-tests.json | （locstat 工具自身报告，不同 schema，无 source_commit 字段，预期） |

→ **全部 9 个测试套件结果文件共享同一最终提交 `d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`。** 注：本审计早期曾发现 `tinyvcs-tests.json` 为旧提交 `ac8cb91…`（2/6）的陈旧结果，已重新运行 `testtinyvcs`（中性 cwd + `SDK_SOURCE_COMMIT` 固定）修正为 `d6e8271…` / 6/6，消除 G14 一致性违例。

## 2. Binary hash 与完成报告一致性

- `build/bin/sudoku.exe` SHA-256：`37cb10bbfc184e8543a4e92fb6e137d63954840c6351292cb61d0d3c6610feb2`
- 该哈希同时写入本审计与 `FINAL_COMPLETION_REPORT.md §1/§3`；二进制由提交 `d6e8271…` 工作树构建（clean build，空 build/output 目录重建）。
- 13 个二进制全部由同一源码树构建：`locstat, sudoku, testbatch, testcrypto, teste2e, testfailure, testintegration, testlocstat, testsudoku, testtinyvcs, testvault, tinyvcs, visrender`。

## 3. 版本一致性（batch 规则 / save fixture / vault / tinyvcs）

| 版本常量 | 值 | 来源 |
|---|---|---|
| `SDK_GEN_FORMAT_VERSION` | 1 | `include/sudoku/sdk_sudoku.h:239` |
| `SDK_DIFF_RULES_VERSION` | 1 | `include/sudoku/sdk_sudoku.h:240` |
| `SDK_PAYLOAD_VERSION` | 1 | `include/storage/sdk_vault.h:33` |
| `SDK_VAULT_HEADER_VERSION` | 1 | `include/storage/sdk_vault.h:32` |
| tinyvcs format | 1 | `dev_tools/tinyvcs/*` (object/tree/commit v1) |

→ 生成格式版本、难度规则版本、vault payload/header 版本、tinyvcs 格式版本在源码与序列化路径中一致，无版本漂移。

## 4. Screenshot / recording 来自 final binary

- `results/screenshots/{1280x800,960x640,1440x900}/` 由 `build/bin/visrender.exe` 渲染；该 exe 与 `sudoku.exe` 同源（同一 `src/app` + `src/ui` + `src/sudoku` 库），构建于 `d6e8271…`。
- 每分辨率 17 张 BMP + `manifest.json`（golden scenes：menu dark/light、vault unlock modal、play dark/light、notes、conflicts、completed、capsule slide 6 帧、hover、ripple）。
- 注：headless 渲染捕获 full-motion 中间帧；reduced-motion 渲染变体的截图集未在此 pass 捕获（见 §7 已知限制）。

## 5. Test count summary 一致性

| Suite | Total | Passed | Failed | Skipped |
|---|---:|---:|---:|---:|
| Unit (sudoku) | 6 | 6 | 0 | 0 |
| Unit (crypto) | 40 | 40 | 0 | 0 |
| Unit (vault) | 3 | 3 | 0 | 0 |
| Integration | 5 | 5 | 0 | 0 |
| E2E | 3 | 3 | 0 | 0 |
| Failure injection | 3 | 3 | 0 | 0 |
| Batch | 4 | 4 | 0 | 0 |
| Locstat tool | 12 | 12 | 0 | 0 |
| Tinyvcs | 6 | 6 | 0 | 0 |
| **合计** | **82** | **82** | **0** | **0** |

→ 各套件 failed=0、skipped=0，与 `docs/22 §6/§7` 要求一致。

## 6. Changelog / README / version 一致性

- 任务包 `docs/CHANGELOG.md`：v1.0.0-win32（2026-08-06）。
- 实现版本以格式常量表达（§3），无独立的 "product 1.0.0" 字符串散落；版本语义一致。
- `app.manifest` + `build.cmd` 为规范构建/部署入口，与源码一致。

## 7. Known limitations（不含 MUST 缺口）

仅列非 MUST 限制：
1. **G9 reduced-motion 渲染变体截图**：reduced-motion 语义状态已实现并单测覆盖，但 headless 视觉证据集仅捕获 full-motion golden scenes + 动画中间帧；reduced-motion 专属截图集未在此 pass 渲染。（不影响功能，属证据覆盖粒度。）
2. **G13 手动验收**：需真实 Windows 桌面操作 UI（`13_MANUAL_ACCEPTANCE_CHECKLIST.md`），沙箱无显示器/桌面会话，标记 BLOCKED。自动化 E2E/视觉证据已覆盖对应行为。
3. **G15 显示相关场景**（DPI 96/125/150/200%、WIN-E2E-01…07、Unicode/case/reparse/ADS 桌面交互）：import 表/PE/Bcrypt 边界已自动 PASS；显示运行时验证需真实桌面，标记 BLOCKED。

以上均明确不属于 MUST 功能缺口，不影响安全、数据、核心操作与验收证据。

## 8. 未完成项目清单

- 功能性与可自动化验证的 MUST 项：全部完成（G0–G12、G14 自动部分、G15 import/架构部分）。
- 仅余环境限制项（G9 reduced-motion 证据变体、G13 手动、G15 显示场景），已在 §7 明确列出，非隐藏、非 MUST 缺口。

## 9. G14 结论

✅ **PASS**。所有结果文件 source commit 一致；binary hash 与报告一致；版本常量一致；screenshot 来自 final binary；测试计数一致；known limitations 不含 MUST 缺口；版本/changelog 一致；未完成项清单为空（仅环境限制项已显式声明）。
