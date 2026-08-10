# 16 — Desktop Interaction, Text Fields, File Picker, and Shortcuts

## 1. Purpose

The application is a desktop editor, not only a simulation canvas. Because high-level GUI frameworks are prohibited, standard desktop workflows still need coherent custom implementation.

## 2. In-app file picker

Open and Save As MUST use a usable in-application file-selection modal or equivalent custom desktop UI. A user must not be required to edit source code or launch the application with a new CLI path merely to open/save a table.

The task package does not prescribe the internal enumeration algorithm, but the Windows variant SHALL handle drive-root navigation, normal directories, spaces, and Unicode path components through a Unicode-safe platform boundary. The required picker remains custom-drawn; native Common Dialog/IFileDialog replacement is prohibited.

## 3. Open dialog requirements

Open dialog provides:

- current directory display;
- parent-directory navigation;
- directory entries;
- regular-file entries;
- `.pbt` filtering or clear identification;
- selectable file row;
- file/path name field or equivalent;
- Open action;
- Cancel action;
- scrolling for long directories;
- readable error if directory/file cannot be read.

Double-click-to-open is SHOULD, not mandatory.

## 4. Save As dialog requirements

Save As provides:

- current directory display;
- directory navigation;
- filename field;
- `.pbt` extension assistance;
- Save action;
- Cancel action;
- overwrite confirmation when target exists;
- clear error on write/path failure.

If user enters filename without extension, implementation SHOULD append `.pbt`. If it does, the final path must be visible/understandable before or after save.

## 5. Overwrite confirmation

Saving over an existing file requires an explicit confirmation modal. Canceling returns to Save As without losing filename/path entry.

## 6. File picker safety

- no shell-command construction from path strings;
- spaces in paths work;
- unreadable directories report error;
- non-file entries cannot be opened as `.pbt` by accident;
- directory list refreshes after navigation;
- modal interaction blocks background editor controls.

## 7. File picker visual behavior

File picker follows the same custom UI system, modal scale/opacity transition, blurred backdrop, focus indication, scroll behavior, and keyboard navigation baseline as other dialogs.

## 8. Text field baseline

Custom text/numeric fields MUST implement:

- visible caret when focused;
- pointer click to focus;
- insertion at caret;
- Backspace;
- Delete;
- Left/Right movement;
- Home/End;
- basic selection with Shift+Left/Right or pointer drag for fields where selection is implemented;
- Enter commit where appropriate;
- Escape cancel/revert current edit where appropriate.

For simple numeric fields, full multiline editing is not required.

## 9. UTF-8 text

Table display name and file/path text handling MUST be UTF-8 safe at the byte/storage level. Cursor movement must not split a multibyte UTF-8 sequence.

Object IDs remain ASCII by specification.

The application MAY use the low-level Win32 font/glyph acquisition path permitted by document 32 (for example GDI font selection, glyph metrics, and glyph bitmap/outline acquisition) and Win32 Unicode/IME input services. This permission does not allow native EDIT controls, DirectWrite text layout/rendering, GDI TextOut/DrawText as the required UI text renderer, or any high-level widget/text-layout framework.

## 10. Unsupported glyph behavior

The application MUST remain stable if the selected Windows font path lacks a glyph. Replacement/fallback glyph is acceptable. Crashing or corrupting text memory is not.

## 11. Numeric field behavior

Numeric fields:

- show current exact authored value;
- allow temporary incomplete text while editing, e.g. `-` or `1.`;
- validate on commit;
- reject NaN/inf and out-of-range values;
- visibly indicate invalid state;
- do not mutate the authored scene until valid commit unless a live-preview design can fully roll back invalid/canceled edit.

## 12. Inspector scrolling

When Inspector content exceeds available height, custom scroll area must make all required fields/actions reachable. Mouse wheel and visible scroll affordance or equivalent are required.

## 13. Keyboard shortcuts

Required default shortcuts unless conflict with platform input is documented:

- `Ctrl+N` New;
- `Ctrl+O` Open;
- `Ctrl+S` Save;
- `Ctrl+Shift+S` Save As;
- `Ctrl+Z` Undo;
- `Ctrl+Y` or `Ctrl+Shift+Z` Redo;
- `Ctrl+C` Copy selected authored objects;
- `Ctrl+V` Paste;
- `Ctrl+D` Duplicate;
- `Delete` Delete selection;
- `Escape` cancel active manipulation/modal where appropriate;
- Space gameplay Launcher in Play Mode, unless focus is currently in a text field where text-entry semantics take precedence.

## 14. Shortcut focus rules

Text editing must not accidentally trigger destructive editor shortcuts from ordinary character input. Modal owns its relevant shortcuts. Gameplay actions must not fire while user is typing in a text/path/numeric field.

## 15. Pointer capture during drag

When moving/rotating/resizing or dragging a scrollbar/slider, the active control continues receiving drag updates until release/cancel even if the pointer leaves its visible bounds. The Windows implementation SHALL use coherent pointer capture semantics equivalent to SetCapture/ReleaseCapture and SHALL recover safely from WM_CAPTURECHANGED, focus loss, or destruction.

A release must not leave the control permanently pressed.

## 16. Scroll wheel routing

Pointer location/focus determines wheel target:

- canvas: zoom when configured modifier/default canvas behavior applies;
- Inspector/palette/file list: scroll that panel;
- modal file list: scroll modal list;
- wheel must not simultaneously zoom canvas through a foreground scroll panel.

## 17. Tooltips

Icon-only controls have tooltips. Tooltip appears after a short hover delay and must not capture pointer clicks intended for underlying control.

## 18. Destructive confirmation

At minimum confirmation is required for:

- discarding dirty document;
- overwriting existing Save As target.

Bulk Delete of ordinary selected table objects does not require modal confirmation because Undo is available.

## 19. Error modal/toast routing

- recoverable field validation: inline;
- file I/O failure: modal or durable error notification;
- scene validation issues: validation panel;
- transient nonfatal runtime capacity warning: toast/diagnostic;
- unrecoverable internal fault: explicit fatal error report before controlled exit where possible.

## 20. Win32 close request

WM_CLOSE/system close/Alt+F4 follows the same dirty Save/Discard/Cancel contract as explicit Quit. It MUST NOT bypass unsaved-work protection.

## 21. Evidence-capture method intentionally unspecified

This specification requires defined visual evidence artifacts but does not prescribe the software or procedure used to capture them. Only the resulting evidence and product behavior are acceptance-relevant.

## 22. v1.0 focus/text precedence

The detailed focus, keyboard traversal, UTF-8 scalar-safe editing, Chinese preservation/search, popup ownership, HiDPI scaling, and Reduced Motion rules in document 23 supersede any baseline ambiguity in this document.

## 23. Structured object clipboard

System clipboard integration for authored object copy/paste is no longer the required primary representation: the application SHALL implement the internal structured clipboard semantics in document 19. Platform clipboard text support remains required for text fields.

## 24. Command Palette

Command Palette interaction is normative in document 29 and participates in the same focus/modal ownership model as other custom UI surfaces.


## 25. Windows path and picker behavior

The custom picker SHALL represent Windows roots without requiring shell dialogs. At minimum it shall permit navigation to available drive roots reachable by the process and into ordinary directories from the current/root view.

Required path cases:

- ASCII and Chinese/other Unicode directory and file names;
- spaces and multiple dots;
- a `.pbt` filename entered without an extension, using the existing extension-assistance rule;
- read-only/unreadable target handling;
- paths longer than legacy `MAX_PATH` shall not overflow fixed buffers. When the OS/filesystem configuration permits the operation, the file layer SHALL support it; otherwise a real OS error is reported transactionally.

The implementation SHALL NOT invoke a shell command to enumerate/open/save user paths.

## 26. Windows system-window behavior

The OS-provided non-client frame, title bar, taskbar integration, minimize/maximize/restore controls, and system menu MAY remain native. Required application toolbar/menu/modal/popup/file-picker controls remain software-drawn inside the client area.

On maximize/restore and client resize, the framebuffer is reallocated/reused safely and the complete application surface is repainted with no stale pixels.
