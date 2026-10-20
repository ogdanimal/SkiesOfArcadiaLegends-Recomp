#!/usr/bin/env python3
"""AKLZ decompressor (Skies of Arcadia Legends, GameCube).

Format (reverse-engineered by testing against disc files; see notes):
  0x00  "AKLZ"            magic
  0x04  7E 3F 51 64 3D CC CC CD   fixed signature bytes
  0x0C  u32 BE            decompressed size
  0x10  LZSS stream: Okumura-style, 4 KiB ring buffer starting at 0xFEE,
        flag byte read LSB-first (1 = literal byte, 0 = back-reference),
        reference = 2 bytes: offset = b0 | (b1 & 0xF0) << 4, length = (b1 & 0x0F) + 3
"""
import struct, sys

def decompress(data: bytes) -> bytes:
    if data[:4] != b"AKLZ":
        raise ValueError("not AKLZ")
    size = struct.unpack(">I", data[12:16])[0]
    ring = bytearray(4096)
    r = 0xFEE
    out = bytearray()
    i = 16
    flags = 0
    while len(out) < size and i < len(data):
        flags >>= 1
        if not flags & 0x100:
            flags = data[i] | 0xFF00
            i += 1
        if flags & 1:
            c = data[i]; i += 1
            out.append(c); ring[r] = c; r = (r + 1) & 0xFFF
        else:
            b0, b1 = data[i], data[i + 1]; i += 2
            off = b0 | ((b1 & 0xF0) << 4)
            for k in range((b1 & 0x0F) + 3):
                c = ring[(off + k) & 0xFFF]
                out.append(c); ring[r] = c; r = (r + 1) & 0xFFF
    return bytes(out[:size])

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: aklz.py <in> <out>")
    out = decompress(open(sys.argv[1], "rb").read())
    open(sys.argv[2], "wb").write(out)
    print(f"{sys.argv[1]}: {len(out)} bytes")
