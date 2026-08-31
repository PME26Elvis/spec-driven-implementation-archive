# G0 — Scope and Dependency Audit

版本：1.0.0 ｜ 审计对象：交付源码 + `build/bin/*`
源提交：`d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`

> G0 确认来源语言为 C17、构建不下载/不 vendor 禁止库、GUI 仅使用允许的 Kernel32/User32/GDI/Bcrypt RNG 边界、无 native child control / 禁止 renderer、无预置题库、无未揭露 prebuilt 对象。

## 1. 链接库（linked libraries）

构建命令（见 `tools/build_exe.sh`、`tools/build_app.sh`）链接：

| 库 | 用途 | 允许？ |
|---|---|---|
| `kernel32.lib` | 文件 I/O、内存、线程、时间、同步 | ✅ |
| `user32.lib` | 单一 top-level HWND、message、真实输入 | ✅ |
| `gdi32.lib` | 仅 `StretchDIBits` blit | ✅ |
| `bcrypt.lib` | 仅 `BCryptGenRandom` | ✅ |

**无任何其他链接库。** 与 `22 §3` / `02` allowlist 一致。

## 2. 资产来源与用途

| 资产 | 来源 | 用途 |
|---|---|---|
| 全部 `.c` / `.h` 源码 | 本工程自研 | 实现 locstat / tinyvcs / C 测试 harness / 数独应用 / 自研 renderer |
| `app.manifest` | 本工程自研 | 声明 `longPathAware=true` 与 Per-Monitor V2 DPI awareness |
| `build.cmd` | 本工程自研 | 规范构建入口 |
| 测试 fixture（crypto 已知答案向量等） | 内联于 `tests/**/test_*.c` | known-answer 测试，固定字节，非预置题库 |
| tinyvcs 自托管仓库 `.tinyvcs/` | 本工程提交生成 | 版本管理自身源码（G10） |

**无 vendor 的第三方源码、无预编译对象/库、无未揭露 prebuilt 资产。**

## 3. 禁止依赖扫描

在交付源码与构建产物中扫描以下禁止项（grep，大小写不敏感）：

| 禁止项 | 是否在交付物中出现 |
|---|---|
| GTK / Qt / SDL / Cairo | 否 |
| OpenGL / Direct2D / DirectWrite / DWM blur | 否（import 表亦无对应 DLL/符号，见 G15） |
| WebView / WinUI / WPF / Forms / MFC | 否 |
| SQLite / OpenSSL / libsodium | 否（crypto 全部手刻，见 `testcrypto` 40/40） |
| Git backend / libgit2 | 否（tinyvcs 为自研内容寻址 VCS） |
| 预置大量题库 / 答案表 | 否（generator 实时生成，无 shipped puzzle DB） |

## 4. 范围与行为边界

- **来源语言**：C17（`/std:c17`）。
- **Windows 原生边界**：Kernel32 / User32（单一 HWND + 真实输入）/ GDI（仅 blit）/ Bcrypt（仅 `BCryptGenRandom`）。
- **自研 UI engine**：`src/ui/*` 自研 software renderer；无 native child control。
- **无超出 v1.0 必要行为的额外功能**：scope freeze（`16_SCOPE_FREEZE_AND_ACCEPTANCE_POLICY.md`）约束；实现未引入改变 MUST 行为的扩展。

## 5. G0 结论

✅ **PASS**。所有链接库、资产来源与用途均已列明；无禁止依赖、无预置题库、无未揭露 prebuilt 对象；范围与 v1.0 MUST 行为一致。

必要输出：`results/audit/G0_dependency_audit.md`（本文件）+ G15 import 表 `results/audit/pe_imports.txt`。
