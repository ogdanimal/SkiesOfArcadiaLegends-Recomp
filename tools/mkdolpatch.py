#!/usr/bin/env python3
"""Write a whole-file DOL patch (ModernGekko's `moderngekko-dol-file-patch 1` format, applied by
tools/dol_patch.cpp in the launcher and the runner) that turns the original main.dol into a patched
one, e.g. the widescreen DOL from tools/gamepatch.py. Players then supply their own disc and the
frontend patches its DOL to exactly the DOL the recomp module was built from.

usage: tools/mkdolpatch.py [--fst ORIGINAL_FST.bin] ORIGINAL.dol PATCHED.dol OUT.dolpatch [KNOWN_CHANGES]

The patch holds only the bytes that differ (runs merged across gaps of up to 16 bytes), so it carries
the patch's own code and data and the rewritten call sites, not the game's code. It also holds the
code hashes of both DOLs (header and text sections), which let the frontend take a romhack that
changes only the game's data. Where the runs do change the original's data (a pointer in a table),
the original bytes go in `data-original` lines: a romhack that changes them too is refused. KNOWN_CHANGES: lines
`known-change NAME OFFSET FOUND ORIGINAL` (tools/dolknown.py) for romhacks' code changes the release
undoes; checked against ORIGINAL and copied in. The original's data hash (`input-data-sha256`) and,
with --fst, its disc's file table hash (`input-fst-sha256`) tell a romhack from the original disc, so
its save states get a folder of their own (tools/dol_patch.cpp RomhackTag)."""
import hashlib
import struct
import sys

GAP = 16


def code_sha256(dol):
    """The header and every text section (tools/dol_patch.cpp DolCodeSha256)."""
    code = bytearray(dol[:0x100])
    for i in range(7):
        offset, size = struct.unpack_from(">I", dol, i * 4)[0], struct.unpack_from(">I", dol, 0x90 + i * 4)[0]
        if size:
            code += dol[offset:offset + size]
    return hashlib.sha256(code).hexdigest()


def data_sha256(dol):
    """Every data section, in order (tools/dol_patch.cpp DolDataSha256)."""
    data = bytearray()
    for i in range(7, 18):
        offset, size = struct.unpack_from(">I", dol, i * 4)[0], struct.unpack_from(">I", dol, 0x90 + i * 4)[0]
        if size:
            data += dol[offset:offset + size]
    return hashlib.sha256(data).hexdigest()


def code_ranges(dol):
    """File ranges a data romhack can't change: the header and the text sections."""
    ranges = [(0, 0x100)]
    for i in range(7):
        offset, size = struct.unpack_from(">I", dol, i * 4)[0], struct.unpack_from(">I", dol, 0x90 + i * 4)[0]
        if size:
            ranges.append((offset, offset + size))
    return ranges


def main():
    fst = None
    if len(sys.argv) > 2 and sys.argv[1] == "--fst":
        fst = open(sys.argv[2], "rb").read()
        del sys.argv[1:3]
    if len(sys.argv) not in (4, 5):
        sys.exit(__doc__)
    original = open(sys.argv[1], "rb").read()
    patched = open(sys.argv[2], "rb").read()
    padded = original + bytes(max(0, len(patched) - len(original)))
    runs = []
    i = 0
    while i < len(patched):
        if padded[i] == patched[i]:
            i += 1
            continue
        start = end = i
        while i < len(patched):
            if padded[i] != patched[i]:
                end = i + 1
                i += 1
            elif i - end < GAP:
                i += 1
            else:
                break
        runs.append((start, patched[start:end]))
    code = code_ranges(original)
    data_bytes = sorted(b for offset, data in runs for b in range(offset, offset + len(data))
                        if b < len(original) and not any(lo <= b < hi for lo, hi in code))
    data_original = []  # (offset, original bytes), contiguous
    for b in data_bytes:
        if data_original and data_original[-1][0] + len(data_original[-1][1]) == b:
            data_original[-1] = (data_original[-1][0], data_original[-1][1] + original[b:b + 1])
        else:
            data_original.append((b, original[b:b + 1]))
    known = []
    if len(sys.argv) == 5:
        for line in open(sys.argv[4]).read().splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            key, name, offset, found, orig = line.split(" ")
            offset, found, orig = int(offset, 16), bytes.fromhex(found), bytes.fromhex(orig)
            if key != "known-change" or len(found) != len(orig) or original[offset:offset + len(orig)] != orig:
                sys.exit(f"known change {name} at {offset:x} doesn't fit the original DOL")
            if not all(any(lo <= b < hi for lo, hi in code) for b in range(offset, offset + len(orig))):
                sys.exit(f"known change {name} at {offset:x} isn't in the code")
            if any(offset < o + len(d) and o < offset + len(orig) for o, d in runs):
                sys.exit(f"known change {name} at {offset:x} overlaps the patch")
            known.append(line)
    with open(sys.argv[3], "w", newline="\n") as out:
        out.write("moderngekko-dol-file-patch 1\n")
        out.write(f"input-sha256 {hashlib.sha256(original).hexdigest()}\n")
        out.write(f"output-sha256 {hashlib.sha256(patched).hexdigest()}\n")
        out.write(f"output-size {len(patched)}\n")
        out.write(f"input-size {len(original)}\n")
        out.write(f"input-code-sha256 {code_sha256(original)}\n")
        out.write(f"output-code-sha256 {code_sha256(patched)}\n")
        out.write(f"input-data-sha256 {data_sha256(original)}\n")
        if fst is not None:
            out.write(f"input-fst-sha256 {hashlib.sha256(fst).hexdigest()}\n")
        for offset, data in data_original:
            out.write(f"data-original {offset:x} {data.hex()}\n")
        for line in known:
            out.write(line + "\n")
        for offset, data in runs:
            out.write(f"{offset:x} {data.hex()}\n")
    total = sum(len(d) for _, d in runs)
    print(f"{len(runs)} runs, {total} bytes, {len(known)} known changes -> {sys.argv[3]}")


if __name__ == "__main__":
    main()
