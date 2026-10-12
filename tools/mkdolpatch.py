#!/usr/bin/env python3
"""Write a whole-file DOL patch (ModernGekko's `moderngekko-dol-file-patch 1` format, applied by
tools/dol_patch.cpp in the launcher and the runner) that turns the original main.dol into a patched
one, e.g. the widescreen DOL from tools/gamepatch.py. Players then supply their own disc and the
frontend patches its DOL to exactly the DOL the recomp module was built from.

usage: tools/mkdolpatch.py ORIGINAL.dol PATCHED.dol OUT.dolpatch

The patch holds only the bytes that differ (runs merged across gaps of up to 16 bytes), so it carries
the patch's own code and data and the rewritten call sites, not the game's code."""
import hashlib
import sys

GAP = 16


def main():
    if len(sys.argv) != 4:
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
    with open(sys.argv[3], "w", newline="\n") as out:
        out.write("moderngekko-dol-file-patch 1\n")
        out.write(f"input-sha256 {hashlib.sha256(original).hexdigest()}\n")
        out.write(f"output-sha256 {hashlib.sha256(patched).hexdigest()}\n")
        out.write(f"output-size {len(patched)}\n")
        for offset, data in runs:
            out.write(f"{offset:x} {data.hex()}\n")
    total = sum(len(d) for _, d in runs)
    print(f"{len(runs)} runs, {total} bytes -> {sys.argv[3]}")


if __name__ == "__main__":
    main()
