#!/usr/bin/env python3
"""List the units of Dreamcast Manatee MLT (SMLT) files: type, bank number, sound-RAM address, size,
and file offset / size of the data the file carries (offset 0xFFFFFFFF: none).

usage: mltinfo.py FILE.MLT [...]"""
import struct
import sys


def units(data):
    assert data[:4] == b"SMLT", "not an MLT"
    ver, count = struct.unpack("<II", data[4:12])
    base = 0x20 if ver >= 1 else 0x10
    for i in range(count):
        e = data[base + i * 0x20:base + i * 0x20 + 0x20]
        kind = e[:4].decode("latin-1")
        num, addr, size, off, dsize = struct.unpack("<IIIII", e[4:24])
        yield kind, num, addr, size, off, dsize


if __name__ == "__main__":
    for path in sys.argv[1:]:
        data = open(path, "rb").read()
        print(path)
        for kind, num, addr, size, off, dsize in units(data):
            has = f"data @{off:#x} +{dsize:#x}" if off != 0xFFFFFFFF and dsize else "-"
            print(f"  {kind} {num:2d}  ram {addr:#08x} size {size:#08x}  {has}")
