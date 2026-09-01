#!/usr/bin/env bash
# build.sh - Build cvc.exe (and test harnesses) with MinGW-w64 GCC (C17).
# Uses Windows-style paths for the native compiler. Output goes to D: to
# conserve C: space. Usage: bash build.sh [all|exe|clean]
set -euo pipefail

TOOLCHAIN="/d/0831-cvc-workbuddy/toolchain/mingw64"
GCC="$TOOLCHAIN/bin/gcc.exe"
AR="$TOOLCHAIN/bin/ar.exe"
ROOT="/d/0831-cvc-workbuddy/cvc"
SRC="$ROOT/src"
INC="$ROOT/include"
OUT="/d/0831-cvc-workbuddy/.cvc_build_tmp"
OBJ="$OUT/obj"
BIN="$OUT/bin"

# Windows path form for the compiler
win() { cygpath -w "$1"; }

mkdir -p "$OBJ" "$BIN"

CFLAGS="-std=c17 -O2 -Wall -Wextra -Wno-unused-parameter -DWIN32_LEAN_AND_MEAN -I$(win "$INC")"

SOURCES=(
  util.c sha256.c utf8.c json.c glob.c diff.c win32.c repo.c objects.c scan.c snapshot.c
  materialize.c merge.c verify.c cli.c
)

compile_one() {
  local name="$1" wname
  wname=$(win "$SRC/$name")
  local obj="$OBJ/${name%.c}.o"
  "$GCC" $CFLAGS -c -o "$(win "$obj")" "$wname"
}

build_exe() {
  local objs=()
  local name o
  for name in "${SOURCES[@]}"; do
    o="$OBJ/${name%.c}.o"
    objs+=("$o")
  done
  local objs_str=""
  for o in "${objs[@]}"; do objs_str="$objs_str $(win "$o")"; done
  "$GCC" $CFLAGS -municode -o "$(win "$BIN/cvc.exe")" $objs_str $(win "$SRC/main.c")
  echo "== built $(win "$BIN/cvc.exe") =="
}

case "${1:-all}" in
  all)
    for name in "${SOURCES[@]}"; do compile_one "$name"; done
    build_exe
    ;;
  exe)
    build_exe
    ;;
  clean)
    rm -rf "$OBJ" "$BIN"
    ;;
  *)
    echo "usage: build.sh [all|exe|clean]" >&2; exit 1
    ;;
esac
