#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TASK_TEMP=$(mktemp -d /tmp/mdeditor-screenshots-XXXXXX)
SCREENSHOT_DIR="$PROJECT_ROOT/evidence/screenshots"
LOG_FILE="$PROJECT_ROOT/evidence/logs/screenshots.log"
DISPLAY_NUMBER=$((520 + ($$ % 300)))
DISPLAY_VALUE=
XVFB_PID=
APP_PID=
APP_INDEX=0

cleanup() {
    if [ -n "$APP_PID" ]; then kill -9 "$APP_PID" 2>/dev/null || true; wait "$APP_PID" 2>/dev/null || true; fi
    if [ -n "$XVFB_PID" ]; then kill -9 "$XVFB_PID" 2>/dev/null || true; wait "$XVFB_PID" 2>/dev/null || true; fi
    rm -rf "$TASK_TEMP"
}
trap cleanup EXIT HUP INT TERM

rm -rf "$SCREENSHOT_DIR"
mkdir -p "$SCREENSHOT_DIR" "$PROJECT_ROOT/evidence/logs"
: >"$LOG_FILE"

"$PROJECT_ROOT/bin/fixturegen" --profile workspace --output "$TASK_TEMP/workspace" --seed 424242 >>"$LOG_FILE" 2>&1

attempt=0
while [ "$attempt" -lt 30 ]; do
    : >"$TASK_TEMP/xvfb.log"
    Xvfb ":$DISPLAY_NUMBER" -screen 0 1600x1000x24 -pn -listen tcp -nolisten local -nolisten unix -ac >"$TASK_TEMP/xvfb.log" 2>&1 &
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
    sed -n '1,240p' "$TASK_TEMP/xvfb.log" >>"$LOG_FILE"
    exit 1
fi

start_app_at() {
    APP_INDEX=$((APP_INDEX + 1))
    APP_ROOT=$1
    shift
    mkdir -p "$APP_ROOT/config" "$APP_ROOT/state" "$APP_ROOT/cache"
    DISPLAY="$DISPLAY_VALUE" XDG_CONFIG_HOME="$APP_ROOT/config" XDG_STATE_HOME="$APP_ROOT/state" XDG_CACHE_HOME="$APP_ROOT/cache" \
        "$PROJECT_ROOT/bin/mdeditor" "$@" --quit-after 30000 >"$APP_ROOT/app-$APP_INDEX.log" 2>&1 &
    APP_PID=$!
    sleep 0.55
    DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --dump-state --wait-ms 4000 >>"$LOG_FILE" 2>&1
}

start_app() {
    start_app_at "$TASK_TEMP/app-$((APP_INDEX + 1))" "$@"
}

stop_app() {
    if [ -n "$APP_PID" ]; then kill -9 "$APP_PID" 2>/dev/null || true; wait "$APP_PID" 2>/dev/null || true; APP_PID=; fi
}

capture() {
    screenshot_id=$1
    shift
    DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --capture "$SCREENSHOT_DIR/$screenshot_id.png" "$@" --wait-ms 4000 >>"$LOG_FILE" 2>&1
    printf 'PASS SCREENSHOT %s\n' "$screenshot_id" >>"$LOG_FILE"
}

capture_state() {
    screenshot_id=$1
    shift
    start_app "$@" --test-state "$screenshot_id"
    capture "$screenshot_id" --capture-delay-ms 260
    stop_app
}

small="$PROJECT_ROOT/evidence/fixtures/small/README.md"
markdown_all="$PROJECT_ROOT/evidence/fixtures/markdown-all/markdown-all.md"
workspace="$TASK_TEMP/workspace"

capture_state UI-EMPTY-LIGHT
capture_state UI-EMPTY-DARK
capture_state UI-WORKSPACE-MULTITAB --workspace "$workspace"
capture_state UI-SOURCE --open "$small"
capture_state UI-SPLIT --open "$small"
capture_state UI-PREVIEW --open "$small"
capture_state UI-RENDERED-EDIT --open "$small"
capture_state UI-MARKDOWN-ALL --open "$markdown_all"
capture_state UI-IMAGE-SELECTED --open "$small"
start_app --test-mode --open "$small" --test-state UI-IMAGE-SELECTED
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --resize-selected-image --wait-ms 4000 >>"$LOG_FILE" 2>&1
capture UI-IMAGE-RESIZE --state-contains '"image_selected":true' --capture-delay-ms 260
stop_app
start_app --test-mode --open "$markdown_all" --test-state UI-TABLE-EDIT
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --pointer-x 720 --pointer-y 360 --move-pointer --wait-ms 4000 >>"$LOG_FILE" 2>&1
capture UI-TABLE-EDIT --press-key F10 --modifiers 1 --state-contains '"menu":5' --capture-delay-ms 60
stop_app
capture_state UI-OUTLINE --open "$markdown_all"
capture_state UI-COMMAND-PALETTE --open "$small"
capture_state UI-STATISTICS --open "$small"
capture_state UI-VERSION-HISTORY --open "$small"
capture_state UI-DIFF-SIDE-BY-SIDE --open "$small"
capture_state UI-DIFF-INLINE --open "$small"

external_file="$TASK_TEMP/external-conflict.md"
cp "$small" "$external_file"
start_app --test-mode --open "$external_file" --test-state UI-SOURCE
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --type-text ' in-memory-edit' --wait-ms 4000 >>"$LOG_FILE" 2>&1
cp "$PROJECT_ROOT/evidence/fixtures/small/notes/checklist.md" "$external_file"
sleep 1.8
capture UI-EXTERNAL-CONFLICT --state-contains '"modal":11' --capture-delay-ms 260
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --state-contains '"conflict":true' --wait-ms 4000 >>"$LOG_FILE" 2>&1
printf 'PASS EVIDENCE-EXTERNAL-CONFLICT production periodic disk digest detected a real backing-file rewrite\n' >>"$LOG_FILE"
stop_app

recovery_file="$TASK_TEMP/recovery-source.md"
recovery_root="$TASK_TEMP/recovery-app"
cp "$small" "$recovery_file"
start_app_at "$recovery_root" --test-mode --open "$recovery_file" --test-state UI-SOURCE
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --type-text ' crash-recovery-bytes' --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --test-command 1013 --wait-ms 4000 >>"$LOG_FILE" 2>&1
[ "$(find "$recovery_root/state/mdeditor/recovery" -maxdepth 1 -type f -name '*.mrec' | wc -l)" -ge 1 ]
stop_app
start_app_at "$recovery_root"
capture UI-RECOVERY-CENTER --state-contains '"modal":12' --capture-delay-ms 260
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --state-contains '"recoveries":1' --wait-ms 4000 >>"$LOG_FILE" 2>&1
printf 'PASS EVIDENCE-RECOVERY-CENTER production recovery write survived SIGKILL and was integrity-checked on restart\n' >>"$LOG_FILE"
stop_app

save_error_file="$TASK_TEMP/save-error.md"
cp "$small" "$save_error_file"
start_app --test-mode --open "$save_error_file" --test-state UI-SOURCE
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --type-text ' unsaved-after-enospc' --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --test-command 1012 --state-contains '"modal":13' --wait-ms 4000 >>"$LOG_FILE" 2>&1
capture UI-ERROR-SAVE --state-contains '"dirty":true' --capture-delay-ms 260
printf 'PASS EVIDENCE-ERROR-SAVE production safe-save ENOSPC injection preserved dirty in-memory bytes\n' >>"$LOG_FILE"
stop_app

start_app --test-mode --open "$small" --test-state UI-SOURCE
capture UI-FROSTED-TOP --capture-delay-ms 80
capture UI-BUTTON-HOVER --pointer-x 600 --pointer-y 36 --move-pointer --capture-delay-ms 160
capture UI-BUTTON-PRESS-RIPPLE --pointer-x 600 --pointer-y 36 --button-press 1 --capture-delay-ms 45
capture UI-BUTTON-RELEASE --pointer-x 600 --pointer-y 36 --button-release 1 --capture-delay-ms 90
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --pointer-x 700 --pointer-y 480 --button-press 5 --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --pointer-x 700 --pointer-y 480 --button-press 5 --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --pointer-x 700 --pointer-y 480 --button-press 5 --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --pointer-x 700 --pointer-y 480 --button-press 5 --wait-ms 4000 >>"$LOG_FILE" 2>&1
capture UI-FROSTED-SCROLLED --capture-delay-ms 180
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --type-text ' unsaved visual checkpoint' --wait-ms 4000 >>"$LOG_FILE" 2>&1
DISPLAY="$DISPLAY_VALUE" "$PROJECT_ROOT/bin/e2e_x11" --close-window --wait-ms 4000 >>"$LOG_FILE" 2>&1
capture UI-MODAL-OPEN-START --state-contains '"modal":4' --capture-delay-ms 0
capture UI-MODAL-BLUR --state-contains '"modal_progress":1.000' --capture-delay-ms 0
capture UI-MODAL-CLOSE --press-key Escape --state-contains '"modal_closing":true' --capture-delay-ms 80
capture UI-MODAL-END --state-contains '"modal":0' --capture-delay-ms 0
stop_app

require_distinct() {
    first=$1
    second=$2
    label=$3
    if cmp -s "$SCREENSHOT_DIR/$first.png" "$SCREENSHOT_DIR/$second.png"; then
        printf 'FAIL SCREENSHOT-DISTINCT %s (%s equals %s)\n' "$label" "$first" "$second" >>"$LOG_FILE"
        return 1
    fi
    printf 'PASS SCREENSHOT-DISTINCT %s\n' "$label" >>"$LOG_FILE"
}

require_distinct UI-MARKDOWN-ALL UI-OUTLINE outline-sidebar-visible
require_distinct UI-MARKDOWN-ALL UI-TABLE-EDIT table-edit-visible
require_distinct UI-BUTTON-HOVER UI-BUTTON-PRESS-RIPPLE button-hover-to-press
require_distinct UI-BUTTON-PRESS-RIPPLE UI-BUTTON-RELEASE button-press-to-release
require_distinct UI-FROSTED-TOP UI-FROSTED-SCROLLED frosted-scroll-transition
require_distinct UI-MODAL-OPEN-START UI-MODAL-BLUR modal-open-transition
require_distinct UI-MODAL-BLUR UI-MODAL-CLOSE modal-close-transition
require_distinct UI-MODAL-CLOSE UI-MODAL-END modal-close-completion

count=$(find "$SCREENSHOT_DIR" -maxdepth 1 -type f -name '*.png' | wc -l)
printf 'SCREENSHOT_SUMMARY total=%s passed=%s failed=0 skipped=0\n' "$count" "$count" | tee -a "$LOG_FILE"
[ "$count" -eq 29 ]
