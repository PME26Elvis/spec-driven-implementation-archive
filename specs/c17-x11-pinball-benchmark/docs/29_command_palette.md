# 29 — Command Palette

## 1. Requirement

The application SHALL provide a keyboard-accessible Command Palette for fast access to existing commands. It does not add hidden capabilities that cannot otherwise be accessed where the main UI requires discoverability.

## 2. Open/close

Default shortcut SHOULD be `Ctrl+Shift+P`; if another binding is chosen it SHALL be documented and conflict-free.

Open behavior:

- palette appears as modal-like overlay;
- search field receives focus;
- background editor input is captured;
- Escape closes and restores focus;
- invoking command closes palette unless command explicitly opens another modal.

## 3. Required commands

At minimum searchable entries for:

- New;
- Open;
- Save;
- Save As;
- Undo;
- Redo;
- Validate Scene;
- Edit Mode;
- Play/Pause;
- Single Step;
- Restart;
- Fit Scene;
- Fit Selection;
- Zoom 100%;
- Toggle Grid;
- Toggle Snap;
- Toggle Physics Inspector;
- Toggle Event Trace;
- Scene Statistics;
- Measurement Tool;
- Toggle Reduced Motion;
- layer/group alignment commands when applicable.

Disabled context commands remain visible with disabled state or are omitted consistently; they must not execute invalid operations.

## 4. Search

Search is incremental and SHALL support:

- ASCII case-insensitive substring match;
- exact Chinese UTF-8 substring match for localized/display aliases if any;
- deterministic ranking: prefix match before substring, then authored/static command order.

Fuzzy edit-distance matching is optional.

## 5. Keyboard navigation

Up/Down moves result selection, Enter executes selected enabled command, Home/End move to first/last result.

Mouse selection is also required.

## 6. No-result state

No-match state displays a clear message and Enter performs no action.

## 7. History

Remembering recently used commands for ranking is optional. If used, it must not make required command search nondeterministic in tests unless history is resettable.

## 8. Acceptance

Required automated/state tests:

- open focus;
- query filtering;
- prefix-before-substring ranking;
- disabled command cannot execute;
- keyboard/mouse execution;
- Escape/focus restore;
- Chinese substring preservation;
- command invokes the same production action as visible UI control.
