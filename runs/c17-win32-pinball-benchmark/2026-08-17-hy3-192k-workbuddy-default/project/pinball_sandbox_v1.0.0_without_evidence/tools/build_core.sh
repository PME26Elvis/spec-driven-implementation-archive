#!/usr/bin/env bash
# Build helper for the pinball sandbox core + pcheck tool.
# Uses the MinGW-w64 toolchain on D: (C: is full).
set -e
CC="D:/0814/toolchain/mingw64/bin/gcc.exe"
ROOT="D:/0814/pinball_sandbox_v1.0.0"
CORE="$ROOT/src/core"
BUILD="$ROOT/build"
mkdir -p "$BUILD"

# compile core objects with C17 + strict warnings-as-errors off (keep build green)
CFLAGS="-std=c17 -O2 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN"

for f in hash rng types scene scene_parse scene_write scene_validate sim replay render png; do
  "$CC" $CFLAGS -c "$CORE/$f.c" -o "$BUILD/$f.o"
done

# pcheck tool
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/pcheck.c" -o "$BUILD/pcheck.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/rtcheck.c" -o "$BUILD/rtcheck.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/simcheck.c" -o "$BUILD/simcheck.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/scenecheck.c" -o "$BUILD/scenecheck.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/replaycheck.c" -o "$BUILD/replaycheck.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/detcompare.c" -o "$BUILD/detcompare.o"
"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/framegen.c" -o "$BUILD/framegen.o"

"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/replay.o" "$BUILD/pcheck.o" -o "$BUILD/pcheck.exe"
"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/replay.o" "$BUILD/rtcheck.o" -o "$BUILD/rtcheck.exe"
"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/simcheck.o" -o "$BUILD/simcheck.exe"
"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/scenecheck.o" -o "$BUILD/scenecheck.exe"
"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/replaycheck.o" -o "$BUILD/replaycheck.exe"
"$CC" $CFLAGS "$BUILD/detcompare.o" -o "$BUILD/detcompare.exe"
"$CC" $CFLAGS "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/render.o" "$BUILD/png.o" "$BUILD/framegen.o" -o "$BUILD/framegen.exe"

echo "BUILD OK"
