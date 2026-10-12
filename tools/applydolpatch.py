#!/usr/bin/env python3
"""Apply a whole-file DOL patch (`moderngekko-dol-file-patch 1`, made by tools/mkdolpatch.py) to the
original main.dol, checking the input and output SHA-256 like the launcher does (tools/dol_patch.cpp).
CI uses it to make the DOL the module is generated from, without the launcher.

usage: tools/applydolpatch.py ORIGINAL.dol PATCH.dolpatch OUT.dol"""
import hashlib
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    original = open(sys.argv[1], "rb").read()
    lines = open(sys.argv[2]).read().splitlines()
    if lines[0] != "moderngekko-dol-file-patch 1":
        sys.exit("not a moderngekko-dol-file-patch 1 file")
    header = dict(line.split(" ", 1) for line in lines[1:4])
    if hashlib.sha256(original).hexdigest() != header["input-sha256"]:
        sys.exit("the DOL isn't the one this patch is for (input SHA-256 differs)")
    out = bytearray(original)
    size = int(header["output-size"])
    out.extend(bytes(max(0, size - len(out))))
    del out[size:]
    for line in lines[4:]:
        offset, data = line.split(" ")
        offset, data = int(offset, 16), bytes.fromhex(data)
        out[offset:offset + len(data)] = data
    if hashlib.sha256(out).hexdigest() != header["output-sha256"]:
        sys.exit("patched DOL has the wrong SHA-256")
    open(sys.argv[3], "wb").write(out)
    print(f"{sys.argv[3]}: {header['output-sha256'][:8]}")


if __name__ == "__main__":
    main()
