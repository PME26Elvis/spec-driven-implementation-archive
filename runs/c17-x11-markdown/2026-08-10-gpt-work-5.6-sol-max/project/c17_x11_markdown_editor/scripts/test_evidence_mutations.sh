#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BASE_MANIFEST=${1:-evidence/manifest.json}
TASK_TEMP=$(mktemp -d /tmp/mdeditor-evidencecheck-XXXXXX)
cleanup() {
    rm -rf -- "$TASK_TEMP"
    rm -f -- "$PROJECT_ROOT/evidence/manifest-missing-file.json" \
        "$PROJECT_ROOT/evidence/manifest-digest-mismatch.json" \
        "$PROJECT_ROOT/evidence/manifest-screenshot-omission.json" \
        "$PROJECT_ROOT/evidence/manifest-failed-summary.json"
}
trap cleanup EXIT HUP INT TERM

expect_failure() {
    test_id=$1
    manifest=$2
    diagnostic=$3
    log="$TASK_TEMP/$test_id.log"
    if "$PROJECT_ROOT/bin/evidencecheck" --root "$PROJECT_ROOT" --manifest "$manifest" >"$log" 2>&1; then
        printf 'FAIL %s validator accepted invalid evidence\n' "$test_id"
        return 1
    fi
    if ! rg -q "$diagnostic" "$log"; then
        printf 'FAIL %s expected diagnostic %s\n' "$test_id" "$diagnostic"
        sed -n '1,120p' "$log"
        return 1
    fi
    printf 'PASS %s\n' "$test_id"
}

cp -- "$PROJECT_ROOT/$BASE_MANIFEST" "$PROJECT_ROOT/evidence/manifest-missing-file.json"
sed -i '0,/"path":"evidence\/logs\/unit.log"/s//"path":"evidence\/logs\/does-not-exist.log"/' \
    "$PROJECT_ROOT/evidence/manifest-missing-file.json"
expect_failure EVIDENCE-MISSING-FILE evidence/manifest-missing-file.json 'unsafe/missing path'

cp -- "$PROJECT_ROOT/$BASE_MANIFEST" "$PROJECT_ROOT/evidence/manifest-digest-mismatch.json"
sed -i '0,/"sha256":"[0-9a-f]\{64\}"/s//"sha256":"0000000000000000000000000000000000000000000000000000000000000000"/' \
    "$PROJECT_ROOT/evidence/manifest-digest-mismatch.json"
expect_failure EVIDENCE-DIGEST-MISMATCH evidence/manifest-digest-mismatch.json 'mismatch'

cp -- "$PROJECT_ROOT/$BASE_MANIFEST" "$PROJECT_ROOT/evidence/manifest-screenshot-omission.json"
sed -i '0,/"id":"UI-EMPTY-LIGHT"/s//"id":"UI-OMITTED-LIGHT"/' \
    "$PROJECT_ROOT/evidence/manifest-screenshot-omission.json"
expect_failure EVIDENCE-SCREENSHOT-OMISSION evidence/manifest-screenshot-omission.json 'missing screenshot UI-EMPTY-LIGHT'

cp -- "$PROJECT_ROOT/$BASE_MANIFEST" "$PROJECT_ROOT/evidence/manifest-failed-summary.json"
sed -i '0,/"total":119,"passed":119,"failed":0,"skipped":0/s//"total":119,"passed":118,"failed":1,"skipped":0/' \
    "$PROJECT_ROOT/evidence/manifest-failed-summary.json"
expect_failure EVIDENCE-FAILED-TEST-COUNT evidence/manifest-failed-summary.json 'top-level schema/test summary'

rm -f -- "$PROJECT_ROOT/evidence/manifest-missing-file.json" \
    "$PROJECT_ROOT/evidence/manifest-digest-mismatch.json" \
    "$PROJECT_ROOT/evidence/manifest-screenshot-omission.json" \
    "$PROJECT_ROOT/evidence/manifest-failed-summary.json"

printf '%s\n' 'EVIDENCE_MUTATION_SUMMARY total=4 passed=4 failed=0 skipped=0'
