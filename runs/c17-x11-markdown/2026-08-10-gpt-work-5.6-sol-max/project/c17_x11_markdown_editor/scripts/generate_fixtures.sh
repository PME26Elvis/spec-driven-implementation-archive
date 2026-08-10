#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTPUT_ROOT="$PROJECT_ROOT/evidence/fixtures"
mkdir -p "$OUTPUT_ROOT"
STAGING_ROOT=$(mktemp -d "$PROJECT_ROOT/evidence/.fixtures-new-XXXXXX")
cleanup() {
    rm -rf -- "$STAGING_ROOT"
}
trap cleanup EXIT HUP INT TERM

for PROFILE in small unicode markdown-all workspace medium large stress-long-line failure; do
    "$PROJECT_ROOT/bin/fixturegen" --profile "$PROFILE" --output "$STAGING_ROOT/$PROFILE" --seed 424242
    "$PROJECT_ROOT/bin/fixturegen" --verify "$STAGING_ROOT/$PROFILE"
done

if [ -e "$STAGING_ROOT/workspace/.mdeditor" ]; then
    printf '%s\n' 'generate_fixtures: clean workspace profile unexpectedly contains .mdeditor state' >&2
    exit 1
fi

for PROFILE in small unicode markdown-all workspace medium large stress-long-line failure; do
    rm -rf -- "$OUTPUT_ROOT/$PROFILE"
    mv -- "$STAGING_ROOT/$PROFILE" "$OUTPUT_ROOT/$PROFILE"
done

printf '%s\n' 'FIXTURE_RELEASE_SUMMARY total=8 passed=8 failed=0 skipped=0'
