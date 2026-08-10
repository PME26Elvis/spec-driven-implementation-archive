# spec-driven-implementation-archive

An archive of fixed, specification-driven software implementation tasks and their implementation runs, intended for manual comparison across execution surfaces and model configurations.

## Execution environment

| Execution Surface | Model | Reasoning Effort |
| --- | --- | --- |
| ChatGPT Chat | GPT-5.6 Sol | High |
| ChatGPT Work | GPT-5.6 Sol | Max |

> [!NOTE]
> These runs use ChatGPT Plus rather than a stateless API harness. No user-installed custom skills were added for these runs. However, the execution context is not fully controlled or observable: ChatGPT may use saved memories or prior chat context when those product features are enabled, and product-level system/developer instructions are not fully exposed to the user. Results should therefore not be interpreted as perfectly reproducible API benchmarks.

## Specifications

Equivalent X11 and Win32 task packs are grouped below as one logical specification.

| Specification | Platforms | Scope |
| --- | --- | --- |
| Markdown Editor | [X11](specs/c17-x11-markdown) / [Win32](specs/c17-win32-markdown) | Native Markdown editor with source, split, preview, rendered editing, workspaces, tabs, images, history, recovery, and extensive verification requirements. |
| 2D Physics Sandbox | [X11](specs/c17-x11-physics-sandbox) / [Win32](specs/c17-win32-physics-sandbox) | Rigid-body physics sandbox and scene editor with collision handling, constraints, diagnostics, persistence, deterministic verification, and automated testing. |
| Pinball Sandbox | [X11](specs/c17-x11-pinball-benchmark) / [Win32](specs/c17-win32-pinball-benchmark) | Deterministic pinball physics sandbox and table editor with edit/play modes, multiball, replay, diagnostics, save/load, and headless verification. |
| Sudoku | [X11](specs/c17-x11-sudoku) / [Win32](specs/c17-win32-sudoku) | Modern 9×9 Sudoku desktop application with a hand-built UI, gameplay assistance, persistence, statistics, automated tests, and strict release gates. |
