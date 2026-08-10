# 13 — Command Palette, Recent Items, Preferences, Themes, and Keyboard-Only Operation

## 1. Command Architecture

The application MUST expose a coherent command model used by menus, toolbar controls, shortcuts, and the Command Palette.

A mandatory command MUST NOT be implemented as separate unrelated code paths whose enabled/disabled state visibly disagrees between menu and palette.

Each command SHOULD have:

- Stable internal identifier.
- User-visible label.
- Enabled/disabled predicate.
- Optional shortcut.
- Execution handler.

The exact internal architecture is not prescribed, but observable command state MUST be consistent.

## 2. Command Palette

`Ctrl+Shift+P` MUST open the Command Palette.

The palette MUST be custom-drawn and integrated with the application's modal/overlay design.

It MUST provide a text query field and a scrollable result list.

## 3. Palette Search

Palette search MUST match command labels using case-insensitive ASCII substring matching at minimum.

A stronger fuzzy matcher MAY be implemented.

Results MUST update as the query changes.

Disabled commands MAY remain visible but MUST be visually disabled and MUST NOT execute.

## 4. Mandatory Palette Commands

At minimum the palette MUST expose commands for:

- New Document.
- Open File.
- Open Workspace.
- Save.
- Save As.
- Save All.
- Close Tab.
- Find.
- Replace.
- Toggle Source Mode.
- Toggle Split Mode.
- Toggle Preview Mode.
- Toggle Rendered Editing Mode.
- Insert Image.
- Document Statistics.
- Version History.
- Toggle Files Sidebar.
- Toggle Outline Sidebar.
- Toggle synchronized scrolling.
- Toggle Light/Dark Theme.
- Preferences.
- Shortcut Reference.

Commands relevant only in a specific context MUST expose correct enabled state.

## 5. Palette Keyboard Operation

When palette opens, keyboard focus MUST enter its query field.

Required keys:

- Up/Down: move result selection.
- Enter: execute selected enabled command.
- Escape: close without action.
- PageUp/PageDown: navigate long result lists when present.

Closing the palette MUST restore focus to the previously active logical control/document surface.

## 6. Recent Workspaces

The application MUST persist a recent-workspace list.

At least the 10 most recent distinct workspace paths MUST be retained.

Reopening an existing workspace MUST move it to the most-recent position rather than create a duplicate.

The start/home surface and File menu MUST provide access to recent workspaces.

## 7. Recent Files

The application MUST persist at least the 20 most recent distinct Markdown/text file paths opened outside or inside workspaces.

A recent-file entry SHOULD include display name and abbreviated parent path sufficient to disambiguate same-named files.

Opening the same file again MUST move it to the front rather than duplicate it.

## 8. Missing Recent Items

When a recent file/workspace no longer exists:

- The application MUST NOT crash.
- The entry MUST be marked unavailable or removed after failed activation.
- The user MUST be able to remove that entry.

A `Clear Recent` command MUST be available for files and workspaces.

## 9. Home / Start Surface

When no document is open, the application MUST show a useful start surface rather than a broken empty editor.

It MUST expose at least:

- New Document.
- Open File.
- Open Workspace.
- Recent Workspaces.
- Recent Files.

The start surface MUST obey the current theme and keyboard focus rules.

## 10. Preferences Persistence

User preferences MUST persist across application restarts independently of workspace metadata.

Preferences MUST be stored under the per-user Roaming AppData application location defined by `16_WINDOWS_PLATFORM_CONTRACT.md`, independently of workspace metadata and the executable directory.

Corrupt preference data MUST fall back safely to defaults while preserving the corrupt file for diagnosis or reporting the reset.

## 11. Mandatory Preferences

The Preferences UI MUST allow changing at least:

- Theme: Light or Dark.
- Editor base font size.
- Editor line spacing.
- Default image insertion storage mode: relative asset or embedded Base64.
- Autosave enabled/disabled.
- Autosave interval, constrained to 10–300 seconds.
- Default document editing mode.
- Split synchronized scrolling enabled/disabled.
- Restore workspace session on startup enabled/disabled.

The autosave preference affects recovery cadence but disabling periodic autosave MUST display an explicit warning because crash recovery coverage is reduced. A final recovery write on orderly shutdown is still required for unresolved dirty documents unless the user discards them.

## 12. Theme Requirement

Both Light and Dark themes are mandatory.

Theme changes MUST apply without application restart.

All mandatory custom controls and editor surfaces MUST have deliberate colors in both themes.

The application MUST NOT implement Dark theme by merely inverting the final framebuffer.

## 13. Theme Coverage

Both themes MUST cover:

- Window/background surfaces.
- Navigation/frosted region.
- Menus.
- Toolbar.
- Tabs.
- Sidebar/file tree/outline.
- Source editor.
- Rendered editor/preview.
- Code blocks.
- Tables.
- Links.
- Selection.
- Caret.
- Status bar.
- Context menus.
- Dialogs/modals.
- Command Palette.
- Diff colors for added/deleted/modified content.
- Error/warning/success states.

Diff meaning MUST remain distinguishable without relying solely on hue; glyphs, bars, labels, or patterns MUST reinforce meaning.

## 14. Font Size and Line Spacing

The Preferences UI MUST expose an editor font size range of at least 10–32 logical pixels/points-equivalent.

`Ctrl+MouseWheel` document zoom remains a per-document/session zoom and MUST NOT rewrite the saved base font preference.

Line spacing MUST provide at least Compact, Normal, and Relaxed presets or an equivalent numeric control spanning at least 1.0–1.8 line-height equivalents.

## 15. Keyboard Focus Model

Every mandatory interactive control MUST participate in a logical keyboard focus model unless explicitly documented as pointer-only visual decoration.

Focus MUST be visibly indicated.

Tab MUST move forward through reachable controls in the current focus scope.

Shift+Tab MUST move backward.

Focus order MUST follow the visual/logical layout sufficiently that a user can predict navigation.

## 16. Modal Focus Trap

When a modal dialog is active:

- Tab/Shift+Tab MUST remain within the modal's focusable controls.
- Background controls MUST not receive keyboard activation.
- Escape MUST close cancellable modals.
- Enter MUST activate the primary/default action when doing so cannot cause an unsafe ambiguous destructive operation.

Closing the modal MUST restore focus to the initiating control or prior editor position.

## 17. Menu Keyboard Operation

Application menus MUST be operable without a pointer.

At minimum:

- Keyboard invocation of the top-level menu system MUST exist.
- Arrow keys MUST traverse open menu items.
- Enter/Space MUST activate the selected enabled item.
- Escape MUST close current menu level.

Exact Alt-letter mnemonics are optional.

## 18. Tabs Keyboard Operation

Required:

- `Ctrl+Tab`: next tab.
- `Ctrl+Shift+Tab`: previous tab.
- `Ctrl+W`: close active tab.
- Keyboard-accessible command to move focus to the tab strip.
- Left/Right when tab strip focused: move active/focused tab selection.

Tab reorder by keyboard is optional; pointer drag reorder remains required elsewhere.

## 19. Workspace Tree Keyboard Operation

When the file tree has focus:

- Up/Down moves selection.
- Right expands a collapsed directory or moves to first child when expanded.
- Left collapses an expanded directory or moves to parent when collapsed.
- Enter opens selected file or toggles selected directory.
- F2 invokes Rename.
- Delete invokes the guarded delete flow.
- Escape cancels inline rename/context operation.

## 20. Outline Keyboard Operation

When Outline has focus:

- Up/Down moves heading selection.
- Enter navigates to heading.
- Home/End moves to first/last heading.

## 21. Context Menu Keyboard Operation

A focused object/selection MUST have a keyboard-accessible way to open its context menu, using the Menu key, `Shift+F10`, or both.

Once open, the context menu MUST use the same arrow/Enter/Escape semantics as ordinary menus.

## 22. Editor Keyboard Baseline

In addition to shortcuts specified elsewhere, the editor MUST support:

- Arrow-key caret navigation.
- Shift+Arrow selection extension.
- Ctrl+Left/Right word-wise navigation using the editor's deterministic word-boundary rules.
- Home/End line navigation.
- Ctrl+Home/Ctrl+End document navigation.
- PageUp/PageDown.
- Backspace/Delete.
- Enter.
- Tab according to current construct/context.

## 23. Shortcut Reference

The application MUST include an in-app shortcut-reference surface listing all default mandatory shortcuts.

The list MUST be generated from or kept consistent with actual command bindings; a knowingly stale static help panel fails acceptance.

## 24. Preference Failure Handling

If preference write fails:

- The in-memory setting MAY apply for the current session.
- The UI MUST notify the user that persistence failed.
- Existing preference data MUST not be truncated to an invalid empty file.

## 25. Mandatory Acceptance Cases

The release suite MUST demonstrate:

1. Open Command Palette and execute Save using keyboard only.
2. Search palette for `stat` and open Document Statistics.
3. Verify a context-disabled command cannot execute.
4. Persist and reopen at least 10 recent workspaces without duplicates.
5. Activate a missing recent path and remove it safely.
6. Switch Light ↔ Dark without restart and capture both required screenshots.
7. Persist font size, line spacing, image mode, autosave interval, and default editor mode.
8. Complete New → type → Save As → Close using keyboard only.
9. Navigate file tree and open a Markdown file using keyboard only.
10. Navigate Outline and jump to a heading using keyboard only.
11. Demonstrate modal focus trapping and focus restoration.
