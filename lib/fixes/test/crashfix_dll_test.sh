#!/usr/bin/env bash
# lib/fixes/native/crashfix.c against gta_sa.exe's own code under Wine, in a
# throwaway prefix, never the game's (crashfix_dll_test.c). The exe is only
# read; the copy that runs imports only KERNEL32 and its entry point only
# loads the test. Needs wine, python3 and i686-w64-mingw32-gcc.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
ml="$(cd "$here/../../.." && pwd)"
exe="${1:-$ml/../gta_sa.exe}"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WINEPREFIX="$work/prefix" WINEDEBUG=-all
i686-w64-mingw32-gcc -O2 -Wall -shared -static -o "$work/crashfix_dll_test.dll" "$here/crashfix_dll_test.c"
python3 -I - "$exe" "$work/gta_sa.exe" <<'PY'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read())
pe = struct.unpack_from('<I', d, 0x3C)[0]
nsec, optsz = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
opt, base = pe + 24, struct.unpack_from('<I', d, pe + 52)[0]
secs = [struct.unpack_from('<IIII', d, opt + optsz + 40 * i + 8) for i in range(nsec)]
def off(rva):
    return next(raw + rva - va for vsz, va, rsz, raw in secs if va <= rva < va + max(vsz, rsz))
imports = struct.unpack_from('<I', d, opt + 96 + 8)[0]
at, iat = off(imports), {}
while struct.unpack_from('<I', d, at + 12)[0]:
    oft, _, _, name, ft = struct.unpack_from('<IIIII', d, at)
    if d[off(name):off(name) + 12].upper() == b'KERNEL32.DLL':
        kernel32 = bytes(d[at:at + 20])
        for i in range(10000):
            e = struct.unpack_from('<I', d, off(oft) + 4 * i)[0]
            if not e: break
            if not e & 0x80000000: iat[bytes(d[off(e) + 2:off(e) + 40]).split(b'\0')[0]] = base + ft + 4 * i
    at += 20
first = off(imports)
d[first:first + 20] = kernel32                      # KERNEL32 alone
d[first + 20:first + 40] = bytes(20)
struct.pack_into('<II', d, opt + 96 + 8, imports, 40)
struct.pack_into('<II', d, opt + 96 + 8 * 13, 0, 0)  # no delay imports
entry = struct.unpack_from('<I', d, opt + 16)[0]
name, proc = base + entry + 48, base + entry + 70
lla, gpa, ep = iat[b'LoadLibraryA'], iat[b'GetProcAddress'], iat[b'ExitProcess']
stub = (b'\x68' + struct.pack('<I', name) + b'\xFF\x15' + struct.pack('<I', lla) + b'\x85\xC0\x74\x19'
        + b'\x68' + struct.pack('<I', proc) + b'\x50\xFF\x15' + struct.pack('<I', gpa) + b'\x85\xC0\x74\x09'
        + b'\xFF\xD0\x50\xFF\x15' + struct.pack('<I', ep)
        + b'\x6A\x03\xFF\x15' + struct.pack('<I', ep))
assert len(stub) == 48
stub += b'crashfix_dll_test.dll\0run\0'
d[off(entry):off(entry) + len(stub)] = stub
open(sys.argv[2], 'wb').write(d)
PY
cd "$work"
wine gta_sa.exe
