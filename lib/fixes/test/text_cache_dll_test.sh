#!/usr/bin/env bash
# lib/fixes/text_cache.dll against the real d3dx9_25.dll under Wine (wined3d), in
# a throwaway prefix, never the game's: text_cache_dll_test.c checks the exports
# and that replays match the real draw. `--bench` also times DrawText per frame:
# stock, the DLL, and TextCacheFix.asi when $TEXTCACHEFIX_ASI names one.
# D3DX9_25 defaults to the game prefix's syswow64 copy (read, never written);
# D3DX9_25=builtin tests Wine's own, as a prefix without Microsoft's has it.
# DXVK_D3D9 (default: Proton's 32-bit DXVK, as the game uses) renders when it
# exists; wined3d's sprite flush slows down when text is drawn faster, so its
# whole-frame numbers mislead. Needs wine and i686-w64-mingw32-gcc.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
ml="$(cd "$here/../../.." && pwd)"
game="$(cd "$ml/.." && pwd)"
d3dx="${D3DX9_25:-/mnt/data/Games/Prefixes/samp/1/pfx/drive_c/windows/syswow64/d3dx9_25.dll}"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
dxvk="${DXVK_D3D9:-/usr/share/steam/compatibilitytools.d/proton-cachyos-slr/files/lib/wine/dxvk/i386-windows/d3d9.dll}"
d3dxmode=n; [ "$d3dx" = builtin ] && d3dxmode=b
export WINEPREFIX="${WINEPREFIX:-$work/prefix}" WINEDEBUG=-all WINEDLLOVERRIDES="d3dx9_25=$d3dxmode"
[ "$d3dxmode" = n ] && cp -L "$d3dx" "$work/d3dx9_25.dll"
if [ -f "$dxvk" ]; then
    cp "$dxvk" "$work/d3d9.dll"
    export WINEDLLOVERRIDES="d3dx9_25=$d3dxmode;d3d9=n" DXVK_LOG_LEVEL=none
    echo "renderer: DXVK ($dxvk)"
else
    echo "renderer: wined3d"
fi
cp "$game/lua51.dll" "$ml/lib/fixes/text_cache.dll" "$work/"
[ -n "${TEXTCACHEFIX_ASI:-}" ] && cp "$TEXTCACHEFIX_ASI" "$work/TextCacheFix.asi"
i686-w64-mingw32-gcc -O2 -o "$work/test.exe" "$here/text_cache_dll_test.c" -ld3d9
mlwin="Z:${ml//\//\\}"
cd "$work"   # a long WORKDIR path reaches Wine mangled: pass "."
run() { wine test.exe "$@" 2>&1 | grep -v 'MESA\|pci id' | tr -d '\r'; }
run check . "$mlwin" | tee out.txt
grep -q ' passed, 0 failed' out.txt
if [ "${1:-}" = --bench ]; then
    modes="stock dll"; [ -f TextCacheFix.asi ] && modes="$modes asi"
    for r in 1 2 3; do for m in $modes; do run "$m" . "$mlwin" 2000; done; done
fi
