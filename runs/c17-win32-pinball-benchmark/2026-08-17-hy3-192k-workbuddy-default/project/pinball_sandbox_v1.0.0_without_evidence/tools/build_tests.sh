#!/usr/bin/env bash
# Build and run the unified automated test suite.
set -e
CC="D:/0814/toolchain/mingw64/bin/gcc.exe"
ROOT="D:/0814/pinball_sandbox_v1.0.0"
CORE="$ROOT/src/core"
BUILD="$ROOT/build"
mkdir -p "$BUILD"

CFLAGS="-std=c17 -O2 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN"

# ensure core objects exist
if [ ! -f "$BUILD/sim.o" ] || [ ! -f "$BUILD/replay.o" ] || [ ! -f "$BUILD/render.o" ] || [ ! -f "$BUILD/png.o" ]; then
  bash "$ROOT/tools/build_core.sh" >/dev/null
fi

"$CC" $CFLAGS -I"$CORE" -c "$ROOT/tools/tests.c" -o "$BUILD/tests.o"
"$CC" $CFLAGS \
  "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" \
  "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" \
  "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/render.o" "$BUILD/png.o" \
  "$BUILD/tests.o" -o "$BUILD/tests.exe"

echo "TESTS BUILD OK"
"$BUILD/tests.exe"
