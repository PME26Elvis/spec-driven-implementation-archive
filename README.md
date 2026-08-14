# spec-driven-implementation-archive

An archive of fixed, specification-driven software implementation tasks and their implementation runs, intended for manual comparison across execution surfaces and model configurations.

Current archive: **5 logical specifications**, represented by **9 specification directories**, with **3 recorded implementation runs**.

## Execution environment

| Execution Surface | Model | Reasoning / Mode | Account Tier Represented |
| --- | --- | --- | --- |
| ChatGPT Chat | GPT-5.6 Sol | High | Plus |
| ChatGPT Work | GPT-5.6 Sol | Max | Plus |
| Grok Web | Grok 4.5 | Fast | Free (recorded runs) |

> [!NOTE]
> Recorded runs use end-user product surfaces rather than a stateless API harness. ChatGPT runs in this archive use ChatGPT Plus, while the currently recorded Grok runs used the free Grok web surface. These environments are not fully controlled or observable and may carry product- or account-level state such as prior conversation context, memory/personalization when available and enabled, UI orchestration, model routing, tool availability, and hidden system/developer instructions. Exact behavior differs by product and may change over time. No user-installed custom skills were added to the recorded ChatGPT runs unless a run states otherwise. Results should therefore be treated as observations of the named product surfaces, not as perfectly reproducible API benchmarks.

Run-specific timing, usage, implementation statistics, validation evidence, and review notes are kept with each run rather than normalized across products.

## Specifications

Equivalent X11 and Win32 task packs are grouped below as one logical specification where applicable.

| Specification | Platforms | Scope |
| --- | --- | --- |
| Markdown Editor | [X11](specs/c17-x11-markdown) / [Win32](specs/c17-win32-markdown) | Native Markdown editor with source, split, preview, rendered editing, workspaces, tabs, images, history, recovery, and extensive verification requirements. |
| 2D Physics Sandbox | [X11](specs/c17-x11-physics-sandbox) / [Win32](specs/c17-win32-physics-sandbox) | Rigid-body physics sandbox and scene editor with collision handling, constraints, diagnostics, persistence, deterministic verification, and automated testing. |
| Pinball Sandbox | [X11](specs/c17-x11-pinball-benchmark) / [Win32](specs/c17-win32-pinball-benchmark) | Deterministic pinball physics sandbox and table editor with edit/play modes, multiball, replay, diagnostics, save/load, and headless verification. |
| Sudoku | [X11](specs/c17-x11-sudoku) / [Win32](specs/c17-win32-sudoku) | Modern 9×9 Sudoku desktop application with a hand-built UI, gameplay assistance, persistence, statistics, automated tests, and strict release gates. |
| DARC v0.1.0 | [Linux/POSIX](specs/c17-darc-v0.1.0) | Headless C17 deterministic deduplicating archive with content-defined chunking, chunk-level deduplication, self-implemented compression, cryptographic IDs, Merkle integrity, snapshots/diff/restore, JSON/YAML configuration, GC, repository verification, crash-safe publication, corruption detection/recovery, and strict release gates. |

## Recorded runs

| Specification | Run | Execution Surface | Notes |
| --- | --- | --- | --- |
| Markdown Editor (X11) | [2026-08-10-gpt-work-5.6-sol-max](runs/c17-x11-markdown/2026-08-10-gpt-work-5.6-sol-max) | ChatGPT Work · GPT-5.6 Sol · Max | Plus; run README records timing, weekly-usage consumption, Repomix statistics, and pending manual testing. |
| DARC v0.1.0 | [2026-08-12-grok-4.5-fast-web](runs/c17-darc-v0.1.0/2026-08-12-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | Free web run; run README includes Repomix statistics and a later independent GPT-5.6 Sol review whose release verdict is FAIL. |
| 2D Physics Sandbox (X11) | [2026-08-14-grok-4.5-fast-web](runs/c17-x11-physics-sandbox/2026-08-14-grok-4.5-fast-web) | Grok Web · Grok 4.5 · Fast | Free web run; Repomix records 81 files / 74,017 tokens, with a later independent GPT-5.6 Sol review assessing the v1.0 release as BLOCKED. |
