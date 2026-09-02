# GPT-5.6 Sol High Chat Static Code Review — C17/Win32 Markdown Editor

**Reviewer:** GPT-5.6 Sol (Chat, High reasoning)  
**Review date:** 2026-09-02  
**Review type:** independent static code review; Windows runtime was not re-executed in this review  
**Reviewed repository snapshot:** `233ae1ffcd22fb9cd49ecbbf56aceb78a5bcfd27`  
**Run:** `runs/c17-win32-markdown/2026-09-01-deepseek-v4-pro-1m-workbuddy-code-development`  
**Primary implementation:** `project/c17-markdown-editor/`  
**Additional inputs:** the run-level `README.md`, project `README.md`, `RELEASE_REPORT.md`, tests, evidence generator, and the archived source tree  

> This is intentionally **not** a requirement-by-requirement/spec-compliance checklist. The main question here is: *what does the submitted code actually do, how is it designed, what is correct, what is fragile or broken, and what concrete runtime consequences should be expected from the implementation itself?*

---

## 1. Executive assessment

This submission contains a **substantial amount of real authored engineering work**. It is not a tiny GUI shell around placeholders. There are genuine low-level modules for UTF-8 handling, buffers, hashing, compression, JSON/YAML parsing, image codecs, Markdown parsing, literal search, Myers-style diffing, document transactions, undo/redo, history serialization, Win32 file I/O, a software-backed UI surface, and a native message loop. The codebase also shows a sensible high-level decomposition into `core/`, `engine/`, `app/`, `tools/`, and `tests/`.

The dominant problem is that the quality drops sharply at the **integration layer**. Many lower-level components exist, but the application state machine and UI wiring do not consistently preserve the invariants those components require. Several advertised UI states are drawn but not interactive; several application fields have no production code that updates them; multiple important public-looking functions are declared but never implemented; and several features are represented in screenshot automation by directly injecting UI state rather than reaching that state through actual product behavior.

More importantly, this is not only a polish problem. The static review identifies several deterministic correctness defects with direct user impact:

1. **A freshly created empty document can deterministically crash on its first insertion.** The code path matches the archived manual observation almost exactly.
2. **Composite edit transactions violate the undo engine's own operation-order invariant.** Formatting and Replace All can therefore corrupt offsets or destabilize undo/redo.
3. **Replace All uses stale match offsets while mutating the same buffer from left to right.** Different-length replacement text will target incorrect locations after the first replacement.
4. **The persistent history delta encoder stores the wrong inserted line after preceding equal lines, and pruning can destroy a delta chain.** This is a data-integrity problem, not merely a missing UI feature.
5. **The Base64 decoder rejects ordinary valid padded Base64.** The release report's attribution of the failure primarily to a test bug is incorrect; the production decoder is itself broken.
6. **Unsaved-document close handling is not functional, and closing the main window bypasses it entirely.** Dirty work can be discarded without a real Save/Discard/Cancel flow.
7. **Find/Replace UI behavior can delete text when Enter is pressed in ordinary Find mode.** The UI also cannot actually edit a replacement string through the shown input path.
8. **Preview/Rendered/Workspace features are much less complete than their names suggest.** Rendered hit testing is explicitly approximate/stubbed; workspace tree click handling is explicitly “not wired”; preview scrolling is disconnected from the wheel handler; image rendering is not present in the preview renderer.

The run-level manual notes therefore look internally consistent with the source. In particular, the observed first-character crash, drifting Source/Split/Preview/Rendered indicator, dead/odd New Document interaction, weak Preview, and Split overflow all have plausible or definitive code-level explanations.

My overall technical characterization is:

**A large, genuine partial implementation with several good low-level building blocks, but an under-finished and internally inconsistent application layer. The current code is not merely missing polish; it contains crash, data-loss, edit-offset, history-integrity, Unicode-input, and interaction-state bugs that make the editor unreliable for real use.**

---

## 2. Review methodology and confidence labels

I read the run metadata and manual notes first, then traced relevant code paths through:

- `src/core/` — especially `buf.c`, `base64.c`, `utf8.c`, `winutil.c`, `imgcodec.c`;
- `src/engine/` — especially `doc.c`, `undo.c`, `history.c`, `diff.c`, `search.c`, `md.h`, `md_parse.c`;
- `src/app/` — especially `app.c`, `app.h`, `input.c`, `view.c`, `render.c`;
- `tests/test_main.c`;
- `build_evidence.py`;
- project `README.md` and `RELEASE_REPORT.md`.

Findings are described using three confidence classes:

- **Definite from code:** the defect follows directly from the submitted source without needing environmental assumptions.
- **High-confidence runtime consequence:** a defect is definite, while the exact visible symptom is inferred from the Win32/runtime path.
- **Static concern:** the source is incomplete, unsafe, or suspicious, but this review did not execute Windows to measure the exact effect.

The manual observations in the run README are used only as corroborating evidence; I do not treat them as a substitute for reading the source.

---

# 3. Highest-severity findings

## 3.1 CRITICAL — first edit of a new empty document dereferences a NULL buffer

**Files:**

- `src/engine/doc.c`
- `src/core/buf.c`
- call path from `src/app/input.c`

**Confidence:** definite from code; directly corroborated by the run's manual observation.

A new tab is created with an empty `ce_buf`:

```c
void ce_buf_init(ce_buf *b){ b->data = NULL; b->len = 0; b->cap = 0; }
```

Typing the first character eventually calls `md_document_insert()`, which builds an edit transaction and then unconditionally calls the generic erase before insert:

```c
ce_buf_erase(&d->source, pos, old_len);
ce_buf_insert(&d->source, pos, new_text, new_len);
```

For a plain insertion, `old_len == 0`. The erase function nevertheless executes:

```c
void ce_buf_erase(ce_buf *b, size_t pos, size_t n){
    if(pos > b->len) return;
    if(pos + n > b->len) n = b->len - pos;
    memmove(b->data + pos, b->data + pos + n, b->len - pos - n);
    b->len -= n;
    b->data[b->len] = 0;
}
```

On a brand-new document, `b->data == NULL`, `b->len == 0`, `pos == 0`, `n == 0`. The function reaches `b->data[0] = 0` and dereferences NULL. Even the preceding pointer arithmetic passed to zero-length `memmove` is not a design that should be relied on with a NULL base.

This precisely explains the archived manual report: a new tab can appear successfully, but entering the first character causes the process to disappear/crash. Opening an existing file can avoid this exact path because `md_document_set_source()` has already allocated backing storage.

### Recommended repair

Fix this at the buffer abstraction boundary, not only in one caller:

```c
if (n == 0) return;
```

should happen before dereferencing `b->data`, and the empty-buffer/NUL-termination invariant should be made explicit. It is also reasonable for `md_document_edit_op()` to skip erase when `old_len == 0`.

Add a regression test whose entire purpose is:

1. `md_document_init()`;
2. insert one ASCII byte at position 0;
3. verify content, dirty flag, undo, redo;
4. repeat with a multibyte UTF-8 insertion.

This one test would have caught the most visible manual crash before release.

---

## 3.2 HIGH — composite transaction order conflicts with undo/redo's own invariant

**Files:**

- `src/engine/doc.c`
- `src/engine/undo.c`
- `src/app/app.c` (`app_apply_fmt`, `app_replace_all`)

**Confidence:** definite invariant violation; exact crash manifestation is high-confidence but not re-executed here.

`md_document_edit_op()` contains the key contract in its own comment:

```c
/* apply immediately (ops must be added in ascending pos order) */
```

The undo layer is written around that ordering assumption. Forward replay and reverse replay deliberately traverse the operation collection in opposite directions so offsets remain meaningful.

However, `app_apply_fmt()` deliberately records closing-delimiter operation first, then opening-delimiter operation:

```c
md_document_edit_begin(&t->doc);
md_document_edit_op(&t->doc, e, 0, close, cl);
md_document_edit_op(&t->doc, s, 0, open, ol);
md_document_edit_end(&t->doc);
```

Removal does the same high-position-first pattern.

There is an understandable reason the caller did this: because each edit is applied immediately, applying the end edit first keeps the lower start position stable. But that is exactly the architectural contradiction: **the immediate-mutating API wants descending positions for some multi-edit transforms, while the stored undo transaction requires ascending positions.**

This is not safely solvable by simply swapping the two calls. Swapping them would satisfy undo storage order but would require recomputing the second edit position after the first mutation.

The release report mentions a composite undo crash. Static review cannot prove that this is the only cause of that observed crash, but this ordering conflict is a strong candidate and independently a correctness bug even if it does not crash every time.

### Recommended repair

Redesign the transaction layer around one coordinate system:

- collect edit operations against the **pre-transaction source snapshot**;
- validate/sort/non-overlap them;
- apply them centrally in descending source order;
- store them in a canonical ascending order for undo bookkeeping;
- make undo/redo consume that canonical representation.

Callers should not need to know whether “high position first” is necessary.

---

## 3.3 HIGH — Replace All uses stale offsets after modifying the buffer

**File:** `src/app/app.c`, `app_replace_all()`

**Confidence:** definite from code.

The implementation first finds every match against the original text:

```c
size_t n = md_find_all(src, len, ... , &m);
```

It then applies replacements from `m[0]` to `m[n-1]`:

```c
for(size_t i = 0; i < n; i++){
    md_document_edit_op(&t->doc, m[i].pos, m[i].len,
                        a->find_repl, strlen(a->find_repl));
}
```

Those positions are only valid in the original string. If replacement length differs from match length, the first operation shifts every following match. Subsequent operations therefore edit the wrong byte ranges.

Example conceptually:

```text
source:  a a a
find:    a
replace: longer
```

After replacing the first `a`, the original offsets for the second and third occurrences no longer identify those occurrences.

Applying matches from right to left would avoid position shifting, but that immediately collides with the transaction-order defect described above. That is another sign the transaction abstraction needs correction rather than a local patch.

**Impact:** incorrect text, possible corruption of unrelated content, and an undo transaction that is also structurally inconsistent.

---

## 3.4 HIGH — persistent history delta encoder stores the wrong inserted lines

**Files:**

- `src/engine/history.c`
- `src/engine/diff.c`

**Confidence:** definite from code.

`md_diff_script()` supplies `a_idx`/`b_idx` for each edit operation. The history encoder does not use `b_idx`. Instead it keeps a separate `bi` child-line counter:

```c
size_t bi = 0;
for(size_t i = 0; i < ne; i++){
    unsigned char t = (unsigned char)ed[i].type;
    ...
    if(t == 2){
        uint32_t l = (uint32_t)cln[bi];
        ...
        ce_buf_append(&b, child + cst[bi], l);
        bi++;
    }
}
```

`bi` advances only for inserts. It does **not** advance when an equal line from the child is consumed.

Therefore, for a normal edit such as:

```text
line 1 unchanged
line 2 unchanged
new line inserted here
```

when the insert operation is reached, `bi` can still be 0 and the encoder may serialize child line 1 as the inserted line instead of the actual new line.

The correct source of truth already exists: the edit's `b_idx`, or a child cursor that advances for both EQUAL and ADD operations.

The existing history unit test mainly exercises very small one-line transitions, which can avoid the defect because there may be no preceding equal lines.

**Impact:** persisted history can reconstruct content different from what the user actually saved.

---

## 3.5 HIGH — history pruning can destroy the snapshot/delta chain

**File:** `src/engine/history.c`

**Confidence:** definite structural defect.

`md_history_prune()` chooses the first non-pinned entry and calls `md_history_delete()`. Deletion simply frees the entry and `memmove()`s subsequent `md_version` objects left:

```c
memmove(&h->v[index], &h->v[index + 1], ...);
h->n--;
```

There is no rebase of the next delta into a snapshot and no repair of parent relationships.

This matters because non-snapshot entries are encoded relative to earlier content. Deleting the first snapshot can leave the first retained entry as a delta whose base no longer exists. `md_history_get()` searches backward for a snapshot, but if index 0 itself is no longer a snapshot it can end up treating a delta payload as if it were a complete snapshot.

The test `history pin prune` pins version 0 before pruning, which avoids precisely the dangerous case rather than testing it.

### Recommended repair

Before deleting a version that is an ancestor/base for retained deltas:

1. reconstruct the first retained descendant content;
2. convert that descendant to a fresh snapshot;
3. repair parent IDs/metadata;
4. only then remove the old base.

Add tests that prune an **un-pinned** history beyond 200 versions and reconstruct every retained version afterward.

---

## 3.6 HIGH — corrupt history records are parsed before payload bounds are validated

**File:** `src/engine/history.c`, `md_history_load()`

**Confidence:** definite from code.

The loader checks the outer record length, computes a SHA, and then reads fixed fields including `payload_len`. It then does:

```c
v.payload = ce_malloc(pl ? pl : 1);
memcpy(v.payload, rec + 29, pl);
```

There is no prior condition equivalent to:

```text
29 + payload_len + 32 <= record_length
```

A malformed history record can therefore encode a very large `pl`, causing excessive allocation and/or an out-of-bounds read from the record buffer. The checksum result is stored as `v.corrupt = !ok`, but parsing and copying still happen even if the checksum does not match.

The safe order should be:

1. validate minimum structural length;
2. parse `pl` with overflow-safe arithmetic;
3. verify `29 + pl + 32 == rl` (or allowed format equivalent);
4. verify checksum;
5. only then allocate/copy payload.

This is especially important because history files are persistent local state and can be truncated or partially written independently of a hostile attacker.

---

## 3.7 HIGH — valid padded Base64 is rejected by the production decoder

**File:** `src/core/base64.c`

**Confidence:** definite from code.

The decoder strips whitespace and gets the total Base64 character count `n`. It then calculates padding and does:

```c
size_t data_len = n - pad;
if(data_len % 4 != 0) return -1;
```

For a perfectly valid Base64 string such as:

```text
aGVsbG8=
```

`n == 8`, `pad == 1`, so `data_len == 7`, and the function immediately returns `-1`.

That condition is conceptually wrong: the *complete encoded length* must be a multiple of four; the non-padding sextet count usually is not.

This also changes how the failing Base64 test should be interpreted. The project release report primarily describes the issue as a test bug/uninitialized-pointer issue. There is indeed a weakness in later tests that do not always check the decoder return code before using output variables, but the decoder itself is objectively rejecting legal padded input.

**Impact:** data URI / embedded image decode paths and any other padded Base64 input are unreliable.

### Testing issue that amplifies the failure

Several later tests use uninitialized locals such as `unsigned char *o; size_t n;`, call the decoder, then consume/free them without first requiring `r == 0`. Once the decoder returns an error, this can turn a functional failure into a test-runner crash. The correct conclusion is therefore **both production decoder bug and unsafe test error handling**, not “only a test bug.”

---

## 3.8 HIGH — dirty work can be discarded because close-state handling is not implemented

**Files:** `src/app/app.c`, `src/app/input.c`, `src/app/view.c`

**Confidence:** definite from code.

Closing a dirty tab does this:

```c
if(t->doc.dirty){
    app_show_modal(a, 8, "");
    a->unsaved_idx = idx;
    return;
}
```

That suggests a Save/Discard/Cancel flow exists. It does not. All mouse clicks while any modal is active execute the same behavior:

```c
if(a->modal){
    if(down && button == 1){
        a->modal = 0;
    }
    return;
}
```

The modal renderer itself is also generic, with a title `"Modal"` and generic OK/Cancel-looking buttons that are not semantically dispatched.

Worse, `WM_CLOSE` bypasses dirty-tab logic entirely:

```c
case WM_CLOSE:
    a->running = false;
    DestroyWindow(hwnd);
    return 0;
```

So closing the top-level window can destroy unsaved edits without even entering the incomplete tab-close modal path.

For an editor, this is a direct data-loss defect.

---

# 4. Manual observations explained by source code

The run README contains several useful manual observations. The source gives strong explanations for them.

| Manual observation | Static source explanation | Confidence |
|---|---|---|
| Source/Split/Preview/Rendered selector drifts | `ui_draw_capsule()` treats its value like normalized 0..1 and multiplies by 3 item widths, while mouse code stores raw mode index 0..3 in `capsule_anim`. Mode 3 can place the capsule far outside the intended fourth slot. | Definite |
| Blue start-page New Document feels dead/abnormal | The no-document welcome buttons are rendered, but `handle_mouse()` has no hit-testing for those buttons. The editor-area branch requires an active `DocTab`. | Definite |
| New tab appears, then first typed character crashes | Empty `ce_buf` + zero-length `ce_buf_erase()` NULL dereference described in §3.1. | Definite and directly matching |
| Open works | Open reads file data before editing, so the backing buffer is allocated and avoids the exact empty-buffer first-insert defect. | High confidence |
| Preview is weak | The preview renderer omits several styles/features and does simplistic run-level wrapping; images are shown as alt text rather than decoded/rendered. | Definite |
| Split text overflows | `flow_text()` measures an entire inline text run and only moves the whole run to the next line; it never breaks an over-wide run into words/chunks. A run wider than the pane still draws past the right edge. | Definite |
| Bottom-left line/column looks normal | There is a real line-start index and offset-to-line/column calculation; this is one of the pieces that is genuinely implemented. | Definite positive |
| Light/Dark works | Theme toggle is actually wired in `app_do_command()` and the renderer switches `Theme *`. | Definite positive |

---

# 5. Application/UI integration findings

## 5.1 HIGH — mode-capsule positioning formula is inconsistent with stored state

**Files:** `src/app/render.c`, `src/app/input.c`, `src/app/app.c`

`ui_draw_capsule()` computes something equivalent to:

```c
item_w = w / 4;
cx = x + frac * item_w * 3;
```

That makes sense only if `frac` is normalized across `[0, 1]`.

Mouse mode selection does:

```c
int m = ...;          /* 0..3 */
app_set_mode(a, m);
a->capsule_anim = (double)m;
```

Thus mode 1 already moves three slots, and modes 2/3 move the highlight beyond the intended capsule.

There is a second state-consistency problem: `app_set_mode()` updates `t->mode` but does not update `capsule_anim`. Keyboard/command-driven mode changes can therefore leave text labels/content and highlight position disagreeing.

**Better design:** derive visual indicator position from active tab `mode` as the single source of truth. If animated transition is desired, keep a separate visual current position and a target derived from `mode`, preferably in the same coordinate system.

---

## 5.2 HIGH — the start surface is visual-only; its three primary buttons have no hit targets

**Files:** `src/app/view.c`, `src/app/input.c`

When there is no active document, the renderer draws prominent actions such as New Document/Open File/Open Workspace. Mouse handling only knows about:

- nav bar;
- tab strip;
- sidebar;
- editor area when `app_active(a)` returns a document.

There is no branch that recognizes the welcome-surface buttons.

This is a classic custom-UI integration failure: the pixels exist, but there is no semantic/interaction object corresponding to them.

The fix should not be three more ad-hoc coordinate checks. The project would benefit from a reusable button hit-target system that records the same rectangles used for rendering and dispatches activation on mouse-up/keyboard activation.

---

## 5.3 HIGH — Shift+Left/Shift+Right selection is internally self-cancelling

**File:** `src/app/input.c`

The key handler tries to start a selection before calling the ordinary caret helper:

```c
if(shift){
    if(!t->has_sel){
        t->sel_start=t->sel_end=t->caret;
        t->has_sel=true;
    }
    caret_left(a,t);
    ...
}
```

But `caret_left()` begins with:

```c
if(t->has_sel){
    t->caret = min(selection endpoints);
    t->has_sel = false;
    return;
}
```

Therefore, on the first Shift+Left, the caller sets `has_sel=true`, and the helper immediately interprets that as “collapse an existing selection,” clears the selection, and returns without moving left. Shift+Right has the same structural problem.

Home/End selection logic also mixes anchor/active-end semantics inconsistently; `Shift+Home` sets `caret=0` before preserving the original anchor and therefore can create a zero-length selection.

**Impact:** keyboard selection behaves incorrectly, and downstream code receives inconsistent `sel_start`/`sel_end` state.

---

## 5.4 HIGH — reversed selection can underflow the status-bar byte length

**File:** `src/app/view.c`, `render_status()`

Most editing code correctly normalizes selection endpoints using min/max. `render_status()` does not:

```c
size_t sel_chars = t->has_sel
    ? ce_utf8_count((const uint8_t*)src + t->sel_start,
                    t->sel_end - t->sel_start)
    : 0;
```

If `sel_start > sel_end`, the unsigned subtraction underflows into a huge length and `ce_utf8_count()` can read far beyond the document.

That is a potential crash caused by a display-only status calculation.

Normalize the range before every use, or better represent a selection as `{anchor, active}` and expose one helper that produces canonical `{start,end}`.

---

## 5.5 HIGH — ordinary Find + Enter can replace/delete the selected match

**Files:** `src/app/input.c`, `src/app/app.c`

`Ctrl+F` opens Find with `find_replace == false`. Yet Enter handling does not distinguish Find from Replace:

```c
case VK_RETURN:
    if(t && a->find_open){
        app_replace_one(a);
    }
```

`app_replace_one()` replaces the selected match with `a->find_repl`. That replacement buffer starts empty and, through the visible input path, is never editable.

A normal flow can therefore be:

1. Ctrl+F;
2. type a query;
3. a match is selected;
4. press Enter expecting “next match”;
5. the selected match is replaced by the empty replacement string — effectively deleted.

That is a surprisingly severe behavior for a standard Find shortcut.

---

## 5.6 HIGH — Replace UI does not actually provide a replacement input path

**Files:** `src/app/input.c`, `src/app/view.c`

`find_repl[512]` exists, and the command can set `find_replace=true`. But:

- `render_find_bar()` renders only one text string (`find_query`);
- `WM_CHAR` while `find_open` always appends to `find_query`;
- there is no focus state selecting query vs replacement field;
- Backspace is handled at `WM_KEYDOWN` as an editor deletion, not as find-field editing;
- Replace All is not visibly reachable from this bar.

So the engine functions exist, but the user-facing Replace workflow is not actually implemented.

---

## 5.7 HIGH — Preview mode mouse wheel changes the wrong scroll variable

**Files:** `src/app/input.c`, `src/app/view.c`, `src/app/app.h`

The data model has separate:

```c
int scroll_y;
int preview_scroll_y;
```

Preview rendering uses:

```c
int cy = y - t->preview_scroll_y;
```

The mouse-wheel handler always modifies:

```c
t->scroll_y
```

and never writes `preview_scroll_y`.

Consequences:

- in Preview-only mode, wheel scrolling can have no visible effect;
- in Split mode, source scroll changes while preview remains stationary;
- `prefs.sync_scroll` is toggled by a command, but no synchronization behavior is implemented in the wheel path.

This is another concrete example of state fields existing without the feature being connected end-to-end.

---

## 5.8 HIGH — “Rendered Editing” is essentially preview rendering without editing

**Files:** `src/app/view.c`, `src/app/input.c`

`app_hit_test_rendered()` contains explicit placeholder logic:

```c
(void)mx; (void)my;
*pos = t->caret;
return 0;
```

The mouse handler for non-source modes similarly says:

```c
/* rendered: approximate */
```

and performs no caret mapping/edit action.

Rendered mode is drawn via the preview renderer, but the critical source↔render interaction layer is absent. `app_click_rendered()` is declared in `app.h` but there is no corresponding implementation in the reviewed tree.

The current name therefore overstates what the code does. It is closer to another preview presentation state than an editable rendered document.

---

## 5.9 HIGH — workspace tree is rendered but explicitly not wired to open/toggle

**Files:** `src/app/input.c`, `src/app/view.c`, `src/app/app.c`

The sidebar recursively draws tree nodes, but click handling contains:

```c
/* simple: expand/collapse handled in tree_draw; open file not wired here */
```

`tree_draw()` only draws; it does not handle interaction. Child directory nodes default to collapsed and there is no real path to toggle them from clicks.

Thus the tree can visually suggest a file browser while not functioning as one.

Additional scalability concern: workspace enumeration recursively walks the directory synchronously on the UI thread and uses repeated sorting/allocation. A large workspace can freeze the application even before interaction limitations matter.

---

## 5.10 MEDIUM/HIGH — outline can be stale after edits and is not interactive

**File:** `src/app/view.c`

Outline rendering uses `t->parsed` if it exists, but does not force `app_reparse()` when `parsed_dirty` is true. Editing headings can therefore leave the outline based on an old AST until another code path happens to reparse.

There is also no sidebar click dispatch for heading navigation.

The underlying heading collection helper is real; the application integration is incomplete.

---

## 5.11 HIGH — all modal types collapse into one generic, non-semantic interaction

**Files:** `src/app/app.h`, `src/app/view.c`, `src/app/input.c`

The state model enumerates many modal meanings: palette, stats, history, diff, preferences, error, unsaved, recovery, external conflict, image insert, shortcuts, etc.

The renderer, however, displays the same generic frame/title (`"Modal"`), generic text and generic OK/Cancel buttons. Input handling dismisses any modal on any left click without checking which button was clicked or which modal is active.

This means the `modal` integer creates the *appearance* of a modal state machine in the type definitions without a corresponding action state machine.

Particularly affected workflows include unsaved-close, history selection/restore, external-conflict resolution, preferences, and recovery.

---

## 5.12 HIGH — external-change tracking is mostly dead state

**Files:** `src/app/app.h`, `src/app/app.c`

`DocTab` has:

```c
file_exists
file_mtime
file_hash
external_conflict
external_missing
```

but searches through the reviewed production source show `file_mtime`/`file_hash` are not maintained by an external-change watcher/check loop. There is no timer/watch mechanism in the Win32 message loop to poll or receive change notifications.

`external_conflict` is mainly initialized/reset and screenshot state can show a conflict modal, but the actual detection machinery that would set it from an on-disk change is absent.

Therefore the data model is ahead of the implementation.

---

## 5.13 HIGH — autosave function exists but no production scheduling path calls it

**Files:** `src/app/app.c`, `src/app/input.c`

`app_autosave()` is implemented, and preferences include `autosave_enabled`/`autosave_interval`. However, the Win32 loop contains no `SetTimer()`/`WM_TIMER` path and source search finds no production caller of `app_autosave()` other than its declaration/definition.

As submitted, autosave is effectively dormant.

---

## 5.14 HIGH — recovery record's intended path NUL separator is not actually stored

**Files:** `src/app/app.c`, `src/core/buf.c`

Autosave tries to construct:

```c
ce_buf_append_fmt(&rec, "%s\x00", path);
```

But formatted append functions conventionally count formatted characters excluding the terminating C NUL. `ce_buf` keeps that NUL at `data[len]` only as a terminator; the next append begins at `len` and overwrites it.

So the record does not actually contain a counted NUL separator between path and content. The subsequent document bytes overwrite the terminator.

There is also no completed recovery-record parser/restore workflow in the application; the release report itself calls Recovery Center a stub.

---

## 5.15 HIGH — untitled recovery keys are based on active tab index rather than stable document identity

**File:** `src/app/app.c`

Untitled autosave key:

```c
snprintf(key, sizeof(key), "untitled_%d", a->active);
```

Tab indices are not stable document identifiers. Closing/reordering/switching tabs changes which document occupies an index, and a later untitled document can reuse the same key.

If autosave scheduling were connected, recovery data for different untitled documents could overwrite each other.

The existing `untitled_counter` is already closer to what is needed, though a persistent random/session identity would be more robust.

---

## 5.16 HIGH — Save As never re-associates version-history storage with the new path

**File:** `src/app/app.c`

History path is calculated by `app_load_history()` when opening a disk file. A new document has `history_path == NULL`.

During Save As, the code changes `t->doc.path` and immediately calls `app_commit_history()`, but does not recompute `history_path` first.

Consequences:

- a newly created document saved for the first time can keep `history_path == NULL`, so history is added in memory but not persisted;
- an existing file saved under a new name can keep the old file's history path and continue writing history into the old association.

After any path identity change, history/recovery/external-change state needs an explicit rebind operation.

---

# 6. Unicode, text editing, and Win32 input

## 6.1 HIGH — `WM_CHAR` handling corrupts non-BMP input and does not implement IME composition

**Files:** `src/app/input.c`, `src/core/utf8.c`, `src/app/app.h`

Document input casts each `WM_CHAR` value to one `wchar_t` and directly calls `ce_utf8_encode()`.

On Windows, supplementary Unicode characters can arrive as UTF-16 surrogate pairs. The first surrogate is not a valid Unicode scalar value. `ce_utf8_encode()` does not reject surrogate-range inputs, so a surrogate code unit can be encoded directly into invalid UTF-8 bytes.

Meanwhile, `App` contains fields such as:

```c
ime_composing
ime_comp_len
ime_comp[256]
```

but the window procedure contains no `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION`, or `WM_IME_ENDCOMPOSITION` handling. The fields are effectively dead.

**Impact:** emoji/supplementary characters and CJK IME composition are not reliably supported despite the Unicode-oriented architecture.

A correct Win32 implementation should either accumulate UTF-16 surrogate pairs before UTF-8 encoding and separately support IME composition/result strings, or use a well-defined text-input layer around the Windows APIs.

---

## 6.2 HIGH — find-field input truncates Unicode to one byte

**File:** `src/app/input.c`

When `find_open` is true, `WM_CHAR` appends:

```c
a->find_query[ql] = (char)wp;
```

A `WPARAM` Unicode code unit is narrowed to one byte. Therefore the engine can pass unit tests for Chinese literal search while the actual GUI cannot correctly type a Chinese query into its Find UI.

This distinction — engine capability vs application wiring — recurs throughout the project.

---

## 6.3 HIGH — grapheme backward traversal is incorrect for combining marks

**File:** `src/core/utf8.c`, `ce_grapheme_prev()`

For text `e + COMBINING ACUTE`, if `pos` is after the combining mark:

1. `ce_utf8_prev()` first moves to the combining mark;
2. the loop then examines the character *before* that position (`e`);
3. because `e` is not an extend mark, it returns the combining-mark position.

Backspace can therefore delete only the combining mark instead of deleting the whole grapheme cluster.

The test suite has a grapheme-count test for combining text and a forward ZWJ test, but no symmetric `ce_grapheme_prev()` regression for these cases.

The implementation is a useful partial Unicode grapheme effort, but it is not yet a correct bidirectional grapheme boundary implementation.

---

## 6.4 MEDIUM — UTF-8 navigation helpers assume valid sequences more strongly than their API suggests

`ce_utf8_next()` and `ce_utf8_count()` infer sequence length from the lead byte without validating that enough continuation bytes exist. In production, files are validated on open and editor-generated text is intended to stay valid, so this is less severe than the previous items. Still, these helpers are also used in status/rendering code where a corrupted internal buffer could turn a small inconsistency into out-of-range arithmetic.

A cleaner design would distinguish `*_unchecked()` helpers that require validated UTF-8 from safe boundary routines.

---

# 7. Rendering and visual-layout findings

## 7.1 HIGH — preview wrapping is run-level, not word/line wrapping

**File:** `src/app/view.c`, `flow_text()`

The renderer measures the entire inline `text` string as one run. If it would cross the right edge, it moves the *whole run* to the next line. It never splits a long run.

If the run itself is wider than the pane — a normal paragraph text node, long URL, CJK run without parser subdivision, or long word — it is still drawn beyond the right edge.

This directly explains the observed Split overflow.

Proper wrapping requires shaping/measuring smaller breakable units and hard-breaking an over-wide unbreakable run if necessary.

---

## 7.2 HIGH — source caret/selection hit testing assumes fixed-width text while drawing Segoe UI

**Files:** `src/app/input.c`, `src/app/view.c`, `src/app/render.c`

Caret x positions, selection x positions, and mouse column hit testing are calculated using approximately:

```c
unicode_character_count * (font_size - 2)
```

But text is drawn with GDI using Segoe UI, a proportional font, and `ui_text_width()` is available elsewhere to measure actual glyph widths.

Therefore the visual caret and click mapping drift as soon as glyph widths differ from the assumed constant. Mixed ASCII/CJK text makes this especially visible.

If source mode is intended to behave like a code editor, use a true monospace font consistently. If proportional text is intended, hit testing must measure shaped/prefix widths.

---

## 7.3 MEDIUM/HIGH — preview renderer ignores viewport height and renders off-screen blocks

**File:** `src/app/view.c`, `app_render_preview()`

The `h` parameter is explicitly unused:

```c
(void)h;
```

Every parsed block is rendered on every paint regardless of whether it is above or below the visible pane.

Combined with software framebuffer operations, GDI text calls, per-run UTF conversion, and a full AST walk, large documents can incur substantial paint cost.

The source pane likewise rebuilds a full line-start index for the entire document on every frame before drawing only visible lines.

These choices are acceptable for a prototype but are a performance risk for a large-document editor.

---

## 7.4 MEDIUM/HIGH — Markdown visual fidelity is incomplete in concrete code paths

**File:** `src/app/view.c`

Examples:

- emphasis recursively draws children but does not apply italic styling (`italic` parameter is ignored);
- inline-code background is noted in a comment but not drawn;
- strikethrough does not draw a strike line;
- HTML inline/block nodes are effectively invisible;
- image nodes display alt children; no image decode/render occurs in preview;
- blockquote rule calls `ui_draw_rect(..., height=0, ...)`, so the vertical rule is not drawn;
- table cells have simplistic fixed widths and no proper text wrap/clipping; vertical grid borders are not comprehensively rendered;
- nested inline rendering under strong/strike accesses child `.text` directly instead of recursively rendering all descendants, so combined/nested constructs can lose visible content.

The parser model is considerably richer than the renderer consuming it.

This is why “the Markdown parser recognizes a construct” must not be equated with “Preview renders the construct correctly.”

---

## 7.5 MEDIUM — caret blink/animation fields are present but not driven by a timer

`caret_phase` is read during rendering, but source search finds no production update path. The caret condition also includes `|| a->modal == 0`, which makes the caret effectively always visible during normal editing.

`modal_anim` advances only when renders happen; there is no periodic timer that guarantees animation frames. `ripple_*` state is similarly present without a complete interaction loop.

These are signs of an unfinished animation architecture rather than isolated cosmetic bugs.

---

## 7.6 MEDIUM — custom buttons do not have a real hover/pressed/capture lifecycle

`handle_mouse()` ignores `move`, most rendered buttons are always passed `hover=false`, `pressed=false`, and actions fire from coordinate checks on mouse-down. There is no `SetCapture`/`ReleaseCapture` pressed-control lifecycle.

Consequences include weak feedback, accidental activation semantics, and UI visuals that imply richer interaction states than are actually maintained.

This likely contributes to the manual description that the UI “feels bad” even beyond the explicit bugs.

---

# 8. Win32 platform integration findings

## 8.1 HIGH — drag-and-drop message handler exists, but drag/drop acceptance is never enabled

**File:** `src/app/input.c`

There is a `WM_DROPFILES` handler. However, the reviewed initialization does not call `DragAcceptFiles(hwnd, TRUE)`, and the window is created with `CreateWindowExW(0, ...)` rather than `WS_EX_ACCEPTFILES`.

Without enabling drop acceptance, the presence of a `WM_DROPFILES` switch case does not make shell drag/drop functional.

This is another recurring integration pattern: handler code exists but registration/enabling is missing.

---

## 8.2 HIGH — Per-Monitor DPI fields exist, but there is no DPI-awareness transition handling

**Files:** `src/app/app.h`, `src/app/input.c`

`App` has `dpi` and `scale`, initialized around 96 DPI, but there is no `SetProcessDpiAwarenessContext`/`SetThreadDpiAwarenessContext` and no `WM_DPICHANGED` branch in the reviewed editor window procedure.

Layout constants such as 44/30/22 pixel nav/tab/status heights are treated directly as device pixels.

So the type model anticipates DPI support while the actual native binding does not implement it.

On mixed-DPI Windows setups this can produce scaling blur, incorrect physical sizing, or hit-target/layout mismatch depending on process manifest/default virtualization.

---

## 8.3 MEDIUM/HIGH — end-to-end long-path support is inconsistent

**Files:** `src/core/winutil.c`, `src/app/app.c`, `src/app/input.c`

Positive: `wu_u8_to_native()` adds an extended-length `\\?\` prefix for long absolute paths, and low-level read/write functions use that helper.

But user-facing acquisition paths still use fixed `MAX_PATH` buffers:

- Open dialog;
- Save As dialog;
- image dialog;
- workspace browse result;
- drag/drop path buffer.

`wu_walk_dir()` also builds its `FindFirstFileW` pattern using `wu_u8_to_w()` rather than the extended native-path helper, so recursive walking loses the low-level long-path benefit.

The test named `long path open` is especially weak: if its test setup fails to create/open the long path, it ends with `A1(true)` and can still pass.

Thus the low-level helper is a good start, but “long path support” is not complete through the application.

---

## 8.4 MEDIUM — window and rendering resource creation errors are largely unchecked

**File:** `src/app/input.c` / renderer initialization

`RegisterClassW()` and `CreateWindowExW()` results are not validated before subsequent rendering initialization and `ShowWindow()`. The custom framebuffer/DC/font creation code similarly assumes common GDI calls succeed.

For a desktop editor, graceful failure paths should exist at least for top-level window creation and framebuffer allocation, especially after resize/DPI transitions.

---

# 9. File saving, preferences, and filesystem behavior

## 9.1 Positive — safe-save staging pattern is fundamentally sound

**Files:** `src/app/app.c`, `src/core/winutil.c`

One of the strongest implementation choices is the save strategy:

1. write a temporary file in the target directory;
2. loop until all bytes are written;
3. call `FlushFileBuffers()`;
4. close the temp handle;
5. `MoveFileExW(... MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`;
6. remove the temp file on failure.

This is much better than truncating the destination and writing in place. It preserves the original file through many write/flush failure modes.

There are still edge cases — metadata preservation, temp-name collision, race/conflict handling — but the core durability instinct is correct.

---

## 9.2 MEDIUM — safe save has no implemented external-modification compare-and-swap gate

Because `file_mtime`/`file_hash` are not maintained, `safe_save()` writes/replaces the path without checking whether the on-disk file changed since the editor loaded it.

The application has an `external_conflict` concept but not the dataflow necessary to make save conditional on a known backing-file identity.

A robust editor should capture mtime/file ID/hash at load/save and check again before replacement when the buffer is dirty.

---

## 9.3 MEDIUM — preferences can contain unvalidated mode/spacing values

**File:** `src/app/app.c`

Preferences clamp `font_size` and `autosave_interval`, but do not validate several enum/range fields:

- `line_spacing`;
- `default_image_mode`;
- `default_mode`.

An invalid `default_mode` can eventually index the fixed `modes[]` array in status rendering out of bounds. A sufficiently negative line spacing can also produce nonsensical or zero line height for some font sizes and feed divisions in hit-testing/rendering.

Configuration files are persistent external inputs and should be range-checked comprehensively.

---

## 9.4 MEDIUM — first-run preference save can silently fail because its directory is not created

`app_save_prefs()` writes to the preference path using `wu_write_file()`, but does not ensure the parent application directory exists first. Other subsystems explicitly create recovery/history directories.

On a fresh profile, toggling the theme can work for the current process while persistence silently fails.

---

# 10. Command and state architecture findings

## 10.1 MEDIUM/HIGH — command registry and command dispatcher disagree

**File:** `src/app/app.c`

`g_commands[]` lists commands such as `save-all`, but `app_do_command()` has no implementation branch for `save-all`.

Every command entry also stores `fn=NULL` and `enabled=NULL`; the actual behavior is a long hard-coded `strcmp` chain. That means the registry is not the executable command source of truth.

A command palette built from the list can therefore display commands that do nothing.

A better architecture would put function pointer, enable predicate, shortcut metadata, and label in one registry and have menus/palette/keyboard dispatch through it.

---

## 10.2 MEDIUM — public app header contains multiple declared-but-unimplemented interaction APIs

**File:** `src/app/app.h`

Examples include declarations such as:

- `app_click_rendered()`;
- `app_key_char()`;
- `app_key()`;
- `app_mouse_down()` / `app_mouse_up()` / `app_mouse_move()`;
- `app_wheel()`;
- `app_toggle_inline()`.

The actual window procedure bypasses these declarations with private monolithic handling.

Unused declarations do not by themselves break linking, but they document an architecture that was planned and then abandoned mid-implementation. They also make it harder to tell which API is authoritative.

This kind of architectural drift is worth cleaning before extending features.

---

# 11. Markdown parser/engine assessment

## 11.1 Positive — parser data model is richer than a trivial regex renderer

`md.h` defines explicit block and inline node types with source byte ranges, child nodes, table structure, link/image destination/title data, heading levels, list/task metadata, and an owned document tree.

`md_parse.c` implements real recursive inline parsing logic rather than replacing Markdown with a few regular expressions. It includes handling for emphasis/strong/combined emphasis, code spans, strike, links/images, autolinks/breaks, headings, lists, code fences, tables, blockquotes, and malformed-input fallback.

This is meaningful engineering work and gives a viable foundation for preview, outline, stats, and source mapping.

The weakness is primarily that the renderer and rendered-editing layer do not fully exploit this model.

---

## 11.2 MEDIUM — parser correctness tests are mostly shape assertions, not semantic rendering/source-map tests

The parser tests often assert only that a top-level node has a particular type or count. For example, combined emphasis checks the nested node types, which is useful, but there is much less testing of:

- exact source ranges;
- nested text preservation;
- escaping edge cases;
- delimiter flanking combinations;
- link destination/title corner cases;
- table escaped pipes;
- renderer output using the parsed tree;
- source↔render hit mapping.

This helps explain how a relatively feature-rich parser can coexist with visibly weak preview behavior.

---

# 12. Diff/history architecture: strengths and weaknesses

## 12.1 Positive — the project implements a real line diff and persisted history format

`diff.c` contains a Myers-style shortest-edit-script implementation with line splitting and token-level refinement. History has:

- snapshots at intervals;
- deltas;
- optional LZSS compression;
- per-record SHA-256;
- serialization/deserialization;
- pin/prune concepts.

This is far more substantial than storing arbitrary copies in memory.

The issues in §3.4–3.6 are therefore particularly important: they affect a subsystem that otherwise has significant real structure.

---

## 12.2 MEDIUM — word refinement handles multi-line modified regions incompletely

In `md_diff()`, word refinement is enabled when deleted and added line counts are equal, but the refinement code operates only on `la[a0]` and `lb[b0]`, effectively the first paired line of the region.

For multi-line equal-count replacements, the hunk metadata can imply a region-wide modified hunk while word-level details cover only one line.

This is not as severe as history corruption, but it matters if the history/diff UI expects complete fine-grained highlighting.

---

# 13. Base/core module assessment

## 13.1 Positive — memory helpers fail fast and low-level file reads/writes are looped

The common allocation helpers abort on OOM rather than returning unchecked NULL throughout the codebase. That is a defensible desktop-app policy if intentional.

`wu_read_file()` and the save code also correctly loop over large reads/writes instead of assuming one `ReadFile`/`WriteFile` transfers the entire requested buffer.

---

## 13.2 MEDIUM — image decoder's size limit is too high to be a useful memory-safety budget

**File:** `src/core/imgcodec.c`

The decoder accepts dimensions up to 65,536 × 65,536, then allocates `width * height * 4` bytes. At the upper bound this is about 16 GiB before accounting for WIC objects and source data.

Because allocation helpers abort on OOM, a crafted or merely huge image can terminate the editor.

Additionally, WIC `CopyPixels` receives 32-bit buffer/stride size parameters, so extremely large dimensions interact badly with 32-bit size limits even on 64-bit builds.

Use an explicit decoded-pixel/byte budget appropriate for an editor, with overflow checks before multiplication and a recoverable “image too large” error.

---

# 14. Test-suite review

The test suite is sizeable and covers many lower-level modules, but its structure leaves important integration bugs invisible.

## 14.1 Positive — broad low-level module coverage exists

`tests/test_main.c` exercises UTF-8, Base64, SHA-256, LZSS, PRNG, JSON, YAML, path matching, Markdown parse shapes, stats, search, diff, history, image codec, malformed inputs, and some performance-oriented operations.

That breadth is useful and demonstrates that many engine pieces were intentionally tested.

---

## 14.2 HIGH — the test suite never tests the exact “first insert into empty document” lifecycle

This is the most important missing regression in hindsight. Engine tests exercise history/parser/search, but the path from a default-initialized `md_document` through `md_document_insert()` was not sufficient to expose the NULL erase bug before manual testing.

A small set of document-edit lifecycle tests would have higher practical value than several peripheral utility tests.

---

## 14.3 HIGH — Base64 tests can turn a decoder failure into undefined test behavior

Some decoder tests declare output pointer/length without initialization and use/free them after a decode call without requiring success first.

Since the production decoder rejects padded Base64, later tests can read an uninitialized `n`, compare through an uninitialized pointer, or free an indeterminate pointer depending on short-circuit outcomes.

A test must initialize outputs and branch on the function's return code before consuming them. Otherwise the runner crash masks the original defect.

---

## 14.4 HIGH — missing fixtures frequently become PASS instead of SKIP/FAIL

Examples in the test file use patterns equivalent to:

```c
if(d){
    ... real assertions ...
} else A1(true);
```

If a performance/failure fixture is absent, the test passes.

The long-path test likewise ends with unconditional `A1(true)` even if the environment failed to create the test path, so it can claim success without exercising the intended behavior.

This makes “green” test counts less trustworthy than they appear.

---

## 14.5 HIGH — every recorded test category is hard-coded as `unit`

The result recorder contains:

```c
g_r[g_nr].cat = "unit";
```

regardless of whether the source section is labeled performance/failure/integration.

Therefore a run produced by the C test runner alone cannot truthfully populate multiple evidence categories. This is one reason later evidence generation has to manufacture category-level entries separately.

---

## 14.6 MEDIUM — the signal crash handler is not a reliable Windows crash-reporting strategy

The runner installs handlers for `SIGSEGV`/`SIGABRT` and tries to write JSON from the handler. Calling complex I/O/allocation code from a fatal-signal handler is not generally safe/reliable, and Windows access violations do not map cleanly to portable POSIX signal semantics in every circumstance.

For a Windows validation runner, SEH or an outer process harness is more robust for recording crash status.

---

# 15. Evidence-generation and screenshot-review findings

These are code-quality findings about the validation tooling, not a spec pass/fail matrix.

## 15.1 HIGH — `build_evidence.py` hard-codes performance passes with zero duration

The evidence builder creates records such as:

```python
"duration_ms_avg": 0,
"pass": True
```

and explicitly comments that actual stderr was not captured and values are left as placeholders.

That means the generated manifest contains performance “pass” claims that are not machine-derived measurements.

Even if the real test runner printed valid timings somewhere, the manifest itself is not evidence of them.

---

## 15.2 HIGH — failure-run evidence is also manually asserted

`failure_runs` contains hard-coded entries with `"pass": True` and strings such as `"actual": "opened"` rather than being derived from structured test output.

Combined with the test behavior that passes when fixtures/setup are absent, this can make failure-handling evidence materially stronger-looking than the underlying execution.

---

## 15.3 MEDIUM/HIGH — screenshot automation proves rendering of injected states, not end-to-end reachability

`app_setup_screenshot()` directly creates tabs, sets modes, fills modal text, and chooses states for screenshot capture. Several screens therefore can be generated without exercising the real user interaction that would normally produce them.

That is acceptable as a visual regression harness if clearly labeled as **state injection**. It is not sufficient evidence that:

- the command palette can be opened and operated;
- Recovery Center discovered a real recovery file;
- external conflict detection actually triggered;
- history UI can select/restore a real version;
- a workspace tree can be navigated by the user.

The screenshot files can still be authentic pixels rendered by the real executable while the *state transition* being portrayed remains synthetic. Those are separate claims.

---

## 15.4 MEDIUM — project self-report understates some production defects

The release report is commendably candid about several missing pieces, but at least two root-cause descriptions should be corrected:

- Base64 failure is not just unsafe test code; the decoder rejects legal padded input.
- The composite undo failure should be investigated against the transaction ordering contradiction in the production edit layer, rather than being treated as isolated test instability.

A release report should distinguish “test harness crashed” from “test harness crashed after production API returned an unexpected error.”

---

# 16. Build-system and documentation consistency

## 16.1 MEDIUM/HIGH — default `make` does not build tests despite documentation/release claims

**File:** `Makefile`

The default target is:

```make
all: tools editor
```

`tests`/`testrunner.exe` are separate targets.

Yet the project README/release report describes the normal build as producing the test runner together with the other artifacts.

This is an objective mismatch between build graph and documentation.

---

## 16.2 MEDIUM — build is GNU C17, not strict ISO C17

The Makefile uses:

```make
-std=gnu17
```

and suppresses warnings including `-Wno-redeclaration` and `-Wno-shadow`.

Using GNU C17 under MinGW is not inherently bad, but the project repeatedly describes itself as ISO C17. If strict portability/discipline is intended, use `-std=c17` and avoid disabling warning classes unless there is a documented reason.

`-Wno-redeclaration` is especially unusual for a C project because conflicting declarations often reveal header/API hygiene problems.

---

## 16.3 MEDIUM — Makefile embeds an absolute developer machine path

```make
TC := D:/0901-workbuddy-markdown-editor/toolchain/mingw64
```

It can be overridden, but a checked-in absolute path makes clean builds unnecessarily environment-specific. Prefer a toolchain discoverable from PATH with an optional `TC` override, or a documented relative toolchain location.

---

# 17. Additional lower-priority issues

## 17.1 `path_basename()` ownership is easy to misuse

The helper returns an allocated string, but one image-insertion branch stores it in a `const char *name` and does not free that allocation. This is a small leak, but it reflects unclear ownership naming/API design.

## 17.2 Tab rendering and tab hit testing use different width behavior

Rendering shrinks/breaks the last visible tab near the right edge, while hit testing assumes fixed 140-pixel widths. With many tabs, clickable regions can diverge from visible tab geometry. There is no tab-strip scrolling.

## 17.3 File tree/outline have no scrolling model

Recursive drawing can continue beyond the visible sidebar. Large workspaces/outlines therefore become inaccessible even apart from missing click handling.

## 17.4 Image asset copy ignores some write failures

The asset path copies bytes using `wu_write_file()` but does not surface copy failure before inserting the Markdown reference. The document can end up referring to an asset that was never written.

## 17.5 UI/application cleanup is process-lifetime oriented

Fonts, DC/DIB resources, tabs/tree/recent arrays, preference strings, COM factory lifetime, and screenshot allocations do not have a comprehensive shutdown/destructor path. Windows will reclaim process resources on exit, so this is not a primary correctness issue, but it makes leak checking and future multi-window/refactor work harder.

---

# 18. What is implemented well

A useful review should not treat every line as broken. Several choices are solid foundations.

## 18.1 Clear project layering

The `core` / `engine` / `app` split is sensible. Pure-ish algorithms such as search/diff/history/parser are separated from Win32 UI code. That makes targeted tests and future repair more feasible.

## 18.2 Real UTF-8 validation and nontrivial grapheme work

`ce_utf8_decode()` correctly rejects overlong encodings, UTF-16 surrogate scalar values, and code points beyond U+10FFFF when decoding. The code also attempts combining-mark, variation-selector, emoji modifier, and ZWJ handling rather than treating every byte/code point as a character.

The backward-boundary bug needs fixing, but the module is clearly substantive.

## 18.3 Real Markdown AST with source mapping

The parser's block/inline tree and byte-range fields are a strong architectural direction for a source/rendered editor. The app currently fails to capitalize on this via rendered hit testing, but the underlying model is useful.

## 18.4 Safe-save instinct is good

As discussed earlier, staging + flushing + replacement is the correct general direction for editor saves.

## 18.5 History has integrity-oriented design intent

Snapshot intervals, compressed deltas, SHA-256 records, pinning and pruning show serious design effort. The current delta/prune/load bugs are fixable without throwing away the whole subsystem.

## 18.6 WIC-backed image codec is a pragmatic Windows-native choice

Using Windows Imaging Component for PNG/JPEG/BMP decode/encode keeps the project native and avoids vendoring a large third-party codec library. COM lifetime/error handling and pixel budgets need hardening, but the platform choice itself is reasonable.

## 18.7 Theme switching is actually wired

The manual Light/Dark success is consistent with the source. This is a complete small vertical slice: input command → preference state → selected theme → repaint.

## 18.8 Status line's logical line/column calculation is real

Unlike several cosmetic UI states, the status bar computes line starts and UTF-8 character count from actual document content. The manual observation that it looked normal is unsurprising.

---

# 19. Architectural diagnosis

The codebase's main weakness is not lack of effort; it is **too many parallel representations of application state without one authoritative state machine**.

Examples:

- `t->mode` and `a->capsule_anim` both represent selected mode but are not synchronized.
- modal numeric kind and generic modal UI do not share semantic action dispatch.
- `scroll_y`, `preview_scroll_y`, and `sync_scroll` exist without one scroll controller.
- `file_mtime`, `file_hash`, `external_conflict`, `external_missing` exist without an external-file state machine.
- `autosave_enabled`/interval and `app_autosave()` exist without scheduling.
- command registry and hard-coded `strcmp` dispatcher both represent commands.
- app header declares a modular event API while the window procedure implements a separate monolithic event path.
- parser knows rich Markdown semantics while renderer supports only a subset.
- screenshot setup can create states that real interaction cannot reach.

This creates a codebase that looks feature-rich by symbol/file count but has many broken vertical slices.

The best next engineering move would be to stop adding features and make a small set of vertical workflows complete and invariant-driven:

1. create/open → type → select → undo/redo → save → close;
2. source/split/preview mode switching and scrolling;
3. find/replace with safe text-field focus;
4. workspace tree open/toggle;
5. history add/save/load/prune/restore;
6. recovery scheduling/write/discovery/restore;
7. external change detect/resolve/save.

Each workflow should have one state owner, one interaction path, and an automated integration test.

---

# 20. Recommended repair order

## P0 — fix correctness/data-loss before UI polish

1. **Fix empty-buffer erase / first-character crash.**
2. **Redesign multi-op document transaction semantics** so callers cannot violate operation ordering.
3. **Fix Replace All offset application** under the corrected transaction model.
4. **Implement a real unsaved-close state machine** for tab close and main-window close.
5. **Fix history delta child indexing.**
6. **Fix history pruning by rebasing retained deltas.**
7. **Harden history loader record-size/checksum validation.**
8. **Fix Base64 padding validation and make tests check decode return values.**
9. **Add regression tests for all of the above before touching appearance.**

## P1 — make the visible editor interaction coherent

10. Use active tab `mode` as the authoritative mode and repair capsule animation coordinates.
11. Wire start-page New/Open/Workspace buttons through a reusable hit-target system.
12. Rework keyboard selection using explicit anchor/active endpoint semantics.
13. Fix status selection normalization.
14. Build actual Find and Replace text-field focus/editing; Enter in Find must mean Find Next.
15. Route wheel input to the pane under the pointer; implement `sync_scroll` explicitly.
16. Either implement rendered-edit source mapping or rename/remove “Rendered Editing” until real.
17. Wire workspace tree expand/open and outline navigation.
18. Replace generic modal dismissal with modal-specific actions.

## P1/P2 — finish Windows integration

19. Implement Unicode surrogate handling and Win32 IME composition.
20. Enable drag-and-drop acceptance if retaining `WM_DROPFILES`.
21. Establish Per-Monitor DPI awareness and handle `WM_DPICHANGED`.
22. Make long paths end-to-end rather than only inside low-level file helpers.

## P2 — rendering/performance

23. Implement word-aware wrapping and hard breaking for long runs.
24. Use consistent text metrics for drawing, hit-testing, selection and caret.
25. Render emphasis/code/strike/images/blockquotes/tables consistently with the AST.
26. Cull off-screen preview blocks and cache/reuse line layout instead of rebuilding everything every paint.
27. Add a decoded-image memory budget.

## P2 — validation integrity

28. Make default build/docs agree about whether tests are built.
29. Replace fixture-missing PASS behavior with explicit FAIL/SKIP policy.
30. Record real test categories in the runner.
31. Generate performance/failure evidence from captured structured results, never hard-coded `pass: true` placeholders.
32. Clearly label screenshot state injection as visual-regression setup, separate from E2E interaction evidence.

---

# 21. Suggested regression tests derived directly from this review

These tests would provide unusually high value because each targets a concrete failure mode found in the current source.

### Document editing

- fresh `md_document_init` → insert first byte → undo → redo;
- fresh document → insert first multibyte UTF-8 scalar;
- composite formatting around a middle selection → undo/redo exact byte equality;
- remove existing formatting → undo/redo;
- Replace All shorter, longer, and equal-length replacements with 3+ matches.

### Selection/input

- Shift+Left/Right grows selection one grapheme at a time;
- reverse selection then status rendering does not read out of bounds;
- Backspace over `e + combining acute` removes the entire grapheme;
- supplementary Unicode input surrogate pair produces one valid UTF-8 scalar;
- CJK IME result inserts correct UTF-8 text.

### Find/replace

- Ctrl+F + query + Enter does not mutate document;
- Replace mode can focus/edit replacement field;
- Backspace edits active search field, not document;
- CJK query can be entered from the actual Win32 text-input path.

### History

- delta insertion after 2+ equal lines reconstructs exact child;
- insertion/deletion at beginning/middle/end;
- prune 250 unpinned versions, then reconstruct every retained version;
- malformed record with huge `payload_len` is rejected without allocation/read overflow;
- Save As from Untitled writes history to the new path association;
- Save As existing file does not continue writing old file's history.

### App lifecycle

- no-document welcome New button creates tab;
- first click/type sequence does not crash;
- dirty tab close Save/Discard/Cancel actions work;
- dirty app `WM_CLOSE` cannot silently destroy data;
- Preview wheel scrolls Preview;
- Split sync-scroll follows preference;
- tree folder toggles and file opens;
- dropped file is accepted after window setup;
- mode indicator remains aligned after mouse/keyboard/command changes.

### Evidence tooling

- missing required fixture must not produce PASS;
- manifest duration must equal captured measured duration and must not be zero placeholder;
- every manifest PASS must map to a concrete executed test record/log digest.

---

# 22. Final verdict

From a code-review perspective, the submission deserves more nuanced treatment than either “works” or “mostly placeholder.”

There is **real implementation depth** in the lower layers, and several design choices — native safe-save staging, structured Markdown AST, custom diff/history machinery, WIC codec integration, UTF-8 validation, modular directories — are credible foundations.

However, the application built on top of those foundations is not yet reliable. The strongest problems are concrete and localizable: NULL dereference on first edit, broken multi-op transaction semantics, stale Replace All offsets, history delta/pruning defects, invalid Base64 padding logic, incomplete unsaved-close protection, broken selection, destructive Find/Enter behavior, disconnected scrolling/autosave/external-change state, incomplete Win32 Unicode/DPI/drop integration, and multiple visually present but non-interactive UI surfaces.

The archived manual verdict that the current UX is effectively unusable is therefore not contradicted by the amount of code. In fact, static inspection explains how both things can be true at once: **the project has a large authored code footprint, but too many vertical product flows stop halfway between engine implementation and usable UI behavior.**

If the same codebase were to be repaired rather than rewritten, I would preserve most of the low-level `core/` and much of the parser/diff/history structure, then aggressively simplify and rebuild the app-level state/interaction layer around explicit invariants and end-to-end tests. The highest-value work is not adding another feature; it is making the existing vertical slices deterministic, state-consistent, and data-safe.
