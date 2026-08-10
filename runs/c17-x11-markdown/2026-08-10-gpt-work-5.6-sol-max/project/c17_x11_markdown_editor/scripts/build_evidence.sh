#!/bin/sh
set -u

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LOG_DIR="$PROJECT_ROOT/evidence/logs"
RESULT_DIR="$PROJECT_ROOT/evidence/results"
FAILURES=0
UNIT_MS=0
INTEGRATION_MS=0
E2E_MS=0
PERFORMANCE_MS=0
FAILURE_MS=0
REGRESSION_MS=0

mkdir -p "$LOG_DIR" "$RESULT_DIR"

elapsed_ms() {
    start_ns=$1
    end_ns=$2
    printf '%s' "$(((end_ns-start_ns)/1000000))"
}

run_logged() {
    run_name=$1
    log_path=$2
    shift 2
    start_ns=$(date +%s%N)
    if "$@" >"$log_path" 2>&1; then
        status=0
    else
        status=$?
        FAILURES=$((FAILURES + 1))
    fi
    end_ns=$(date +%s%N)
    RUN_DURATION=$(elapsed_ms "$start_ns" "$end_ns")
    printf '%s %s duration_ms=%s log=%s\n' "$([ "$status" -eq 0 ] && printf PASS || printf FAIL)" "$run_name" "$RUN_DURATION" "$log_path"
    return 0
}

unit_suite() {
    "$PROJECT_ROOT/bin/test_core"
    "$PROJECT_ROOT/bin/test_tools"
}

integration_suite() {
    "$PROJECT_ROOT/bin/test_storage"
    "$PROJECT_ROOT/scripts/test_utilities.sh"
}

regression_suite() {
    "$PROJECT_ROOT/bin/acceptance_matrix"
}

run_logged FIXTURES "$LOG_DIR/fixtures.log" "$PROJECT_ROOT/scripts/generate_fixtures.sh"

run_logged UNIT "$LOG_DIR/unit.log" unit_suite
UNIT_MS=$RUN_DURATION
run_logged INTEGRATION "$LOG_DIR/integration.log" integration_suite
INTEGRATION_MS=$RUN_DURATION

start_ns=$(date +%s%N)
if "$PROJECT_ROOT/scripts/run_e2e.sh"; then
    status=0
else
    status=$?
    FAILURES=$((FAILURES + 1))
fi
end_ns=$(date +%s%N)
E2E_MS=$(elapsed_ms "$start_ns" "$end_ns")
printf '%s E2E duration_ms=%s log=%s\n' "$([ "$status" -eq 0 ] && printf PASS || printf FAIL)" "$E2E_MS" "$LOG_DIR/e2e.log"

run_logged PERFORMANCE "$LOG_DIR/performance.log" "$PROJECT_ROOT/bin/performance" \
    --fixtures "$PROJECT_ROOT/evidence/fixtures" --output "$RESULT_DIR/performance.json"
PERFORMANCE_MS=$RUN_DURATION
run_logged FAILURE "$LOG_DIR/failure.log" "$PROJECT_ROOT/bin/failure_matrix" \
    --fixtures "$PROJECT_ROOT/evidence/fixtures" --output "$RESULT_DIR/failure.json"
FAILURE_MS=$RUN_DURATION
run_logged REGRESSION "$LOG_DIR/regression.log" regression_suite
REGRESSION_MS=$RUN_DURATION

run_logged SCREENSHOTS "$LOG_DIR/screenshots-runner.log" "$PROJECT_ROOT/scripts/capture_evidence.sh"
run_logged LOCSCAN "$LOG_DIR/locscan.log" "$PROJECT_ROOT/bin/locscan" --root "$PROJECT_ROOT" \
    --config "$PROJECT_ROOT/config/locscan.json" --json "$PROJECT_ROOT/evidence/loc-report.json" --details

if [ "$FAILURES" -eq 0 ]; then
    "$PROJECT_ROOT/bin/evidencegen" --root "$PROJECT_ROOT" --output evidence/manifest.json \
        --unit-ms "$UNIT_MS" --integration-ms "$INTEGRATION_MS" --e2e-ms "$E2E_MS" \
        --performance-ms "$PERFORMANCE_MS" --failure-ms "$FAILURE_MS" --regression-ms "$REGRESSION_MS" \
        >"$LOG_DIR/evidencegen-draft.log" 2>&1 || FAILURES=$((FAILURES + 1))
fi

if [ "$FAILURES" -eq 0 ]; then
    start_ns=$(date +%s%N)
    mutation_log="$RESULT_DIR/evidence-mutations.tmp"
    if "$PROJECT_ROOT/scripts/test_evidence_mutations.sh" >"$mutation_log" 2>&1; then
        status=0
    else
        status=$?
        FAILURES=$((FAILURES + 1))
    fi
    sed -n '1,240p' "$mutation_log" >>"$LOG_DIR/regression.log"
    rm -f -- "$mutation_log"
    end_ns=$(date +%s%N)
    REGRESSION_MS=$((REGRESSION_MS + $(elapsed_ms "$start_ns" "$end_ns")))
    printf '%s EVIDENCE-MUTATIONS duration_ms=%s\n' "$([ "$status" -eq 0 ] && printf PASS || printf FAIL)" "$REGRESSION_MS"
fi

if [ "$FAILURES" -eq 0 ]; then
    if ! "$PROJECT_ROOT/bin/evidencegen" --root "$PROJECT_ROOT" --output evidence/manifest.json \
        --unit-ms "$UNIT_MS" --integration-ms "$INTEGRATION_MS" --e2e-ms "$E2E_MS" \
        --performance-ms "$PERFORMANCE_MS" --failure-ms "$FAILURE_MS" --regression-ms "$REGRESSION_MS" \
        >"$LOG_DIR/evidencegen.log" 2>&1; then
        FAILURES=$((FAILURES + 1))
    fi
fi

if [ "$FAILURES" -eq 0 ]; then
    if ! "$PROJECT_ROOT/bin/evidencecheck" --root "$PROJECT_ROOT" --manifest evidence/manifest.json \
        >"$LOG_DIR/evidencecheck.log" 2>&1; then
        FAILURES=$((FAILURES + 1))
    fi
fi

if [ "$FAILURES" -eq 0 ]; then
    "$PROJECT_ROOT/scripts/write_release_report.sh" || FAILURES=$((FAILURES + 1))
fi

printf 'EVIDENCE_BUILD_SUMMARY total_stages=10 failed=%s skipped=0\n' "$FAILURES"
[ "$FAILURES" -eq 0 ]
