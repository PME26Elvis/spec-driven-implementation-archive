#!/usr/bin/env bash
# Standalone MSVC build helper for the GUI application (sudoku.exe).
# Mirrors tools/build_exe.sh but links with /SUBSYSTEM:WINDOWS so the
# WinMain entry point (src/app/entry/sdk_entry.c) is valid. No vcvars/reg.exe.
set -u
export MSYS_NO_PATHCONV=1

ROOTWIN="$(cd "$(dirname "$0")/.." && pwd -W)"
VC="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.41.34120"
SDK="C:/Program Files (x86)/Windows Kits/10"
SDKVER="10.0.22621.0"

export INCLUDE="$VC/include;$SDK/Include/$SDKVER/um;$SDK/Include/$SDKVER/shared;$SDK/Include/$SDKVER/winrt;$SDK/Include/$SDKVER/ucrt"
export LIB="$VC/lib/x64;$SDK/Lib/$SDKVER/um/x64;$SDK/Lib/$SDKVER/ucrt/x64"

CL="$VC/bin/Hostx64/x64/cl.exe"
LINK="$VC/bin/Hostx64/x64/link.exe"

NAME="${1:-sudoku}"
shift || true

OBJ="$ROOTWIN/build/obj/$NAME"
BIN="$ROOTWIN/build/bin"
LOG="$ROOTWIN/build/logs"
mkdir -p "$OBJ" "$BIN" "$LOG"
rm -f "$OBJ"/*.obj 2>/dev/null

# Default: compile every translation unit under src/ (absolute Windows paths).
if [ $# -eq 0 ]; then
  SRCS=()
  while IFS= read -r f; do
    SRCS+=("$ROOTWIN/${f#./}")
  done < <(cd "$ROOTWIN" && find src -name '*.c' | sort)
else
  SRCS=()
  for s in "$@"; do
    case "$s" in
      /*|*:/?*) SRCS+=("$s") ;;
      *)        SRCS+=("$ROOTWIN/$s") ;;
    esac
  done
fi

CFLAGS="/nologo /std:c17 /W4 /WX /O2 /Zi /MT /GS /Gy /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /I$ROOTWIN/include"
LDFLAGS="/nologo /DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /MACHINE:X64 /SUBSYSTEM:WINDOWS"
LDLIBS="kernel32.lib user32.lib gdi32.lib bcrypt.lib"

echo "=== compile $NAME (WINDOWS subsystem) ===" > "$LOG/$NAME.log"
"$CL" $CFLAGS /c /Fo"$OBJ/" /Fd"$OBJ/$NAME.pdb" "${SRCS[@]}" >> "$LOG/$NAME.log" 2>&1
if [ $? -ne 0 ]; then echo "[build] compile failed - see $LOG/$NAME.log"; tail -30 "$LOG/$NAME.log"; exit 1; fi

echo "=== link $NAME ===" >> "$LOG/$NAME.log"
"$LINK" $LDFLAGS /OUT:"$BIN/$NAME.exe" /PDB:"$BIN/$NAME.pdb" "$OBJ/*.obj" $LDLIBS >> "$LOG/$NAME.log" 2>&1
if [ $? -ne 0 ]; then echo "[build] link failed - see $LOG/$NAME.log"; tail -30 "$LOG/$NAME.log"; exit 1; fi

echo "EXITCODE=0" >> "$LOG/$NAME.log"
echo "[build] ok: $BIN/$NAME.exe"
