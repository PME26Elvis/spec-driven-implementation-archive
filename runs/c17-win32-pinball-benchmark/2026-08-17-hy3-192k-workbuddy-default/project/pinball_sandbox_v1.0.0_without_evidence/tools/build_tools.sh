#!/usr/bin/env bash
# Build the engineering utilities (locscan, releasecheck) and their JSON/YAML parsers.
set -e
CC="D:/0814/toolchain/mingw64/bin/gcc.exe"
ROOT="D:/0814/pinball_sandbox_v1.0.0"
TOOLS="$ROOT/tools"
BUILD="$ROOT/build"
mkdir -p "$BUILD"
CFLAGS="-std=c17 -O2 -Wall -Wextra -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN -D_GNU_SOURCE"

"$CC" $CFLAGS -c "$TOOLS/json.c" -o "$BUILD/json.o"
"$CC" $CFLAGS -c "$TOOLS/yaml.c" -o "$BUILD/yaml.o"
"$CC" $CFLAGS -I"$TOOLS" -c "$TOOLS/locscan.c" -o "$BUILD/locscan.o"
"$CC" $CFLAGS -I"$TOOLS" -c "$TOOLS/releasecheck.c" -o "$BUILD/releasecheck.o"

"$CC" $CFLAGS "$BUILD/json.o" "$BUILD/yaml.o" "$BUILD/locscan.o" -o "$BUILD/locscan.exe"
"$CC" $CFLAGS "$BUILD/json.o" "$BUILD/releasecheck.o" -o "$BUILD/releasecheck.exe"

echo "TOOLS BUILD OK"
