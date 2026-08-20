#!/usr/bin/env bash
# Build the Win32 pinball sandbox application (editor + playable sandbox).
# Depends on build_core.sh having been run (core .o objects in build/).
set -e
CC="D:/0814/toolchain/mingw64/bin/gcc.exe"
ROOT="D:/0814/pinball_sandbox_v1.0.0"
CORE="$ROOT/src/core"
APP="$ROOT/src/app"
BUILD="$ROOT/build"
mkdir -p "$BUILD"

CFLAGS="-std=c17 -O2 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN"

# Ensure core objects are present (rebuild core if needed)
if [ ! -f "$BUILD/types.o" ] || [ ! -f "$BUILD/scene.o" ] || [ ! -f "$BUILD/render.o" ] || [ ! -f "$BUILD/png.o" ]; then
  echo "core objects missing; running build_core.sh first"
  bash "$ROOT/tools/build_core.sh" >/dev/null
fi

# App objects
"$CC" $CFLAGS -I"$CORE" -I"$APP" -c "$APP/editor.c" -o "$BUILD/editor.o"
"$CC" $CFLAGS -I"$CORE" -I"$APP" -c "$APP/platform.c" -o "$BUILD/platform.o"

# Link Win32 app (User32 + GDI32 + Imm32 + common dialogs + shell)
"$CC" $CFLAGS \
  "$BUILD/hash.o" "$BUILD/rng.o" "$BUILD/types.o" "$BUILD/scene.o" \
  "$BUILD/scene_parse.o" "$BUILD/scene_write.o" "$BUILD/scene_validate.o" \
  "$BUILD/sim.o" "$BUILD/replay.o" "$BUILD/render.o" "$BUILD/png.o" \
  "$BUILD/editor.o" "$BUILD/platform.o" \
  -o "$BUILD/pinball_sandbox.exe" \
  -luser32 -lgdi32 -limm32 -lcomdlg32 -lshell32

echo "APP BUILD OK -> $BUILD/pinball_sandbox.exe"
