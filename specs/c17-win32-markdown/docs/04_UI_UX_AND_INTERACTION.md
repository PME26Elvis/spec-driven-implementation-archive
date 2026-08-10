# 04 — UI/UX and Interaction Requirements

## 1. Design Standard

The application MUST present a deliberate modern desktop interface rather than a collection of default Win32 controls.

The visual design is part of the assignment and is subject to screenshot-based acceptance evidence.

The specification defines observable states and effects, but does not prescribe how screenshots are captured.

## 2. Required Primary Regions

The main window MUST contain identifiable regions for:

- Top navigation / command area.
- Document mode controls.
- Primary editing/preview workspace.
- Contextual formatting controls.
- Status bar.
- Access to document statistics.
- Access to version history.

Exact global pixel dimensions are intentionally not fixed; the UI must adapt to window size while satisfying the explicit minimum sizes, breakpoints, and evidence states defined by v1.0.

## 3. Custom Buttons

Every primary clickable button MUST implement the required states:

- Default.
- Hover.
- Pressed.
- Disabled where applicable.
- Keyboard focus where keyboard focus is supported.

A button MUST not depend on text color alone to communicate every state.

## 4. Hover Elevation

On pointer hover, primary buttons MUST visually rise/elevate.

Elevation MUST be communicated through a combination such as:

- Small positional translation.
- Shadow change.
- Highlight/border change.

The transition MUST be animated rather than instantaneous.

When the pointer leaves, the button MUST settle back smoothly.

## 5. Click Ripple

Activating a button by pointer MUST produce a visible ripple originating from or near the pointer activation position.

The ripple MUST:

- Expand over time.
- Fade over time.
- Be clipped to the intended control shape.
- Not permanently alter the button appearance.

Repeated clicks MUST not leave stale ripple artifacts.

## 6. Border Glow

Interactive controls designated by the visual system MUST support a border-glow response for hover/focus/active state.

Glow MUST be rendered as an actual visual effect rather than a static always-on border.

## 7. Capsule Sliding Indicator

Navigation or mode selectors MUST use a capsule/pill-style active indicator.

Changing active item MUST animate the indicator from its previous position to the new item.

The active capsule MUST not simply disappear and reappear at the destination.

The label content must remain legible during transition.

The feature is particularly appropriate for Source / Split / Preview / Rendered Edit mode selection.

## 8. Dynamic Collapse

At least one top-level navigation/command region MUST support dynamic compacting/collapse according to a defined UI state such as reduced window width or scroll state.

The transition MUST preserve access to mandatory commands.

The UI MUST NOT permanently hide a mandatory feature when collapsed.

The primary command/navigation region MUST enter its compact/collapsed presentation when the application content width falls below 900 logical pixels and return to the expanded presentation above 960 logical pixels, providing hysteresis that avoids rapid toggling near one threshold. Mandatory commands that no longer fit MUST remain reachable through an overflow/menu control.

## 9. Modal Open Animation

Dialogs and modal surfaces MUST animate on open using combined:

- Opacity transition.
- Scale transition.

The scale SHOULD begin slightly below 1.0 and settle at 1.0.

The motion MUST use a non-linear easing curve with a spring-like or elastic-feeling cubic Bézier profile without severe overshoot.

Modal open duration MUST be 180–260 ms. The required transform starts between scale 0.94 and 0.97 and ends at 1.0 while opacity moves from 0 to 1. The easing MUST use a cubic Bézier curve within the family `cubic-bezier(0.16..0.30, 0.90..1.20, 0.30..0.55, 1.0)` or an authored cubic curve producing an observably equivalent fast-out, soft-settle motion. Close duration MUST be 140–220 ms and reverse toward scale 0.96–0.98 with opacity 0. These ranges are visual acceptance requirements, not permission to skip animation.

## 10. Modal Close Animation

Closing a modal MUST animate rather than vanish in one frame.

Input MUST not leak to obscured background controls while the modal is logically active.

After the close transition completes, background interaction MUST be restored.

## 11. Modal Background Dimming and Blur

When a modal is open:

- The application content behind it MUST progressively darken.
- The application content behind it MUST progressively blur.
- Blur/dim amount MUST animate with modal progress.

The blur requirement applies to the application's own rendered background, not to arbitrary desktop windows behind the application.

The modal itself MUST remain crisp.

## 12. Dynamic Frosted Navigation

The top navigation region MUST support a frosted-glass-style treatment tied to document scrolling.

As the active scrollable document moves away from the top:

- Background blur beneath the navigation MUST increase smoothly over an initial scroll range.
- Navigation surface opacity/tint MAY increase to preserve legibility.
- A bottom shadow MUST transition from absent/subtle to visibly established.

Returning toward the top MUST reverse the transition smoothly.

The effect MUST be continuous over the defined range rather than a binary threshold toggle.

## 13. Scroll Performance and Visual Stability

While scrolling a long document:

- The navigation blur transition MUST not flicker between unrelated states.
- Text selection MUST not visually detach from content.
- The caret MUST stay aligned with text.
- Preview and rendered content MUST not leave uncleared trails.

## 14. Editor Caret

Editable surfaces MUST display a visible caret when focused and no selection obscures it.

Caret movement MUST correspond to logical text position.

Caret blinking MAY be implemented, but if implemented it must not interfere with input responsiveness.

## 15. Text Selection Appearance

Selected text MUST have a clearly visible selection background.

Selection foreground/background contrast MUST remain readable over:

- Plain text.
- Bold/italic text.
- Headings.
- Inline code.
- Links.

## 16. Scrollbars

Scrollable application content MUST expose a clear scroll position.

If custom scrollbars are used, they MUST support:

- Dragging the thumb.
- Clicking/scrolling through the content by ordinary wheel input.
- Correct thumb-size relation for materially different document lengths.

## 17. Context Menus

Right-click context menus MUST be custom integrated into the UI visual system.

They MUST:

- Appear near the activation point while remaining within usable window bounds.
- Have clear hover selection.
- Dismiss on outside click or Escape.
- Not remain visually orphaned after mode changes.

Image-specific context actions are defined in `05_IMAGES_AND_MEDIA.md`.

## 18. Keyboard Focus

Keyboard focus MUST be visually identifiable for mandatory keyboard-interactive controls.

Pressing Escape MUST dismiss the topmost dismissible transient UI such as a context menu or non-destructive modal, subject to dialog semantics.

Tab/Shift+Tab keyboard traversal, modal focus trapping, menus, tabs, file tree, Outline, context menus, and keyboard-only core flows are mandatory as defined in `13_COMMANDS_RECENTS_PREFERENCES_KEYBOARD.md`.

## 19. Window Resize

The application MUST remain usable across a documented minimum window size and larger sizes.

At the minimum supported size:

- Mandatory controls must remain reachable.
- The editor must retain a usable content region.
- Controls must not overlap into unreadable piles.

Split Mode MUST resize both panes predictably.

The Split Mode divider MUST be draggable, persist its ratio, and enforce the minimum-pane behavior defined in `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.

## 19A. Windows DPI Scaling

The custom UI MUST remain coherent and correctly hit-testable under Windows DPI scaling.

At 100%, 150%, and 200% scale factors:

- Mandatory controls remain reachable.
- Text remains crisp enough for normal use.
- Caret/selection geometry remains aligned with rendered text.
- Pointer hit regions match visible custom controls.
- Ripple, glow, shadow, blur radius, border radius, and spacing scale consistently.
- Scrollbar/thumb geometry remains usable.
- The 900/960 logical-pixel collapse thresholds are evaluated in logical application units rather than raw unscaled device pixels.

When the open window transitions between monitors/DPI values, the UI MUST recompute framebuffer/layout geometry without requiring application restart.

A scaled Windows screenshot is mandatory under `UI-DPI-SCALED`.

## 20. Visual Acceptance Evidence

The final delivery MUST include screenshots of specified states.

At minimum the final checklist is expected to include:

- Empty new document.
- Source Mode with mixed Markdown constructs.
- Split Mode with matching preview.
- Preview Mode.
- Rendered Editing Mode with caret/selection.
- Button hover state.
- Button pressed/ripple state.
- Modal open with dim + blur.
- Scrolled document with frosted navigation/shadow.
- Image selected for resize.
- Version-history view.
- Inline diff view.
- Side-by-side diff view.
- Statistics surface.
- Unsaved-changes dialog.

The implementation method for capturing these screenshots is intentionally unspecified.

## 21. Workspace Sidebar

When a workspace is active, the primary window MUST include a left workspace/file-tree sidebar.

The sidebar MUST support animated collapse/expand, drag resizing, nested directory indentation, clear expand/collapse affordances, and active-file highlighting.

The sidebar transition MUST use the same motion-quality requirements as the rest of the custom UI and MUST NOT simply disappear/reappear in one frame.

## 22. Document Tab Strip

A multi-document tab strip MUST be visible whenever one or more document tabs are open.

Active, inactive, hovered, pressed, dirty, and close-button states MUST be visually distinguishable.

Tab drag-reorder MUST show an insertion indicator or equivalent positional preview.

Tab overflow MUST remain operable without reducing controls to unusable sizes.

## 23. Tree/Tab Visual Synchronization

When the active tab belongs to the current workspace, its file-tree entry MUST be visually identifiable when that entry is currently visible.

Activating a tree entry that is already open MUST animate/focus the existing tab rather than creating a duplicate independent buffer.

## 24. Image Storage Choice UX

The image-insert flow MUST make the difference between **Relative asset** and **Embed in Markdown** understandable without requiring knowledge of Base64 syntax.

The choice MAY be remembered, but the remembered choice MUST be visible or discoverable before insertion.

Per-image conversion commands MUST be available from the rendered image context menu.

## 25. Workspace Visual Acceptance Evidence

Final visual evidence MUST additionally include at least:

- Workspace with nested file tree and multiple tabs.
- Dirty active tab plus clean inactive tabs.
- Tab drag/reorder intermediate state.
- Collapsed and expanded workspace sidebar states.
- Image insertion storage-choice UI.
- One rendered relative-path image and one rendered embedded image in the same document.
