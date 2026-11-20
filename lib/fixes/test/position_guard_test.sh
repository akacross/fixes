#!/usr/bin/env bash
# fixes.lua's position guard on fake entities under Wine, in a throwaway
# prefix, never the game's (position_guard_test.c). Needs wine, luajit and
# i686-w64-mingw32-gcc.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/native_env.sh"   # CC, PY, run_exe, winpath: Linux (Wine) or Windows (native)
work="$(mktemp -d)"; trap 'rc=$?; cd /; rm -rf "$work"; exit $rc' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
"$CC" -O2 -Wall -static -o "$work/position_guard_test.exe" "$here/position_guard_test.c"
luajit "$here/fixes_test.lua" --guard-bytes "$work/guard.bin" >/dev/null
cd "$work"
run_exe position_guard_test.exe
