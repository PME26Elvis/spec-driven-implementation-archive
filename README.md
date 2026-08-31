# spec-driven-implementation-archive

An archive of fixed, specification-driven software implementation tasks and their implementation runs, intended for manual comparison across execution surfaces and model configurations.

Current archive: **11 logical specifications**, represented by **17 specification directories**, with **9 recorded implementation runs**.

## Execution environment

| Execution Surface | Model | Reasoning / Mode | Account Tier Represented |
| --- | --- | --- | --- |
| ChatGPT Chat | GPT-5.6 Sol | High / Medium (run-specific) | Plus |
| ChatGPT Work | GPT-5.6 Sol | Max | Plus |
| Grok Web | Grok 4.5 | Fast | Free (recorded runs) |
| [Tencent WorkBuddy](platforms/workbuddy.zh-TW.md) | Hy3 / selectable models | Agentic desktop; model-dependent | Run-specific |

> [!NOTE]
> Recorded runs use end-user product surfaces rather than a stateless API harness. ChatGPT runs in this archive use ChatGPT Plus, while the currently recorded Grok runs used the free Grok web surface. These environments are not fully controlled or observable and may carry product- or account-level state such as prior conversation context, memory/personalization when available and enabled, UI orchestration, model routing, tool availability, and hidden system/developer instructions. Exact behavior differs by product and may change over time. No user-installed custom skills were added to the recorded ChatGPT runs unless a run states otherwise. Results should therefore be treated as observations of the named product surfaces, not as perfectly reproducible API benchmarks.
>
> WorkBuddy is a newer, less familiar execution surface whose behavior depends on more than the selected foundation model. Detailed platform profiles: [繁體中文](platforms/workbuddy.zh-TW.md) · [English](platforms/workbuddy.md). They cover its architecture, Hy3, model selection, permissions, Skills/MCP/Connectors, memory, privacy, enterprise/runtime model, benchmark implications, and comparisons with Antigravity, Codex, Claude Code, Grok Build, and Pi.

Run-specific timing, usage, implementation statistics, validation evidence, and review notes are kept with each run rather than normalized across products.

## Specifications

Equivalent X11 and Win32 task packs are grouped below as one logical specification where applicable.

| Specification | Platforms | Spec token estimate | Scope |
| --- | --- | ---: | --- |
| Markdown Editor | [X11](specs/c17-x11-markdown) / [Win32](specs/c17-win32-markdown) | X11: 39,576 · Win32: 49,398 | Native Markdown editor with source, split, preview, rendered editing, workspaces, tabs, images, history, recovery, and extensive verification requirements. |
| 2D Physics Sandbox | [X11](specs/c17-x11-physics-sandbox) / [Win32](specs/c17-win32-physics-sandbox) | X11: 73,866 · Win32: 83,740 | Rigid-body physics sandbox and scene editor with collision handling, constraints, diagnostics, persistence, deterministic verification, and automated testing. |
| Pinball Sandbox | [X11](specs/c17-x11-pinball-benchmark) / [Win32](specs/c17-win32-pinball-benchmark) | X11: 255,261 · Win32: 270,774 | Deterministic pinball physics sandbox and table editor with edit/play modes, multiball, replay, diagnostics, save/load, and headless verification. |
| Sudoku | [X11](specs/c17-x11-sudoku) / [Win32](specs/c17-win32-sudoku) | X11: 67,016 · Win32: 75,059 | Modern 9×9 Sudoku desktop application with a hand-built UI, gameplay assistance, persistence, statistics, automated tests, and strict release gates. |
| Embedded Database Engine | [X11 / Linux](specs/c17-x11-embedded-database-engine) | 36,870 | Self-contained C17 relational database engine and native X11 workbench with page-oriented persistence, B+ tree and composite indexes, SQL execution, snapshot-isolation MVCC, WAL/recovery, authenticated encryption, integrity checking, salvage, CLI/GUI front ends, and strict release gates. |
| DARC v0.1.0 | [Linux/POSIX](specs/c17-darc-v0.1.0) | 34,641 | Headless C17 deterministic deduplicating archive with content-defined chunking, chunk-level deduplication, self-implemented compression, cryptographic IDs, Merkle integrity, snapshots/diff/restore, JSON/YAML configuration, GC, repository verification, crash-safe publication, corruption detection/recovery, and strict release gates. |
| Virtual Piano | [Win32](specs/c17-win32-virtual-piano) | 35,298 | Native C17 virtual piano with a hand-built software-rendered UI, polyphonic synthesis, customizable keyboard mapping, chord recognition, WAV recording, a headless companion, DPI scaling, and deterministic acceptance requirements. |
| Analog Clock Workbench | [X11](specs/c17-x11-clock-workbench) / [Win32](specs/c17-win32-clock-workbench) | X11: 39,557 · Win32: 47,431 | Interactive C17 analog clock workbench with hand-built rendering, analog/digital time interaction, playback controls, configuration, undo/redo, and deterministic verification requirements. |
| CVC | [POSIX](specs/c17-posix-cvc) / [Win32](specs/c17-win32-cvc) | POSIX: 26,721 · Win32: 35,695 | Local C17 version-control system with content-addressed storage, commits, branches, restore-through-new-commit semantics, diff, merge-base and three-way merge, conflict handling, recovery-safe writes, and repository verification. |
| TableTool | [Portable C17](specs/c17-tabletool) | 31,399 | Pure CLI typed table-management tool with structured tabular I/O, scriptable transformations, deterministic sorting and filtering, URL semantics, barcode support, validation, and file-based outputs. |
| Elevator Group Control Simulator | [Portable C17](specs/c17-elevator-group-control) | 52,744 | Deterministic multi-elevator group-control simulator with scenario files, passenger traces, multiple dispatch strategies, discrete-time kinematics, capacity and door timing, metrics, replay, and fixed acceptance fixtures. |

> [!NOTE]
> Spec token estimates come from the **Count spec tokens** workflow using Repomix with the `o200k_base` encoding. The latest completed count (workflow run 6, 2026-08-30) covers all 17 specification directories and totals **1,255,046 tokens**. When a spec contains large data-only fixture corpora, a reviewed override under [`analysis/repomix/specs/`](analysis/repomix/specs/) may exclude those corpora while retaining normative specification documents; the policy and rationale are documented in [`analysis/repomix/README.md`](analysis/repomix/README.md). Token counts are estimates of authored specification artifacts, not model inference usage.

## Recorded runs

| Specification | Run | Execution Surface | Repomix token estimate | Notes |
| --- | --- | --- | ---: | --- |
| Markdown Editor (X11) | [2026-08-10-gpt-work-5.6-sol-max](runs/c17-x11-markdown/2026-08-10-gpt-work-5.6-sol-max) | ChatGPT Work · GPT-5.6 Sol · Max | 161,533 | Plus; run README records timing, weekly-usage consumption, Repomix statistics, and pending manual testing. |
| DARC v0.1.0 | [2026-08-12-grok-4.5-fast-web](runs/c17-darc-v0.1.0/2026-08-12-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | 49,490 | Free web run; run README includes Repomix statistics and a later independent GPT-5.6 Sol review whose release verdict is FAIL. |
| 2D Physics Sandbox (X11) | [2026-08-14-grok-4.5-fast-web](runs/c17-x11-physics-sandbox/2026-08-14-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | 74,017 | Free web run; Repomix records 81 files, with a later independent GPT-5.6 Sol review assessing the v1.0 release as BLOCKED. |
| Embedded Database Engine (X11) | [2026-08-17-grok-4.5-fast-web](runs/c17-x11-embedded-database-engine/2026-08-17-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | — | Free web run; archive includes the submitted project snapshot and conversation export, with a later independent GPT-5.6 Sol review assessing the v1.0 release as **FAIL / BLOCKED**. |
| Pinball Sandbox (Win32) | [2026-08-17-hy3-192k-workbuddy-default](runs/c17-win32-pinball-benchmark/2026-08-17-hy3-192k-workbuddy-default) | Tencent WorkBuddy · Hy3 · High | 114,644 | Default-style code-development run using the time-limited free Hy3 access; no manual experts, skills, or connectors enabled. The submitted project reports all automated/headless gates passing while live-GUI gates remain NOT RUN. |
| Sudoku (Win32) | [2026-08-20-hy3-192k-workbuddy-code-development](runs/c17-win32-sudoku/2026-08-20-hy3-192k-workbuddy-code-development) | Tencent WorkBuddy · Hy3 · High | — | Time-limited free Hy3 run with a 192k context window in Code Development mode; no experts, skills, or connectors were manually enabled. Git stores a curated source/evidence snapshot while raw-workspace provenance and checksum are recorded with the run. |
| DARC v0.1.0 | [2026-08-31-gpt-chat-5.6-sol-high](runs/c17-darc-v0.1.0/2026-08-31-gpt-chat-5.6-sol-high) | ChatGPT Chat · GPT-5.6 Sol · High | — | Plus Chat run; final submitted package and conversation are archived, with generated evidence excluded only from the normalized authored-corpus metric. |
| Elevator Group Control Simulator | [2026-08-31-gpt-chat-5.6-sol-medium](runs/c17-elevator-group-control/2026-08-31-gpt-chat-5.6-sol-medium) | ChatGPT Chat · GPT-5.6 Sol · Medium | — | Plus Chat run; Git stores the implementation-owned curated snapshot while the large generated evidence corpus and duplicate task-pack inputs are tracked by archive provenance. |
| CVC (POSIX) | [2026-08-31-gpt-chat-5.6-sol-high](runs/c17-posix-cvc/2026-08-31-gpt-chat-5.6-sol-high) | ChatGPT Chat · GPT-5.6 Sol · High | — | Plus Chat run; final release package and conversation are archived, with generated evidence logs excluded only from normalized Repomix measurement. |

> [!NOTE]
> The table above preserves each run's **historical run-specific Repomix measurement** and counting rules. It does not retroactively rewrite those observations. They measure only implementation artifacts selected by the original run's rules and do not include conversational output unless explicitly saved into the project, platform-hidden reasoning/thoughts, hidden internal context, or other non-exported model activity.

### Normalized implementer-authored corpus

For cross-run implementation-size comparisons, the archive now also maintains a separate normalized metric using a **reviewed per-run Repomix config**. Each config lives under the run's `analysis/repomix/` directory rather than inside the archived `project/`, so analysis metadata does not alter or masquerade as part of the submitted implementation. See [`analysis/repomix/README.md`](analysis/repomix/README.md) for the policy.

| Recorded run | Authored files | Authored tokens (`o200k_base`) |
| --- | ---: | ---: |
| Markdown Editor (X11) · GPT-5.6 Sol Max | 42 | 231,852 |
| DARC v0.1.0 · Grok 4.5 Fast | 36 | 49,326 |
| 2D Physics Sandbox (X11) · Grok 4.5 Fast | 76 | 59,873 |
| Embedded Database Engine (X11) · Grok 4.5 Fast | 76 | 113,373 |
| Pinball Sandbox (Win32) · WorkBuddy Hy3 | 53 | 112,534 |
| Sudoku (Win32) · WorkBuddy Hy3 192k | 71 | 218,447 |
| DARC v0.1.0 · GPT-5.6 Sol High | 52 | 160,382 |
| Elevator Group Control · GPT-5.6 Sol Medium | 13 | 74,992 |
| CVC POSIX · GPT-5.6 Sol High | 40 | 86,167 |

These normalized measurements were most recently validated for all nine recorded runs on 2026-08-31. They exclude only snapshot-reviewed non-authored material such as compiled outputs, generated evidence/reports, logs/traces, mechanically expanded data corpora, and byte-identical benchmark inputs already preserved in the canonical specification tree; the exact exclusions and rationale are stored beside each recorded run.
