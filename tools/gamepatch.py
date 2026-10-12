#!/usr/bin/env python3
"""Build game patches into a copy of main.dol, so the recompiled module (and the Android build,
which has no mod loader) run them as ordinary game code.

usage: tools/gamepatch.py SPEC.json [SPEC.json ...] [--dol IN] [--out-root DIR]
  SPEC     patches/game/*.json: sources (C / .S, built with clang for the Gekko), symbols, and
           call redirections (see patches/game/README.md)
  --dol    the original DOL (default extracted/SkiesOfArcadiaLegends/sys/main.dol); its SHA-1 must
           be the USA Rev 0 one unless --any-dol
  --out-root  game root to write (default build/game/GEAE8P-patched): sys/ is copied with the
           patched main.dol, files/ is a symlink to the original; patch.elf (for disassembly) and
           patches.txt (symbols and patched sites) go beside them

The patch code is linked into low memory (patches/game/cave.ld) and added as a new text section and
a new data section; the original sections are untouched apart from the rewritten call sites."""
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLVM = os.environ.get("LLVM_BIN", "/usr/lib/llvm-20/bin")
CLANG = os.environ.get("PATCH_CC", "clang-20")
DOL_SHA1 = "8c0e278126fa3b0173400fdb632038172743cc13"  # GEAE8P (USA Rev 0) main.dol
# MetroTRK's message handlers, dead without a debugger: the second code area (cave.ld).
TRK_CAVE = (0x802548E8, 0x80257074)
CFLAGS = ["--target=powerpc-unknown-eabi", "-mcpu=750", "-Os", "-fno-pic", "-ffreestanding",
          "-fno-builtin", "-fno-asynchronous-unwind-tables", "-Wall", "-Werror"]


class Dol:
    def __init__(self, data):
        self.data = bytearray(data)
        words = struct.unpack(">64I", data[:0x100])
        self.text = [[words[i], words[18 + i], words[36 + i]] for i in range(7)]       # off, addr, size
        self.dat = [[words[7 + i], words[25 + i], words[43 + i]] for i in range(11)]
        self.bss = list(words[54:56])
        self.entry = words[56]

    def locate(self, addr):
        for off, base, size in self.text + self.dat:
            if size and base <= addr < base + size:
                return off + addr - base
        raise ValueError(f"0x{addr:08X} isn't in the DOL")

    def word(self, addr):
        return struct.unpack(">I", self.data[self.locate(addr):][:4])[0]

    def set_word(self, addr, value):
        o = self.locate(addr)
        self.data[o:o + 4] = struct.pack(">I", value)

    def text_words(self):
        for off, base, size in self.text:
            for i in range(0, size, 4):
                yield base + i, struct.unpack(">I", self.data[off + i:off + i + 4])[0]

    def add_section(self, table, addr, payload):
        slot = next(i for i, s in enumerate(table) if s[2] == 0)
        for off, base, size in self.text + self.dat:
            if size and addr < base + size and base < addr + len(payload):
                raise ValueError(f"section at 0x{addr:08X} overlaps 0x{base:08X}")
        while len(self.data) % 32:
            self.data.append(0)
        table[slot] = [len(self.data), addr, len(payload)]
        self.data += payload

    def save(self):
        t, d = self.text, self.dat
        words = ([s[0] for s in t] + [s[0] for s in d] + [s[1] for s in t] + [s[1] for s in d] +
                 [s[2] for s in t] + [s[2] for s in d] + self.bss + [self.entry] + [0] * 7)
        self.data[:0x100] = struct.pack(">64I", *words)
        return bytes(self.data)


def bl(site, target):
    delta = target - site
    assert -0x2000000 <= delta < 0x2000000 and delta % 4 == 0
    return 0x48000001 | (delta & 0x03FFFFFC)


def bl_target(site, word):
    if word & 0xFC000003 != 0x48000001:
        return None
    li = word & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (site + li) & 0xFFFFFFFF


def run(*cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit(f"{' '.join(cmd)}\n{r.stdout}{r.stderr}")
    return r.stdout


def symbol_address(name, table):
    if name in table:
        return int(table[name], 16)
    if name.startswith("fn_") and len(name) == 11:
        return int(name[3:], 16)
    sys.exit(f"unknown game symbol {name}: add it to the spec's \"symbols\"")


def build(specs, work):
    objects, table, spec_dirs = [], {}, set()
    for path in specs:
        spec = json.load(open(path))
        base = os.path.dirname(os.path.abspath(path))
        spec_dirs.add(base)
        for name, value in spec.get("symbols", {}).items():
            if name in table and int(table[name], 16) != int(value, 16):
                sys.exit(f"{path}: symbol {name} = {value}, but an earlier spec says {table[name]}")
            table[name] = value
        for src in spec["sources"]:
            obj = os.path.join(work, f"{len(objects)}-{os.path.basename(src)}.o")
            run(CLANG, *CFLAGS, "-c", os.path.join(base, src), "-o", obj)
            objects.append(obj)
    undefined = set()
    for obj in objects:
        # Weak undefined symbols (optional hooks such as trace2d.c's) stay 0 unless defined.
        undefined |= {line.split()[-1] for line in run(f"{LLVM}/llvm-nm", "-u", obj).splitlines()
                      if line.split()[0] != "w"}
    defined = set()
    for obj in objects:
        defined |= {line.split()[-1] for line in run(f"{LLVM}/llvm-nm", "--defined-only", obj).splitlines()}
    defsyms = [f"--defsym={n}=0x{symbol_address(n, table):08X}" for n in sorted(undefined - defined)]
    script = os.path.join(ROOT, "patches", "game", "cave.ld")
    elf = os.path.join(work, "patch.elf")
    run("ld.lld", "-m", "elf32ppc", "-T", script, "-e", "0", *defsyms, "-o", elf, *objects)
    addresses = {}
    for line in run(f"{LLVM}/llvm-objdump", "-h", elf).splitlines():
        cols = line.split()
        if len(cols) >= 4 and cols[1] in (".text", ".data", ".trkcave"):
            addresses[cols[1]] = int(cols[3], 16)
    sections = {}
    for name in [n for n in (".text", ".data", ".trkcave") if n in addresses]:
        out = os.path.join(work, name[1:] + ".bin")
        run(f"{LLVM}/llvm-objcopy", "-O", "binary", f"--only-section={name}", elf, out)
        sections[name] = (addresses[name], open(out, "rb").read())
    symbols = {}
    for line in run(f"{LLVM}/llvm-nm", "--defined-only", elf).splitlines():
        addr, _, name = line.split()
        symbols[name] = int(addr, 16)
    return sections, symbols, table


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("specs", nargs="+")
    ap.add_argument("--dol", default=os.path.join(ROOT, "extracted/SkiesOfArcadiaLegends/sys/main.dol"))
    ap.add_argument("--out-root", default=os.path.join(ROOT, "build/game/GEAE8P-patched"))
    ap.add_argument("--any-dol", action="store_true")
    args = ap.parse_args()

    original = open(args.dol, "rb").read()
    if hashlib.sha1(original).hexdigest() != DOL_SHA1 and not args.any_dol:
        sys.exit(f"{args.dol} isn't the GEAE8P USA Rev 0 main.dol (SHA-1 {DOL_SHA1})")
    dol = Dol(original)
    os.makedirs(args.out_root, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        sections, symbols, table = build(args.specs, work)
        shutil.copy(os.path.join(work, "patch.elf"), os.path.join(args.out_root, "patch.elf"))

    log = []
    for path in args.specs:
        spec = json.load(open(path))
        for r in spec.get("redirect_sites", []):
            target, to = symbol_address(r["target"], table), symbols[r["to"]]
            for site in [int(a, 16) for a in r.get("sites", [r.get("site")])]:
                if bl_target(site, dol.word(site)) != target:
                    sys.exit(f"0x{site:08X} doesn't hold 'bl {r['target']}' (got 0x{dol.word(site):08X})")
                dol.set_word(site, bl(site, to))
                log.append(f"0x{site:08X}  bl {r['target']} -> bl {r['to']}")
        for r in spec.get("redirect_calls", []):
            target, to = symbol_address(r["target"], table), symbols[r["to"]]
            sites = [a for a, w in dol.text_words() if bl_target(a, w) == target]
            if not sites:
                sys.exit(f"no calls to {r['target']}")
            for site in sites:
                dol.set_word(site, bl(site, to))
                log.append(f"0x{site:08X}  bl {r['target']} -> bl {r['to']}")
            # A pointer to the function in data would bypass the wrapper; say so (unless the spec
            # redirects it with redirect_pointers).
            covered = {int(a, 16) for p in spec.get("redirect_pointers", [])
                       for a in p.get("addresses", [p.get("address")])}
            needle = struct.pack(">I", target)
            for off, base, size in dol.dat:
                chunk = dol.data[off:off + size]
                i = chunk.find(needle)
                while i >= 0:
                    if i % 4 == 0 and base + i not in covered:
                        print(f"warning: data at 0x{base + i:08X} points to {r['target']}")
                    i = chunk.find(needle, i + 1)

        for r in spec.get("redirect_pointers", []):
            # A function pointer in data (a handler table) that must reach the wrapper too.
            target, to = symbol_address(r["target"], table), symbols[r["to"]]
            for addr in [int(a, 16) for a in r.get("addresses", [r.get("address")])]:
                if dol.word(addr) != target:
                    sys.exit(f"0x{addr:08X} doesn't hold a pointer to {r['target']} (got 0x{dol.word(addr):08X})")
                dol.set_word(addr, to)
                log.append(f"0x{addr:08X}  .4byte {r['target']} -> .4byte {r['to']}")

    # The second code area (cave.ld) overwrites dead game code in place.
    if ".trkcave" in sections and sections[".trkcave"][1]:
        base, code = sections[".trkcave"]
        if base < TRK_CAVE[0] or base + len(code) > TRK_CAVE[1] or len(code) % 4:
            sys.exit(f".trkcave at 0x{base:08X} +0x{len(code):X} is outside 0x{TRK_CAVE[0]:08X}-0x{TRK_CAVE[1]:08X}")
        for i in range(0, len(code), 4):
            dol.set_word(base + i, struct.unpack(">I", code[i:i + 4])[0])
        log.append(f"0x{base:08X}  .trkcave 0x{len(code):X} bytes written in place")
    else:
        sections.pop(".trkcave", None)
    dol.add_section(dol.text, *sections[".text"])
    dol.add_section(dol.dat, *sections[".data"])
    patched = dol.save()

    sys_out = os.path.join(args.out_root, "sys")
    # The DOL's folder is copied whole (apploader, bi2, boot, fst beside it): only an extracted
    # disc's sys/, not e.g. a Downloads folder the DOL was saved in
    dol_dir = os.path.dirname(os.path.abspath(args.dol))
    if os.path.basename(dol_dir) != "sys":
        sys.exit(f"--dol must be the main.dol in an extracted disc's sys/ folder, not {dol_dir}")
    if os.path.isdir(sys_out):
        shutil.rmtree(sys_out)
    shutil.copytree(dol_dir, sys_out)
    with open(os.path.join(sys_out, "main.dol"), "wb") as f:
        f.write(patched)
    files = os.path.join(args.out_root, "files")
    source_files = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(args.dol))), "files")
    if not os.path.lexists(files):
        os.symlink(os.path.realpath(source_files), files)
    report = os.path.join(args.out_root, "patches.txt")
    with open(report, "w") as f:
        f.write("".join(f"{n} 0x{a:08X} +0x{len(b):X}\n" for n, (a, b) in sections.items()))
        f.write("".join(f"{n} = 0x{a:08X}\n" for n, a in sorted(symbols.items(), key=lambda x: x[1])))
        f.write("\n".join(log) + "\n")
    print(f"{len(log)} call sites patched; text 0x{len(sections['.text'][1]):X} bytes, data "
          f"0x{len(sections['.data'][1]):X} bytes -> {args.out_root} (sha1 {hashlib.sha1(patched).hexdigest()})")


if __name__ == "__main__":
    main()
