#!/bin/sh

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TASK_TEMP=$(mktemp -d /tmp/mdeditor-e2e-XXXXXX) || exit 1
LOG_DIR="$PROJECT_ROOT/evidence/logs"
LOG_FILE="$LOG_DIR/e2e.log"
APP_LOGS="$LOG_DIR/e2e-apps.log"
DISPLAY_NUMBER=$((200 + ($$ % 300)))
DISPLAY_VALUE=
XVFB_PID=
APP_PID=
APP_LOG=
CASE_INDEX=0
TOTAL=0
PASSED=0
FAILED=0

mkdir -p "$LOG_DIR"
: >"$LOG_FILE"
: >"$APP_LOGS"

cleanup() {
    if [ -n "$APP_PID" ]; then kill -9 "$APP_PID" 2>/dev/null || true; wait "$APP_PID" 2>/dev/null || true; fi
    if [ -n "$XVFB_PID" ]; then kill -9 "$XVFB_PID" 2>/dev/null || true; wait "$XVFB_PID" 2>/dev/null || true; fi
    rm -rf "$TASK_TEMP"
}
trap cleanup EXIT HUP INT TERM

attempt=0
while [ "$attempt" -lt 30 ]; do
    : >"$TASK_TEMP/xvfb.log"
    Xvfb ":$DISPLAY_NUMBER" -screen 0 1440x900x24 -pn -listen tcp -nolisten local -nolisten unix -ac >"$TASK_TEMP/xvfb.log" 2>&1 &
    XVFB_PID=$!
    sleep 0.25
    if kill -0 "$XVFB_PID" 2>/dev/null; then
        DISPLAY_VALUE="127.0.0.1:$DISPLAY_NUMBER.0"
        break
    fi
    wait "$XVFB_PID" 2>/dev/null || true
    XVFB_PID=
    DISPLAY_NUMBER=$((DISPLAY_NUMBER + 1))
    attempt=$((attempt + 1))
done
if [ -z "$DISPLAY_VALUE" ]; then
    sed -n '1,240p' "$TASK_TEMP/xvfb.log" >"$APP_LOGS"
    printf 'E2E_RELEASE_SUMMARY total=10 passed=0 failed=10 skipped=0\n' >"$LOG_FILE"
    exit 1
fi

ctl() {
    DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" "$@" --wait-ms 5000
}

new_scope() {
    CASE_INDEX=$((CASE_INDEX + 1))
    SCOPE_ROOT="$TASK_TEMP/case-$CASE_INDEX"
    SCOPE_CONFIG="$SCOPE_ROOT/config"
    SCOPE_STATE="$SCOPE_ROOT/state"
    SCOPE_CACHE="$SCOPE_ROOT/cache"
    mkdir -p "$SCOPE_ROOT" "$SCOPE_CONFIG" "$SCOPE_STATE" "$SCOPE_CACHE"
}

start_app() {
    APP_LOG="$SCOPE_ROOT/app-$(date +%s%N).log"
    DISPLAY="$DISPLAY_VALUE" XDG_CONFIG_HOME="$SCOPE_CONFIG" XDG_STATE_HOME="$SCOPE_STATE" XDG_CACHE_HOME="$SCOPE_CACHE" \
        "$PROJECT_ROOT/bin/mdeditor" "$@" --quit-after 60000 >"$APP_LOG" 2>&1 &
    APP_PID=$!
    sleep 0.75
    ctl --dump-state >/dev/null 2>&1
}

stop_app() {
    if [ -n "$APP_PID" ]; then
        kill -9 "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
        APP_PID=
    fi
    if [ -n "$APP_LOG" ] && [ -f "$APP_LOG" ]; then
        printf 'APP_LOG %s\n' "$APP_LOG" >>"$APP_LOGS"
        sed -n '1,400p' "$APP_LOG" >>"$APP_LOGS"
    fi
}

run_case() {
    case_id=$1
    shift
    TOTAL=$((TOTAL + 1))
    case_output="$TASK_TEMP/run-$TOTAL.log"
    if "$@" >"$case_output" 2>&1; then
        PASSED=$((PASSED + 1))
        case_status=0
    else
        FAILED=$((FAILED + 1))
        case_status=1
    fi
    stop_app
    sed -n '1,1600p' "$case_output" >>"$LOG_FILE"
    if [ "$case_status" -eq 0 ]; then
        printf 'PASS %s\n' "$case_id"
        printf 'PASS %s\n' "$case_id" >>"$LOG_FILE"
    else
        printf 'FAIL %s\n' "$case_id"
        printf 'FAIL %s\n' "$case_id" >>"$LOG_FILE"
    fi
}

generate_profile() {
    profile=$1
    output=$2
    "$PROJECT_ROOT/bin/fixturegen" --profile "$profile" --output "$output" --seed 424242 >/dev/null &&
        "$PROJECT_ROOT/bin/fixturegen" --verify "$output" >/dev/null
}

case_clipboard_editor() {
    new_scope
    generate_profile small "$SCOPE_ROOT/small" || return 1
    start_app --test-mode --open "$SCOPE_ROOT/small/README.md" || return 1
    ctl --exercise
}

case_ui_interaction() {
    new_scope
    generate_profile markdown-all "$SCOPE_ROOT/markdown-all" || return 1
    start_app --test-mode --open "$SCOPE_ROOT/markdown-all/markdown-all.md" || return 1
    ctl --ui-scenario || return 1
    ctl --rendered-scenario
}

case_search_replace() {
    new_scope
    generate_profile small "$SCOPE_ROOT/small" || return 1
    start_app --test-mode --open "$SCOPE_ROOT/small/README.md" || return 1
    ctl --search-scenario
}

case_xim() {
    new_scope
    generate_profile unicode "$SCOPE_ROOT/unicode" || return 1
    unicode_file="$SCOPE_ROOT/unicode/中文 路徑/Unicode 測試.md"
    start_app --test-mode --open "$unicode_file" || return 1
    ctl --ime-scenario
}

case_xdnd() {
    new_scope
    generate_profile small "$SCOPE_ROOT/small" || return 1
    start_app --test-mode --open "$SCOPE_ROOT/small/README.md" || return 1
    ctl --xdnd "file://$SCOPE_ROOT/small/notes/guide.md" || return 1
    ctl --xdnd "file://$SCOPE_ROOT/small/assets/gradient.png"
}

state_field() {
    field_name=$1
    sed -n "s/.*\"$field_name\":\"\([^\"]*\)\".*/\1/p"
}

state_number_field() {
    field_name=$1
    sed -n "s/.*\"$field_name\":\([0-9][0-9]*\).*/\1/p"
}

case_image_workflows() {
    new_scope
    generate_profile small "$SCOPE_ROOT/small" || return 1
    image_doc="$SCOPE_ROOT/small/README.md"
    original_asset="$SCOPE_ROOT/small/assets/blue.bmp"
    asset_sha_before=$(sha256sum "$original_asset" | awk '{print $1}') || return 1
    start_app --test-mode --open "$image_doc" --test-state UI-IMAGE-SELECTED || return 1
    ctl --image-scenario || return 1
    embedded_before=$(ctl --dump-state) || return 1
    printf '%s\n' "$embedded_before"
    saved_sha=$(printf '%s\n' "$embedded_before" | state_field source_sha256)
    embedded_width=$(printf '%s\n' "$embedded_before" | state_number_field image_persisted_width)
    ctl --test-command 1009 || return 1
    relative_before=$(ctl --dump-state) || return 1
    relative_width=$(printf '%s\n' "$relative_before" | state_number_field image_persisted_width)
    [ -n "$saved_sha" ] && [ -n "$relative_width" ] && [ "$relative_width" -ge 48 ] && [ -n "$embedded_width" ] && [ "$embedded_width" -ge 48 ] || return 1
    rg -q '<img[^>]*width="[0-9]+"' "$image_doc" || return 1
    [ "$asset_sha_before" = "$(sha256sum "$original_asset" | awk '{print $1}')" ] || return 1
    stop_app
    start_app --test-mode --open "$image_doc" --test-state UI-IMAGE-SELECTED || return 1
    after=$(ctl --dump-state --state-contains '"image_selected":true') || return 1
    printf '%s\n' "$after"
    reopened_sha=$(printf '%s\n' "$after" | state_field source_sha256)
    reopened_width=$(printf '%s\n' "$after" | state_number_field image_persisted_width)
    [ "$saved_sha" = "$reopened_sha" ] && [ "$relative_width" = "$reopened_width" ] || return 1
    ctl --test-command 1014 || return 1
    embedded_after=$(ctl --dump-state) || return 1
    reopened_embedded_width=$(printf '%s\n' "$embedded_after" | state_number_field image_persisted_width)
    [ "$embedded_width" = "$reopened_embedded_width" ] || return 1
    ctl --test-command 1009 || return 1
    saved_image="$SCOPE_ROOT/saved-image.bmp"
    ctl --image-save-as "$saved_image" || return 1
    cmp "$saved_image" "$original_asset" || return 1
    cp "$SCOPE_ROOT/small/notes/guide.md" "$saved_image" || return 1
    ctl --image-save-as "$saved_image" || return 1
    cmp "$saved_image" "$original_asset" || return 1
    failed_image="$SCOPE_ROOT/missing-parent/failure.bmp"
    ctl --image-save-failure "$failed_image" || return 1
    [ ! -e "$failed_image" ] || return 1
    printf 'PASS E2E-IMAGE-SAVE-AS-BYTES exact_source_bytes=true overwrite_confirmed=true failure_non_destructive=true\n'
    printf 'PASS E2E-IMAGE-SAVE-REOPEN exact_source_sha256=%s relative_width=%s embedded_width=%s original_asset_unchanged=true\n' "$reopened_sha" "$reopened_width" "$reopened_embedded_width"
}

case_workspace_session() {
    new_scope
    generate_profile workspace "$SCOPE_ROOT/workspace" || return 1
    workspace="$SCOPE_ROOT/workspace"
    start_app --test-mode --workspace "$workspace" \
        --open "$workspace/documents/group-0/document-04.md" \
        --open "$workspace/documents/group-1/document-05.md" \
        --open "$workspace/documents/group-2/document-06.md" \
        --open "$workspace/documents/group-3/document-07.md" \
        --open "$workspace/duplicate/a/note.md" \
        --open "$workspace/duplicate/b/note.md" \
        --open "$workspace/包含 空格/繁體中文.md" || return 1
    ctl --workspace-scenario || return 1
    before=$(ctl --dump-state) || return 1
    printf '%s\n' "$before"
    before_active=$(printf '%s\n' "$before" | state_field active_id)
    before_order=$(printf '%s\n' "$before" | state_field tab_order_sha256)
    ctl --close-window || return 1
    wait "$APP_PID" || return 1
    APP_PID=
    start_app --test-mode --workspace "$workspace" || return 1
    after=$(ctl --dump-state --state-contains '"mode":"Split"') || return 1
    printf '%s\n' "$after"
    after_active=$(printf '%s\n' "$after" | state_field active_id)
    after_order=$(printf '%s\n' "$after" | state_field tab_order_sha256)
    [ -n "$before_active" ] && [ "$before_active" = "$after_active" ] || return 1
    [ -n "$before_order" ] && [ "$before_order" = "$after_order" ] || return 1
    printf 'PASS E2E-WORKSPACE-SESSION-RESTORE exact_active_and_tab_order=true\n'
}

case_keyboard_lifecycle() {
    new_scope
    start_app --test-mode || return 1
    ctl --state-contains '"documents":0' || return 1
    ctl --press-key n --modifiers 4 --state-contains '"documents":1' || return 1
    ctl --type-text KeyboardFlow123 --state-contains '"dirty":true' || return 1
    ctl --press-key s --modifiers 5 --state-contains '"modal":2' || return 1
    saved_path="$SCOPE_ROOT/keyboard-flow.md"
    ctl --type-text "$saved_path" || return 1
    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --state-contains '"dirty":false' || return 1
    rg -q 'KeyboardFlow123' "$saved_path" || return 1
    ctl --press-key w --modifiers 4 --state-contains '"documents":0' || return 1
    ctl --press-key t --modifiers 5 --state-contains '"documents":1' || return 1
    ctl --state-contains '"dirty":false' || return 1
    ctl --press-key n --modifiers 4 --state-contains '"documents":2' || return 1
    ctl --type-text DirtyCloseSave --state-contains '"dirty":true' || return 1
    ctl --press-key w --modifiers 4 --state-contains '"modal":4' || return 1
    ctl --press-key Return --state-contains '"modal":2' || return 1
    close_saved="$SCOPE_ROOT/dirty-close-save.md"
    ctl --type-text "$close_saved" || return 1
    ctl --press-key Return --state-contains '"documents":1' || return 1
    rg -q 'DirtyCloseSave' "$close_saved" || return 1
    printf 'PASS E2E-KEYBOARD-NEW-EDIT-SAVE-AS-CLOSE-REOPEN exact_file_content=true\n'
}

case_external_conflicts() {
    new_scope
    generate_profile small "$SCOPE_ROOT/small" || return 1
    target="$SCOPE_ROOT/small/active.md"
    cp "$SCOPE_ROOT/small/README.md" "$target" || return 1
    start_app --open "$target" || return 1

    cp "$SCOPE_ROOT/small/notes/guide.md" "$target" || return 1
    sleep 1.8
    ctl --state-contains '"modal":11' || return 1
    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --state-contains '"conflict":false' || return 1
    printf 'PASS E2E-EXTERNAL-CLEAN-RELOAD\n'

    cp "$SCOPE_ROOT/small/notes/checklist.md" "$target" || return 1
    sleep 1.8
    ctl --state-contains '"modal":11' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":1' || return 1
    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --state-contains '"conflict":true' || return 1
    ctl --press-key s --modifiers 4 --state-contains '"modal":11' || return 1
    printf 'PASS E2E-EXTERNAL-KEEP-CURRENT-BLOCKED-SAVE\n'

    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --press-key x --state-contains '"dirty":true' || return 1
    cp "$SCOPE_ROOT/small/README.md" "$target" || return 1
    sleep 1.8
    ctl --state-contains '"modal":11' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":1' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":2' || return 1
    ctl --press-key Return --state-contains '"modal":7' || return 1
    ctl --press-key Escape --state-contains '"modal":0' || return 1
    printf 'PASS E2E-EXTERNAL-DIRTY-COMPARE\n'

    ctl --press-key s --modifiers 4 --state-contains '"modal":11' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":1' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":2' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":3' || return 1
    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --state-contains '"dirty":false' || return 1
    ctl --state-contains '"conflict":false' || return 1
    printf 'PASS E2E-EXTERNAL-DIRTY-EXPLICIT-OVERWRITE\n'

    mv "$target" "$target.deleted" || return 1
    sleep 1.8
    ctl --state-contains '"orphaned":true' || return 1
    ctl --state-contains '"modal":11' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":1' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":2' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":3' || return 1
    ctl --press-key Tab --state-contains '"modal_selection":4' || return 1
    ctl --press-key Return --state-contains '"modal":2' || return 1
    recovered="$SCOPE_ROOT/recovered.md"
    ctl --type-text "$recovered" || return 1
    ctl --press-key Return --state-contains '"modal":0' || return 1
    ctl --state-contains '"orphaned":false' || return 1
    ctl --state-contains '"dirty":false' || return 1
    [ -f "$recovered" ] || return 1
    printf 'PASS E2E-EXTERNAL-DELETION-SAVE-AS\n'
}

case_killed_recovery() {
    new_scope
    generate_profile unicode "$SCOPE_ROOT/unicode" || return 1
    mkdir -p "$SCOPE_CONFIG/mdeditor"
    cp "$PROJECT_ROOT/tests/data/preferences-autosave-10.json" "$SCOPE_CONFIG/mdeditor/preferences.json" || return 1
    recovery_source="$SCOPE_ROOT/unicode/中文 路徑/Unicode 測試.md"
    start_app --open "$recovery_source" || return 1
    ctl --press-key x --state-contains '"dirty":true' || return 1
    sleep 11
    before=$(ctl --dump-state) || return 1
    printf '%s\n' "$before"
    expected_sha=$(printf '%s\n' "$before" | state_field source_sha256)
    [ -n "$expected_sha" ] || return 1
    kill -9 "$APP_PID" || return 1
    wait "$APP_PID" 2>/dev/null || true
    APP_PID=
    start_app || return 1
    ctl --state-contains '"modal":12' || return 1
    ctl --press-key Return --state-contains '"dirty":true' || return 1
    ctl --state-contains '"modal":0' || return 1
    after=$(ctl --dump-state) || return 1
    printf '%s\n' "$after"
    actual_sha=$(printf '%s\n' "$after" | state_field source_sha256)
    [ "$expected_sha" = "$actual_sha" ] || return 1
    printf 'PASS E2E-KILLED-PROCESS-RECOVERY exact_source_sha256=%s\n' "$actual_sha"
}

run_case E2E-CLIPBOARD-EDITOR case_clipboard_editor
run_case E2E-UI-INTERACTION case_ui_interaction
run_case E2E-SEARCH-REPLACE case_search_replace
run_case E2E-XIM case_xim
run_case E2E-XDND case_xdnd
run_case E2E-IMAGE-WORKFLOWS case_image_workflows
run_case E2E-WORKSPACE-SESSION case_workspace_session
run_case E2E-KEYBOARD-LIFECYCLE case_keyboard_lifecycle
run_case E2E-EXTERNAL-CONFLICTS case_external_conflicts
run_case E2E-KILLED-RECOVERY case_killed_recovery

printf 'E2E_RELEASE_SUMMARY total=%d passed=%d failed=%d skipped=0\n' "$TOTAL" "$PASSED" "$FAILED"
printf 'E2E_RELEASE_SUMMARY total=%d passed=%d failed=%d skipped=0\n' "$TOTAL" "$PASSED" "$FAILED" >>"$LOG_FILE"
[ "$FAILED" -eq 0 ]
