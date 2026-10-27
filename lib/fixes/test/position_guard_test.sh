#!/usr/bin/env bash
# fixes.lua's position guard on fake entities under Wine, in a throwaway
# prefix, never the game's (position_guard_test.c). Needs wine, luajit and
# i686-w64-mingw32-gcc.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
i686-w64-mingw32-gcc -O2 -Wall -static -o "$work/position_guard_test.exe" "$here/position_guard_test.c"
luajit "$here/fixes_test.lua" --guard-bytes "$work/guard.bin" >/dev/null
cd "$work"
wine position_guard_test.exe
