#!/usr/bin/env python3
"""Read the ISO 9660 file system of a Dreamcast GD-ROM's high-density area (track 3 of a GDI, raw
2352-byte sectors starting at LBA 45000).

usage: gdrom.py DISC.gdi list | extract OUTDIR [NAME ...]"""
import os
import struct
import sys

SECTOR = 2352


class GdRom:
    def __init__(self, gdi):
        base = os.path.dirname(os.path.abspath(gdi))
        self.tracks = []
        with open(gdi) as f:
            lines = [l.split() for l in f.read().splitlines()[1:] if l.strip()]
        for t in lines:
            lba, ttype, size, name = int(t[1]), int(t[2]), int(t[3]), t[4].strip('"')
            self.tracks.append((lba, ttype, size, os.path.join(base, name)))
        self.data = [t for t in self.tracks if t[1] == 4 and t[0] >= 45000][-1]
        self.f = open(self.data[3], "rb")

    def read(self, lba, count=1):
        out = bytearray()
        start, _, size, _ = self.data
        for i in range(count):
            self.f.seek((lba + i - start) * size + (16 if size == 2352 else 0))
            out += self.f.read(2048)
        return bytes(out)

    def read_bytes(self, lba, length):
        return self.read(lba, (length + 2047) // 2048)[:length]

    def walk(self, lba=None, length=None, path=""):
        if lba is None:
            pvd = self.read(self.data[0] + 16)
            assert pvd[1:6] == b"CD001", "no ISO 9660 volume"
            root = pvd[156:156 + 34]
            lba, length = struct.unpack("<I", root[2:6])[0], struct.unpack("<I", root[10:14])[0]
        data = self.read_bytes(lba, length)
        pos = 0
        while pos < len(data):
            n = data[pos]
            if n == 0:
                pos = (pos // 2048 + 1) * 2048
                continue
            rec = data[pos:pos + n]
            pos += n
            ext = struct.unpack("<I", rec[2:6])[0]
            size = struct.unpack("<I", rec[10:14])[0]
            flags = rec[25]
            name = rec[33:33 + rec[32]]
            if name in (b"\0", b"\1"):
                continue
            name = name.decode("latin-1").split(";")[0]
            full = f"{path}/{name}" if path else name
            if flags & 2:
                yield from self.walk(ext, size, full)
            else:
                yield full, ext, size


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    g = GdRom(sys.argv[1])
    if sys.argv[2] == "list":
        for name, lba, size in g.walk():
            print(f"{lba:7d} {size:10d} {name}")
    elif sys.argv[2] == "extract":
        out, want = sys.argv[3], set(n.upper() for n in sys.argv[4:])
        for name, lba, size in g.walk():
            if want and name.upper() not in want and os.path.basename(name).upper() not in want:
                continue
            # The names come from the disc: one that leaves OUTDIR (../, an absolute path) is skipped
            dest = os.path.join(out, name)
            root = os.path.realpath(out)
            if os.path.commonpath([root, os.path.realpath(dest)]) != root:
                print(f"skipped {name!r}: outside {out}", file=sys.stderr)
                continue
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "wb") as f:
                f.write(g.read_bytes(lba, size))


if __name__ == "__main__":
    main()
