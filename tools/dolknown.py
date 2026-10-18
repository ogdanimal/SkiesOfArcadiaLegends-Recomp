#!/usr/bin/env python3
"""Print `known-change` lines (tools/mkdolpatch.py KNOWN_CHANGES) for a romhack's code changes: every
run where HACKED.dol's header or text sections differ from ORIGINAL.dol, under NAME. The frontend
undoes them on import when all of NAME's bytes are found, so the hack's data still plays on the
release's module (the release has an option of its own for what the code did).

usage: tools/dolknown.py ORIGINAL.dol HACKED.dol NAME >> release/<game>.known"""
import struct
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    original = open(sys.argv[1], "rb").read()
    hacked = open(sys.argv[2], "rb").read()
    if original[:0x100] != hacked[:0x100]:
        sys.exit("the DOL headers differ (sections moved or added): not a change the release can undo")
    ranges = [(0, 0x100)]
    for i in range(7):
        offset, size = struct.unpack_from(">I", original, i * 4)[0], struct.unpack_from(">I", original, 0x90 + i * 4)[0]
        if size:
            ranges.append((offset, offset + size))
    for lo, hi in ranges:
        i = lo
        while i < hi:
            if original[i] == hacked[i]:
                i += 1
                continue
            start = end = i
            while i < hi and i - end < 16:  # runs merged across gaps of up to 16 bytes
                if original[i] != hacked[i]:
                    end = i + 1
                i += 1
            i = end
            print(f"known-change {sys.argv[3]} {start:x} {hacked[start:end].hex()} {original[start:end].hex()}")


if __name__ == "__main__":
    main()
