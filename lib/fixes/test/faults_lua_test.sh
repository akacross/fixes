#!/usr/bin/env bash
# lib/fixes/native/faults.c against the installation's own lua51.dll under Wine,
# in a throwaway prefix, never the game's (faults_lua_test.c).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
game="$(cd "$here/../../../.." && pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
i686-w64-mingw32-gcc -O2 -Wall -static -o "$work/faults_lua_test.exe" "$here/faults_lua_test.c"
cp "$game/lua51.dll" "$work/"
cd "$work"
wine faults_lua_test.exe 'Z:'"${work//\//\\}" 'Z:'"${work//\//\\}"'\faults.log'
