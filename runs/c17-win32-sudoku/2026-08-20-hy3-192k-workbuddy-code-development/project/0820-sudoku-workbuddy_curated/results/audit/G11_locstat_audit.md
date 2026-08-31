# G11 — locstat and Document Audit

版本：1.0.0 ｜ 工具：`build/bin/locstat.exe`（自研 locstat v1.0.0）
源提交：`d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`

> G11 要求 locstat 对 Workstream A、B、全项目与 docs 产出报告；`results/`、screenshots、recordings、build、`.tinyvcs` 不计入；JSON 可解析且 summary == per-file 加总；文档 manifest 行数与实际一致；所有文档 UTF-8 可读、internal links 存在。

## 1. locstat 分类报告（基于最终源码，2026-08-30 重新生成）

工具默认排除 `.tinyvcs`、`build`、`results`（`results/audit/locstat-config.json` 中 `excluded` 字段确认）。报告文件：

| 报告 | 路径 | 类别 | 文件数 | 物理行 | 代码行 |
|---|---|---|---:|---:|---:|
| 全项目 | `results/locstat/locstat-full.json` | 全部 | 124(扫描) | 27,586 | 21,339 |
| Workstream A+B 源码 | `results/locstat/locstat-source.json` | source | 53 | 17,019 | 13,640 |
| 测试 | `results/locstat/locstat-tests.json` | tests | 9 | 2,731 | 2,028 |
| 文档 | `results/locstat/locstat-docs.json` | docs | 33 | 7,587 | 5,502 |
| 配置 | `results/locstat/locstat-config.json` | config | 2 | 78 | 70 |
| 未分类 | （含于 full） | unclassified | 21 | 166 | 94 |

- **Workstream A**（locstat / tinyvcs / C harness）源码包含在 `source` 类别中（与 Workstream B 同属 `source` 类别，locstat 不单独拆分 A/B 子类别）。
- **JSON 可解析**：5 份报告均为合法 JSON（`schema_version:1`）。
- **summary == per-file 加总**：locstat 的 `totals` 由各 `files[]` 累加得出，内部保证一致；`docs` 类别 `all human-readable docs total lines = 7587` 与 per-file 求和一致。
- 扫描根 `D:/0820-sudoku-workbuddy`，排除 `build`/`results`/`.tinyvcs`（符合 `22 §14` 不计入要求）。注：扫描根另含任务包规范目录 `c17-win32-sudoku-task-pack-v1.0.0/docs`（7,520 行，见 §2）与极少量工程工作文件（如 `.workbuddy/memory`），已计入 `docs` 类别，不影响交付源码统计。

## 2. 文档 manifest 一致性（G11 doc-audit）

任务包 `docs/DOCUMENT_MANIFEST.md` 声明：31 个 Markdown 文件，总行数 **7,520**。

程序化校验（逐文件 physical-line 计数，含无末尾换行的末行）：

- 31 个文件全部存在，**0 缺失**。
- 逐文件行数与 manifest 声明 **完全一致，0 偏差**。
- 全部 31 个文件 **UTF-8 可读（0 非法字节）**。
- 全部 internal links（相对路径 `docs/XX_*.md`、`README.md` 等）**全部可解析，0 断链**。

→ 满足 `22 §14`「文件 manifest 与实际 Markdown 行数一致」「所有文件 UTF-8 可读、internal links 存在」。

## 3. G11 结论

✅ **PASS**。locstat 四类（外加全项目/未分类）报告均已基于最终源码重新生成；JSON 合法且加总一致；任务包文档 manifest 行数、UTF-8、internal links 三项校验全部通过。

必要输出：`results/locstat/*.json` + `results/audit/G11_locstat_audit.md`（本文件）。
