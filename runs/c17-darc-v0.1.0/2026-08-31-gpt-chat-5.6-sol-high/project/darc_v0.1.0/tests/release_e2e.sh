#!/bin/sh
set -eu

ROOT=${DARC_E2E_ROOT:-/tmp/darc-release-e2e}
rm -rf "$ROOT"
mkdir -p "$ROOT/src/nested"
REPO="$ROOT/repo"
SRC="$ROOT/src"

# Deterministic 2 MiB payload: actual bytes are read/chunked; a few offsets are non-zero.
dd if=/dev/zero of="$SRC/data.bin" bs=1048576 count=2 status=none
printf 'DARC-E2E-A' | dd of="$SRC/data.bin" bs=1 seek=12345 conv=notrunc status=none
printf 'nested-v1\n' > "$SRC/nested/note.txt"
ln "$SRC/data.bin" "$SRC/hard-a.bin"
ln "$SRC/data.bin" "$SRC/hard-b.bin"
ln -s nested/note.txt "$SRC/note-link"

./darc init "$REPO"
./darc --repo "$REPO" snapshot create "$SRC" --name base --timestamp 1000000000
S1=$(tr -d '\n' < "$REPO/HEAD")

printf 'DARC-E2E-B' | dd of="$SRC/data.bin" bs=1 seek=700000 conv=notrunc status=none
printf 'added-v2\n' > "$SRC/added.txt"
chmod 0750 "$SRC/nested"
./darc --repo "$REPO" snapshot create "$SRC" --name current --timestamp 2000000000
S2=$(tr -d '\n' < "$REPO/HEAD")
[ "$S1" != "$S2" ]

./darc --repo "$REPO" snapshot diff "$S1" "$S2" --format json > "$ROOT/diff.json"
./darc --repo "$REPO" restore "$S1" --to "$ROOT/restore-v1"
printf 'nested-v1\n' > "$ROOT/expected-note"
cmp "$ROOT/restore-v1/src/nested/note.txt" "$ROOT/expected-note"

# Corrupt one raw CHUNK object directly. Object type byte is offset 8 in DARCOBJ1 framing.
CHUNK=
for f in "$REPO"/objects/sha256/*/*; do
  [ -f "$f" ] || continue
  T=$(od -An -tu1 -j 8 -N 1 "$f" | tr -d '[:space:]')
  if [ "$T" = "1" ]; then CHUNK=$f; break; fi
done
[ -n "$CHUNK" ]
SZ=$(wc -c < "$CHUNK" | tr -d '[:space:]')
OFF=$((SZ - 1))
OLD=$(od -An -tu1 -j "$OFF" -N 1 "$CHUNK" | tr -d '[:space:]')
NEW=$((OLD ^ 1))
OCT=$(printf '%03o' "$NEW")
printf "\\$OCT" | dd of="$CHUNK" bs=1 seek="$OFF" conv=notrunc status=none

set +e
./darc --repo "$REPO" verify --level scrub > "$ROOT/verify-corrupt.out" 2> "$ROOT/verify-corrupt.err"
DETECT_RC=$?
set -e
[ "$DETECT_RC" -eq 6 ]
./darc --repo "$REPO" verify --level scrub --repair > "$ROOT/verify-repair.out" 2> "$ROOT/verify-repair.err"
grep -q 'REPAIRED' "$ROOT/verify-repair.out"

./darc --repo "$REPO" snapshot delete "$S1" --yes
./darc --repo "$REPO" gc
./darc --repo "$REPO" verify --level scrub > "$ROOT/final-scrub.out"
grep -q 'HEALTHY' "$ROOT/final-scrub.out"

./darc --repo "$REPO" restore "$S2" --to "$ROOT/restore-v2"
cmp "$SRC/data.bin" "$ROOT/restore-v2/src/data.bin"
cmp "$SRC/added.txt" "$ROOT/restore-v2/src/added.txt"
cmp "$SRC/nested/note.txt" "$ROOT/restore-v2/src/nested/note.txt"
[ "$(readlink "$ROOT/restore-v2/src/note-link")" = 'nested/note.txt' ]
I1=$(stat -c %i "$ROOT/restore-v2/src/data.bin")
I2=$(stat -c %i "$ROOT/restore-v2/src/hard-a.bin")
I3=$(stat -c %i "$ROOT/restore-v2/src/hard-b.bin")
[ "$I1" = "$I2" ] && [ "$I2" = "$I3" ]

printf 'E2E_PASS snapshot1=%s snapshot2=%s corrupt_detect_rc=%s final_scrub=HEALTHY\n' "$S1" "$S2" "$DETECT_RC"
