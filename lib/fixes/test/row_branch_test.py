"""No branch in gta_sa.exe lands inside a crashfix.dll row: a row's bytes
past its first instruction become its jump and NOPs, so a jump there runs
the middle of our jump (planesteer4 at 0x6CB1B1 did, and faulted).
    python3 -I row_branch_test.py <gta_sa.exe> <lib/fixes/native>
Needs objdump."""
import bisect, glob, os, re, subprocess, sys

exe, native = sys.argv[1], sys.argv[2]
# A branch from inside the row itself is replaced with it; shadow's jump
# sits in the padding before the function and the entry hops back to it.
ALLOWED = {('shadow', 0x70BDA0)}

rows = []
for path in glob.glob(os.path.join(native, '*.c')):
    src = re.sub(r'\\\n\s*', ' ', open(path, errors='replace').read())
    for m in re.finditer(r'\b(?:FJ|H)\((\w+),\s*(?:"\w*",\s*)?"[^"]*",\s*(0x[0-9A-Fa-f]+),\s*(\d+)', src):
        rows.append((m.group(1), int(m.group(2), 16), int(m.group(3))))
    for m in re.finditer(r'\bFP\((\w+),\s*"\w*",\s*KIND_\w+,\s*"[^"]*",\s*(0x[0-9A-Fa-f]+),\s*(\d+)', src):
        rows.append((m.group(1), int(m.group(2), 16), int(m.group(3))))
rows = [r for r in rows if 0x401000 <= r[1] < 0x900000]

dis = subprocess.run(['objdump', '-d', '-M', 'intel', '-j', '.text', exe], capture_output=True, text=True, check=True).stdout
targets = {}
for m in re.finditer(r'^\s+([0-9a-f]+):\t(?:[0-9a-f]{2} )+\s*\t(?:j\w+|call|loop\w*)\s+0x([0-9a-f]+)', dis, re.M):
    targets.setdefault(int(m.group(2), 16), []).append(int(m.group(1), 16))
keys = sorted(targets)

bad = 0
for key, va, size in rows:
    i = bisect.bisect_right(keys, va)
    while i < len(keys) and keys[i] < va + size:
        froms = [f for f in targets[keys[i]] if not va <= f < va + size]
        if froms and (key, keys[i]) not in ALLOWED:
            bad += 1
            print('  FAIL: %s (0x%X, %d bytes): 0x%X jumps to 0x%X inside it' % (key, va, size, froms[0], keys[i]))
        i += 1
print('rows and branches: %d rows, %d branched into' % (len(rows), bad))
sys.exit(1 if bad else 0)
