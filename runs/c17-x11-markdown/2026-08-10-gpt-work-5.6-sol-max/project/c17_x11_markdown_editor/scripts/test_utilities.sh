#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TASK_TEMP=$(mktemp -d /tmp/mdeditor-tools-XXXXXX)
TEST_ROOT="$TASK_TEMP/tree"
mkdir -p "$TEST_ROOT/src" "$TEST_ROOT/tests" "$TEST_ROOT/docs" "$TEST_ROOT/build" "$TEST_ROOT/generated"
printf 'int main(void) { return 0; }\n' > "$TEST_ROOT/src/main.c"
printf 'one\ntwo' > "$TEST_ROOT/tests/test_sample.c"
printf '# Guide\n\nText\n' > "$TEST_ROOT/docs/guide.md"
printf 'excluded\n' > "$TEST_ROOT/build/generated.c"
printf 'override\n' > "$TEST_ROOT/generated/keep.md"
printf '\000binary\n' > "$TEST_ROOT/docs/binary.md"

JSON_CONFIG="$TASK_TEMP/config.json"
YAML_CONFIG="$TASK_TEMP/config.yaml"
sed 's#"evidence/fixtures/\*"#"evidence/fixtures/*", "generated/*"#; s#"Makefile"#"Makefile", "generated/keep.md"#' "$PROJECT_ROOT/config/locscan.json" > "$JSON_CONFIG"
sed 's#"evidence/fixtures/\*"#"evidence/fixtures/*", "generated/*"#; s#"Makefile"#"Makefile", "generated/keep.md"#' "$PROJECT_ROOT/config/locscan.yaml" > "$YAML_CONFIG"

"$PROJECT_ROOT/bin/locscan" --root "$TEST_ROOT" --config "$JSON_CONFIG" --json "$TASK_TEMP/json-report.json" --details > "$TASK_TEMP/json.log"
"$PROJECT_ROOT/bin/locscan" --root "$TEST_ROOT" --config "$YAML_CONFIG" --json "$TASK_TEMP/yaml-report.json" --details > "$TASK_TEMP/yaml.log"
cmp "$TASK_TEMP/json-report.json" "$TASK_TEMP/yaml-report.json"
rg -q '"grand_authored_lines": 7' "$TASK_TEMP/json-report.json"
rg -q 'generated/keep.md' "$TASK_TEMP/json-report.json"
if rg -q 'build/generated.c' "$TASK_TEMP/json-report.json"; then exit 1; fi
if rg -q 'binary.md' "$TASK_TEMP/json-report.json"; then exit 1; fi

printf '{broken' > "$TASK_TEMP/bad.json"
if "$PROJECT_ROOT/bin/locscan" --root "$TEST_ROOT" --config "$TASK_TEMP/bad.json" >/dev/null 2>"$TASK_TEMP/bad-json.log"; then exit 1; fi
rg -q 'Malformed JSON' "$TASK_TEMP/bad-json.log"
printf 'unknown_key:\n  - value\n' > "$TASK_TEMP/bad.yaml"
if "$PROJECT_ROOT/bin/locscan" --root "$TEST_ROOT" --config "$TASK_TEMP/bad.yaml" >/dev/null 2>"$TASK_TEMP/bad-yaml.log"; then exit 1; fi
rg -q 'Malformed YAML' "$TASK_TEMP/bad-yaml.log"

"$PROJECT_ROOT/bin/fixturegen" --profile unicode --output "$TASK_TEMP/fixture-a" --seed 8675309 > "$TASK_TEMP/fixture-a.log"
"$PROJECT_ROOT/bin/fixturegen" --profile unicode --output "$TASK_TEMP/fixture-b" --seed 8675309 > "$TASK_TEMP/fixture-b.log"
cmp "$TASK_TEMP/fixture-a/fixture-manifest.json" "$TASK_TEMP/fixture-b/fixture-manifest.json"
"$PROJECT_ROOT/bin/fixturegen" --verify "$TASK_TEMP/fixture-a" > "$TASK_TEMP/verify.log"
printf 'tamper\n' >> "$TASK_TEMP/fixture-a/path with spaces/搜尋 #1.md"
if "$PROJECT_ROOT/bin/fixturegen" --verify "$TASK_TEMP/fixture-a" >/dev/null 2>"$TASK_TEMP/tamper.log"; then exit 1; fi
rg -q 'mismatch' "$TASK_TEMP/tamper.log"

if "$PROJECT_ROOT/bin/evidencecheck" --root "$PROJECT_ROOT" --manifest ../escape.json >/dev/null 2>"$TASK_TEMP/path-escape.log"; then exit 1; fi
rg -q 'unsafe' "$TASK_TEMP/path-escape.log"
printf '{broken' > "$TASK_TEMP/bad-manifest.json"
if "$PROJECT_ROOT/bin/evidencecheck" --root "$TASK_TEMP" --manifest bad-manifest.json >/dev/null 2>"$TASK_TEMP/bad-evidence.log"; then exit 1; fi
rg -q 'malformed manifest' "$TASK_TEMP/bad-evidence.log"

printf 'UTILITY_TEST_SUMMARY total=14 passed=14 failed=0 skipped=0 temp=%s\n' "$TASK_TEMP"
