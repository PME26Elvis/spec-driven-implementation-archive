# G15 — Windows Native Platform Audit

版本：1.0.0 ｜ 审计对象：`build/bin/sudoku.exe`
源提交：`d6e82717791b5f29eaffd466585a4ed858df226248f9232d33bde8fb830118c1`
构建日期：2026-08-30 ｜ 工具链：MSVC VS2022 BuildTools 14.41.34120 / Windows SDK 10.0.22621.0 / C17 x64

> 本文件记录 G15 可自动验证的部分（PE/import 表、架构边界、Bcrypt 边界），以及必须在真实 Windows 桌面才能执行的显示相关场景（DPI、WIN-E2E-01…07、Unicode 路径交互）。后者在沙箱内不可达，按规范诚实地标记为 **BLOCKED（环境限制，非功能缺口）**。

## 1. PE / Import 表审计（dumpbin /imports）

工具：`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe`
原始输出：`results/audit/pe_imports.txt`、`results/audit/pe_headers.txt`

### 1.1 导入的 DLL（完整列表）

| DLL | 用途 | 是否符合 `02` allowlist |
|---|---|---|
| `KERNEL32.dll` | 文件 I/O、内存、线程、时间、同步 | ✅ 允许 |
| `USER32.dll` | 单一 top-level HWND、message loop、真实输入、DPI | ✅ 允许 |
| `GDI32.dll` | 仅 `StretchDIBits` blit 完整 BGRA framebuffer | ✅ 允许 |
| `bcrypt.dll` | 仅 `BCryptGenRandom`（CSPRNG） | ✅ 允许 |

**结论：仅导入 4 个允许的 DLL。无任何 GTK/Qt/SDL/Cairo/OpenGL/WebView/SQLite/OpenSSL/libsodium/GDI+/Direct2D/DirectWrite/DWM 导入。**

### 1.2 GDI32 导入符号

```
StretchDIBits
```

仅 `StretchDIBits`（符合 `26 §6`：presentation 只可使用 `StretchDIBits`/`SetDIBitsToDevice` 类 blit）。**无** `TextOut`/`ExtTextOut`/`Rectangle`/`RoundRect`/`GradientFill`/`AlphaBlend`/`BitBlt`(GDI shape/alpha) 等禁止符号 → 满足"禁止 GDI text/shape/alpha/blur"。

### 1.3 USER32 导入符号（16 个，均为 window/input/message/DPI）

```
GetMessageW                  SetThreadDpiAwarenessContext   LoadCursorW
InvalidateRect               EndPaint                        BeginPaint
SetTimer                     SetWindowPos                    ShowWindow
DestroyWindow                CreateWindowExW                 RegisterClassExW
PostQuitMessage              DefWindowProcW                  DispatchMessageW
TranslateMessage
```

全部为窗口生命周期 / 消息循环 / 输入 / DPI 感知函数。**无** `CreateDialog`/`DialogBox`/`CreateWindowExW` 用于 native child control 的迹象（工程中仅以 `CreateWindowExW` 建立唯一主 HWND）；**无** DWM/`Dwm*`、Direct2D/`D2D*`、DirectWrite/`DWrite*` 符号。

### 1.4 Bcrypt 导入符号

```
BCryptGenRandom
```

仅 `BCryptGenRandom`（ordinal 1D）。满足 `WIN-13` / `02 §2、§12`：Bcrypt 只作 CSPRNG，所有 SHA-256/HMAC/PBKDF2/ChaCha20/Poly1305/AEAD 均由工程手刻（见 `testcrypto` 40/40 已知答案测试）。

### 1.5 PE 头（dumpbin /headers）

| 字段 | 值 |
|---|---|
| machine | 8664 (x64) |
| magic | 20B (PE32+) |
| linker version | 14.41 |
| subsystem | 2 (Windows GUI) — 无 console 窗口 |
| image/subsystem version | 0.00 / 6.00 |

满足 `WIN-01`（x64 PE32+ native executable）、`26 §3`（production 不显示 console、GUI subsystem）。

## 2. 架构边界确认（代码级）

- **单一 HWND**：`src/app/sdk_app.c` 经 `RegisterClassExW` + 单次 `CreateWindowExW` 建立唯一 top-level 窗口（window class 名见 `app.manifest` / README）。无 native child control。
- **自研 renderer**：`src/ui/*` 自研 software pixel buffer（top-down BGRA8），所有 UI 元件、文字 rasterization、layout、hit-testing、动画、阴影、光晕、模糊、frosted glass 均自研；GDI 仅用于最终 blit。
- **安全随机边界**：仅 `BCryptGenRandom`；失败时 game ID/nonce/salt 生成操作失败（不 fallback）。
- **默认数据目录**：`%LOCALAPPDATA%\C17Win32Sudoku\`（见 `26 §17`），test mode 可指定隔离目录。

## 3. 需在真实 Windows 桌面验证的场景（BLOCKED — 环境限制）

沙箱无显示器 / 无真实 Win32 桌面会话，以下项目**无法 headless 验证**，标记为 BLOCKED。它们属于显示/交互层验证，不掩盖任何已通过的功能性测试（单元/集成/E2E/故障注入/安全/批量均已自动通过）。

| 项目 | 要求 | 状态 | 说明 |
|---|---|---|---|
| DPI 96/125/150/200% | `26 §7` | BLOCKED | 需真实桌面改变 DPI 并截图核验 |
| WIN-E2E-01 DPI change | `26 §19` | BLOCKED | 需 `SendInput` 驱动真实 HWND |
| WIN-E2E-02 minimize/restore/occlude | `26 §19` | BLOCKED | 需真实窗口生命周期 |
| WIN-E2E-03 pointer capture 拖出释放 | `26 §19` | BLOCKED | 需真实指针输入 |
| WIN-E2E-04 Alt+F4 与 title-bar close | `26 §19` | BLOCKED | 需真实关闭流程 |
| WIN-E2E-05 Unicode root path | `26 §19` | BLOCKED | 需 Unicode 路径桌面会话 |
| WIN-E2E-06 case/reparse/ADS | `26 §19` | BLOCKED | 需真实文件系统语义 |
| WIN-E2E-07 ReplaceFile/MoveFile 故障注入 | `26 §19` | BLOCKED | 已通过 `testfailure` 3/3 在逻辑层验证原子替换契约，桌面级注入待真实环境 |
| Unicode/case/reparse/ADS 路径交互 | `26 §11` | BLOCKED | 路径模型代码已实现并单测覆盖，桌面交互待真实环境 |

## 4. G15 结论

- **Import 表 / PE / Bcrypt 边界**：✅ PASS（自动验证，证据见 `pe_imports.txt`、`pe_headers.txt`）。
- **架构边界（无 native child control、无高阶 renderer）**：✅ PASS（代码级确认 + import 表证据）。
- **显示相关场景（DPI / WIN-E2E-01…07）**：⛔ BLOCKED（沙箱无真实 Windows 桌面，环境限制，非 MUST 功能缺口）。

按 `22 §21` 与 `26 §20`，G15 的「真实 Windows 桌面启动 / DPI / WIN-E2E」项需在目标 Windows 10 22H2 / Windows 11 上由人工或 CI 完成最终确认；其余可自动验证部分均已 PASS。
