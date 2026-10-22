#!/usr/bin/env python3
"""Derive and verify samp.dll signatures offline, across several builds at once.

This is the tool that was missing. patterns.lua previously held six hand-written
"draft" signatures that had never been run against a samp.dll; four of them match
nothing at all and two match 49-62 times. Anything added to patterns.lua should
come out of this tool, with the match counts recorded in a comment.

Examples
--------
Identify every DLL you have:

    ./gen_pattern.py identify /path/to/*/samp.dll

Derive a pattern from a known offset in one build, then check it against all of
them (this is how the F1KeyHandler signature in patterns.lua was produced):

    ./gen_pattern.py derive R1/samp.dll 0x713D6 --length 33 \\
        --verify R1/samp.dll R5/samp.dll

Check a pattern you already have, and decode what it points at:

    ./gen_pattern.py find "A1 ?? ?? ?? ?? 85 C0 74 18 6A 70" *//samp.dll \\
        --extract rel32 --offset 28

Notes
-----
* All addresses printed are RVAs (offsets from the module base), matching what
  patterns.lua, version.lua and the cache store.
* R1 and R2 are packed (extra .poopN sections, entry point in the packer stub),
  but their .text is plain x86 on disk, so file-derived patterns work for them.
  Do not scan .poop2 at runtime: it is marked DISCARDABLE.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
import sys

# AddressOfEntryPoint -> version, same table as version.lua
ENTRYPOINTS = {
    0x0FDB60: "DLR1",
    0x31DF13: "R1",
    0x3195DD: "R2",
    0x0CC4D0: "R3",
    0x0CBCB0: "R4",
    0x0CBC90: "R5",
}

SCN_CNT_CODE = 0x00000020
SCN_MEM_DISCARDABLE = 0x02000000
SCN_MEM_EXECUTE = 0x20000000


class Section:
    def __init__(self, name, rva, vsize, rptr, rsize, chars):
        self.name = name
        self.rva = rva
        self.vsize = vsize
        self.rptr = rptr
        self.rsize = rsize
        self.chars = chars

    @property
    def code(self):
        return bool(self.chars & (SCN_CNT_CODE | SCN_MEM_EXECUTE))

    @property
    def discardable(self):
        return bool(self.chars & SCN_MEM_DISCARDABLE)

    @property
    def span(self):
        return self.vsize or self.rsize


class Image:
    """A PE32 file, addressable by RVA."""

    def __init__(self, path):
        self.path = path
        with open(path, "rb") as fh:
            self.data = fh.read()
        d = self.data
        e_lfanew = struct.unpack_from("<I", d, 0x3C)[0]
        if d[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise ValueError(f"{path}: not a PE file")
        fh_off = e_lfanew + 4
        nsec = struct.unpack_from("<H", d, fh_off + 2)[0]
        self.timestamp = struct.unpack_from("<I", d, fh_off + 4)[0]
        size_opt = struct.unpack_from("<H", d, fh_off + 16)[0]
        opt = fh_off + 20
        self.entry = struct.unpack_from("<I", d, opt + 16)[0]
        self.image_base = struct.unpack_from("<I", d, opt + 28)[0]
        self.size_of_image = struct.unpack_from("<I", d, opt + 56)[0]
        self.sections = []
        for i in range(nsec):
            s = opt + size_opt + i * 40
            name = d[s:s + 8].rstrip(b"\0").decode("latin1")
            vsize, rva, rsize, rptr = struct.unpack_from("<IIII", d, s + 8)
            chars = struct.unpack_from("<I", d, s + 36)[0]
            self.sections.append(Section(name, rva, vsize, rptr, rsize, chars))
        self.md5 = hashlib.md5(d).hexdigest()
        self.version = ENTRYPOINTS.get(self.entry, "UNKNOWN")

    # -- address translation -------------------------------------------------
    def rva_to_raw(self, rva):
        for s in self.sections:
            if s.rptr and s.rva <= rva < s.rva + max(s.span, s.rsize):
                off = rva - s.rva
                if off < s.rsize:
                    return s.rptr + off
        return None

    def raw_to_rva(self, raw):
        for s in self.sections:
            if s.rptr and s.rptr <= raw < s.rptr + s.rsize:
                return s.rva + (raw - s.rptr)
        return None

    def section_of_rva(self, rva):
        for s in self.sections:
            if s.rva <= rva < s.rva + s.span:
                return s
        return None

    def rva_in_image(self, rva):
        return self.section_of_rva(rva) is not None

    def read(self, rva, length):
        raw = self.rva_to_raw(rva)
        if raw is None:
            return None
        return self.data[raw:raw + length]

    def scan_sections(self, names=None):
        if names:
            return [s for s in self.sections if s.name in names]
        return [s for s in self.sections if s.code and not s.discardable and s.rsize]


# -- pattern helpers --------------------------------------------------------

def pattern_to_regex(pattern):
    out = b""
    for tok in pattern.split():
        if tok in ("?", "??"):
            out += b"."
        elif re.fullmatch(r"[0-9a-fA-F]{1,2}", tok):
            out += re.escape(bytes([int(tok, 16)]))
        else:
            raise ValueError(f"bad pattern token {tok!r}")
    return re.compile(out, re.DOTALL)


def find(img, pattern, sections=None, limit=64):
    """Return a list of RVAs where `pattern` matches."""
    rx = pattern_to_regex(pattern)
    hits = []
    for s in img.scan_sections(sections):
        blob = img.data[s.rptr:s.rptr + s.rsize]
        for m in rx.finditer(blob):
            hits.append(s.rva + m.start())
            if len(hits) >= limit:
                return hits
    return hits


# Same wildcarding rule as seed.lua: only dwords that decode to an address inside
# this image, and only behind an opcode that takes such an operand.
def _abs_operand_offset(b0, b1):
    if b0 in (0xA1, 0xA3, 0x68) or 0xB8 <= b0 <= 0xBF:
        return 1
    # ModRM mod=00 rm=101 -> the operand is a bare disp32
    if b0 in (0x8B, 0x89, 0x3B, 0x39, 0x85, 0xC6, 0xC7, 0xFF, 0x8A, 0x88) \
            and b1 is not None and (b1 & 0xC7) == 0x05:
        return 2
    return None


# rel32 branch operands (E8 call / E9 jmp / 0F 8x jcc) differ between builds too,
# so they get wildcarded when the target lands inside the image.
def _rel32_operand_offset(b0, b1):
    if b0 in (0xE8, 0xE9):
        return 1
    if b0 == 0x0F and b1 is not None and 0x80 <= b1 <= 0x8F:
        return 2
    return None


def derive(img, rva, length):
    buf = img.read(rva, length)
    if buf is None or len(buf) < length:
        raise ValueError(f"cannot read 0x{rva:X}+{length} from {img.path}")
    tokens = []
    i = 0
    while i < length:
        b1 = buf[i + 1] if i + 1 < length else None
        done = False

        at = _abs_operand_offset(buf[i], b1)
        if at is not None and i + at + 4 <= length:
            va = struct.unpack_from("<I", buf, i + at)[0]
            if img.rva_in_image(va - img.image_base):
                tokens += [f"{buf[i + k]:02X}" for k in range(at)]
                tokens += ["??"] * 4
                i += at + 4
                done = True

        if not done:
            at = _rel32_operand_offset(buf[i], b1)
            if at is not None and i + at + 4 <= length:
                rel = struct.unpack_from("<i", buf, i + at)[0]
                if img.rva_in_image(rva + i + at + 4 + rel):
                    tokens += [f"{buf[i + k]:02X}" for k in range(at)]
                    tokens += ["??"] * 4
                    i += at + 4
                    done = True

        if not done:
            tokens.append(f"{buf[i]:02X}")
            i += 1
    return " ".join(tokens)


def extract(img, rva, kind, offset):
    """Same semantics as resolver.lua's extract(), so --offset values can be
    copied straight into a patterns.lua descriptor."""
    site = rva + offset
    if kind is None:
        return site
    if kind == "rel32":
        # site points at the E8/E9 opcode, as it does in patterns.lua
        raw = img.read(site + 1, 4)
        if raw is None:
            return None
        return site + 5 + struct.unpack("<i", raw)[0]
    raw = img.read(site, 4)
    if raw is None:
        return None
    value = struct.unpack("<I", raw)[0]
    if kind == "imm32va":
        return value - img.image_base
    return value


# -- commands ---------------------------------------------------------------

def cmd_identify(args):
    for path in args.files:
        img = Image(path)
        print(f"{path}")
        print(f"  version={img.version} entry=0x{img.entry:X} SizeOfImage=0x{img.size_of_image:X} "
              f"TimeDateStamp=0x{img.timestamp:X} md5={img.md5}")
        for s in img.sections:
            flags = " ".join(f for f, on in
                             (("code", s.code), ("discardable", s.discardable)) if on)
            print(f"    {s.name:<8} rva=0x{s.rva:06X} vsize=0x{s.span:06X} "
                  f"raw=0x{s.rptr:06X} rsize=0x{s.rsize:06X} {flags}")
    return 0


def _report(pattern, files, extract_kind, offset, sections):
    bad = 0
    print(f'pattern: "{pattern}"')
    print(f"  ({len(pattern.split())} bytes, {pattern.split().count('??') + pattern.split().count('?')} wildcards)")
    for path in files:
        img = Image(path)
        hits = find(img, pattern, sections)
        tag = f"{img.version:<5}"
        if len(hits) == 1:
            value = extract(img, hits[0], extract_kind, offset)
            extra = ""
            if extract_kind or offset:
                extra = f" -> 0x{value:X}" if value is not None else " -> <unreadable>"
            sec = img.section_of_rva(hits[0])
            print(f"  {tag} 1 match  rva=0x{hits[0]:X} ({sec.name if sec else '?'}){extra}   {path}")
        else:
            bad += 1
            print(f"  {tag} {len(hits)} matches{'' if hits else ' (NOT FOUND)'}   {path}")
            for h in hits[:5]:
                print(f"        0x{h:X}")
    return bad


def cmd_derive(args):
    img = Image(args.file)
    length = args.length
    pattern = derive(img, args.rva, length)
    if args.widen:
        # Grow until the pattern is unique in the source image.
        while length <= args.max_length:
            pattern = derive(img, args.rva, length)
            hits = find(img, pattern, args.sections)
            if len(hits) == 1:
                break
            length += 8
    print(f"derived from {img.path} ({img.version}) at rva 0x{args.rva:X}, length {length}")
    bad = _report(pattern, args.verify or [args.file], args.extract, args.offset, args.sections)
    if bad:
        print(f"\n{bad} build(s) did not match uniquely -- widen with --length/--widen "
              f"or add a per-version override in patterns.lua")
    return 1 if bad else 0


def cmd_find(args):
    bad = _report(args.pattern, args.files, args.extract, args.offset, args.sections)
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def add_common(p):
        p.add_argument("--section", dest="sections", action="append",
                       help="restrict scanning to this section (repeatable); "
                            "default: executable, non-discardable sections")
        p.add_argument("--extract", choices=["rel32", "imm32va", "imm32"],
                       help="decode the operand at match+offset")
        p.add_argument("--offset", type=lambda s: int(s, 0), default=0,
                       help="byte offset from the match before extracting")

    p = sub.add_parser("identify", help="print version / sections for each DLL")
    p.add_argument("files", nargs="+")
    p.set_defaults(func=cmd_identify)

    p = sub.add_parser("derive", help="build a pattern from a known offset")
    p.add_argument("file")
    p.add_argument("rva", type=lambda s: int(s, 0))
    p.add_argument("--length", type=int, default=32)
    p.add_argument("--widen", action="store_true",
                   help="grow the length until the pattern is unique")
    p.add_argument("--max-length", type=int, default=128)
    p.add_argument("--verify", nargs="+", metavar="DLL",
                   help="also check the pattern against these builds")
    add_common(p)
    p.set_defaults(func=cmd_derive)

    p = sub.add_parser("find", help="check an existing pattern against DLLs")
    p.add_argument("pattern")
    p.add_argument("files", nargs="+")
    add_common(p)
    p.set_defaults(func=cmd_find)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
