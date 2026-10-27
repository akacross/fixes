#!/usr/bin/env bash
# fixes.lua's frame-rate stubs under Wine with real x87 maths, in a throwaway
# prefix, never the game's (fps_fixes_test.c). Needs wine, luajit and
# i686-w64-mingw32-gcc.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
i686-w64-mingw32-gcc -O2 -Wall -msse2 -mfpmath=sse -static -o "$work/fps_fixes_test.exe" "$here/fps_fixes_test.c"
luajit "$here/fixes_test.lua" --fps-bytes "$work/fps.bin" >/dev/null
cd "$work"
wine fps_fixes_test.exe
