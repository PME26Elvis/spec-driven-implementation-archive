# spec-driven-implementation-archive

An archive of fixed, specification-driven software implementation tasks and their implementation runs, intended for manual comparison across execution surfaces and model configurations.

Current archive: **11 logical specifications**, represented by **17 specification directories**, with **4 recorded implementation runs**.

## Execution environment

| Execution Surface | Model | Reasoning / Mode | Account Tier Represented |
| --- | --- | --- | --- |
| ChatGPT Chat | GPT-5.6 Sol | High | Plus |
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
| Virtual Piano | [Win32](specs/c17-win32-virtual-piano) | — | Native C17 virtual piano with a hand-built software-rendered UI, polyphonic synthesis, customizable keyboard mapping, chord recognition, WAV recording, a headless companion, DPI scaling, and deterministic acceptance requirements. |
| Analog Clock Workbench | [X11](specs/c17-x11-clock-workbench) / [Win32](specs/c17-win32-clock-workbench) | — | Interactive C17 analog clock workbench with hand-built rendering, analog/digital time interaction, playback controls, configuration, undo/redo, and deterministic verification requirements. |
| CVC | [POSIX](specs/c17-posix-cvc) / [Win32](specs/c17-win32-cvc) | — | Local C17 version-control system with content-addressed storage, commits, branches, restore-through-new-commit semantics, diff, merge-base and three-way merge, conflict handling, recovery-safe writes, and repository verification. |
| TableTool | [Portable C17](specs/c17-tabletool) | — | Pure CLI typed table-management tool with structured tabular I/O, scriptable transformations, deterministic sorting and filtering, URL semantics, barcode support, validation, and file-based outputs. |
| Elevator Group Control Simulator | [Portable C17](specs/c17-elevator-group-control) | — | Deterministic multi-elevator group-control simulator with scenario files, passenger traces, multiple dispatch strategies, discrete-time kinematics, capacity and door timing, metrics, replay, and fixed acceptance fixtures. |

> [!NOTE]
> Spec token estimates come from the **Count spec tokens** workflow using Repomix with the `o200k_base` encoding. Rows marked `—` have not yet had a token count recorded in this README. Run the workflow after specification additions to refresh those values. Token counts are estimates of checked-in specification artifacts, not model inference usage.

## Recorded runs

| Specification | Run | Execution Surface | Repomix token estimate | Notes |
| --- | --- | --- | ---: | --- |
| Markdown Editor (X11) | [2026-08-10-gpt-work-5.6-sol-max](runs/c17-x11-markdown/2026-08-10-gpt-work-5.6-sol-max) | ChatGPT Work · GPT-5.6 Sol · Max | 161,533 | Plus; run README records timing, weekly-usage consumption, Repomix statistics, and pending manual testing. |
| DARC v0.1.0 | [2026-08-12-grok-4.5-fast-web](runs/c17-darc-v0.1.0/2026-08-12-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | 49,490 | Free web run; run README includes Repomix statistics and a later independent GPT-5.6 Sol review whose release verdict is FAIL. |
| 2D Physics Sandbox (X11) | [2026-08-14-grok-4.5-fast-web](runs/c17-x11-physics-sandbox/2026-08-14-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | 74,017 | Free web run; Repomix records 81 files, with a later independent GPT-5.6 Sol review assessing the v1.0 release as BLOCKED. |
| Embedded Database Engine (X11) | [2026-08-17-grok-4.5-fast-web](runs/c17-x11-embedded-database-engine/2026-08-17-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | — | Free web run; archive includes the submitted project snapshot and conversation export, with a later independent GPT-5.6 Sol review assessing the v1.0 release as **FAIL / BLOCKED**. |

> [!NOTE]
> Repomix token estimates measure only the implementation artifacts included in each recorded run snapshot and selected by that run's Repomix counting rules. They do not include conversational output unless it was explicitly saved into the project, and they cannot include platform-hidden reasoning/thoughts, hidden internal context, or other non-exported model activity. Treat these figures as estimates of implementation-artifact size, not as total inference token consumption, total product usage, or cost.
