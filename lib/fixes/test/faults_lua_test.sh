#!/usr/bin/env bash
# lib/fixes/native/faults.c against the installation's own lua51.dll under Wine,
# in a throwaway prefix, never the game's (faults_lua_test.c).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/native_env.sh"   # CC, PY, run_exe, winpath: Linux (Wine) or Windows (native)
game="$(cd "$here/../../../.." && pwd)"
work="$(mktemp -d)"; trap 'rc=$?; cd /; rm -rf "$work"; exit $rc' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
"$CC" -O2 -Wall -static -o "$work/faults_lua_test.exe" "$here/faults_lua_test.c"
cp "$game/lua51.dll" "$work/"
cd "$work"
run_exe faults_lua_test.exe "$(winpath "$work")" "$(winpath "$work")\faults.log"
